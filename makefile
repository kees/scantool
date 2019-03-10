CC = gcc
AL_LIBS = $(shell allegro-config --libs)

ifdef DEBUGMODE
   CFLAGS = -g -Wall
   DEFINES = -DDEBUG
else
ifdef RELEASE
   CFLAGS = -Wall -O3 -fexpensive-optimizations -s
else
   CFLAGS = -O -Wall
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
   LIBS = $(AL_LIBS) -lX11 -lXext -lXpm -lXcursor -lXxf86vm -ldl -lpthread -lm
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
BIN = scantool$(EXT)
CODES = codes.dat
SUBDIRS = codes

all: $(BIN) $(SUBDIRS) $(CODES)

ifdef MINGDIR
endif

$(SUBDIRS):
	$(MAKE) -C $@

$(CODES): $(SUBDIRS)
	cp -a codes/codes.dat $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS) $(LIBS)

ifdef MINGDIR
release:
	make RELEASE=1 STATICLINK=1
else
release:
	make RELEASE=1
endif

clean:
	$(MAKE) -C codes clean
	rm -f $(OBJ) $(BIN) $(CODES)

veryclean: clean
	rm -f $(BIN)

scantool.res: scantool.rc scantool.ico
	windres -O coff -o scantool.res -i scantool.rc

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

.PHONY: all clean $(SUBDIRS)
