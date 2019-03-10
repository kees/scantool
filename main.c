#include "globals.h"
#include "main_menu.h"
#include "error_handlers.h"
#include "options.h"
#include "serial.h"
#include "version.h"

#include <string.h>

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
   
   logfile = stderr;
   if (logfile == NULL)
      fatal_error("Could not open log file for writing!");
   fprintf(logfile, "%s", log_string);
   if (logfile != stderr)
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
   if (asprintf(&options_file_name, "%s/.scantoolrc", getenv("HOME")) < 0) {
      perror("asprintf");
      exit(1);
   }
   data_file_name = strdup("/usr/share/scantool/scantool.dat");
   code_defs_file_name = strdup("/usr/share/scantool/codes.dat");
   
   datafile = NULL;
   comport.status = NOT_OPEN;
   display_mode = 0;

   set_uformat(U_ASCII);
   
   /* initialize hardware */
   write_log("Initializing Allegro... ");
   allegro_init();
   write_log("OK\n");
   
   set_window_title(WINDOW_TITLE);
   
   write_log("Installing Timers... ");
   if (install_timer() != 0)
   {
      write_log("Error!\n");
      fatal_error("Error installing timers");
   }
   write_log("OK\n");
   write_log("Installing Keyboard... ");
   install_keyboard();
   write_log("OK\n");
   write_log("Installing Mouse... ");
   install_mouse();
   write_log("OK\n");

   /* load options from file, the defaults will be automatically substituted if file does not exist */
   write_log("Loading Preferences... ");
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
   write_log("OK\n");

   display_mode |= FULLSCREEN_MODE_SUPPORTED;
   
   write_log("Trying Windowed Graphics Mode... ");
   if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0) == 0)
   {
      display_mode |= WINDOWED_MODE_SUPPORTED;
      write_log("OK\n");
   }
   else
   {
      display_mode &= ~(WINDOWED_MODE_SUPPORTED | WINDOWED_MODE_SET);
      write_log(allegro_error);
   }

   if (!(display_mode & WINDOWED_MODE_SET))
   {
      write_log("Trying Full Screen Graphics Mode... ");
      if (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, 640, 480, 0, 0) == 0)
      {
         display_mode |= FULLSCREEN_MODE_SUPPORTED;
         write_log("OK\n");
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
   
   write_log("Loading Data File... ");
   packfile_password(PASSWORD);
   datafile = load_datafile(data_file_name);
   packfile_password(NULL);
   if (datafile == NULL)
   {
      sprintf(temp_buf, "Error loading %s!\n", data_file_name);
      write_log(temp_buf);
      fatal_error(temp_buf);
   }
   write_log("OK\n");

   set_pallete(datafile[MAIN_PALETTE].dat);
   font = datafile[ARIAL12_FONT].dat;
   gui_fg_color = C_BLACK;  // set the foreground color
   gui_bg_color = C_WHITE;  // set the background color
   gui_mg_color = C_GRAY;   // set the disabled color
   set_mouse_sprite(NULL); // make mouse use current palette

   write_log("Initializing Serial Module... ");
   serial_module_init();
   write_log("OK\n");

   sprintf(temp_buf, "Opening COM%i... ", comport.number + 1);
   write_log(temp_buf);
   /* try opening comport (comport.status will be set) */
   open_comport();
   switch (comport.status)
   {
      case READY:
         write_log("OK\n");
         break;

      case NOT_OPEN:
         write_log("Error!\n");
         break;
         
      default:
         write_log("Unknown Status\n");
         break;
   }
}


static void shut_down()
{
   //clean up
   flush_config_file();
   write_log("Shutting Down Serial Module... ");
   serial_module_shutdown();
   write_log("OK\n");
   write_log("Unloading Data File... ");
   unload_datafile(datafile);
   write_log("OK\n");
   write_log("Shutting Down Allegro... ");
   allegro_exit();
   write_log("OK\n");
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
   write_log("\n");
#ifdef LOG_COMMS
   strcpy(comm_log_file_name, "comm_log.txt");
   remove(comm_log_file_name);
   write_comm_log("START_TIME", temp_buf);
#endif

   sprintf(temp_buf, "Version: %s for %s\n", SCANTOOL_VERSION_STR, SCANTOOL_PLATFORM_STR);
   write_log(temp_buf);

   write_log("\nInitializing All Modules...\n---------------------------\n");
   init(); // initialize everything

   write_log("\nDisplaying Main Menu...\n-----------------------\n");
   display_main_menu(); // dislpay main menu
   write_log("Main Menu Closed\n");

   write_log("\nShutting Down All Modules...\n----------------------------\n");
   shut_down(); // shut down

   return EXIT_SUCCESS;
}
END_OF_MAIN()
