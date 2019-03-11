#include <string.h>

#include "globals.h"
#include "custom_gui.h"
#include "serial.h"
#include "options.h"
#include "error_handlers.h"
#ifdef ALLEGRO_WINDOWS
   #include "get_port_names.h"
#elif TERMIOS
   #define PORT_NAME_BUF_SIZE   16
#else
   #define PORT_NAME_BUF_SIZE    5
#endif

#define MSG_SAVE_OPTIONS   MSG_USER
#define MSG_REFRESH        MSG_USER + 1

// Define defaults
#ifdef ALLEGRO_WINDOWS
   #define DEFAULT_DISPLAY_MODE         WINDOWED_MODE
#else
   #define DEFAULT_DISPLAY_MODE         WINDOWED_MODE
#endif
#define DEFAULT_SYSTEM_OF_MEASURMENTS   IMPERIAL
#define DEFAULT_COMPORT_NUMBER          -1
#ifdef TERMIOS
   #define DEFAULT_BAUD_RATE               BAUD_RATE_9600
#else
   #define DEFAULT_BAUD_RATE               BAUD_RATE_115200
#endif

typedef struct
{
   int option_value;
} OPTION_ELEMENT;


static OPTION_ELEMENT option_metric = {METRIC};
static OPTION_ELEMENT option_imperial = {IMPERIAL};
static OPTION_ELEMENT option_baud_rate_9600 = {BAUD_RATE_9600};
static OPTION_ELEMENT option_baud_rate_38400 = {BAUD_RATE_38400};
static OPTION_ELEMENT option_baud_rate_115200 = {BAUD_RATE_115200};
#ifdef ALLEGRO_WINDOWS
   static OPTION_ELEMENT option_baud_rate_230400 = {BAUD_RATE_230400};
#endif
static OPTION_ELEMENT option_windowed_mode = {WINDOWED_MODE};
static OPTION_ELEMENT option_full_screen_mode = {FULL_SCREEN_MODE};


static int option_element_proc(int msg, DIALOG *d, int c);
static int save_options_proc(int msg, DIALOG *d, int c);
static int comport_list_proc(int msg, DIALOG *d, int c);
static char *listbox_getter(int index, int *list_size);
static void fill_comport_list();
static void clear_comport_list();

