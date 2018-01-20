
CFLAGS	= -v -O 
INCLUDE = -I\tc\include;.
LIB = -L\tc\lib;.\tools
CC	= \tc\tcc

MODEL  = -mc
CLIB     = 

all: control.exe
COMOBJ = y_tab.obj lexyy.obj

# ------------------------------------------------------------

.c.obj:
	 $(CC) $(INCLUDE) -c $(CFLAGS) $(MODEL) $*.c

#------------------------------------------------------------

lexyy.c:  control.l
        flex  -i control.l

y_tab.c:  control.y
        byacc -td control.y
#------------------------------------------------------------

control.exe:   $(COMOBJ) 
        $(CC) -econtrol.exe $(LIB) $(MODEL) $(COMOBJ) $(CLIB)
        del lexyy.c 
        del y_tab.c 
	del lexyy.obj 
        del y_tab.obj
        del y_tab.h

# ----------------------------------------------------------------------

clean: 
        del lexyy.c 
        del y_tab.c 
	del lexyy.obj 
        del y_tab.obj
        del y_tab.h
#
lexyy.obj:      lexyy.c y_tab.h
y_tab.obj:      y_tab.c y_tab.h
y_tab.h:        control.y

