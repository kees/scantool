#include <string.h>
#include "globals.h"
#include "error_handlers.h"

char temp_error_buf[256];

void fatal_error(char *msg)
{
   if (datafile != NULL)
      unload_datafile(datafile);

   allegro_message("\nERROR: %s\n", msg);
   
   exit(EXIT_FAILURE);
}