static DIALOG options_dialog[] =
{
   /* (proc)              (x)  (y)  (w)  (h)  (fg)     (bg)           (key) (flags) (d1) (d2) (dp)                       (dp2) (dp3)                     */
   { d_shadow_box_proc,   0,   0,   231, 466, 0,       C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,                      NULL, NULL                     },
   { d_shadow_box_proc,   0,   0,   231, 24,  0,       C_DARK_GRAY,   0,    0,      0,   0,   NULL,                      NULL, NULL                     },
   { caption_proc,        115, 2,   113, 19,  C_WHITE, C_TRANSP,      0,    0,      0,   0,   "Program Options",         NULL, NULL                     },
   { d_text_proc,         16,  32,  200, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "System Of Measurements:", NULL, NULL                     },
   { option_element_proc, 48,  56,  80,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   "Metric",                  NULL, &option_metric           },
   { option_element_proc, 48,  72,  88,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   "US",                      NULL, &option_imperial         },
   { d_text_proc,         16,  96,  152, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Serial Port:",            NULL, NULL                     },
   { comport_list_proc,   20,  120, 190,  148, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   listbox_getter,            NULL, NULL                     },
   { d_text_proc,         16,  282, 200, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Baud Rate:",              NULL, NULL                     },
   { option_element_proc, 20,  306, 80,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      1,   0,   "9600",                    NULL, &option_baud_rate_9600   },
   { option_element_proc, 110, 306, 80,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      1,   0,   "38400",                   NULL, &option_baud_rate_38400  },
   { option_element_proc, 20,  322, 80,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      1,   0,   "115200",                  NULL, &option_baud_rate_115200 },
#ifdef ALLEGRO_WINDOWS
   { option_element_proc, 110, 322, 80,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      1,   0,   "230400",                  NULL, &option_baud_rate_230400 },
#endif
   { d_text_proc,         16,  346, 200, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Display Mode:",           NULL, NULL                     },
   { option_element_proc, 48,  370, 80,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      2,   0,   "Windowed",                NULL, &option_windowed_mode    },
   { option_element_proc, 48,  386, 80,  10,  C_BLACK, C_LIGHT_GRAY,  0,    0,      2,   0,   "Full Screen",             NULL, &option_full_screen_mode },
   { save_options_proc,   16,  410, 92,  40,  C_BLACK, C_GREEN,       's',  D_EXIT, 0,   0,   "&Save",                   NULL, NULL                     },
   { d_button_proc,       123, 410, 92,  40,  C_BLACK, C_DARK_YELLOW, 'c',  D_EXIT, 0,   0,   "&Cancel",                 NULL, NULL                     },
   { d_yield_proc,        0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,                      NULL, NULL                     },
   { NULL,                0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,                      NULL, NULL                     }
};

static char* comport_list_strings = NULL;
static int* comport_list_numbers = NULL;
static int comport_list_size = 0;



int display_options()
{
   if (display_mode & WINDOWED_MODE_SET)
      display_mode |= WINDOWED_MODE;
   else
      display_mode &= ~WINDOWED_MODE;
   centre_dialog(options_dialog);
   return popup_dialog(options_dialog, -1);
}


char* listbox_getter(int index, int *list_size)
{
   if (index < 0)
   {
      *list_size = comport_list_size;
      return NULL;
   }
   else
      return comport_list_strings + index * PORT_NAME_BUF_SIZE;
}


int comport_list_proc(int msg, DIALOG *d, int c)
{
   int i;
   
   switch (msg)
   {
      case MSG_START:
      case MSG_REFRESH:
         fill_comport_list();
         d->d1 = 0;
         
         if (comport.number >= 0)
         {
            for (i = 0; i < comport_list_size; i++)
               if (comport_list_numbers[i] == comport.number)
                  d->d1 = i;
         }
         
         if (msg == MSG_REFRESH)
            msg = MSG_DRAW;
         break;
         
      case MSG_SAVE_OPTIONS:
         if (d->d1 >= 0 && d->d1 < comport_list_size)
            comport.number = comport_list_numbers[d->d1];
         else
            comport.number = -1;
         break;
         
      case MSG_END:
         clear_comport_list();
   }
   
   return d_list_proc(msg, d, c);
}


int option_element_proc(int msg, DIALOG *d, int c)
{
   OPTION_ELEMENT *option_element = (OPTION_ELEMENT *)d->dp3;

   switch (msg)
   {
      case MSG_START:
      case MSG_REFRESH:
         switch (d->d1)
         {
            case 0:
               if (option_element->option_value == system_of_measurements) // if the element should be selected
                  d->flags |= D_SELECTED;
               else
                  d->flags &= ~D_SELECTED;
               break;
               
            case 1:
               if (option_element->option_value == comport.baud_rate)
                  d->flags |= D_SELECTED;
               else
                  d->flags &= ~D_SELECTED;
               break;
               
            case 2:
               if (option_element->option_value == (display_mode & WINDOWED_MODE))
                  d->flags |= D_SELECTED; // make it selected
               else
                  d->flags &= ~D_SELECTED;
               if (((option_element->option_value == WINDOWED_MODE) && !(display_mode & WINDOWED_MODE_SUPPORTED)) || ((option_element->option_value == FULL_SCREEN_MODE) && !(display_mode & FULLSCREEN_MODE_SUPPORTED)))
                  d->flags |= D_DISABLED;
               else
                  d->flags &= ~D_DISABLED;
               break;
         }
         if (msg == MSG_REFRESH)
            msg = MSG_DRAW;
         break;

      case MSG_SAVE_OPTIONS:
         if (d->flags & D_SELECTED)  // if the element is selected,
            switch (d->d1)
            {
               case 0:
                  system_of_measurements = option_element->option_value;
                  break;
                  
               case 1:
                  comport.baud_rate = option_element->option_value;
                  break;
                  
               case 2:
                  display_mode &= ~WINDOWED_MODE;
                  display_mode |= option_element->option_value;
                  break;
            }
         break;
   
      case MSG_END:
         d->flags &= ~D_SELECTED;  // deselect
         d->flags &= ~D_DISABLED;  // and enable the element
         break;
   }

   return d_radio_proc(msg, d, c);
}


int save_options_proc(int msg, DIALOG *d, int c)
{
   int ret;
   int old_baud_rate;
   BITMAP *bmp;
   FILE *file;

   ret = d_button_proc(msg, d, c);

   if (ret == D_CLOSE)
   {
      old_baud_rate = comport.baud_rate;
      broadcast_dialog_message(MSG_SAVE_OPTIONS, 0);
      
      if (comport.baud_rate != old_baud_rate)
      {
         if (alert("WARNING!", "This operation may cause scan tool to stop responding.", "Are you sure you want to change the baud rate?", "Yes", "No", 0, 0) != 1)
         {
            comport.baud_rate = old_baud_rate;
            broadcast_dialog_message(MSG_REFRESH, 0);
            return D_O_K;
         }
      }

      close_comport(); // close current comport
      open_comport(); // try reinitializing comport (comport.status will be set)
      
      if ((!(display_mode & WINDOWED_MODE) && (display_mode & WINDOWED_MODE_SET)) || ((display_mode & WINDOWED_MODE) && !(display_mode & WINDOWED_MODE_SET)))
      {
         bmp = create_bitmap(SCREEN_W, SCREEN_H);
         if (bmp)
         {
            scare_mouse();
            blit(screen, bmp, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
            unscare_mouse();

            if (display_mode & WINDOWED_MODE)
            {
               if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) == 0)
                  display_mode |= WINDOWED_MODE_SET;
               else
                  display_mode &= ~WINDOWED_MODE_SUPPORTED;
            }
            else
            {
               if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, 640, 480, 0, 0) == 0)
                  display_mode &= ~WINDOWED_MODE_SET;
               else
                  display_mode &= ~FULLSCREEN_MODE_SUPPORTED;
            }

            set_pallete(datafile[MAIN_PALETTE].dat);
            gui_fg_color = C_BLACK;  // set the foreground color
            gui_bg_color = C_WHITE;  // set the background color
            gui_mg_color = C_GRAY;   // set the disabled color
            set_mouse_sprite(NULL); // make mouse use current palette
            blit(bmp, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
            show_mouse(screen);
            destroy_bitmap(bmp);
         }
         else
            alert("Error switching display modes.", "Not enough memory to save screen.", NULL, "OK", NULL, 0, 0);
      }

      file = fopen(options_file_name, "a");

      if (file == NULL)
         alert("Options could not be saved, because file", options_file_name, "could not be open for writing", "OK", NULL, 0, 0);
      else
      {
         fclose(file);
         save_program_options();
      }
   }

   return ret;
}

// DO NOT TRANSLATE BELOW THIS LINE
void load_program_options()
{
   comport.number = get_config_int("comm", "comport_number", DEFAULT_COMPORT_NUMBER);
   comport.baud_rate = get_config_int("comm", "baud_rate", DEFAULT_BAUD_RATE);
   system_of_measurements = get_config_int("general", "system_of_measurements", DEFAULT_SYSTEM_OF_MEASURMENTS);
   if (get_config_int("general", "display_mode", DEFAULT_DISPLAY_MODE))
      display_mode |= WINDOWED_MODE_SET;
   else
      display_mode &= ~WINDOWED_MODE_SET;
}


void save_program_options()
{
   set_config_int("general", "system_of_measurements", system_of_measurements);
   if (display_mode & WINDOWED_MODE_SET)
      set_config_int("general", "display_mode", WINDOWED_MODE);
   else
      set_config_int("general", "display_mode", FULL_SCREEN_MODE);
   set_config_int("comm", "baud_rate", comport.baud_rate);
   set_config_int("comm", "comport_number", comport.number);
   flush_config_file();
}

void fill_comport_list()
{
   int i;
   
   clear_comport_list();
   
#ifdef ALLEGRO_WINDOWS
   
   if (get_port_names(&comport_list_strings, &comport_list_size) != 0)
      fatal_error("Could not allocate memory for comport_list_strings.");
   
   if (comport_list_size > 0)
   {
      comport_list_numbers = (int*)malloc(comport_list_size * sizeof(int));
      if (comport_list_numbers == NULL)
         fatal_error("Could not allocate memory for comport_list_numbers.");
      
      for (i = 0; i < comport_list_size; i++)
         comport_list_numbers[i] = atoi((comport_list_strings + i * PORT_NAME_BUF_SIZE) + 3) - 1;
   }
#elif TERMIOS
   char tmp[PORT_NAME_BUF_SIZE];
   comport_list_strings = calloc(16 , (sizeof(char) * PORT_NAME_BUF_SIZE));
   if (comport_list_strings == NULL)
      fatal_error("Could not allocate memory for comport_list_strings.");

   comport_list_numbers = malloc(16 * sizeof(int));
   if (comport_list_numbers == NULL)
      fatal_error("Could not allocate memory for comport_list_numbers.");

    int nb=0;
    FILE *f;
    for(i=0;i<8;i++)
    {
        snprintf(tmp,sizeof(tmp),"/dev/ttyS%d",i);
        f=fopen(tmp,"r");
        if( f != NULL )
        {
            strcpy(comport_list_strings + nb * PORT_NAME_BUF_SIZE,tmp);
            comport_list_numbers[nb]=i;
            nb++;
            fclose(f);
        }
    }
    for(i=0;i<8;i++)
    {
        snprintf(tmp,sizeof(tmp),"/dev/ttyUSB%d",i);
        f=fopen(tmp,"r");
        if( f != NULL )
        {
            strcpy(comport_list_strings + nb * PORT_NAME_BUF_SIZE,tmp);
            comport_list_numbers[nb]=100+i;
            nb++;
            fclose(f);
        }
    }
    comport_list_size = nb;
#else
   
   comport_list_strings = malloc(8 * (sizeof(char) * PORT_NAME_BUF_SIZE));
   if (comport_list_strings == NULL)
      fatal_error("Could not allocate memory for comport_list_strings.");
   
   comport_list_numbers = malloc(8 * sizeof(int));
   if (comport_list_numbers == NULL)
      fatal_error("Could not allocate memory for comport_list_numbers.");
   
   for (i = 0; i < 8; i++)
      sprintf(comport_list_strings + i * PORT_NAME_BUF_SIZE, "COM%i", i + 1);
   
   comport_list_numbers[0] = _com1;
   comport_list_numbers[1] = _com2;
   comport_list_numbers[2] = _com3;
   comport_list_numbers[3] = _com4;
   comport_list_numbers[4] = _com5;
   comport_list_numbers[5] = _com6;
   comport_list_numbers[6] = _com7;
   comport_list_numbers[7] = _com8;
   
   comport_list_size = 8;
   
#endif
}


void clear_comport_list()
{
   if (comport_list_strings != NULL)
   {
      free(comport_list_strings);
      comport_list_strings = NULL;
   }
   if (comport_list_numbers != NULL)
   {
      free(comport_list_numbers);
      comport_list_numbers = NULL;
   }
   comport_list_size = 0;
}
