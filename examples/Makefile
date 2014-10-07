CONTIKI_PROJECT = link

TARGET=sky

all: $(CONTIKI_PROJECT) $(CONTIKI_PROJECT).symbols.mls hello-world.mlk
	cp $(CONTIKI_PROJECT).symbols.mls sym.mls
	cp hello-world.mlk hello.mlk

CONTIKI=../../contiki
MINILINKROOT=..

#Add some debugginginformation
CFLAGS+=-gstabs+
LDFLAGS+=-gstabs+

#Set correct LD
LD = msp430-ld

include $(MINILINKROOT)/Makefile.minilink
include $(CONTIKI)/Makefile.include
