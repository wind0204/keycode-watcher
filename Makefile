# clear default .SUFFIXES
.SUFFIXES :

CFLAGS = -time -march=native -O2 -Wall -Wno-unused-local-typedefs -Wno-int-conversion -std=gnu99 -pipe -flto -I /usr/lib/modules/4.5.4-1-ARCH/build/include
#CFLAGS = -time -march=native -O2 -Wall -Wno-unused-local-typedefs -Wno-int-conversion -std=gnu99 -pipe -flto
CXXFLAGS=$(CFLAGS)
CC = gcc

# RM = del
RM = rm -f
# MV = ren
MV = mv
# CAT = type con
CAT = cat
# MAKEDEP = GCCMAKEDEP
MAKEDEP = makedepend

OBJDIR = obj
SRCDIR = src
OUTDIR = out

DEBUG_SUFFIX = -dbg
TARGET_PROGRAM = keycode-watcher
TARGET = $(OUTDIR)/$(TARGET_PROGRAM)
OBJECTS_LIST = main.o
OBJECTS = $(addprefix $(OBJDIR)/,$(OBJECTS_LIST))
SOURCES = $(patsubst $(OBJDIR)/%.o,$(SRCDIR)/%.c,$(OBJECTS))
LIBS = -lsystemd

DEBUGOBJECTS = $(OBJECTS:.o=$(DEBUG_SUFFIX).o)
DEBUGTARGET = $(TARGET)$(DEBUG_SUFFIX) #$(TARGET:.exe=$(DEBUG_SUFFIX).exe)
DEBUGLIBS = $(LIBS)

$(TARGET) : $(OBJECTS)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/%$(DEBUG_SUFFIX).o : $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -g -c $< -o $@

compile : $(OBJECTS)

debug : $(DEBUGTARGET)
$(DEBUGTARGET) : $(DEBUGOBJECTS)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) -o $(DEBUGTARGET) $(DEBUGOBJECTS) $(DEBUGLIBS)

clean :
	$(RM) $(OBJECTS) $(TARGET) $(DEBUGOBJECTS) $(DEBUGTARGET)

