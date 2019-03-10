#include <string.h>
#include "globals.h"
#include "custom_gui.h"
#include "serial.h"
#include "sensors.h"
#include "options.h"
#include "version.h"
#include "about.h"

#define MSG_REFRESH  MSG_USER

static int logo_proc(int msg, DIALOG *d, int c);
static int large_text_proc(int msg, DIALOG *d, int c);
static int about_this_computer_proc(int msg, DIALOG *d, int c);
static int obd_info_proc(int msg, DIALOG *d, int c);
static int refresh_proc(int msg, DIALOG *d, int c);
static int obd_info_getter_proc(int msg, DIALOG *d, int c);
static int thanks_proc(int msg, DIALOG *d, int c);

static char whatisit[256];
static char whatcanitdo[256];
static char wheretoget[256];

static char obd_interface[64];
static char obd_mfr[64];
static char obd_protocol[64];
static char obd_system[64];

#define VER_STR   "Version " SCANTOOL_VERSION_EX_STR " for " SCANTOOL_PLATFORM_STR ", " SCANTOOL_COPYRIGHT_STR

static DIALOG about_dialog[] =
{
   /* (proc)                   (x)  (y)  (w)  (h)  (fg)     (bg)          (key) (flags) (d1) (d2) (dp)                    (dp2) (dp3) */
   { d_clear_proc,             0,   0,   0,   0,   0,       C_WHITE,       0,    0,      0,   0,   NULL,                   NULL, NULL },
   { logo_proc,                105, 25,  430, 58,  0,       0,             0,    0,      0,   0,   NULL,                   NULL, NULL },
   { st_ctext_proc,            320, 86,  216, 18,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   VER_STR,                NULL, NULL },
   { large_text_proc,          25,  112, 256, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "What is it?",          NULL, NULL },
   { super_textbox_proc,       25,  136, 407, 80,  C_BLACK, C_WHITE,       0,    0,      0,   0,   whatisit,               NULL, NULL },
   { large_text_proc,          25,  224, 256, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "What can it do?",      NULL, NULL },
   { super_textbox_proc,       25,  248, 407, 98,  C_BLACK, C_WHITE,       0,    0,      0,   0,   whatcanitdo,            NULL, NULL },
   { large_text_proc,          25,  354, 256, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Where can I get it?",  NULL, NULL },
   { super_textbox_proc,       25,  378, 590, 80,  C_BLACK, C_WHITE,       0,    0,      0,   0,   wheretoget,             NULL, NULL },
   { d_box_proc,               440, 136, 175, 231, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,                   NULL, NULL },
   { about_this_computer_proc, 448, 144, 160, 48,  C_BLACK, C_GREEN,       'c',  D_EXIT, 0,   0,   "About this &computer", NULL, NULL },
   { obd_info_proc,            448, 200, 160, 48,  C_BLACK, C_DARK_YELLOW, 'o',  D_EXIT, 0,   0,   "&OBD Information",     NULL, NULL },
   { thanks_proc,              448, 256, 160, 48,  C_BLACK, C_PURPLE,      'r',  D_EXIT, 0,   0,   "C&redits",             NULL, NULL },
   { d_button_proc,            448, 312, 160, 48,  C_BLACK, C_GREEN,       'm',  D_EXIT, 0,   0,   "Main &Menu",           NULL, NULL },
   { d_yield_proc,             0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,                   NULL, NULL },
   { NULL,                     0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,                   NULL, NULL }
};

static DIALOG thanks_dialog[] =
{
   /* (proc)            (x)  (y)  (w)  (h)  (fg)   (bg)            (key) (flags) (d1) (d2) (dp)      (dp2) (dp3) */
   { d_shadow_box_proc, 0,   0,   608, 290, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,     NULL, NULL },
   { d_shadow_box_proc, 0,   0,   608, 24,  C_BLACK, C_DARK_GRAY,   0,    0,      0,   0,   NULL,     NULL, NULL },
   { caption_proc,      304, 2,   301, 19,  C_WHITE, C_TRANSP,      0,    0,      0,   0,   "We would like to thank the following people and organizations:", NULL, NULL },
   { d_text_proc,       24,  36,  560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- DJ Delorie (www.delorie.com) for the DJGPP compiler ", NULL, NULL },
   { d_text_proc,       24,  60,  560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- Bloodshed Software (www.bloodshed.net) for the Dev-C++ IDE", NULL, NULL },
   { d_text_proc,       24,  84,  560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- Shawn Hargreaves for creating the Allegro library", NULL, NULL },
   { d_text_proc,       24,  108, 560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- Dim Zegebart and Neil Townsend for the DZComm serial library", NULL, NULL },
   { d_text_proc,       24,  132, 560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- Julien Cugniere for his Allegro dialog editor", NULL, NULL },
   { d_text_proc,       24,  156, 560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- Eric Botcazou and Allegro mailing list folks for their tips and suggestions", NULL, NULL },
   { d_text_proc,       24,  180, 560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- Joaquín Mª López Muñoz (joaquin@tid.es) for listports library", NULL, NULL },
   { d_text_proc,       24,  204, 560, 24,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "- All users who provided feedback and bug reports", NULL, NULL },
   { d_button_proc,     248, 235, 112, 40,  C_BLACK, C_DARK_YELLOW, 'c',  D_EXIT, 0,   0,   "&Close", NULL, NULL },
   { d_yield_proc,      0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,     NULL, NULL },
   { NULL,              0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,     NULL, NULL }
};

static DIALOG obd_info_dialog[] =
{
   /* (proc)               (x)  (y)  (w)  (h)  (fg)     (bg)           (key) (flags) (d1) (d2) (dp)               (dp2) (dp3) */
   { d_shadow_box_proc,    0,   0,   444, 188, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,              NULL, NULL },
   { d_shadow_box_proc,    0,   0,   444, 24,  C_BLACK, C_DARK_GRAY,   0,    0,      0,   0,   NULL,              NULL, NULL },
   { caption_proc,         222, 2,   218, 19,  C_WHITE, C_TRANSP,      0,    0,      0,   0,   "OBD Information", NULL, NULL },
   { d_rtext_proc,         12,  36,  108, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Interface:",      NULL, NULL },
   { d_text_proc,          124, 36,  316, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   obd_interface,     NULL, NULL },
   { d_rtext_proc,         12,  60,  108, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Manufacturer:",   NULL, NULL },
   { d_text_proc,          124, 60,  316, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   obd_mfr,           NULL, NULL },
   { d_rtext_proc,         12,  84,  108, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Protocol:",       NULL, NULL },
   { d_text_proc,          124, 84,  316, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   obd_protocol,      NULL, NULL },
   { d_rtext_proc,         12,  108, 108, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "OBD System:",     NULL, NULL },
   { d_text_proc,          124, 108, 316, 16,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   obd_system,        NULL, NULL },
   { refresh_proc,         140, 144, 76,  32,  C_BLACK, C_GREEN,       0,    D_EXIT, 0,   0,   "Refresh",         NULL, NULL },
   { d_button_proc,        231, 144, 76,  32,  C_BLACK, C_DARK_YELLOW, 0,    D_EXIT, 0,   0,   "Close",           NULL, NULL },
   { obd_info_getter_proc, 0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,              NULL, NULL },
   { d_yield_proc,         0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,              NULL, NULL },
   { NULL,                 0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,              NULL, NULL }
};


int display_about()
{
   strcpy(whatisit, "ScanTool.net is multi-platform, open-source software designed to work with the ElmScan and OBDLink families of OBD-II interfaces, our inexpensive alternatives to professional diagnostic scan tools.");
   sprintf(whatcanitdo, "ScanTool.net v%s allows you to read and clear trouble codes, and display real-time sensor data.  Software with more advanced features, such as data logging,  virtual dashboard, and support for additional test modes is available from third-party vendors.", SCANTOOL_VERSION_STR);
   strcpy(wheretoget, "You can download the latest version of this software, and buy your scan tool from www.ScanTool.net. There, you can also find contact information for your local distributor, and links to third-party software.");

   return do_dialog(about_dialog, -1); // do the dialog
}


int logo_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         d->dp = create_bitmap(430, 58);
         pivot_sprite(d->dp, datafile[LOGO_BMP].dat, 430, 0, 0, 0, itofix(64));
         break;
         
      case MSG_END:
         destroy_bitmap(d->dp);
         break;
   }
   
   return d_bitmap_proc(msg, d, c);
}


int obd_info_proc(int msg, DIALOG *d, int c)
{
   int ret = d_button_proc(msg, d, c);

   if (msg == MSG_START)
      centre_dialog(obd_info_dialog);

   if (ret == D_CLOSE)
   {
      if (comport.status == READY)
      {
         obd_interface[0] = 0;
         obd_mfr[0] = 0;
         obd_protocol[0] = 0;
         obd_system[0] = 0;

         popup_dialog(obd_info_dialog, -1);
      }
      else
      {
         while (comport.status != READY)
         {
            if (alert("Port is not ready.", "Please check that you specified the correct port", "and that no other application is using it", "Configure &Port", "&Cancel", 'p', 'c') == 1)
               display_options(); // let the user choose correct settings
            else
               return D_REDRAWME;
         }
      }

      return D_O_K;
   }

   return ret;
}

int refresh_proc(int msg, DIALOG *d, int c)
{
   int ret = d_button_proc(msg, d, c);

   if (ret == D_CLOSE)
   {
      broadcast_dialog_message(MSG_REFRESH, 0);
      return D_O_K;
   }
      
   return ret;
}


void clear_obd_info()
{
   strcpy(obd_interface, "N/A");
   strcpy(obd_mfr, "N/A");
   strcpy(obd_protocol, "N/A");
   strcpy(obd_system, "N/A");
}


void format_id_string(char *str)
{
   if (strncmp(str, "ELM32", 4) == 0)
   {
      memmove(str + 7, str + 6, strlen(str) - 5);
      str[6] = ' ';
   }
   else if (strncmp(str, "OBDLinkCI", 9) == 0)
   {
      memmove(str + 11, str + 9, strlen(str) - 8);
      memmove(str + 8, str + 7, 2);
      str[7] = ' ';
      str[10] = ' ';
   }
   else if (strncmp(str, "OBDLink", 7) == 0)
   {
      memmove(str + 8, str + 7, strlen(str) - 6);
      str[7] = ' ';
   }
   else if (strncmp(str, "STN", 3) == 0)
   {
      memmove(str + 8, str + 7, strlen(str) - 6);
      str[7] = ' ';
   }
}


// OBD info getter states
#define OBD_INFO_IDLE         0
#define OBD_INFO_START        1
#define OBD_INFO_TX_0100      2
#define OBD_INFO_WAIT_ATZ     3
#define OBD_INFO_ECU_TIMEOUT  4
#define OBD_INFO_WAIT_0100    5
#define OBD_INFO_WAIT_011C    6

#define OBD_INFO_MAX_RETRIES  3

int obd_info_getter_proc(int msg, DIALOG *d, int c)
{
   static int state = OBD_INFO_START;
   static int device = 0;
   static int retries = 0;
   static char response[256];
   char buf[128];
   int status;

   switch (msg)
   {
      case MSG_START:
      case MSG_REFRESH:
         state = OBD_INFO_START;
         break;
         
      case MSG_IDLE:
         switch (state)
         {
            case OBD_INFO_IDLE:
               break;

            case OBD_INFO_START:
               strcpy(obd_interface, "detecting...");
               strcpy(obd_mfr, empty_string);
               strcpy(obd_protocol, empty_string);
               strcpy(obd_system, empty_string);
               retries = 0;
               state = OBD_INFO_TX_0100;
               return D_REDRAW;

            case OBD_INFO_TX_0100:
               if (serial_timer_running)
               {
                  // wait until we either get a prompt or the serial timer times out
                  while ((read_comport(buf) != PROMPT) && !serial_time_out)
                     ;
               }
               
               send_command("sti");
               start_serial_timer(AT_TIMEOUT);
               device = 0;
               response[0] = 0;
               
               while ((status = read_comport(buf)) != PROMPT && !serial_time_out)
               {
                  if (status == DATA) // if new data detected in com port buffer
                     strcat(response, buf); // append contents of buf to response
               }
               stop_serial_timer();
               
               if (status == PROMPT)
               {
                  strcat(response, buf);
                  status = process_response("sti", response);
                  
                  if (status != INTERFACE_OBDLINK)
                  {
                     send_command("ati");
                     start_serial_timer(AT_TIMEOUT);
                     response[0] = 0;
                     
                     while ((status = read_comport(buf)) != PROMPT && !serial_time_out)
                     {
                        if (status == DATA) // if new data detected in com port buffer
                           strcat(response, buf); // append contents of buf to response
                     }
                     stop_serial_timer();
                     
                     if (status == PROMPT)
                     {
                        strcat(response, buf);
                        status = process_response("ati", response);
                     }
                  }
               }
               
               if (serial_time_out)  // if serial timeout
               {
                  if (retries < OBD_INFO_MAX_RETRIES)
                  {
                     retries++;
                     sprintf(obd_interface, "retrying (%i)...", retries);
                     state = OBD_INFO_TX_0100;
                     return D_REDRAW;
                  }
                  else
                  {
                     if (alert("Connection to interface was lost", NULL, NULL, "&Retry", "&Cancel", 'r', 'c') == 1)
                     {
                        state = OBD_INFO_START;
                        break;
                     }
                     else
                     {
                        clear_obd_info();
                        state = OBD_INFO_IDLE;
                        return D_REDRAW;
                     }
                  }
                  break;
               }

               if (status < INTERFACE_ID)
               {
                  send_command("atz"); // reset chip
                  start_serial_timer(ATZ_TIMEOUT);
                  response[0] = 0;
                  state = OBD_INFO_WAIT_ATZ;
               }
               else
               {
                  format_id_string(response);
                  strcpy(obd_interface, response);
                  
                  device = status;
                  if (device == INTERFACE_ELM327 || device == INTERFACE_OBDLINK)
                  {
                     send_command("at@1"); // get mfr string
                     start_serial_timer(AT_TIMEOUT);
                     response[0] = 0;

                     while ((status = read_comport(buf)) != PROMPT && !serial_time_out)
                     {
                        if(status == DATA) // if new data detected in com port buffer
                           strcat(response, buf); // append contents of buf to response
                     }
                     
                     if (status == PROMPT)  // if we got the prompt
                     {
                        stop_serial_timer();
                        strcat(response, buf);
                        
                        response[strlen(response) - 2] = 0;
                        if (strcmp(response, "OBDII to RS232 Interpreter") == 0)
                        {
                           strcpy(obd_mfr, "N/A");
                           
                           send_command("at@2");
                           start_serial_timer(AT_TIMEOUT);
                           response[0] = 0;
                           
                           while ((status = read_comport(buf)) != PROMPT && !serial_time_out)
                           {
                              if(status == DATA) // if new data detected in com port buffer
                                 strcat(response, buf); // append contents of buf to response
                           }
                           
                           if (status == PROMPT)
                           {
                              stop_serial_timer();
                              strcat(response, buf);
                              
                              response[strlen(response) - 2] = 0;
                              if (strcmp(response, "SCANTOOL.NET") == 0)
                                 strcpy(obd_mfr, "SCANTOOL.NET LLC");
                           }
                           // We'll let the next request handle serial timeout
                        }
                        else
                           strcpy(obd_mfr, response);
                        
                        send_command("atsp0");
                        start_serial_timer(AT_TIMEOUT);

                        while ((status = read_comport(buf)) != PROMPT && !serial_time_out)
                           ;
                        
                        if (status == PROMPT)
                        {
                           start_serial_timer(ECU_TIMEOUT);
                           strcpy(obd_protocol, "waiting for ECU timeout...");
                           state = OBD_INFO_ECU_TIMEOUT;
                           return D_REDRAW;
                        }
                        else  // if serial timeout
                        {
                           stop_serial_timer();
                           if (alert("Connection to interface was lost", NULL, NULL, "&Retry", "&Cancel", 'r', 'c') == 1)
                           {
                              state = OBD_INFO_START;
                              break;
                           }
                           else
                           {
                              clear_obd_info();
                              state = OBD_INFO_IDLE;
                              return D_REDRAW;
                           }
                        }
                     }
                     else  // if serial timeout
                     {
                        stop_serial_timer();
                        if (alert("Connection to interface was lost", NULL, NULL, "&Retry", "&Cancel", 'r', 'c') == 1)
                        {
                           state = OBD_INFO_START;
                           break;
                        }
                        else
                        {
                           clear_obd_info();
                           state = OBD_INFO_IDLE;
                           return D_REDRAW;
                        }
                     }
                  }
                  else
                     strcpy(obd_mfr, "N/A");
                     
                  send_command("0100");
                  start_serial_timer(OBD_REQUEST_TIMEOUT);
                  response[0] = 0;
                  strcpy(obd_protocol, "detecting...");
                  state = OBD_INFO_WAIT_0100;
                  return D_REDRAW;
               }
               break;

            case OBD_INFO_WAIT_ATZ:
               status = read_comport(buf);
               
               if(status == DATA) // if new data detected in com port buffer
                  strcat(response, buf); // append contents of buf to response
               else if (status == PROMPT)  // if we got the prompt
               {
                  stop_serial_timer();
                  strcat(response, buf);
                  status = process_response("atz", response);
                  
                  format_id_string(response);
                  strcpy(obd_interface, response);
                  strcpy(obd_mfr, "N/A");
                  
                  if (status == INTERFACE_ELM323)
                  {
                     start_serial_timer(ECU_TIMEOUT);
                     strcpy(obd_protocol, "waiting for ECU timeout...");
                     device = INTERFACE_ELM323;
                     state = OBD_INFO_ECU_TIMEOUT;
                  }
                  else if (status >= INTERFACE_ID)
                  {
                     send_command("0100");
                     start_serial_timer(OBD_REQUEST_TIMEOUT);
                     response[0] = 0;
                     strcpy(obd_protocol, "detecting...");
                     device = status;
                     state = OBD_INFO_WAIT_0100;
                  }
                  else if (status == HEX_DATA)
                  {
                     state = OBD_INFO_START;
                     break;
                  }
                  else
                  {
                     if (display_error_message(status, TRUE) == 1)
                     {
                        state = OBD_INFO_START;
                        break;
                     }
                     else
                     {
                        clear_obd_info();
                        state = OBD_INFO_IDLE;
                        return D_REDRAW;
                     }
                  }
                  
                  return D_REDRAW;
               }
               else if (serial_time_out)
               {
                  stop_serial_timer();
                  if (alert("Connection to interface was lost", NULL, NULL, "&Retry", "&Cancel", 'r', 'c') == 1)
                  {
                     state = OBD_INFO_START;
                     break;
                  }
                  else
                  {
                     clear_obd_info();
                     state = OBD_INFO_IDLE;
                     return D_REDRAW;
                  }
               }
               break;

            case OBD_INFO_ECU_TIMEOUT:
               if (serial_time_out)
               {
                  stop_serial_timer();
                  send_command("0100");
                  start_serial_timer(OBD_REQUEST_TIMEOUT);
                  response[0] = 0;
                  strcpy(obd_protocol, "detecting...");
                  state = OBD_INFO_WAIT_0100;
                  return D_REDRAW;
               }
               break;

            case OBD_INFO_WAIT_0100:
               status = read_comport(buf);

               if(status == DATA) // if new data detected in com port buffer
                  strcat(response, buf); // append contents of buf to response
               else if (status == PROMPT)  // if we got the prompt
               {
                  stop_serial_timer();
                  strcat(response, buf);
                  status = process_response("0100", response);

                  if (status == HEX_DATA)
                  {
                     if (device != INTERFACE_ELM327 && device != INTERFACE_OBDLINK)
                        strcpy(obd_protocol, get_protocol_string(device, 0));
                     else
                     {
                        send_command("atdpn");
                        start_serial_timer(AT_TIMEOUT);
                        response[0] = 0;

                        while ((status = read_comport(buf)) != PROMPT && !serial_time_out)
                        {
                           if (status == DATA) // if new data detected in com port buffer
                              strcat(response, buf); // append contents of buf to response
                        }

                        if (status == PROMPT)  // if we got the prompt
                        {
                           stop_serial_timer();
                           strcat(response, buf);
                           process_response("atdpn", response);

                           status = (response[0] == 'A') ? response[1] : response[0];
                           strcpy(obd_protocol, get_protocol_string(device, status - 48));
                        }
                        else // if serial timeout
                        {
                           stop_serial_timer();
                           if (alert("Connection to interface was lost", NULL, NULL, "&Retry", "&Cancel", 'r', 'c') == 1)
                           {
                              state = OBD_INFO_START;
                              break;
                           }
                           else
                           {
                              clear_obd_info();
                              state = OBD_INFO_IDLE;
                              return D_REDRAW;
                           }
                        }
                     }
                  }
                  else
                  {
                     if (status == ERR_NO_DATA || status == UNABLE_TO_CONNECT)
                        status = alert("There may have been a loss of connection.", "Please check connection to the vehicle,", "and make sure the ignition is ON", "&Retry", "&Cancel", 'r', 'c');
                     else if (status >= INTERFACE_ID)
                        status = 1;
                     else  // all other errors
                        status = display_error_message(status, TRUE);

                     if (status == 1)
                     {
                        state = OBD_INFO_START;
                        break;
                     }
                     else
                     {
                        clear_obd_info();
                        state = OBD_INFO_IDLE;
                        return D_REDRAW;
                     }
                  }

                  send_command("011C");
                  start_serial_timer(OBD_REQUEST_TIMEOUT);
                  response[0] = 0;
                  strcpy(obd_system, "detecting...");
                  state = OBD_INFO_WAIT_011C;
                  return D_REDRAW;
               }
               else if (serial_time_out)
               {
                  stop_serial_timer();
                  if (alert("Connection to interface was lost", NULL, NULL, "&Retry", "&Cancel", 'r', 'c') == 1)
                  {
                     state = OBD_INFO_START;
                     break;
                  }
                  else
                  {
                     clear_obd_info();
                     state = OBD_INFO_IDLE;
                     return D_REDRAW;
                  }
               }
               break;

            case OBD_INFO_WAIT_011C:
               status = read_comport(buf);

               if(status == DATA) // if new data detected in com port buffer
                  strcat(response, buf); // append contents of buf to response
               else if (status == PROMPT)  // if we got the prompt
               {
                  stop_serial_timer();
                  strcat(response, buf);
                  status = process_response("011C", response);

                  if (status == HEX_DATA)
                  {
                     if (find_valid_response(buf, response, "411C", NULL))
                     {
                        buf[6] = 0;  // solves problem where response is padded with zeroes
                        obd_requirements_formula((int)strtol(buf + 4, NULL, 16), buf);  // get OBD requirements string
                        strcpy(obd_system, buf);
                     }
                  }
                  else if (status == ERR_NO_DATA)
                     strcpy(obd_system, "N/A");
                  else  // other errors
                  {
                     if (display_error_message(status, TRUE) == 1)
                     {
                        state = OBD_INFO_START;
                        break;
                     }
                     else
                        clear_obd_info();
                  }

                  state = OBD_INFO_IDLE;
                  return D_REDRAW;
               }
               else if (serial_time_out)
               {
                  stop_serial_timer();
                  if (alert("Connection to interface was lost", NULL, NULL, "&Retry", "&Cancel", 'r', 'c') == 1)
                  {
                     state = OBD_INFO_START;
                     break;
                  }
                  else
                  {
                     clear_obd_info();
                     state = OBD_INFO_IDLE;
                     return D_REDRAW;
                  }
               }
               break;
         }
         break;
   }

   return D_O_K;
}


int thanks_proc(int msg, DIALOG *d, int c)
{
   int ret = d_button_proc(msg, d, c);

   if (msg == MSG_START)
      centre_dialog(thanks_dialog);
   
   if (ret == D_CLOSE)
   {
      popup_dialog(thanks_dialog, -1);
      return D_REDRAWME;
   }

   return ret;
}


int large_text_proc(int msg, DIALOG *d, int c)
{
   if (msg == MSG_START)
      d->dp2 = datafile[ARIAL18_FONT].dat; // load the font

   return d_text_proc(msg, d, c);
}


int about_this_computer_proc(int msg, DIALOG *d, int c)
{
   char cpu_info_buf[96];
   char os_type_buf[96];
   char os_name[32];
   char processor_vendor[64];
   char processor_family[64];
   char processor_model[64];
   int ret;
   
   // clear strings
   cpu_info_buf[0] = 0;
   os_type_buf[0] = 0;
   os_name[0] = 0;
   processor_vendor[0] = 0;
   processor_family[0] = 0;
   processor_model[0] = 0;
   
   ret = d_button_proc(msg, d, c);
   
   if (ret == D_CLOSE)
   {
      // reset the variables
      processor_model[0] = 0;

      // determine processor vendor
      if(strcmp("GenuineIntel", cpu_vendor) == 0)
         strcpy(processor_vendor, "an Intel");
      else if(strcmp("AuthenticAMD", cpu_vendor) == 0)
         strcpy(processor_vendor, "an AMD");
      else if(strcmp("CyrixInstead", cpu_vendor) == 0)
         strcpy(processor_vendor, "a Cyrix");
      else if(strcmp("CentaurHauls", cpu_vendor) == 0)
         strcpy(processor_vendor, "a Centaur");
      else if(strcmp("NexGenDriven", cpu_vendor) == 0)
         strcpy(processor_vendor, "a NexGen");
      else if(strcmp("GenuineTMx86", cpu_vendor) == 0)
         strcpy(processor_vendor, "a Transmeta");
      else if(strcmp("RISERISERISE", cpu_vendor) == 0)
         strcpy(processor_vendor, "a Rise");
      else if(strcmp("UMC UMC UMC", cpu_vendor) == 0)
         strcpy(processor_vendor, "an UMC");
      else
         strcpy(processor_vendor, "an unknown CPU vendor");


      if(!strcmp("GenuineIntel", cpu_vendor))
      {
         if (cpu_family == 4)
         {
            switch(cpu_model)
            {
               case 0:
                  strcpy(processor_model, " 486 DX-25/33");
                  break;
               case 1:
                  strcpy(processor_model, " 486 DX-50");
                  break;
               case 2:
                  strcpy(processor_model, " 486 SX");
                  break;
               case 3:
                  strcpy(processor_model, " 486 DX/2");
                  break;
               case 4:
                  strcpy(processor_model, " 486 SL");
                  break;
               case 5:
                  strcpy(processor_model, " 486 SX/2");
                  break;
               case 7:
                  strcpy(processor_model, " 486 DX/2-WB");
                  break;
               case 8:
                  strcpy(processor_model, " 486 DX/4");
                  break;
               case 9:
                  strcpy(processor_model, " 486 DX/4-WB");
            }
         }

         if (cpu_family == 5)
         {
            switch(cpu_model)
            {
               case 0:
                  strcpy(processor_model, " Pentium 60/66 A-step");
                  break;
               case 1:
                  strcpy(processor_model, " Pentium 60/66");
                  break;
               case 2:
                  strcpy(processor_model, " Pentium 75 - 200");
                  break;
               case 3:
                  strcpy(processor_model, " OverDrive PODP5V83");
                  break;
               case 4:
                  strcpy(processor_model, " Pentium MMX");
                  break;
               case 7:
                  strcpy(processor_model, " Mobile Pentium 75 - 200");
                  break;
               case 8:
                  strcpy(processor_model, " Mobile Pentium MMX");
                  break;
            }
         }

         if (cpu_family == 6)
         {
            switch(cpu_model)
            {
               case 0:
                  strcpy(processor_model, " Pentium Pro A-step");
                  break;
               case 1:
                  strcpy(processor_model, " Pentium Pro");
                  break;
               case 3:
                  strcpy(processor_model, " Pentium II (Klamath)");
                  break;
               case 5:
                  strcpy(processor_model, " Pentium II, Celeron, or Mobile Pentium II");
                  break;
               case 6:
                  strcpy(processor_model, " Mobile Pentium II or Celeron (Mendocino)");
                  break;
               case 7:
                  strcpy(processor_model, " Pentium III (Katmai)");
                  break;
               case 8:
                  strcpy(processor_model, " Pentium III (Coppermine)");
                  break;
            }
         }

         if (cpu_family >= 15)
            strcpy(processor_model, " Pentium IV or better");
      } // end of Intel block

      if(!strcmp("AuthenticAMD", cpu_vendor))
      {
         switch(cpu_family)
         {
            case 4:
               switch (cpu_model)
               {
                  case 3:
                     strcpy(processor_model, " 486 DX/2");
                     break;
                  case 7:
                     strcpy(processor_model, " 486 DX/2-WB");
                     break;
                  case 8:
                     strcpy(processor_model, " 486 DX/4");
                     break;
                  case 9:
                     strcpy(processor_model, " 486 DX/4-WB");
                     break;
                  case 14:
                     strcpy(processor_model, " Am5x86-WT");
                     break;
                  case 15:
                     strcpy(processor_model, " Am5x86-WB");
                     break;
               }
               break;

            case 5:
               switch (cpu_model)
               {
                  case 0:
                     strcpy(processor_model, " K5/SSA5");
                     break;
                  case 1: case 2: case 3:
                     strcpy(processor_model, " K5");
                     break;
                  case 6: case 7:
                     strcpy(processor_model, " K6");
                     break;
                  case 8:
                     strcpy(processor_model, " K6-2");
                     break;
                  case 9:
                     strcpy(processor_model, " K6-3");
                     break;
                  case 13:
                     strcpy(processor_model, " K6-2+ or K6-III+");
                     break;
               }
               break;
            
            case 6:
               switch (cpu_model)
               {
                  case 0: case 1:
                     strcpy(processor_model, " Athlon (25 um)");
                     break;
                  case 2:
                     strcpy(processor_model, " Athlon (18 um)");
                     break;
                  case 3:
                     strcpy(processor_model, " Duron");
                     break;
                  case 4:
                     strcpy(processor_model, " Athlon (Thunderbird)");
                     break;
                  case 6:
                     strcpy(processor_model, " Athlon (Palamino)");
                     break;
                  case 7:
                     strcpy(processor_model, " Duron (Morgan)");
                     break;
               }
               break;
         }
      } // end of AMD block

      if(strcmp("CyrixInstead", cpu_vendor) == 0)
      {
         switch(cpu_family)
         {
            case 4:
               strcpy(processor_model, " MediaGX");
               break;
               
            case 5:
               switch(cpu_model)
               {
                  case 2:
                     strcpy(processor_model, " 6x86/6x86L");
                     break;
                  case 4:
                     strcpy(processor_model, " MediaGX MMX Enhanced");
                     break;
               }
               break;
               
            case 6:
               switch(cpu_model)
               {
                  case 0:
                     strcpy(processor_model, " m II");
                     break;
                  case 5:
                     strcpy(processor_model, " VIA Cyrix M2 core");
                     break;
                  case 6:
                     strcpy(processor_model, " WinChip C5A");
                     break;
                  case 7:
                     strcpy(processor_model, " WinChip C5B");
                     break;
               }
               break;
         }
      } // end of Cyrix block
      
      
      if(!strcmp("UMC UMC UMC", cpu_vendor))
      {
         if (cpu_family == 4)
         {
            if (cpu_model == 1)
               strcpy(processor_model, " U5D");
            if (cpu_model == 2)
               strcpy(processor_model, " U5S");
         }
      }  // end of UMC block
      

      if(strcmp("GenuineTMx86", cpu_vendor) == 0)
         if ((cpu_family == 5) && (cpu_model == 0))
            strcpy(processor_model, " Nx586"); // end of NexGen block
            
      if(strcmp("RISERISERISE", cpu_vendor) == 0)
         if ((cpu_family == 5) && ((cpu_model == 0) || (cpu_model == 1)))
            strcpy(processor_model, " mP6"); // end of Rise block
            

      switch (os_type)
      {
         case OSTYPE_UNKNOWN:
            strcpy(os_name, "MSDOS");
            break;

         case OSTYPE_WIN3:
            strcpy(os_name, "Windows 3.x");
            break;

         case OSTYPE_WIN95:
            strcpy(os_name, "Windows 95");
            break;
            
         case OSTYPE_WIN98:
            strcpy(os_name, "Windows 98");
            break;

         case OSTYPE_WINME:
            strcpy(os_name, "Windows ME");
            break;

         case OSTYPE_WINNT:
            strcpy(os_name, "Windows NT");
            break;

         case OSTYPE_WIN2000:
            strcpy(os_name, "Windows 2000");
            break;

         case OSTYPE_WINXP:
            strcpy(os_name, "Windows XP");
            break;

         case OSTYPE_OS2:
            strcpy(os_name, "OS/2");
            break;

         case OSTYPE_WARP:
            strcpy(os_name, "OS/2 Warp 3");
            break;

         case OSTYPE_DOSEMU:
            strcpy(os_name, "Linux DOSEMU");
            break;

         case OSTYPE_OPENDOS:
            strcpy(os_name, "Caldera OpenDOS");
            break;

         case OSTYPE_LINUX:
            strcpy(os_name, "Linux");
            break;

         case OSTYPE_FREEBSD:
            strcpy(os_name, "Free BSD"); // do not translate!
            break;

         case OSTYPE_UNIX:
            strcpy(os_name, "Unix");
            break;

         case OSTYPE_BEOS:
            strcpy(os_name, "BeOS");
            break;

         case OSTYPE_QNX:
            strcpy(os_name, "QNX");
            break;

         case OSTYPE_MACOS:
            strcpy(os_name, "Mac OS");
            break;

         default:
            strcpy(os_name, "Unknown OS");
      }


      sprintf(cpu_info_buf, "You have %s%s%s based computer (%i:%i),", processor_vendor, processor_family, processor_model, cpu_family, cpu_model);
      sprintf(os_type_buf, "running %s version %i.%i", os_name, os_version, os_revision);
      alert(cpu_info_buf, os_type_buf, NULL, "OK", NULL, 0, 0);

      return D_REDRAWME;
   }

   return ret;
}
