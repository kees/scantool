#include <string.h>
#include <ctype.h>
#include <math.h>
#include "globals.h"
#include "serial.h"
#include "options.h"
#include "custom_gui.h"
#include "error_handlers.h"
#include "trouble_code_reader.h"

#define MSG_READ_CODES    MSG_USER
#define MSG_CLEAR_CODES   MSG_USER+1
#define MSG_READY         MSG_USER+2

#define CRQ_NONE       0
#define NUM_OF_CODES   1
#define READ_CODES     2
#define READ_PENDING   3
#define CLEAR_CODES    4

#define FIELD_DELIMITER     '\t'
#define RECORD_DELIMITER    0xA

#define SIM_CODES_STRING   "43012507360455\n43114301960234\n43044302990357\n43C001C101C106"

#define NUM_OF_RETRIES   3

#define CLEAR_CODES_WARNING \
"This will reset the MIL and clear all emission-related diagnostic information, including:\n\n\
   - Diagnostic trouble codes\n\
   - Freeze frame data\n\
   - Oxygen sensor test data\n\
   - Status of system monitoring tests\n\
   - On-board monitoring tests results\n\
   - Distance travelled while MIL activated\n\
   - Number of warm-ups since DTCs cleared\n\
   - Distance travelled since DTCs cleared\n\
   - Engine run time while MIL activated\n\
   - Time since DTCs cleared\n\n\
Other manufacturer-specific \"clearing/resetting\" actions may occur. The loss of data may cause \
the vehicle to run poorly for a short period of time while the ECU recalibrates itself."

typedef struct TROUBLE_CODE
{
   char code[7];
   char *description;
   char *solution;
   int pending;
   struct TROUBLE_CODE *next;
} TROUBLE_CODE;

static char mfr_code_description[] = "Manufacturer-specific code.  Please refer to your vehicle's service manual for more information";
static char mfr_pending_code_description[] = "[Pending]\nManufacturer-specific code.  Please refer to your vehicle's service manual for more information";
static char code_no_description[] = "";
static char pending_code_no_description[] = "[Pending]";

static int num_of_codes_reported = 0;
static int current_code_index;
static int simulate = FALSE;
static int mil_is_on; // MIL is ON or OFF

static TROUBLE_CODE *trouble_codes = NULL;

static void add_trouble_code(const TROUBLE_CODE *);
static TROUBLE_CODE *get_trouble_code(int index);
static int get_number_of_codes();
static void clear_trouble_codes();

// procedure definitions:
static int tr_code_proc(int msg, DIALOG *d, int c);
static int mil_status_proc(int msg, DIALOG *d, int c);
static int mil_text_proc(int msg, DIALOG *d, int c);
static int simulate_proc(int msg, DIALOG *d, int c);
static int is_mfr_code(const char *code);
static int num_of_codes_proc(int msg, DIALOG *d, int c);
static int code_list_proc(int msg, DIALOG *d, int c);
static char *code_list_getter(int index, int *list_size);
static int read_tr_codes_proc(int msg, DIALOG *d, int c);
static int clear_codes_proc(int msg, DIALOG *d, int c);
static int tr_description_proc(int msg, DIALOG *d, int c);
static int tr_solution_proc(int msg, DIALOG *d, int c);
static int current_code_proc(int msg, DIALOG *d, int c);

// function definitions:
static void read_codes();
static void trouble_codes_simulator(int show);
static int handle_num_of_codes(char *);
static int handle_read_codes(char *, int);
static void populate_trouble_codes_list();
static void swap_codes(TROUBLE_CODE *, TROUBLE_CODE *);
static void handle_errors(int error, int operation);
static PACKFILE *file_handle(char code_letter);

