# Generic unix/gcc Makefile for abcMIDI package
#
#
# compilation #ifdefs - you need to compile with these defined to get
#                       the code to compile with PCC.
#
# NOFTELL in midifile.c and tomidi.c selects a version of the file-writing
#         code which doesn't use file seeking.
#
# PCCFIX in mftext.c midifile.c midi2abc.c
#        comments out various things that aren't available in PCC
#
# ANSILIBS includes some ANSI header files (which gcc can live without,
#          but other compilers may want).
#
# USE_INDEX causes index() to be used instead of strchr(). This is needed
#           by some pre-ANSI C compilers.
#
# ASCTIME causes asctime() to be used instead of strftime() in pslib.c.
#         If ANSILIBS is not set, neither routine is used.
#
# KANDR selects functions prototypes without argument prototypes.
#       currently yaps will only compile in ANSI mode.
#
#
# On running make, you may get the mysterious message :
#
# ', needed by `parseabc.o'. Stop `abc.h
#
# This means you are using GNU make and this file is in DOS text format. To
# cure the problem, change this file from using PC-style end-of-line (carriage
# return and line feed) to unix style end-of-line (line feed).

CC=x86_64-conda_cos6-linux-gnu-cc
CFLAGS=-c -DANSILIBS
LNK=x86_64-conda_cos6-linux-gnu-cc

all : abc2midi midi2abc abc2abc mftext yaps

abc2midi : parseabc.o store.o genmidi.o midifile.o queues.o parser2.o
	$(LNK) -o abc2midi parseabc.o store.o genmidi.o queues.o \
	parser2.o midifile.o

abc2abc : parseabc.o toabc.o
	$(LNK) -o abc2abc parseabc.o toabc.o

midi2abc : midifile.o midi2abc.o
	$(LNK) midifile.o midi2abc.o -o midi2abc

mftext : midifile.o mftext.o crack.o
	$(LNK) midifile.o mftext.o crack.o -o mftext

yaps : parseabc.o yapstree.o drawtune.o debug.o pslib.o position.o parser2.o
	$(LNK) -o yaps parseabc.o yapstree.o drawtune.o debug.o \
	position.o pslib.o parser2.o

parseabc.o : parseabc.c abc.h parseabc.h
	$(CC) $(CFLAGS) parseabc.c

parser2.o : parser2.c abc.h parseabc.h parser2.h
	$(CC) $(CFLAGS) parser2.c

toabc.o : toabc.c abc.h parseabc.h
	$(CC) $(CFLAGS) toabc.c

# could use -DNOFTELL here
genmidi.o : genmidi.c abc.h midifile.h genmidi.h
	$(CC) $(CFLAGS) genmidi.c

store.o : store.c abc.h parseabc.h midifile.h genmidi.h
	$(CC) $(CFLAGS) store.c

queues.o : queues.c genmidi.h
	$(CC) $(CFLAGS) queues.c

# could use -DNOFTELL here
midifile.o : midifile.c midifile.h
	$(CC) $(CFLAGS) midifile.c

midi2abc.o : midi2abc.c midifile.h
	$(CC) $(CFLAGS) midi2abc.c

crack.o : crack.c
	$(CC) $(CFLAGS) crack.c

mftext.o : mftext.c midifile.h
	$(CC) $(CFLAGS) mftext.c

# objects needed by yaps
#
yapstree.o: yapstree.c abc.h parseabc.h structs.h drawtune.h
	$(CC) $(CFLAGS) yapstree.c

drawtune.o: drawtune.c structs.h sizes.h abc.h drawtune.h
	$(CC) $(CFLAGS) drawtune.c

pslib.o: pslib.c drawtune.h
	$(CC) $(CFLAGS) pslib.c

position.o: position.c abc.h structs.h sizes.h
	$(CC) $(CFLAGS) position.c

debug.o: debug.c structs.h abc.h
	$(CC) $(CFLAGS) debug.c

clean :
	rm *.o abc2midi midi2abc abc2abc mftext
