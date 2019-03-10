#include <string.h>

#include "reset.h"
#include "globals.h"
#include "custom_gui.h"
#include "main_menu.h"
#include "serial.h"


static int reset_proc(int msg, DIALOG *d, int c);

static char reset_status_msg[64];


static DIALOG reset_chip_dialog[] =
{
   /* (proc)            (x)  (y) (w)  (h) (fg)     (bg)          (key) (flags) (d1) (d2) (dp)              (dp2) (dp3) */
   { d_shadow_box_proc, 0,   0,  340, 64, C_BLACK, C_LIGHT_GRAY, 0,    0,      0,   0,   NULL,             NULL, NULL },
   { caption_proc,      170, 24, 158, 16, C_BLACK, C_TRANSP,     0,    0,      0,   0,   reset_status_msg, NULL, NULL },
   { reset_proc,        0,   0,  0,   0,  0,       0,            0,    0,      0,   0,   NULL,             NULL, NULL },
   { d_yield_proc,      0,   0,  0,   0,  0,       0,            0,    0,      0,   0,   NULL,             NULL, NULL },
   { NULL,              0,   0,  0,   0,  0,       0,            0,    0,      0,   0,   NULL,             NULL, NULL }
};


void reset_chip()
{
   centre_dialog(reset_chip_dialog);
   popup_dialog(reset_chip_dialog, -1);
}


enum ResetState
{
   RESET_SEND_RESET_REQUEST,
   RESET_GET_REPLY_TO_RESET,
   RESET_SEND_AT_AT1_REQUEST,
   RESET_GET_AT_AT1_RESPONSE,
   RESET_SEND_AT_AT2_REQUEST,
   RESET_GET_AT_AT2_RESPONSE,
   RESET_START_ECU_TIMER,
   RESET_WAIT_FOR_ECU_TIMEOUT,
   RESET_SEND_DETECT_PROTOCOL_REQUEST,
   RESET_GET_REPLY_TO_DETECT_PROTOCOL,
   RESET_CLOSE_DIALOG,
   RESET_INTERFACE_NOT_FOUND,
   RESET_HANDLE_CLONE
};


int Reset_send_reset_request(char *response)
{
   char buf[128];
   
   if (serial_timer_running) // if serial timer is running
   {
      // wait until we either get a prompt or the timer times out
      while ((read_comport(buf) != PROMPT) && !serial_time_out)
         ;
   }
   
   send_command("atz"); // reset the chip
   start_serial_timer(ATZ_TIMEOUT);  // start serial timer
   response[0] = 0;
   
   return RESET_GET_REPLY_TO_RESET;
}


int Reset_get_reply_to_reset(char *response, int *device)
{
   char buf[128];
   int next_state;
   int status;
   
   status = read_comport(buf);                  // read comport
   
   if (status == DATA)                           // if new data detected in com port buffer
   {
      strcat(response, buf);                    // append contents of buf to response
      next_state = RESET_GET_REPLY_TO_RESET;    // come back for more data
   }
   else if (status == PROMPT) // if '>' detected
   {
      stop_serial_timer();
      strcat(response, buf);
      *device = process_response("atz", response);
      
      if (*device == INTERFACE_ELM323)
         next_state = RESET_START_ECU_TIMER;
      else if (*device == INTERFACE_ELM327)
         next_state = RESET_SEND_AT_AT1_REQUEST;
      else
         next_state = RESET_CLOSE_DIALOG;
   }
   else if (serial_time_out) // if the timer timed out
   {
      stop_serial_timer(); // stop the timer
      alert("Interface was not found", NULL, NULL, "OK", NULL, 0, 0);
      next_state = RESET_CLOSE_DIALOG; // close dialog
   }  
   else  // serial buffer was empty, but we still got time
      next_state = RESET_GET_REPLY_TO_RESET;
   
   return next_state;
}


int Reset_send_at_at1_request(char *response)
{
   send_command("at@1");
   start_serial_timer(AT_TIMEOUT);  // start serial timer
   response[0] = 0;
   
   return RESET_GET_AT_AT1_RESPONSE;
}


int Reset_get_at_at1_response(char *response)
{
   char buf[128];
   int next_state;
   int status;
   
   status = read_comport(buf);                  // read comport
   
   if (status == DATA)                          // if new data detected in com port buffer
   {
      strcat(response, buf);                    // append contents of buf to response
      next_state = RESET_GET_AT_AT1_RESPONSE;   // come back for more data
   }
   else if (status == PROMPT) // if '>' detected
   {
      stop_serial_timer();
      strcat(response, buf);
      status = process_response("at@1", response);
      
      if (status == STN_MFR_STRING)
         next_state = RESET_START_ECU_TIMER;
      else if (status == ELM_MFR_STRING)
         next_state = RESET_SEND_AT_AT2_REQUEST;
      else
         next_state = RESET_HANDLE_CLONE;
   }
   else if (serial_time_out)
   {
      stop_serial_timer();
      alert("Connection with interface lost", NULL, NULL, "OK", NULL, 0, 0);
      next_state = RESET_CLOSE_DIALOG; // close dialog
   }
   else  // serial buffer was empty, but we still got time
      next_state = RESET_GET_AT_AT1_RESPONSE;
   
   return next_state;
}


int Reset_send_at_at2_request(char *response)
{
   send_command("at@2");
   start_serial_timer(AT_TIMEOUT);  // start serial timer
   response[0] = 0;
   
   return RESET_GET_AT_AT2_RESPONSE;
}


