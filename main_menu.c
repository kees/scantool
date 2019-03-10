#include <string.h>
#include "globals.h"
#include "trouble_code_reader.h"
#include "about.h"
#include "sensors.h"
#include "options.h"
#include "serial.h"
#include "custom_gui.h"
#include "reset.h"
#include "main_menu.h"

#define LOGO                       1
#define INEXPENSIVE_ALTERNATIVES   2
#define SUNFIRE                    3
#define READ_CODES                 5
#define SENSOR_DATA                6
#define FREEZE_FRAME               7
#define TESTS                      8
#define OPTIONS                    9
#define ABOUT                      10
#define EXIT                       11


// procedure declarations
static int read_codes_proc(int msg, DIALOG *d, int c);
static int sensor_data_proc(int msg, DIALOG *d, int c);
static int freeze_frame_proc(int msg, DIALOG *d, int c);
static int tests_proc(int msg, DIALOG *d, int c);
static int options_proc(int msg, DIALOG *d, int c);
static int about_proc(int msg, DIALOG *d, int c);
static int exit_proc(int msg, DIALOG *d, int c);
static int button_desc_proc(int msg, DIALOG *d, int c);
static int genuine_proc(int msg, DIALOG *d, int c);

static char button_description[256];
static char current_description[256];
static char welcome_message[256];

static DIALOG main_dialog[] =
{
   /* (proc)            (x)  (y)  (w)  (h)  (fg)         (bg)     (key) (flags) (d1) (d2) (dp)                 (dp2) (dp3) */
   { d_clear_proc,      0,   0,   0,   0,   0,           C_WHITE, 0,    0,      0,   0,   NULL,                NULL, NULL },
   { d_bitmap_proc,     25,  25,  58,  430, 0,           0,       0,    0,      0,   0,   NULL,                NULL, NULL },
   { d_bitmap_proc,     115, 25,  260, 106, 0,           0,       0,    0,      0,   0,   NULL,                NULL, NULL },
   { d_bitmap_proc,     115, 141, 260, 142, 0,           0,       0,    0,      0,   0,   NULL,                NULL, NULL },
   { button_desc_proc,  115, 283, 260, 147, C_DARK_GRAY, C_WHITE, 0,    0,      0,   0,   current_description, NULL, NULL },
   { read_codes_proc,   408, 25,  207, 57,  0,           0,       0,    D_EXIT, 0,   0,   NULL,                NULL, NULL },
   { sensor_data_proc,  408, 87,  207, 57,  0,           0,       0,    D_EXIT, 0,   0,   NULL,                NULL, NULL },
   { freeze_frame_proc, 408, 149, 207, 57,  0,           0,       0,    D_EXIT, 0,   0,   NULL,                NULL, NULL },
   { tests_proc,        408, 211, 207, 57,  0,           0,       0,    D_EXIT, 0,   0,   NULL,                NULL, NULL },
   { options_proc,      408, 273, 207, 57,  0,           0,       0,    D_EXIT, 0,   0,   NULL,                NULL, NULL },
   { about_proc,        408, 335, 207, 57,  0,           0,       0,    D_EXIT, 0,   0,   NULL,                NULL, NULL },
   { exit_proc,         408, 397, 207, 57,  0,           0,       'x',  D_EXIT, 0,   0,   NULL,                NULL, NULL },
   { genuine_proc,      0,   0,   0,   0,   0,           0,       0,    0,      0,   0,   NULL,                NULL, NULL },
   { d_yield_proc,      0,   0,   0,   0,   0,           0,       0,    0,      0,   0,   NULL,                NULL, NULL },
   { NULL,              0,   0,   0,   0,   0,           0,       0,    0,      0,   0,   NULL,                NULL, NULL }
};


