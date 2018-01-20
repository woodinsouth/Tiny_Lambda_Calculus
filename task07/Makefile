CC = gcc
CLIB     = 
CFLAGS = -g3 -DGCC
all: control
COMOBJ = y.tab.o lex.yy.o

# ------------------------------------------------------------

.c.obj:
	$(CC) $(INCLUDE) -c $(CFLAGS)  $*.c

#------------------------------------------------------------

lex.yy.c:  control.l
	flex  -i control.l

y.tab.c:  control.y
	byacc -tdv control.y
#------------------------------------------------------------

control:   $(COMOBJ) 
	$(CC) -o control $(LIB) $(COMOBJ) $(CLIB)

# ----------------------------------------------------------------------

clean: 
	rm y.tab.c y.tab.h *.o lex.yy.c
#
lex.yy.o:      lex.yy.c y.tab.h
y.tab.obj:      y.tab.c y.tab.h
y.tab.h:        control.y

