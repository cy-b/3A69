#include "helper.h"


int create_hard_link(struct ext2_inode *dir_inode, char *link_file_path, char *source_file_path);
int create_soft_link(struct ext2_inode *dir_inode, char *link_file_path, char *source_file_path);
unsigned int create_softlink_file(char *source_file_path);
struct ext2_inode *get_inode_for_file_path(char *source_file_path);


int main(int argc, char **argv) {
	char *link_file_path;
	char *source_file_path;
	// flag for -s command
	char soft_link = 0;
	// check input argument
	if (argc == 4) {
		// create hard link
		link_file_path = argv[2];
		source_file_path = argv[3];
    } else if (argc == 5 && !strcmp("-s", argv[2])) {
    	// create soft link
    	link_file_path = argv[3];
    	source_file_path = argv[4];
    	soft_link = 1;
    } else {
        fprintf(stderr, "Usage: ext2_ls <image file name> [-s] <link file path> <target absolute path>\n");
        exit(1);
    }

	// check if the disk image is valid
    int fd = open(argv[1], O_RDWR);
    init(fd);

	int path_index = 0;
	char token[EXT2_NAME_LEN];

	// first check if link file path exists and if it's valid
	path_index = get_next_token(token, link_file_path, path_index);
	if (path_index == -1) {
		fprintf(stderr, "ln: %s: No such file or directory\n", link_file_path);
		return ENOENT;
	}
	if (strlen(link_file_path) == 1 && link_file_path[0] == '/') {
		fprintf(stderr, "ln: %s: Directory exists\n", link_file_path);
		return ENOENT;
	}

	struct ext2_inode *current_inode = &(inode_table[EXT2_ROOT_INO - 1]);
	struct ext2_inode *last_inode;
	int size = strlen(link_file_path);
	while (path_index < size) {
		last_inode = current_inode;
		path_index = get_next_token(token, link_file_path, path_index);

		if (path_index == -1) {
			// path too long
			fprintf(stderr, "ln: %s: No such file or directory\n", link_file_path);
			return ENOENT;
		}

		current_inode = find_inode_2(token, strlen(token), current_inode, inode_table, disk);
		if (!current_inode) {
			// reach not existing path component
			if (path_index != size) {
				// the component is in the middle of the path
				printf("Here\n");
				fprintf(stderr, "ln: %s: No such file or directory\n", link_file_path);
				return ENOENT;
			} else {
				// the component is at the end of the path
				// the file path does not exist yet, but valid
				if (soft_link) {
					return create_soft_link(last_inode, link_file_path, source_file_path);
				} else {
					// hard link will check for source file path, may return error value
					return create_hard_link(last_inode, link_file_path, source_file_path);
				}
			}
		}

		if (current_inode->i_mode & EXT2_S_IFDIR) {
			// directory component in the path
			if (path_index == size || path_index + 1 == size) {
				// the component is at the end of path
				// directory exists at link path
				printf("HERE\n");
				fprintf(stderr, "ln: %s: Directory exists\n", link_file_path);
				return EISDIR;
			} else {
				// read slash
				path_index++;
			}
		}

		if (current_inode->i_mode & EXT2_S_IFLNK) {
			// reach a file
			if (path_index != size) {
				// file in the middle of path -> invalid path
				fprintf(stderr, "ln: %s: No such file or directory\n", link_file_path);
				return ENOENT;
			} else {	
				// file at the end of path, link file path already exists
				fprintf(stderr, "ln: %s: File exists\n", link_file_path);
				return EEXIST;
			}
		}
	}
	return 0;
}

/*
 * Create a hard link in the given directory inode, with the source file path given. 
 * 
 */
int create_hard_link(struct ext2_inode *dir_inode, char *link_file_path, char *source_file_path) {
	printf("create_hard_link\n");
	// get source_file_inode at given path
	struct ext2_inode *source_file_inode = get_inode_for_file_path(source_file_path);
	if (source_file_inode) {
		// source path is valid
		if (source_file_inode->i_mode & EXT2_S_IFDIR) {
			// source is directory
			fprintf(stderr, "ln: %s: Directory exists\n", source_file_path);
			return EISDIR;
		} else {
			// source is file
			unsigned int source_inode_number = inode_number(source_file_inode);
			char *file_name = get_file_name(link_file_path);
			// create the directory entry
			create_directory_entry(dir_inode, source_inode_number, file_name, 0);
			// increment link count for the file inode
			source_file_inode->i_links_count++;
		}
	} else {
		// file path invalid
		fprintf(stderr, "ln: %s: No such file or directory\n", link_file_path);
		return ENOENT;
	}
	return 0;
}

