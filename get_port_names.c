#include <stdio.h>
#include <string.h>
#include <allegro.h>
#include <winalleg.h>

#include "listports.h"
#include "get_port_names.h"


static char* ports_list;
static int ports_list_index;
static int ports_list_size;


static BOOL CALLBACK listport_callback(LPVOID lpCallbackValue, LISTPORTS_PORTINFO* lpPortInfo)
{
   char* new_list;
   int new_size;
   
   if (strlen(lpPortInfo->lpPortName) < PORT_NAME_BUF_SIZE && strnicmp(lpPortInfo->lpPortName, "COM", 3) == 0)
   {
      if (ports_list_index == ports_list_size)
      {
         new_size = ports_list_size + 8;
         new_list = (char*)realloc(ports_list, new_size * PORT_NAME_BUF_SIZE);
         if (new_list == NULL)
            return FALSE;
         ports_list = new_list;
         ports_list_size = new_size;
      }
      
      strncpy(ports_list + ports_list_index * PORT_NAME_BUF_SIZE, lpPortInfo->lpPortName, 7);
      ports_list_index++;
   }
   
   return TRUE;
}


static int ports_cmp(const void* port1, const void* port2)
{
   int len1 = strlen(port1);
   int len2 = strlen(port2);
   
   if (len1 > len2)
      return 1;
   else if (len1 < len2)
      return -1;
   else
      return stricmp(port1, port2);
}


int get_port_names(char** list, int* list_size)
{
   int i, j;
   char* new_list;
   int new_list_size = 0;
   
   ASSERT(list != NULL);
   ASSERT(list_size != NULL);
   
   *list = NULL;
   *list_size = 0;
   
   ports_list = NULL;
   ports_list_size = 0;
   ports_list_index = 0;
   
   ListPorts(listport_callback, NULL);
   
   if (ports_list_index == 0)
      return 0;
   
   // Remove duplicates
   for (i = 0; i < ports_list_index; i++)
   {
      for (j = 0; j < new_list_size; j++)
         if (strcmp(ports_list + i * PORT_NAME_BUF_SIZE, ports_list + j * PORT_NAME_BUF_SIZE) == 0)
            break;
      if (j == new_list_size)
      {
         if (new_list_size != i)
            strncpy(ports_list + new_list_size * PORT_NAME_BUF_SIZE, ports_list + i * PORT_NAME_BUF_SIZE, PORT_NAME_BUF_SIZE - 1);
         new_list_size++;
      }
   }
   
   // Shrink
   if (new_list_size < ports_list_size)
   {
      new_list = (char*)realloc(ports_list, new_list_size * PORT_NAME_BUF_SIZE);
      if (new_list != NULL)
         ports_list = new_list;
   }
   
   // Sort
   qsort(ports_list, new_list_size, PORT_NAME_BUF_SIZE, ports_cmp);
   
   *list = ports_list;
   *list_size = new_list_size;
   return 0;
}