static DIALOG read_codes_dialog[] =
{
   /* (proc)              (x)  (y)  (w)  (h)  (fg)    (bg)           (key) (flags) (d1) (d2) (dp)                                   (dp2) (dp3) */
   { d_clear_proc,        0,   0,   0,   0,   0,       C_WHITE,       0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { d_box_proc,          25,  25,  112, 24,  C_BLACK, C_DARK_GRAY,   0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { code_list_proc,      25,  96,  112, 208, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   code_list_getter,                      NULL, NULL },
   { caption_proc,        81,  28,  54,  19,  C_WHITE, C_TRANSP,      0,    0,      0,   0,   "Current DTC",                         NULL, NULL },
   { d_box_proc,          25,  49,  112, 40,  C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { current_code_proc,   81,  57,  54,  24,  C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { d_box_proc,          25,  311, 112, 64,  C_BLACK, C_WHITE,       0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { num_of_codes_proc,   81,  319, 54,  24,  C_BLACK, C_WHITE,       0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { st_ctext_proc,       80,  351, 55,  20,  C_BLACK, C_TRANSP,      0,    0,      0,   0,   "DTCs",                                NULL, NULL },
   { d_box_proc,          25,  383, 112, 72,  C_BLACK, C_WHITE,       0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { mil_status_proc,     56,  390, 50,  30,  0,       0,             0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { mil_text_proc,       81,  430, 54,  23,  C_BLACK, C_WHITE,       0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { d_box_proc,          152, 25,  463, 24,  C_BLACK, C_DARK_GRAY,   0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { caption_proc,        383, 28,  230, 19,  C_WHITE, C_TRANSP,      0,    0,      0,   0,   "Diagnostic Trouble Code Definition",  NULL, NULL },
   { tr_description_proc, 152, 49,  463, 100, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { d_box_proc,          152, 164, 463, 25,  C_BLACK, C_DARK_GRAY,   0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { caption_proc,        383, 167, 230, 20,  C_WHITE, C_TRANSP,      0,    0,      0,   0,   "Possible Causes And Known Solutions", NULL, NULL },
   { tr_solution_proc,    152, 189, 463, 185, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { simulate_proc,       152, 408, 88,  16,  C_BLACK, C_WHITE,       0,    0,      1,   0,   "Simulate",                            NULL, NULL },
   { read_tr_codes_proc,  256, 408, 110, 48,  C_BLACK, C_GREEN,       'r',  D_EXIT, 0,   0,   "&Read",                               NULL, NULL },
   { clear_codes_proc,    381, 408, 110, 48,  C_BLACK, C_DARK_YELLOW, 'c',  D_EXIT, 0,   0,   "&Clear",                              NULL, NULL },
   { d_button_proc,       506, 408, 110, 48,  C_BLACK, C_GREEN,       'm',  D_EXIT, 0,   0,   "Main &Menu",                          NULL, NULL },
   { tr_code_proc,        0,   0,   0,   0,   0,     0,               0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { d_yield_proc,        0,   0,   0,   0,   0,     0,               0,    0,      0,   0,   NULL,                                  NULL, NULL },
   { NULL,                0,   0,   0,   0,   0,     0,               0,    0,      0,   0,   NULL,                                  NULL, NULL }
};

static DIALOG confirm_clear_dialog[] =
{
   /* (proc)             (x)  (y)  (w)  (h)  (fg)     (bg)           (key) (flags) (d1) (d2) (dp)                                 (dp2) (dp3) */
   { d_shadow_box_proc,  0,   0,   440, 460, C_BLACK, C_WHITE,       0,    0,      0,   0,   NULL,                                NULL, NULL },
   { d_shadow_box_proc,  0,   0,   440, 24,  C_BLACK, C_DARK_GRAY,   0,    0,      0,   0,   NULL,                                NULL, NULL },
   { caption_proc,       220, 2,   218, 19,  C_WHITE, C_TRANSP,      0,    0,      0,   0,   "Clear Trouble Codes",               NULL, NULL },
   { super_textbox_proc, 16,  32,  408, 332, C_BLACK, C_WHITE,       0,    0,      0,   0,   CLEAR_CODES_WARNING,                 NULL, NULL },
   { caption_proc,       220, 372, 204, 24,  C_RED,   C_TRANSP,      0,    0,      0,   0,   "Are you sure you want to do this?", NULL, NULL },
   { d_button_proc,      72,  408, 140, 35,  C_BLACK, C_GREEN,       'y',  D_EXIT, 0,   0,   "&Yes, I am sure",                   NULL, NULL },
   { d_button_proc,      228, 408, 140, 35,  C_BLACK, C_DARK_YELLOW, 'n',  D_EXIT, 0,   0,   "&No, cancel",                       NULL, NULL },
   { d_yield_proc,       0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,                                NULL, NULL },
   { NULL,               0,   0,   0,   0,   0,       0,             0,    0,      0,   0,   NULL,                                NULL, NULL }
};


int display_trouble_codes()
{
   int ret;

   mil_is_on = FALSE; // reset MIL status
   num_of_codes_reported = 0;
   clear_trouble_codes();
   
   centre_dialog(confirm_clear_dialog);  // center the popup dialog

   if (comport.status == USER_IGNORED)
      comport.status = NOT_OPEN;

   ret = do_dialog(read_codes_dialog, -1);
   
   if (get_number_of_codes() > 0)    // if the structure is not empty,
      clear_trouble_codes();
      
   return ret;
}


void trouble_codes_simulator(int show)
{
   char buf[128];
   
   if (show)
   {
      strcpy(buf, SIM_CODES_STRING);
      process_response(NULL, buf);
      clear_trouble_codes();
      num_of_codes_reported = handle_read_codes(buf, FALSE);
      populate_trouble_codes_list();
      mil_is_on = TRUE;
   }
   else
   {
      clear_trouble_codes();
      num_of_codes_reported = 0;
      mil_is_on = FALSE;
   }

   broadcast_dialog_message(MSG_READY, 0);
}
      

int current_code_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         d->dp2 = datafile[ARIAL18_FONT].dat;
         d->dp = empty_string;
         break;

      case MSG_READY:
         if (get_number_of_codes() == 0)
            d->dp = empty_string;
         else
            d->dp = get_trouble_code(current_code_index)->code;

         return D_REDRAWME;
      
      case MSG_DRAW:
         rectfill(screen, d->x-d->w, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the element
         break;
   }
      
   return st_ctext_proc(msg, d, c);
}


int tr_description_proc(int msg, DIALOG *d, int c)   // procedure which displays a textbox
{
   TROUBLE_CODE *dtc;

   switch (msg)
   {
      case MSG_START:
         d->dp = empty_string;
         break;

      case MSG_READY:
         if (get_number_of_codes() == 0)
            d->dp = empty_string;
         else
         {
            if (!(dtc = get_trouble_code(current_code_index)))
               d->dp = empty_string;
            else
            {
               if (!(d->dp = dtc->description))
               {
                  if (is_mfr_code(dtc->code))
                     d->dp = (dtc->pending) ? mfr_pending_code_description : mfr_code_description;
                  else
                     d->dp = (dtc->pending) ? pending_code_no_description : code_no_description;
               }
            }
         }
         return D_REDRAWME;
   }
      
   return d_textbox_proc(msg, d, c);
}


int tr_solution_proc(int msg, DIALOG *d, int c)   // procedure which displays a textbox
{
   switch (msg)
   {
      case MSG_START:
         d->dp = empty_string;
         break;
      
      case MSG_READY:
         if (get_number_of_codes() == 0)
            d->dp = empty_string;
         else if(!(d->dp = get_trouble_code(current_code_index)->solution))
            d->dp = empty_string;
         return D_REDRAWME;
   }

   return d_textbox_proc(msg, d, c);
}


int num_of_codes_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         d->dp2 = datafile[ARIAL18_FONT].dat;
         if (!(d->dp = calloc(8, sizeof(char))))
            fatal_error("Could not allocate enough memory for num_of_codes_proc buffer");
         strcpy(d->dp, "0");
         break;

      case MSG_END:
         free(d->dp);
         break;
         
      case MSG_DRAW:
         rectfill(screen, d->x-d->w, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the element
         break;

      case MSG_READY:
         sprintf(d->dp, "%i", num_of_codes_reported);
         return D_REDRAWME;
   }
   
   return st_ctext_proc(msg, d, c);
}


int mil_status_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         d->dp = datafile[MIL_OFF_BMP].dat;
         break;

      case MSG_READY:
         if (mil_is_on)
            d->dp = datafile[MIL_ON_BMP].dat;
         else
            d->dp = datafile[MIL_OFF_BMP].dat;
            
         return D_REDRAWME;
   }
   
   return d_bitmap_proc(msg, d, c);
}


int mil_text_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         if (!(d->dp = calloc(16, sizeof(char))))
            fatal_error("Could not allocate enough memory for mil_text_proc buffer");
         strcpy(d->dp, "MIL is OFF");
         break;
         
      case MSG_END:
         free(d->dp);
         break;
         
      case MSG_DRAW:
         rectfill(screen, d->x-d->w, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the element
         break;
         
      case MSG_READY:
         if (mil_is_on)
            strcpy(d->dp, "MIL is ON");
         else
            strcpy(d->dp, "MIL is OFF");
      
         return D_REDRAWME;
   }
   
   return st_ctext_proc(msg, d, c);
}


int simulate_proc(int msg, DIALOG *d, int c)
{
   int ret;

   switch (msg)
   {
      case MSG_READ_CODES: case MSG_CLEAR_CODES:
         // if we are currently reading or clearing codes, and the button is enabled,
         if (!(d->flags & D_DISABLED))
         {
            d->flags |= D_DISABLED;     // disable the button
            return D_REDRAWME;
         }
         break;

      case MSG_READY:
         // if we're not reading or clearing codes, and the button is disabled,
         if (d->flags & D_DISABLED)
         {
            d->flags &= ~D_DISABLED;   // enable it
            return D_REDRAWME;
         }
         break;
   }
   
   ret = d_check_proc(msg, d, c);

   if ((d->flags & D_SELECTED) && (d->d2 == 0))
   {
      d->d2 = 1;
      simulate = TRUE;
      trouble_codes_simulator(TRUE);
   }
   else if (!(d->flags & D_SELECTED) && (d->d2 == 1))
   {
      d->d2 = 0;
      simulate = FALSE;
      trouble_codes_simulator(FALSE);
   }

   return ret;
}   


void read_codes()
{
   while (comport.status != READY)
   {
      if (alert("Port is not ready.", "Please check that you specified the correct port", "and that no other application is using it", "Configure &Port", "&Cancel", 'p', 'c') == 1)
         display_options(); // let the user choose correct settings
      else
         return;
   }

   clear_trouble_codes();
   num_of_codes_reported = 0;
   mil_is_on = FALSE;
   broadcast_dialog_message(MSG_READY, 0);
   broadcast_dialog_message(MSG_READ_CODES, 0);
}


int read_tr_codes_proc(int msg, DIALOG *d, int c)
{
   int ret;
   
   switch (msg)
   {
      case MSG_READ_CODES: case MSG_CLEAR_CODES:
         // if we are currently reading or clearing codes, and the button is enabled,
         if (!(d->flags & D_DISABLED))
         {
            d->flags |= D_DISABLED;     // disable the button
            return D_REDRAWME;
         }
         break;
         
      case MSG_READY:
         // if we're not reading or clearing codes, and the button is disabled,
         if (d->flags & D_DISABLED)
         {
            d->flags &= ~D_DISABLED;   // enable it
            return D_REDRAWME;
         }
         break;
   }

   ret = d_button_proc(msg, d, c);

   if (ret == D_CLOSE)
   {
      if (simulate)
         trouble_codes_simulator(TRUE);
      else
         read_codes();
      
      return D_REDRAWME;
   }

   return ret;
}


int clear_codes_proc(int msg, DIALOG *d, int c)
{
   int ret;
   switch (msg)
   {
      case MSG_READ_CODES: case MSG_CLEAR_CODES:
         // if we are currently reading or clearing codes, and the button is enabled,
         if (!(d->flags & D_DISABLED))
         {
            d->flags |= D_DISABLED;     // disable the button
            return D_REDRAWME;
         }
         break;
         
      case MSG_READY:
         // if we're not reading or clearing codes, and the button is disabled,
         if (d->flags & D_DISABLED)
         {
            d->flags &= ~D_DISABLED;   // enable it
            return D_REDRAWME;
         }
         break;
   }
   
   ret = d_button_proc(msg, d, c);

   if (ret == D_CLOSE)
   {
      while (!simulate && comport.status != READY)
      {
         if (alert("Port is not ready.", "Please check that you specified the correct port", "and that no other application is using it", "Configure &Port", "&Cancel", 'p', 'c') == 1)
            display_options(); // let the user choose correct settings
         else
            return D_REDRAWME;
      }
      
      // if user confirmed that he wants to erase the codes,
      if(popup_dialog(confirm_clear_dialog, 6) == 5)
      {
         if (simulate)
            trouble_codes_simulator(FALSE);
         else
            broadcast_dialog_message(MSG_CLEAR_CODES, 0);  //send everyone MSG_CLEAR_CODES
      }
      
      return D_REDRAWME;
   }

   return ret;
}


// heart of the trouble_code_reader module:
int tr_code_proc(int msg, DIALOG *d, int c)
{
   static char vehicle_response[1024];        // character buffer for car response
   static int first_read_occured = FALSE;
   static int receiving_response = FALSE;    // flag, "are we receiving response?"
   static int verifying_connection = FALSE;  // flag, "are we verifying connection?"
   static int current_request = CRQ_NONE;    // NUM_OF_CODES, READ_CODES, CLEAR_CODES
   int response_status = EMPTY;              // EMPTY, DATA, PROMPT
   int response_type;                        // BUS_BUSY, BUS_ERROR, DATA_ERROR, etc.
   int pending_codes_cnt = 0;
   char comport_buffer[256];                  // temporary storage for comport data
   char buf1[64];
   char buf2[64];

   switch (msg)
   {
      case MSG_IDLE:
         if (!first_read_occured)
         {
            if (!simulate)
               read_codes();
            first_read_occured = TRUE;
            return D_O_K;
         }
         
         if (simulate)
            break;
         
         if (comport.status == READY)
         {
            if (!receiving_response)
            {
               if (verifying_connection)
               {
                  send_command("0100"); // send request that requires a response
                  receiving_response = TRUE; // now we're waiting for response
                  vehicle_response[0] = 0; //get buffer ready for the response
                  start_serial_timer(OBD_REQUEST_TIMEOUT); // start the timer
               }
               else if (current_request == READ_PENDING)
               {
                  send_command("07");   // request "pending" codes
                  receiving_response = TRUE;     // and receiving response
                  vehicle_response[0] = '\0';    // clear the buffer
                  start_serial_timer(OBD_REQUEST_TIMEOUT); // start the timer...
               }
            }
            else
            {
               response_status = read_comport(comport_buffer);

               if (response_status == DATA) // if data detected in com port buffer
               {
                  // append contents of comport_buffer to vehicle_response:
                  strcat(vehicle_response, comport_buffer);
                  start_serial_timer(OBD_REQUEST_TIMEOUT);  // we got data, reset the timer
               }
               else if (response_status == PROMPT) // if ">" is detected
               {
                  receiving_response = FALSE; // we're not waiting for response any more
                  stop_serial_timer();        // stop the timer
                  // append contents of comport_buffer to vehicle_response:
                  strcat(vehicle_response, comport_buffer);

                  if (verifying_connection)     // *** if we are verifying connection ***
                  {  // NOTE: we only get here if we got "NO DATA" somewhere else
                     response_type = process_response("0100", vehicle_response);
                     verifying_connection = FALSE; // we're not verifying connection anymore

                     if (response_type == HEX_DATA) // if everything seems to be fine now,
                     {
                        if (current_request == CLEAR_CODES)
                           alert("There may have been a temporary loss of connection.", "Please try clearing codes again.", NULL, "OK", NULL, 0, 0);
                        else if (current_request == NUM_OF_CODES)
                           alert("There may have been a temporary loss of connection.", "Please try reading codes again.", NULL, "OK", NULL, 0, 0);
                        else if (current_request == READ_CODES)
                        {
                           current_request = READ_PENDING;
                           break;
                        }
                     }
                     else if (response_type == ERR_NO_DATA)
                     {
                        if (current_request == CLEAR_CODES) // if we were clearing codes,
                           alert("Communication problem: vehicle did not confirm successful", "deletion of trouble codes.  Please check connection to the vehicle,", "make sure the ignition is ON, and try clearing the codes again.", "OK", NULL, 0, 0);
                        else // if we were reading codes or requesting number or DTCs
                           alert("There may have been a loss of connection.", "Please check connection to the vehicle,", "and make sure the ignition is ON", "OK", NULL, 0, 0);
                     }
                     else
                        display_error_message(response_type, FALSE);
                        
                     broadcast_dialog_message(MSG_READY, 0); // tell everyone we're done
                  }

                  else if (current_request == NUM_OF_CODES) // *** if we are getting number of codes ***
                  {
                     response_type = process_response("0101", vehicle_response);

                     if (response_type == ERR_NO_DATA)   // if we received "NO DATA"
                        verifying_connection = TRUE;  // verify connection
                     else if (response_type != HEX_DATA) // if we got an error,
                        handle_errors(response_type, NUM_OF_CODES);  // handle it
                     else    // if process response returned HEX_DATA (i.e. there are no errors)
                     {  // extract # of codes from vehicle_response
                        num_of_codes_reported = handle_num_of_codes(vehicle_response);
                     
                        send_command("03");   // request "stored" codes
                        current_request = READ_CODES;  // we're reading stored codes now
                        receiving_response = TRUE;     // and receiving response
                        vehicle_response[0] = '\0';    // clear the buffer
                        start_serial_timer(OBD_REQUEST_TIMEOUT); // start the timer...
                     }
                  }
                  else if (current_request == READ_CODES) // if we are reading codes,
                  {
                     response_type = process_response("03", vehicle_response);

                     if (response_type == ERR_NO_DATA) // vehicle didn't respond, check connection
                     {
                        if (num_of_codes_reported > 0)
                           verifying_connection = TRUE;
                        else
                           current_request = READ_PENDING;
                     }
                     else if (response_type == HEX_DATA)
                     {
                        handle_read_codes(vehicle_response, FALSE);
                        current_request = READ_PENDING;
                     }
                     else  // if we got an error
                        handle_errors(response_type, READ_CODES);
                  }
                  else if(current_request == READ_PENDING) // if we are reading pending codes,
                  {
                     response_type = process_response("07", vehicle_response);

                     if (response_type == ERR_NO_DATA)
                     {
                        if (get_number_of_codes() == 0 && num_of_codes_reported == 0)
                           alert("No Diagnostic Trouble Codes (DTCs) detected", NULL, NULL, "OK", NULL, 0, 0);
                     }
                     else if(response_type != HEX_DATA) // if we got an error,
                     {
                        handle_errors(response_type, READ_PENDING);
                        break;
                     }
                     else  // if there were *no* errors,
                        pending_codes_cnt = handle_read_codes(vehicle_response, TRUE);

                     // if number of DTCs reported by 0101 request does not equal either number or total DTCs or just stored DTCs
                     if ((get_number_of_codes() != num_of_codes_reported) && (get_number_of_codes() - pending_codes_cnt != num_of_codes_reported))
                     {
                        sprintf(buf1, "Vehicle reported %i Diagnostic Trouble Codes (DTCs).", num_of_codes_reported);
                        sprintf(buf2, "However, %i non-pending DTC(s) have been successfully read.", get_number_of_codes() - pending_codes_cnt);
                        alert(buf1, buf2, "Try reading codes again.", "OK", NULL, 0, 0);
                     }

                     populate_trouble_codes_list();
                     broadcast_dialog_message(MSG_READY, 0); // tell everyone we're done
                  }
                  else if(current_request == CLEAR_CODES)
                  {
                     response_type = process_response("04", vehicle_response);

                     if (response_type == ERR_NO_DATA)// vehicle didn't respond, check connection
                        verifying_connection = TRUE;
                     else if(response_type != HEX_DATA) // if we got an error,
                        handle_errors(response_type, CLEAR_CODES);
                     else // if everything's fine (received confirmation)
                     {
                        clear_trouble_codes();
                        num_of_codes_reported = 0;
                        mil_is_on = FALSE;
                        broadcast_dialog_message(MSG_READY, 0);
                     }
                  }
               }
               else if (serial_time_out)     // if request timed out,
               {
                  stop_serial_timer();
                  receiving_response = FALSE;
                  num_of_codes_reported = 0;
                  mil_is_on = FALSE;
                  clear_trouble_codes();
                  broadcast_dialog_message(MSG_READY, 0);

                  if(alert("Device is not responding.", "Please check that it is connected", "and the port settings are correct", "OK",  "&Configure Port", 27, 'c') == 2)
                     display_options();   // let the user choose correct settings

                  while (comport.status == NOT_OPEN)
                  {
                     if (alert("Port is not ready.", "Please check that you specified the correct port", "and that no other application is using it", "&Configure Port", "&Ignore", 'c', 'i') == 1)
                        display_options(); // let the user choose correct settings
                     else
                        comport.status = USER_IGNORED;
                  }
               }
            }
         }
         break;  // end case MSG_IDLE

      case MSG_START:
         first_read_occured = FALSE;
         num_of_codes_reported = 0;
         mil_is_on = FALSE;
         // fall through
         
      case MSG_READY:
         receiving_response = FALSE;
         verifying_connection = FALSE;
         current_request = CRQ_NONE;
         break;

      case MSG_READ_CODES:
         if (comport.status == READY)
         {
            send_command("0101"); // request number of trouble codes
            start_serial_timer(OBD_REQUEST_TIMEOUT); // start the timer
            current_request = NUM_OF_CODES;
            receiving_response = TRUE; // now we're waiting for response
            vehicle_response[0] = 0;
            clear_trouble_codes();
            num_of_codes_reported = 0;
            mil_is_on = FALSE;
         }
         else
            serial_time_out = TRUE;
         break;

      case MSG_CLEAR_CODES:
         if (comport.status == READY)
         {
            send_command("04"); // "clear codes" request
            current_request = CLEAR_CODES;
            receiving_response = TRUE; // now we're waiting for response
            vehicle_response[0] = 0;
            start_serial_timer(OBD_REQUEST_TIMEOUT); // start the timer
         }
         else
            serial_time_out = TRUE;
         break;

      case MSG_END:
         stop_serial_timer();
         break;
   }

   return D_O_K;
}  // end of tr_codes_proc()


int code_list_proc(int msg, DIALOG *d, int c)
{
   static int curr_num_of_codes = 0;
   int num_of_codes = get_number_of_codes();
   int ret;
   
   if (msg == MSG_READY)
   {
      if ((num_of_codes == 0) || (curr_num_of_codes != num_of_codes))
      {
         d->d1 = 0;
         d->d2 = 0;
         current_code_index = 0;
         curr_num_of_codes = num_of_codes;
      }
      return D_REDRAWME;
   }
     
   ret = d_list_proc(msg, d, c);
   
   if (current_code_index != d->d1)
   {
      current_code_index = d->d1;
      broadcast_dialog_message(MSG_READY, 0);
   }
   
   return ret;
}


char* code_list_getter(int index, int *list_size)
{
   if (index < 0)
   {
      *list_size = get_number_of_codes();
      return NULL;
   }

   return get_trouble_code(index)->code;
}


// Mfr./SAE/ISO ranges are defined in SAE J2012 standard
int is_mfr_code(const char *code)
{
   int num;

   if (!code)
      return FALSE;
      
   switch (code[1])
   {
      case '0':
         return FALSE;
      case '1':
         return TRUE;
      case '2':
         if (code[0] == 'P')
            return FALSE;
         return TRUE;
      case '3':
         num = strtol(code + 1, NULL, 16);
         if (code[0] == 'P' && num >= 0x3000 && num <= 0x3399)
            return TRUE;
         return FALSE;
   }
   
   return TRUE;
}


int handle_num_of_codes(char *vehicle_response)
{
   int temp;
   char *response = vehicle_response;
   char buf[16];
   int ret = 0;

   while (*response)
   {
      if (find_valid_response(buf, response, "4101", &response))
      {
         buf[6] = 0;
         temp = (int)strtol(buf + 4, NULL, 16); // convert hex ascii string to integer
         if (temp & 0x80)
            mil_is_on = TRUE; // get MIL status from temp
         ret = ret + (temp & 0x7F);
      }
      else
         break;
   }

   return ret;
}


int parse_dtcs(const char *response, int pending)
{
   char code_letter[] = "PCBU";
   int dtc_count = 0;
   int i, j;
   TROUBLE_CODE temp_trouble_code;

   for(i = 0; i < strlen(response); i += 4)    // read codes
   {
      temp_trouble_code.code[0] = ' '; // make first position a blank space
      // begin to copy from response to temp_trouble_code.code beginning with position #1
      for (j = 1; j < 5; j++)
         temp_trouble_code.code[j] = response[i+j-1];
      temp_trouble_code.code[j] = '\0';  // terminate string

      if (strcmp(temp_trouble_code.code, " 0000") == 0) // if there's no trouble code,
         break;      // break out of the for() loop

      // begin with position #1 (skip blank space), convert to hex, extract first two bits
      // use the result as an index into the code_letter array to get the corresponding code letter
      temp_trouble_code.code[0] = code_letter[strtol(temp_trouble_code.code + 1, NULL, 16) >> 14];
      temp_trouble_code.code[1] = (strtol(temp_trouble_code.code + 1, NULL, 16) >> 12) & 0x03;
      temp_trouble_code.code[1] += 0x30; // convert to ASCII
      if (pending)
      {
         temp_trouble_code.code[5] = '*';
         temp_trouble_code.code[6] = 0;
         temp_trouble_code.pending = TRUE;
      }
      else
         temp_trouble_code.pending = FALSE;
      temp_trouble_code.description = NULL; // clear the corresponding description...
      temp_trouble_code.solution = NULL;  // ..and solution
      add_trouble_code(&temp_trouble_code);
      dtc_count++;
   }
   
   return dtc_count;
}


/* NOTE:
 *  ELM327 multi-message CAN responses are parsed using the following assumptions:
 *   - max 8 ECUs (per ISO15765-4 standard)
 *   - max 51 DTCs per ECU - there is no way to tell which ECU a response belongs to when the message counter wraps (0-F)
 *   - ECUs respond sequentially (i.e. 2nd msg from the 1st ECU to respond will come before 2nd msg from the 2nd ECU to respond)
 *     This has been observed imperically (in fact most of the time 2nd ECU lags several messages behind the 1st ECU),
 *     this should be true for most cases due to arbitration, unless 1st ECU takes a very long time to prepare next message,
 *     which should not happen. There is no other choice at the moment, unless we turn on headers, but that would mean rewriting
 *     whole communication paradigm.
 */

int handle_read_codes(char *vehicle_response, int pending)
{
   int dtc_count = 0;
   char *start = vehicle_response;
   char filter[3];
   char msg[48];
   int can_resp_cnt = 0;
   int can_msg_cnt = 0;
   int can_resp_len[8];
   char *can_resp_buf[8];  // 8 CAN ECUs max
   int buf_len, max_len, trim;
   int i, j;
   
   // First, look for non-CAN and single-message CAN responses
   strcpy(filter, (pending) ? "47" : "43");
   while (find_valid_response(msg, start, filter, &start))
   {
      if (strlen(msg) == 4)  // skip '4X 00' CAN responses
         continue;
      // if even number of bytes (CAN), skip first 2 bytes, otherwise, skip 1 byte
      i = (((strlen(msg)/2) & 0x01) == 0) ? 4 : 2;
      dtc_count += parse_dtcs(msg + i, pending);
   }

   // Look for CAN multi-message responses
   start = vehicle_response;
   while (find_valid_response(msg, start, "", &start))  // step through all responses
   {
      if (strlen(msg) == 3)  // we're looking for 3-byte response length messages
      {
         can_resp_len[can_resp_cnt] = strtol(msg, NULL, 16);  // get total length for the response
         can_resp_buf[can_resp_cnt] = calloc((can_resp_len[can_resp_cnt]-2)*2 + 1, sizeof(char));
         i = ceil((float)(can_resp_len[can_resp_cnt] + 1) / 7);  // calculate number of messages necessary to transmit specified number of bytes
         can_msg_cnt = MAX(can_msg_cnt, i);  // calculate max number of messages for any response
         can_resp_cnt++;
      }
   }
   
   for (i = 0; i < can_msg_cnt; i++)
   {
      j = 0;
      start = vehicle_response;
      sprintf(filter, "%X:", i);
      while (find_valid_response(msg, start, filter, &start))
      {
         for (; j < can_resp_cnt; j++)  // find next response that is not full
         {
            buf_len = strlen(can_resp_buf[j]);
            max_len = (can_resp_len[j]-2)*2;
            if (buf_len < max_len)
            {
               // first response -- skip '0:4XXX', all other -- skip 'X:'
               // first response -- 6 bytes total, all other -- 7 bytes
               // if this is last message for a response, trim padding
               trim = (buf_len + strlen(msg) - 2 >= max_len) ? buf_len + strlen(msg) - 2 - max_len : 0;
               strncat(can_resp_buf[j], msg + ((i == 0) ? 6 : 2), (i == 0) ? 8 : 14 - trim);
               j++;
               break;
            }
         }
      }
   }
   
   for (i = 0; i < can_resp_cnt; i++)
   {
      dtc_count += parse_dtcs(can_resp_buf[i], pending);
      free(can_resp_buf[i]);
   }
   
   return dtc_count; // return the actual number of codes read
}


void populate_trouble_codes_list()
{
   char character;
   int i, j, min;
   char temp_buf[1024];
   TROUBLE_CODE *trouble_code;
   int count = get_number_of_codes();
   PACKFILE *code_def_file;

   if (count == 0)
      return;

   for (i = 0; i < count; i++)    // sort codes in ascending order
   {
      min = i;

      for (j = i+1; j < count; j++)
         if(strcmp(get_trouble_code(j)->code, get_trouble_code(min)->code) < 0)
            min = j;

      swap_codes(get_trouble_code(i), get_trouble_code(min));
   }
   
   for (trouble_code = trouble_codes; trouble_code; trouble_code = trouble_code->next)   // search for descriptions and solutions
   {
      // pass the letter (B, C, P, or U) to file_handle, which returns the file handle
      // if we reached EOF, or the file does not exist, go to the next DTC
      if ((code_def_file = file_handle(trouble_code->code[0])) == NULL)
         continue;

      while (TRUE)
      {
         j = 0;

         // copy DTC from file to temp_buf
         while (((character = pack_getc(code_def_file)) != FIELD_DELIMITER) && (character != RECORD_DELIMITER) && (character != EOF))
         {
            temp_buf[j] = character;
            j++;
         }
         temp_buf[j] = '\0';

         if (character == EOF) // reached end of file, break out of while()
            break;             // advance to next code

         if (strncmp(trouble_code->code, temp_buf, 5) == 0) // if we found the code,
         {
            if (character == RECORD_DELIMITER)  // reached end of record, no description or solution,
               break;                        // break out of while(), advance to next code

            j = 0;

            //copy description from file to temp_buf
            while (((character = pack_getc(code_def_file)) != FIELD_DELIMITER) && (character != RECORD_DELIMITER) && (character != EOF))
            {
               temp_buf[j] = character;
               j++;
            }
            temp_buf[j] = '\0';  // terminate string
            if (j > 0)
            {
               if (!(trouble_code->description = (char *)malloc(sizeof(char)*(j + 1 + ((trouble_code->pending) ? 10 : 0)))))
               {
                  sprintf(temp_error_buf, "Could not allocate enough memory for trouble code description [%i]", count);
                  fatal_error(temp_error_buf);
               }
               if (trouble_code->pending)
               {
                  strcpy(trouble_code->description, "[Pending]\n");
                  strcpy(trouble_code->description + 10, temp_buf);  // copy description from temp_buf
               }
               else
                  strcpy(trouble_code->description, temp_buf);  // copy description from temp_buf
            }

            if (character == FIELD_DELIMITER)   // if we have solution,
            {
               j = 0;

               // copy solution from file to temp_buf
               while (((character = pack_getc(code_def_file)) != RECORD_DELIMITER) && (character != EOF))
               {
                  temp_buf[j] = character;
                  j++;
               }
               temp_buf[j] = '\0';   // terminate string
               if (j > 0)
               {
                  if (!(trouble_code->solution = (char *)malloc(sizeof(char)*(j+1))))
                  {
                     sprintf(temp_error_buf, "Could not allocate enough memory for trouble code solution [%i]", count);
                     fatal_error(temp_error_buf);
                  }
                  strcpy(trouble_code->solution, temp_buf);  // copy solution from temp_buf
               }
            }
            break;  // break out of while(TRUE)
         }
         else
         {
            // skip to next record
            while (((character = pack_getc(code_def_file)) != RECORD_DELIMITER) && (character != EOF));

            if (character == EOF)
               break;   // break out of while(TRUE), advance to next code
         }
      } // end of while(TRUE)
   } // end of for() loop
   file_handle(0); // close the code definition file if it's still open
}


void handle_errors(int error, int operation)
{
   static int retry_attempts = NUM_OF_RETRIES;

   if (error == BUS_ERROR || error == UNABLE_TO_CONNECT || error == BUS_INIT_ERROR)
   {
      display_error_message(error, FALSE);
      retry_attempts = NUM_OF_RETRIES;
      clear_trouble_codes();
      num_of_codes_reported = 0;
      mil_is_on = FALSE;
      broadcast_dialog_message(MSG_READY, 0); // tell everyone that we're done
   }
   else    // if we received "BUS BUSY", "DATA ERROR", "<DATA ERROR", SERIAL_ERROR, or RUBBISH,
   {       // try to re-send the request, do nothing if successful and alert user if failed:
      if(retry_attempts > 0) //
      {
         retry_attempts--;
         switch (operation)
         {
            case READ_CODES:
            case READ_PENDING:
            case NUM_OF_CODES:  // if we are currently reading codes,
               broadcast_dialog_message(MSG_READ_CODES, 0); // try reading again
               break;

            case CLEAR_CODES:   // if we are currently clearing codes,
               broadcast_dialog_message(MSG_CLEAR_CODES, 0);  // try clearing again
               break;
         }
      }
      else
      {
         display_error_message(error, FALSE);
         retry_attempts = NUM_OF_RETRIES; // reset the number of retry attempts
         clear_trouble_codes();
         num_of_codes_reported = 0;
         mil_is_on = FALSE;
         broadcast_dialog_message(MSG_READY, 0); // tell everyone that we're done
      }
   }
}


void swap_codes(TROUBLE_CODE *code1, TROUBLE_CODE *code2)
{
   char temp_str[256];
   int temp_int;
   
   temp_int = code1->pending;
   strcpy(temp_str, code1->code);
   code1->pending = code2->pending;
   strcpy(code1->code, code2->code);
   code2->pending = temp_int;
   strcpy(code2->code, temp_str);
}


void add_trouble_code(const TROUBLE_CODE * init_code)
{
   TROUBLE_CODE *next = trouble_codes;

   if (!(trouble_codes = (TROUBLE_CODE *)malloc(sizeof(TROUBLE_CODE))))
      fatal_error("Could not allocate enough memory for new trouble code");
   
   if (init_code)
   {
      strcpy(trouble_codes->code, init_code->code);
      trouble_codes->description = init_code->description;
      trouble_codes->solution = init_code->solution;
      trouble_codes->pending = init_code->pending;
   }
   else
   {
      trouble_codes->code[0] = 0;
      trouble_codes->description = NULL;
      trouble_codes->solution = NULL;
      trouble_codes->pending = FALSE;
   }
   
   trouble_codes->next = next;
}


TROUBLE_CODE *get_trouble_code(int index)
{
   int i;
   TROUBLE_CODE *trouble_code = trouble_codes;

   for(i = 0; i < index; i++)
   {
      if (trouble_code->next == NULL)
         return NULL;
      trouble_code = trouble_code->next;
   }

   return trouble_code;
}


int get_number_of_codes()
{
   TROUBLE_CODE *trouble_code = trouble_codes;
   int ret = 0;
   
   while(trouble_code)
   {
      trouble_code = trouble_code->next;
      ret++;
   }
   
   return ret;
}


void clear_trouble_codes()
{
   TROUBLE_CODE *next;
   
   while (trouble_codes)
   {
      next = trouble_codes->next;

      if (trouble_codes->description)
         free(trouble_codes->description);
      if (trouble_codes->solution)
         free(trouble_codes->solution);
      free(trouble_codes);

      trouble_codes = next;
   }
}


PACKFILE *file_handle(char code_letter)
{
   static PACKFILE *file = NULL;
   static char current_code_letter = 0;
   char file_name[30];
   
   if (code_letter == 0)
   {
      current_code_letter = 0;
      if (file != NULL)
         pack_fclose(file);
      file = NULL;
   }
   else if (code_letter != current_code_letter)
   {
      if (file != NULL)
      {
         pack_fclose(file);
         file = NULL;
      }
   
      sprintf(file_name, "%s#%ccodes", code_defs_file_name, tolower(code_letter));
      packfile_password(PASSWORD);
      file = pack_fopen(file_name, F_READ_PACKED);
      packfile_password(NULL);
      current_code_letter = code_letter;
   }
   
   if (file == NULL)     
      return NULL;
      
   if (pack_feof(file) == TRUE) 
      return NULL;
      
   return file;
}