/*
 * Create the soft link file in side given directory inode with content as the given
 * source file path.
 */
int create_soft_link(struct ext2_inode *dir_inode, char *link_file_path, char *source_file_path) {
	printf("create_soft\n");
	unsigned link_file_inode_number = create_softlink_file(source_file_path);
	char *link_file_name = get_file_name(link_file_path);
	create_directory_entry(dir_inode, link_file_inode_number, link_file_name, 1);	
	return 0;
}


/* 
 * Create a link file with the content as given source path.
 * Return the inode number of the created file.
 */
unsigned int create_softlink_file(char *source_file_path) {
	printf("create_softlink_file\n");
	// allocate inode
	unsigned int source_file_inode_number = allocate_inode();
	printf("Inode number is %d\n", source_file_inode_number);
	struct ext2_inode *source_inode = &(inode_table[source_file_inode_number - 1]);

	// set inode info
	source_inode->i_mode = EXT2_S_IFLNK;
	source_inode->i_size = strlen(source_file_path);
	source_inode->i_links_count = 1;

	unsigned int sector_needed = source_inode->i_size / 512;
	if (source_inode->i_size % 512) {
		sector_needed++;
	}
	sector_needed += sector_needed % 2;
	source_inode->i_blocks = sector_needed;
	
	// initiate indirect table if needed
	unsigned int *single_indirect = NULL;
	if (sector_needed > 24) {
		source_inode->i_block[12] = allocate_data_block();
		single_indirect = (unsigned int *) (disk + source_inode->i_block[12] * EXT2_BLOCK_SIZE);
	}

	// writing path into content blocks
	unsigned int block_count = 0;
	unsigned int index = 0;
	unsigned int block, num;
	char *block_pointer;
	while (sector_needed > 0) {
		// allocate content block
		block = allocate_data_block();
		if (block_count < 12) {
			source_inode->i_block[block_count] = block;
		} else {
			single_indirect[block_count - 12] = block;
		}
		block_count++;

		block_pointer = (char *) (disk + block * EXT2_BLOCK_SIZE);
		// determine the size to write
		num = source_inode->i_size - index;
		if (num > EXT2_BLOCK_SIZE) {
			num = EXT2_BLOCK_SIZE;
		}
		// copy content
		for (int i=0; i < num; i++) {
			block_pointer[i] = source_file_path[index++];
		}

		sector_needed -= 2;
	}

	// set 0 after the last used node
	if (block_count <= 12) {
		source_inode->i_block[block_count] = 0;
	} else {
		single_indirect[block_count - block_count] = 0;
	}

	return source_file_inode_number;
}

struct ext2_inode *get_inode_for_file_path(char *source_file_path) {
	printf("get_inode_for_file_path\n");
	int path_index = 0;
	char token[EXT2_NAME_LEN];

	// first check if first file path exists and if it's valid
	path_index = get_next_token(token, source_file_path, path_index);
	if (path_index == -1) {
		return NULL;
	}

	struct ext2_inode *current_inode = &(inode_table[EXT2_ROOT_INO - 1]);
	struct ext2_inode *last_inode;
	int size = strlen(source_file_path);
	while (path_index < size) {
		last_inode = current_inode;
		path_index = get_next_token(token, source_file_path, path_index);

		if (path_index == -1) {
			// path too long
			return NULL;
		}

		current_inode = find_inode_2(token, strlen(token), current_inode, inode_table, disk);
		// reach not existing
		if (!current_inode) {
			return NULL;
		}

		if (current_inode->i_mode & EXT2_S_IFDIR) {
			path_index++;
		}

		if (current_inode->i_mode & EXT2_S_IFLNK && path_index != size) {
			return NULL;
		}
	}

	return current_inode;
}

