#include <string.h>
#include "globals.h"
#include "main_menu.h"
#include "error_handlers.h"
#include "options.h"
#include "serial.h"
#include "version.h"

#if (defined ALLEGRO_DOS) || (defined ALLEGRO_STATICLINK)
// Define color depths used
BEGIN_COLOR_DEPTH_LIST
   COLOR_DEPTH_8
END_COLOR_DEPTH_LIST
#endif

#define WINDOW_TITLE   "ScanTool.net " SCANTOOL_VERSION_EX_STR


void write_log(const char *log_string)
{
   FILE *logfile = NULL;
   
   logfile = fopen(log_file_name, "a");
   if (logfile == NULL)
      fatal_error("Could not open log file for writing!");
   fprintf(logfile, log_string);
   fclose(logfile);
}


#ifdef LOG_COMMS
void write_comm_log(const char *marker, const char *data)
{
   FILE *logfile = NULL;

   logfile = fopen(comm_log_file_name, "a");
   if (logfile == NULL)
      fatal_error("Could not open comm log file for writing!");
   fprintf(logfile, "[%s]%s[/%s]\n", marker, data, marker);
   fclose(logfile);
}
#endif


static void init()
{
   char temp_buf[256];

   is_not_genuine_scan_tool = FALSE;
   
   /* initialize some varaibles with default values */
   strcpy(options_file_name, "scantool.cfg");
   strcpy(data_file_name, "scantool.dat");
   strcpy(code_defs_file_name, "codes.dat");
   
   datafile = NULL;
   comport.status = NOT_OPEN;
   display_mode = 0;

   set_uformat(U_ASCII);
   
   /* initialize hardware */
   write_log("\nInitializing Allegro... ");
   allegro_init();
   write_log("OK");
   
   set_window_title(WINDOW_TITLE);
   
   write_log("\nInstalling Timers... ");
   if (install_timer() != 0)
   {
      write_log("Error!");
      fatal_error("Error installing timers");
   }
   write_log("OK");
   write_log("\nInstalling Keyboard... ");
   install_keyboard();
   write_log("OK");
   write_log("\nInstalling Mouse... ");
   install_mouse();
   write_log("OK");

   /* load options from file, the defaults will be automatically substituted if file does not exist */
   write_log("\nLoading Preferences... ");
   set_config_file(options_file_name);
   /* if config file doesn't exist or is of an incorrect version */
   if (strcmp(get_config_string(NULL, "version", ""), SCANTOOL_VERSION_STR) != 0)
   {
      /* update config file */
      remove(options_file_name);
      set_config_file(options_file_name);
      set_config_string(NULL, "version", SCANTOOL_VERSION_STR);
      load_program_options();  // Load defaults
      save_program_options();
   }
   else
      load_program_options();
   write_log("OK");

   display_mode |= FULLSCREEN_MODE_SUPPORTED;
   
   write_log("\nTrying Windowed Graphics Mode... ");
   if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) == 0)
   {
      display_mode |= WINDOWED_MODE_SUPPORTED;
      write_log("OK");
   }
   else
   {
      display_mode &= ~(WINDOWED_MODE_SUPPORTED | WINDOWED_MODE_SET);
      write_log(allegro_error);
   }

   if (!(display_mode & WINDOWED_MODE_SET))
   {
      write_log("\nTrying Full Screen Graphics Mode... ");
      if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, 640, 480, 0, 0) == 0)
      {
         display_mode |= FULLSCREEN_MODE_SUPPORTED;
         write_log("OK");
      }
      else
      {
         display_mode &= ~FULLSCREEN_MODE_SUPPORTED;
         write_log(allegro_error);
      }
   }
   
   if (!(display_mode & (FULLSCREEN_MODE_SUPPORTED | WINDOWED_MODE_SUPPORTED)))
      fatal_error(allegro_error);
   else if ((display_mode & WINDOWED_MODE_SUPPORTED) && !(display_mode & FULLSCREEN_MODE_SUPPORTED))
   {
      set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0);
      display_mode &= WINDOWED_MODE_SET;
   }
   
   write_log("\nLoading Data File... ");
   packfile_password(PASSWORD);
   datafile = load_datafile(data_file_name);
   packfile_password(NULL);
   if (datafile == NULL)
   {
      sprintf(temp_buf, "Error loading %s!", data_file_name);
      write_log(temp_buf);
      fatal_error(temp_buf);
   }
   write_log("OK");

   set_pallete(datafile[MAIN_PALETTE].dat);
   font = datafile[ARIAL12_FONT].dat;
   gui_fg_color = C_BLACK;  // set the foreground color
   gui_bg_color = C_WHITE;  // set the background color
   gui_mg_color = C_GRAY;   // set the disabled color
   set_mouse_sprite(NULL); // make mouse use current palette

   write_log("\nInitializing Serial Module... ");
   serial_module_init();
   write_log("OK");

   sprintf(temp_buf, "\nOpening COM%i... ", comport.number + 1);
   write_log(temp_buf);
   /* try opening comport (comport.status will be set) */
   open_comport();
   switch (comport.status)
   {
      case READY:
         write_log("OK");
         break;

      case NOT_OPEN:
         write_log("Error!");
         break;
         
      default:
         write_log("Unknown Status");
         break;
   }
}


static void shut_down()
{
   //clean up
   flush_config_file();
   write_log("\nShutting Down Serial Module... ");
   serial_module_shutdown();
   write_log("OK");
   write_log("\nUnloading Data File... ");
   unload_datafile(datafile);
   write_log("OK");
   write_log("\nShutting Down Allegro... ");
   allegro_exit();
   write_log("OK");
}


int main()
{
   char temp_buf[64];
   time_t current_time;
   
   time(&current_time);  // get current time, and store it in current_time
   strcpy(temp_buf, ctime(&current_time));
   temp_buf[strlen(temp_buf)-1] = 0;
   
   strcpy(log_file_name, "log.txt");
   remove(log_file_name);
   write_log(temp_buf);
#ifdef LOG_COMMS
   strcpy(comm_log_file_name, "comm_log.txt");
   remove(comm_log_file_name);
   write_comm_log("START_TIME", temp_buf);
#endif

   sprintf(temp_buf, "\nVersion: %s for %s", SCANTOOL_VERSION_STR, SCANTOOL_PLATFORM_STR);
   write_log(temp_buf);

   write_log("\n\nInitializing All Modules...\n---------------------------");
   init(); // initialize everything

   write_log("\n\nDisplaying Main Menu...\n-----------------------");
   display_main_menu(); // dislpay main menu
   write_log("\nMain Menu Closed");

   write_log("\n\nShutting Down All Modules...\n----------------------------");
   shut_down(); // shut down

   return EXIT_SUCCESS;
}
END_OF_MAIN()