int display_main_menu()
{
   // load all the buttons:
   // dp = original image
   // dp2 = mouseover image
   // dp3 = clicked image
   main_dialog[LOGO].dp = datafile[LOGO_BMP].dat;
   main_dialog[INEXPENSIVE_ALTERNATIVES].dp = datafile[INEXPENSIVE_ALTERNATIVES_BMP].dat;
   main_dialog[SUNFIRE].dp = datafile[SUNFIRE_BMP].dat;
   main_dialog[READ_CODES].dp = datafile[READ_CODES1_BMP].dat;
   main_dialog[SENSOR_DATA].dp = datafile[SENSOR_DATA1_BMP].dat;
   main_dialog[FREEZE_FRAME].dp = datafile[FREEZE_FRAME1_BMP].dat;
   main_dialog[TESTS].dp = datafile[TESTS1_BMP].dat;
   main_dialog[OPTIONS].dp = datafile[OPTIONS1_BMP].dat;
   main_dialog[ABOUT].dp = datafile[ABOUT1_BMP].dat;
   main_dialog[EXIT].dp = datafile[EXIT1_BMP].dat;

   main_dialog[READ_CODES].dp2 = datafile[READ_CODES2_BMP].dat;
   main_dialog[SENSOR_DATA].dp2 = datafile[SENSOR_DATA2_BMP].dat;
   main_dialog[FREEZE_FRAME].dp2 = datafile[FREEZE_FRAME2_BMP].dat;
   main_dialog[TESTS].dp2 = datafile[TESTS2_BMP].dat;
   main_dialog[OPTIONS].dp2 = datafile[OPTIONS2_BMP].dat;
   main_dialog[ABOUT].dp2 = datafile[ABOUT2_BMP].dat;
   main_dialog[EXIT].dp2 = datafile[EXIT2_BMP].dat;

   main_dialog[READ_CODES].dp3 = datafile[READ_CODES3_BMP].dat;
   main_dialog[SENSOR_DATA].dp3 = datafile[SENSOR_DATA3_BMP].dat;
   main_dialog[FREEZE_FRAME].dp3 = datafile[FREEZE_FRAME3_BMP].dat;
   main_dialog[TESTS].dp3 = datafile[TESTS3_BMP].dat;
   main_dialog[OPTIONS].dp3 = datafile[OPTIONS3_BMP].dat;
   main_dialog[ABOUT].dp3 = datafile[ABOUT3_BMP].dat;
   main_dialog[EXIT].dp3 = datafile[EXIT3_BMP].dat;

   sprintf(welcome_message, "Roll mouse cursor over menu buttons to see their descriptions.");
   strcpy(button_description, welcome_message);

   return do_dialog(main_dialog, -1);
}


int read_codes_proc(int msg, DIALOG *d, int c)
{
   int ret;
   ret = nostretch_icon_proc(msg, d, c); // call the parent object

   if (msg == MSG_GOTMOUSE) // if we got mouse, display description
      sprintf(button_description, "Read codes and their definitions, turn off MIL and erase diagnostic test data.");

   if (ret == D_CLOSE)           // trap the close value
   {
     display_trouble_codes(); // display trouble code dialog
     strcpy(button_description, welcome_message);
     return D_REDRAW;
   }
   return ret;  // return
}


int sensor_data_proc(int msg, DIALOG *d, int c)
{
   int ret;
   int reset = FALSE;
   ret = nostretch_icon_proc(msg, d, c); // call the parent object

   if (msg == MSG_GOTMOUSE) // if we got mouse, display description
      sprintf(button_description, "Display current sensor data (RPM, Engine Load, Coolant Temperature, Speed, etc.)");

   if (ret == D_CLOSE)           // trap the close value
   {  
      if (comport.status != READY)
      {
         reset = TRUE;
         if (open_comport() != 0)
         {
            comport.status = NOT_OPEN;   // reset comport status
            while (comport.status == NOT_OPEN)
            {
               if (alert("COM Port could not be opened.", "Please check that port settings are correct", "and that no other application is using it", "&Configure Port", "&Ignore", 'c', 'i') == 1)
                  display_options();
               else
                  comport.status = USER_IGNORED;
            }
         }
         else
            comport.status = READY;
      }
      display_sensor_dialog(reset);  // display sensor data dialog
      strcpy(button_description, welcome_message);
      return D_REDRAW;
   }
   return ret;  // return
}