int Reset_get_at_at2_response(char *response)
{
   char buf[128];
   int next_state;
   int status;
   
   status = read_comport(buf);                  // read comport
   
   if (status == DATA)                          // if new data detected in com port buffer
   {
      strcat(response, buf);                    // append contents of buf to response
      next_state = RESET_GET_AT_AT2_RESPONSE;   // come back for more data
   }
   else if (status == PROMPT) // if '>' detected
   {
      stop_serial_timer();
      strcat(response, buf);
      status = process_response("at@2", response);
      
      if (status == STN_MFR_STRING)
         next_state = RESET_START_ECU_TIMER;
      else
         next_state = RESET_HANDLE_CLONE;
   }
   else if (serial_time_out)
   {
      stop_serial_timer();
      alert("Connection with interface lost", NULL, NULL, "OK", NULL, 0, 0);
      next_state = RESET_CLOSE_DIALOG; // close dialog
   }
   else  // serial buffer was empty, but we still got time
      next_state = RESET_GET_AT_AT2_RESPONSE;
   
   return next_state;
}


int Reset_start_ecu_timer()
{
   start_serial_timer(ECU_TIMEOUT);
   return RESET_WAIT_FOR_ECU_TIMEOUT;
}


int Reset_wait_for_ecu_timeout(int device)
{
   int next_state;
   
   if (serial_time_out) // if the timer timed out
   {
      stop_serial_timer(); // stop the timer
      if (device == INTERFACE_ELM327)
         next_state = RESET_SEND_DETECT_PROTOCOL_REQUEST;
      else
         next_state = RESET_CLOSE_DIALOG;
   }
   else   
      next_state = RESET_WAIT_FOR_ECU_TIMEOUT;
   
   return next_state;
}


int Reset_send_detect_protocol_request(char *response)
{
   int next_state;
   
   send_command("0100");
   start_serial_timer(OBD_REQUEST_TIMEOUT);  // start serial timer
   response[0] = 0;
   next_state = RESET_GET_REPLY_TO_DETECT_PROTOCOL;
   
   return next_state;
}


int Reset_get_reply_to_detect_protocol(char *response)
{
   char buf[128];
   int next_state;
   int status;
   
   status = read_comport(buf);
   
   if(status == DATA) // if new data detected in com port buffer
   {
      strcat(response, buf); // append contents of buf to response
      next_state = RESET_GET_REPLY_TO_DETECT_PROTOCOL; // come back for more
   }
   else if (status == PROMPT)  // if we got the prompt
   {
      stop_serial_timer();
      strcat(response, buf);
      status = process_response("0100", response);

      if (status == ERR_NO_DATA || status == UNABLE_TO_CONNECT)
         alert("Protocol could not be detected.", "Please check connection to the vehicle,", "and make sure the ignition is ON", "OK", NULL, 0, 0);
      else if (status != HEX_DATA)
         alert("Communication error", buf, NULL, "OK", NULL, 0, 0);
      
      next_state = RESET_CLOSE_DIALOG;
   }
   else if (serial_time_out)
   {
      stop_serial_timer();
      alert("Connection with interface lost", NULL, NULL, "OK", NULL, 0, 0);
      next_state = RESET_CLOSE_DIALOG;
   }
   else
      next_state = RESET_GET_REPLY_TO_DETECT_PROTOCOL;
   
   return next_state;
}


/* We don't care if it's a clone; let it run anyway. */
int Reset_handle_clone()
{
   is_not_genuine_scan_tool = FALSE;
   return RESET_START_ECU_TIMER;
}


int reset_proc(int msg, DIALOG *d, int c)
{
   static int state = RESET_SEND_RESET_REQUEST;
   static int device = 0;
   static char response[256];
    
   switch(msg)
   {
      case MSG_START:
         state = RESET_SEND_RESET_REQUEST;
         strcpy(reset_status_msg, "Resetting hardware interface...");
         break;
   
      case MSG_IDLE:
         switch (state)
         {
            case RESET_SEND_RESET_REQUEST:
               strcpy(reset_status_msg, "Resetting hardware interface...");
               state = Reset_send_reset_request(response);
               return D_REDRAW;
               
            case RESET_GET_REPLY_TO_RESET:
               state = Reset_get_reply_to_reset(response, &device);
               break;
               
            case RESET_SEND_AT_AT1_REQUEST:
               strcpy(reset_status_msg, "Making sure scan tool is genuine...");
               state = Reset_send_at_at1_request(response);
               return D_REDRAW;
               
            case RESET_GET_AT_AT1_RESPONSE:
               state = Reset_get_at_at1_response(response);
               break;
               
            case RESET_SEND_AT_AT2_REQUEST:
               state = Reset_send_at_at2_request(response);
               break;
               
            case RESET_GET_AT_AT2_RESPONSE:
               state = Reset_get_at_at2_response(response);
               break;
               
            case RESET_START_ECU_TIMER:
               strcpy(reset_status_msg, "Waiting for ECU timeout...");
               state = Reset_start_ecu_timer();
               return D_REDRAW;
               
            case RESET_WAIT_FOR_ECU_TIMEOUT:
               state = Reset_wait_for_ecu_timeout(device);
               break;
               
            case RESET_SEND_DETECT_PROTOCOL_REQUEST:
               strcpy(reset_status_msg, "Detecting OBD protocol...");
               state = Reset_send_detect_protocol_request(response);
               return D_REDRAW;
               
            case RESET_GET_REPLY_TO_DETECT_PROTOCOL:
               state = Reset_get_reply_to_detect_protocol(response);
               break;
               
            case RESET_HANDLE_CLONE:
               state = Reset_handle_clone();
               break;
               
            case RESET_CLOSE_DIALOG:
               return D_CLOSE;
         }
         break;
   }
   
   return D_O_K;
}
