#include <string.h>
#include "globals.h"
#include "error_handlers.h"


void fatal_error(char *msg)
{
   if (datafile != NULL)
      unload_datafile(datafile);

   allegro_message("\nERROR: %s\n", msg);
   
   exit(EXIT_FAILURE);
}
