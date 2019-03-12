CC = gcc
AL_LIBS = $(shell allegro-config --libs)
CFLAGS ?= -O2 -g -Wall

ifdef DEBUGMODE
   DEFINES = -DDEBUG
else
ifdef RELEASE
   CFLAGS += -O3 -fexpensive-optimizations -s
endif
endif

ifdef MINGDIR
   ifdef STATICLINK
      LIBS = $(AL_LIBS)_s -lkernel32 -luser32 -lgdi32 -lcomdlg32 -lole32 -ldinput -lddraw -ldxguid -lwinmm -ldsound
      DEFINES += -DALLEGRO_STATICLINK
   else
      LIBS = $(AL_LIBS)
   endif
   WINDRES = windres -I rc -O coff
   CFLAGS += -mwindows
   OBJ = scantool.res get_port_names.o
   EXT = .exe
else
ifdef DZCOMM
   LIBS = -ldzcom $(AL_LIBS)
   EXT = .exe
else
   DEFINES += -DTERMIOS
   LIBS = $(AL_LIBS)
   EXT =
endif
endif

ifndef NOWERROR
   CFLAGS += -Werror
endif

ifdef DEFINES
   CFLAGS += $(DEFINES)
endif

OBJ += main.o main_menu.o serial.o options.o sensors.o trouble_code_reader.o custom_gui.o error_handlers.o about.o reset.o
EXE = scantool
BIN = $(EXE)$(EXT)
CODES_TXT = $(wildcard codes/codes-*.txt)
CODES_OUT = $(CODES_TXT:.txt=.txt.out)
CODES = codes.dat

all: $(BIN) $(CODES)

ifdef MINGDIR
endif

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS) $(LIBS)

ifdef MINGDIR
release:
	make RELEASE=1 STATICLINK=1
else
release:
	make RELEASE=1
endif

tarball: clean
	ver=$(shell grep SCANTOOL_VERSION_STR version.h | cut -d'"' -f2) && \
	mkdir -p scantool-$${ver} && \
	git ls-tree -r master --name-only | grep -v ^debian/ | grep -v '^\.' | cpio -pdm scantool-$${ver} && \
	tar jcf scantool-$${ver}.tar.bz2 scantool-$${ver} && \
	rm -rf scantool-$${ver}

install: $(BIN) $(CODES)
	install -D $(BIN) $(DESTDIR)/usr/bin/$(BIN)
	install -D -m 0644 $(EXE).dat $(DESTDIR)/usr/share/$(EXE)/$(EXE).dat
	install -D -m 0644 $(CODES) $(DESTDIR)/usr/share/$(EXE)/codes.dat

clean:
	rm -f $(OBJ) $(BIN)

veryclean: clean
	rm -f $(CODES_OUT) $(CODES)

scantool.res: scantool.rc scantool.ico
	windres -O coff -o scantool.res -i scantool.rc

# Build Allegro objects based on code prefix.
$(CODES): $(CODES_TXT)
	# Inject column 2 based on file name.
	set -e; for code in $^ ; do \
		origin=$$(echo $$code | cut -d- -f2 | cut -d. -f1); \
		awk -F"\t" '{out=$$1"\t'$$origin'"; for (i=2;i<=NF;i++) { out=out"\t"$$i }; print out}' $$code > $$code.out ; \
	done
	# Use only unique entries, without ;-prefixed comments.
	sort -u $(CODES_OUT) | grep -v '^;' >codes-all.out
	rm -f $(CODES_OUT)
	rm -f $@.new
	set -e; for code in $$(cut -c1 codes-all.out | sort -u); do \
		lower=$$(echo $$code | tr A-Z a-z); \
		grep ^$$code codes-all.out > $${lower}codes; \
		dat -t CDEF -a $@.new $${lower}codes; \
		rm -f $${lower}codes ; \
	done
	rm -f codes-all.out
	mv $@.new $@

main.o: main.c globals.h main_menu.h error_handlers.h options.h serial.h version.h
	$(CC) $(CFLAGS) -c main.c

main_menu.o: main_menu.c globals.h about.h trouble_code_reader.h sensors.h options.h serial.h custom_gui.h main_menu.h
	$(CC) $(CFLAGS) -c main_menu.c

serial.o: serial.c globals.h serial.h
	$(CC) $(CFLAGS) -c serial.c

options.o: options.c globals.h custom_gui.h serial.h options.h
	$(CC) $(CFLAGS) -c options.c

sensors.o: sensors.c globals.h serial.h options.h error_handlers.h sensors.h custom_gui.h
	$(CC) $(CFLAGS) -c sensors.c

trouble_code_reader.o: trouble_code_reader.c globals.h serial.h options.h custom_gui.h error_handlers.h trouble_code_reader.h
	$(CC) $(CFLAGS) -c trouble_code_reader.c

custom_gui.o: custom_gui.c globals.h custom_gui.h
	$(CC) $(CFLAGS) -c custom_gui.c

error_handlers.o: error_handlers.c globals.h error_handlers.h
	$(CC) $(CFLAGS) -c error_handlers.c

about.o: about.c globals.h custom_gui.h serial.h sensors.h options.h version.h about.h
	$(CC) $(CFLAGS) -c about.c

reset.o: reset.c globals.h custom_gui.h main_menu.h serial.h reset.h
	$(CC) $(CFLAGS) -c reset.c

get_port_names.o: get_port_names.c get_port_names.h
	$(CC) $(CFLAGS) -c get_port_names.c

.PHONY: all install tarball clean veryclean
