#ifndef VERSION_H
#define VERSION_H

#define SCANTOOL_VERSION          2
#define SCANTOOL_SUB_VERSION      1
#define SCANTOOL_WIP_VERSION      0
#define SCANTOOL_VERSION_STR      "2.1"
#define SCANTOOL_VERSION_EX_STR   "2.1"
#define SCANTOOL_YEAR_STR         "2020"
#define SCANTOOL_DATE             20200314    /* yyyymmdd */

#ifdef ALLEGRO_WINDOWS
   #define SCANTOOL_PLATFORM_STR   "Windows"
#else
 #ifdef TERMIOS
   #define SCANTOOL_PLATFORM_STR   "POSIX"
 #else
   #define SCANTOOL_PLATFORM_STR   "DOS"
 #endif
#endif

#define SCANTOOL_COPYRIGHT_STR     "Copyright © " SCANTOOL_YEAR_STR

#endif
