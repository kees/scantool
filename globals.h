#ifndef GLOBALS_H
#define GLOBALS_H

#define _GNU_SOURCE
#include <stdio.h>
#include <allegro.h>
#include "resource.h"

// system_of_measurements
#define METRIC     0
#define IMPERIAL   1

// Display mode flags
#define FULL_SCREEN_MODE            0
#define WINDOWED_MODE               1
#define FULLSCREEN_MODE_SUPPORTED   2
#define WINDOWED_MODE_SUPPORTED     4
#define WINDOWED_MODE_SET           8

// Password for Datafiles (Tr. Code Definitions and Resources)
#define PASSWORD NULL

// Colors in the main palette
#define C_TRANSP       -1
#define C_BLACK         0
#define C_WHITE         1
#define C_RED           255
#define C_BLUE          254
#define C_GREEN         99
#define C_DARK_YELLOW   54
#define C_PURPLE        9
#define C_DARK_GRAY     126
#define C_GRAY          50
#define C_LIGHT_GRAY    55

// Options
extern int system_of_measurements;
extern int display_mode;
extern int log_comms;

// File names
extern char *options_file_name;
extern char *code_defs_file_name;

void write_log(const char *log_string);

void write_comm_log(const char *marker, const char *data);

extern DATAFILE *datafile;

#endif