int freeze_frame_proc(int msg, DIALOG *d, int c)
{
   int ret;
   ret = nostretch_icon_proc(msg, d, c); // call the parent object

   if (msg == MSG_GOTMOUSE) // if we got mouse, display description
      sprintf(button_description, "Display freeze frame data (not implemented in this version).");

   if (ret == D_CLOSE)           // trap the close value
   {
      alert("This feature is not implemented", " in this version", NULL, "OK", NULL, 0, 0);
      //display_freeze_frame(); // display freeze frame data
      //strcpy(button_description, welcome_message);
      return D_REDRAWME;
   }
   return ret;  // return
}


int tests_proc(int msg, DIALOG *d, int c)
{
   int ret;

   if (msg == MSG_GOTMOUSE) // if we got mouse, display description
      sprintf(button_description, "Display mode 5, 6, & 7 test results (not implemented in this version).");

   ret = nostretch_icon_proc(msg, d, c); // call the parent object

   if (ret == D_CLOSE)           // trap the close value
   {  
      alert("This feature is not implemented", " in this version", NULL, "OK", NULL, 0, 0);
      return D_REDRAWME;
   }
   return ret;  // return
}


int options_proc(int msg, DIALOG *d, int c)
{
   static int chip_was_reset = FALSE;
   static int opened_first_time = TRUE;
   int old_port;
   int ret;

   switch (msg)
   {
      case MSG_GOTMOUSE: // if we got mouse, display description
         sprintf(button_description, "Configure program options.");
         break;

      case MSG_IDLE:
         if (opened_first_time && comport.number < 0 && comport.status != USER_IGNORED)
         {
            display_options();
            opened_first_time = FALSE;
         }
         
         while (comport.status == NOT_OPEN)
         {
            if (comport.number < 0)
               ret = alert("COM Port has not been selected.", NULL, NULL, "&Configure Port", "&Ignore", 'c', 'i');
            else
               ret = alert("COM Port could not be opened.", "Please check that port settings are correct", "and that no other application is using it", "&Configure Port", "&Ignore", 'c', 'i');
            
            if (ret == 1)
               display_options();
            else
               comport.status = USER_IGNORED;
         }
         
         if (comport.status == READY && !chip_was_reset) // if the port is ready,
         {
            reset_chip();
            chip_was_reset = TRUE;
         }
         break;
   }
   
   ret = nostretch_icon_proc(msg, d, c); // call the parent object
   
   if (ret == D_CLOSE)           // trap the close value
   {
      old_port = comport.number;
      display_options(); // display options dialog
      if (comport.number != old_port)
         chip_was_reset = FALSE;
      return D_REDRAWME;
   }
   
   return ret;  // return
}


int about_proc(int msg, DIALOG *d, int c)
{
   int ret;
   ret = nostretch_icon_proc(msg, d, c); // call the parent object

   if (msg == MSG_GOTMOUSE) // if we got mouse, display description
      sprintf(button_description, "Learn more about this program, and find out where you can buy the OBD-II interface.");

   if (ret == D_CLOSE)           // trap the close value
   {
      display_about(); // display information about the program
      strcpy(button_description, welcome_message);
      return D_REDRAW;
   }
   return ret;  // return
}


int exit_proc(int msg, DIALOG *d, int c)
{
   int ret;
   ret = nostretch_icon_proc(msg, d, c); // call the parent object

   if (msg == MSG_GOTMOUSE) // if we got mouse, display description
      sprintf(button_description, "Exit the program.");

   if (ret == D_CLOSE)           // trap the close value
   {
      if (alert("Do you really want to exit?", NULL, NULL, "&Yes", "&No", 'y', 'n') == 2)
         return D_REDRAWME;
   }
   return ret;  // return
}


int button_desc_proc(int msg, DIALOG *d, int c)
{
   int ret;
   ret = super_textbox_proc(msg, d, c); // call the parent object

   if (msg == MSG_START)
      d->dp2 = datafile[ARIAL18_FONT].dat;

   if (strcmp(current_description, button_description)) // if buttons are different
   {
      strcpy(current_description, button_description);
      return D_REDRAWME;
   }

   return ret;  // return
}

int genuine_proc(int msg, DIALOG *d, int c)
{
   if (msg == MSG_IDLE)
      if (is_not_genuine_scan_tool)
         return D_CLOSE;
   
   return D_O_K;
}
