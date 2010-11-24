# ***************************************************************************
# **                                                                       **
# ** GNU Makefile for mini linker tools on sky platform                    **
# ** Written by                                                            **
# **   Klaus Stengel <Klaus.Stengel@informatik.uni-erlangen.de>            **
# **                                                                       **
# ***************************************************************************



HOST_CC = gcc
HOST_STRIP = strip -R .comment
#HOST_CFLAGS = -I -g -O3 -Wall -W -pedantic -std=c99 -D_XOPEN_SOURCE=600
HOST_CFLAGS = -g -O0 -Wall -W -pedantic -std=c99 -D_XOPEN_SOURCE=600
HOST_LDFLAGS = -g
BFDLIBDIR = libbfd-lin32
BFDLIB = $(BFDLIBDIR)/libbfd.a $(BFDLIBDIR)/libiberty.a 
MINILINKROOT =..



# -*---No user serviceable parts below ------------------------------------*-


HOST_CPPFLAGS += -I$(BFDLIBDIR) -I$(MINILINKROOT)/lib -I$(MINILINKROOT)/src -DCOMPILE_HOSTED_TOOLS 

MKMINIMOD_OBJ = mkminimod.o crc32k.o filelib.o
MKSYMTAB_OBJ = mksymtab.o crc32k.o filelib.o

ALL_COBJS = $(sort $(MKSYMTAB_OBJ) $(MKMINIMOD_OBJ))
ALL_TARGETS = mkminimod mksymtab

all: $(ALL_TARGETS)

$(ALL_TARGETS):
	$(HOST_CC) $(HOST_LDFLAGS) $^ $(BFDLIB) -o $@


$(ALL_COBJS):
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -c $< -o $@

mkminimod: $(MKMINIMOD_OBJ)

mksymtab: $(MKSYMTAB_OBJ)

clean:
	rm -f $(ALL_TARGETS) $(ALL_COBJS)

.PHONY: clean

# -*-----------------------------------------------------------------------*-
#    Dependencies
# -*-----------------------------------------------------------------------*-
crc32k.o: $(MINILINKROOT)/lib/crc32k.c $(MINILINKROOT)/lib/crc32k.h



mkminimod.o: mkminimod.c $(MINILINKROOT)/lib/crc32k.h \
  $(MINILINKROOT)/src/minilink.h

mksymtab.o: mksymtab.c $(MINILINKROOT)/lib/crc32k.h \
  $(MINILINKROOT)/src/minilink.h 
  
filelib.o: filelib.c filelib.h 

