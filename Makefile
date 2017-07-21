FLAGS = -Wall -std=gnu99
IMAGES = trace/deleteddirectory.txt trace/deletedfile.txt trace/emptydisk.txt trace/hardlink.txt trace/largefile.txt trace/onedirectory.txt trace/onefile.txt trace/twolevel.txt
ASSIGNMENTFILES = ext2_ls ext2_mkdir ext2_rm ext2_cp ext2_ln 
UNFINISHED = ext2_rm_bonus
DEPENDENCIES = helper.h ext2.h


all : trace imgtrc ${ASSIGNMENTFILES}

readimagek : readimage_keegan.c
	gcc ${FLAGS} -o $@ $^

readimagel : readimage_luke.c
	gcc ${FLAGS} -o $@ $^

ext2_cp : ext2_cp.c
	gcc ${FLAGS} -o $@ $^
    
ext2_ln : ext2_cp.c
	gcc ${FLAGS} -o $@ $^

trace :
	mkdir trace

imgtrc : ${IMAGES}

trace/%.txt : %.img
	xxd $<>trace/$*.txt
  
clean: cleantrc cleandumps
	rm -f *.exe *.o *~ .*~

cleandumps:
	rm -f *.stackdump
  
cleantrc:
	rm -f trace/*

%.o: %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

ext2_%: ext2_%.o helper.o
	gcc ${FLAGS} -o $@ $^
  
