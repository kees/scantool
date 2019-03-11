#include <string.h>
#include <ctype.h>
#include "globals.h"
#include <allegro/internal/aintern.h>
#ifdef ALLEGRO_WINDOWS
   #include <winalleg.h>
#elif TERMIOS
   #include <stdio.h>
   #include <termios.h>
#else
   #include <dzcomm.h>
#endif
#include "serial.h"

#ifdef ALLEGRO_WINDOWS
   static HANDLE com_port;
#elif TERMIOS
   static int fdtty;
   static struct termios oldtio,newtio;
#else
   static comm_port *com_port;
#endif

#define TX_TIMEOUT_MULTIPLIER    0
#define TX_TIMEOUT_CONSTANT      1000



//timer interrupt handler for sensor data
static void serial_time_out_handler()
{
   serial_time_out = TRUE;
   serial_timer_running = FALSE;
}
END_OF_STATIC_FUNCTION(serial_time_out_handler)


void start_serial_timer(int delay)
{
   stop_serial_timer();
   install_int_ex(serial_time_out_handler, MSEC_TO_TIMER(delay));  // install the timer
   serial_time_out = FALSE;
   serial_timer_running = TRUE;
}


void stop_serial_timer()
{
   remove_int(serial_time_out_handler);
   serial_time_out = FALSE;
   serial_timer_running = FALSE;
}


void serial_module_init()
{
#ifndef ALLEGRO_WINDOWS
#ifndef TERMIOS
   dzcomm_init();
#endif
#endif
   serial_timer_running = FALSE;
   /* all variables and code used inside interrupt handlers must be locked */
   LOCK_VARIABLE(serial_time_out);
   LOCK_FUNCTION(serial_time_out_handler);
   _add_exit_func(serial_module_shutdown, "serial_module_shutdown");
}


void serial_module_shutdown()
{
   close_comport();
   
#ifndef ALLEGRO_WINDOWS
#ifndef TERMIOS
    dzcomm_closedown();
#endif
#endif
   
   _remove_exit_func(serial_module_shutdown);
}


int open_comport()
{
#ifdef ALLEGRO_WINDOWS
   DCB dcb;
   COMMTIMEOUTS timeouts;
   DWORD bytes_written;
   char temp_str[16];
#endif
   
   if (comport.status == READY)    // if the comport is open,
      close_comport();    // close it
   
#ifdef ALLEGRO_WINDOWS
   // Naming of serial ports 10 and higher: See
   // http://www.connecttech.com/KnowledgeDatabase/kdb227.htm
   // http://support.microsoft.com/?id=115831
   sprintf(temp_str, "\\\\.\\COM%i", comport.number + 1);
   com_port = CreateFile(temp_str, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
   if (com_port == INVALID_HANDLE_VALUE)
   {
      comport.status = NOT_OPEN; //port was not open
      return -1; // return error
   }
   
   // Setup comport
   GetCommState(com_port, &dcb);
   dcb.BaudRate = comport.baud_rate;
   dcb.ByteSize = 8;
   dcb.StopBits = ONESTOPBIT;
   dcb.fParity = FALSE;
   dcb.Parity = NOPARITY;
   dcb.fOutxCtsFlow = FALSE;
   dcb.fOutxDsrFlow = FALSE;
   dcb.fOutX = FALSE;
   dcb.fInX = FALSE;
   dcb.fDtrControl = DTR_CONTROL_ENABLE;
   dcb.fRtsControl = RTS_CONTROL_ENABLE;
   dcb.fDsrSensitivity = FALSE;
   dcb.fErrorChar = FALSE;
   dcb.fAbortOnError = FALSE;
   SetCommState(com_port, &dcb);
   
   // Setup comm timeouts
   timeouts.ReadIntervalTimeout = MAXWORD;
   timeouts.ReadTotalTimeoutMultiplier = 0;
   timeouts.ReadTotalTimeoutConstant = 0;
   timeouts.WriteTotalTimeoutMultiplier = TX_TIMEOUT_MULTIPLIER;
   timeouts.WriteTotalTimeoutConstant = TX_TIMEOUT_CONSTANT;
   SetCommTimeouts(com_port, &timeouts);
   // Hack to get around Windows 2000 multiplying timeout values by 15
   GetCommTimeouts(com_port, &timeouts);
   if (TX_TIMEOUT_MULTIPLIER > 0)
      timeouts.WriteTotalTimeoutMultiplier = TX_TIMEOUT_MULTIPLIER * TX_TIMEOUT_MULTIPLIER / timeouts.WriteTotalTimeoutMultiplier;
   if (TX_TIMEOUT_CONSTANT > 0)
      timeouts.WriteTotalTimeoutConstant = TX_TIMEOUT_CONSTANT * TX_TIMEOUT_CONSTANT / timeouts.WriteTotalTimeoutConstant;
   SetCommTimeouts(com_port, &timeouts);
   
   // If the port is Bluetooth, make sure device is active
   PurgeComm(com_port, PURGE_TXCLEAR|PURGE_RXCLEAR);
   WriteFile(com_port, "?\r", 2, &bytes_written, 0);
   if (bytes_written != 2)  // If Tx timeout occured
   {
      PurgeComm(com_port, PURGE_TXCLEAR|PURGE_RXCLEAR);
      CloseHandle(com_port);
      comport.status = NOT_OPEN; //port was not open
      return -1;
   }
#elif TERMIOS
    char tmp[54];
    if( comport.number < 100 )
        snprintf(tmp, sizeof(tmp), "/dev/ttyS%d", comport.number);
    else
        snprintf(tmp, sizeof(tmp), "/dev/ttyUSB%d", comport.number-100);
    fdtty = open( tmp, O_RDWR | O_NOCTTY );
    if (fdtty <0) { return(-1); }

    tcgetattr(fdtty,&oldtio); /* save current port settings */

    bzero(&newtio, sizeof(newtio));

    cfsetspeed(&newtio, comport.baud_rate);

    cfmakeraw(&newtio);
    newtio.c_cflag |= (CLOCAL | CREAD);

            // No parity (8N1):
    newtio.c_cflag &= ~PARENB;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;

        // disable hardware flow control
    newtio.c_cflag &= ~CRTSCTS ;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received */

    tcflush(fdtty, TCIFLUSH);
    tcsetattr(fdtty,TCSANOW,&newtio);
#else
   com_port = comm_port_init(comport.number);
   if (!com_port) {
      write_log(szDZCommErr);
      comport.status = NOT_OPEN;
      return -1;
   }
   comm_port_set_baud_rate(com_port, comport.baud_rate);
   comm_port_set_parity(com_port, NO_PARITY);
   comm_port_set_data_bits(com_port, BITS_8);
   comm_port_set_stop_bits(com_port, STOP_1);
   comm_port_set_flow_control(com_port, NO_CONTROL);
   if (comm_port_install_handler(com_port) != 1)
   {
      write_log(szDZCommErr);
      comport.status = NOT_OPEN; //port was not open
      return -1; // return error
   }
#endif
   
   serial_time_out = FALSE;
   comport.status = READY;
   
   return 0; // everything is okay
}


void close_comport()
{
   if (comport.status == READY)    // if the comport is open, close it
   {
#ifdef ALLEGRO_WINDOWS
      PurgeComm(com_port, PURGE_TXCLEAR|PURGE_RXCLEAR);
      CloseHandle(com_port);
#elif TERMIOS
      tcsetattr(fdtty,TCSANOW,&oldtio);
      close(fdtty);
#else
      comm_port_flush_output(com_port);
      comm_port_flush_input(com_port);
      comm_port_uninstall(com_port);
#endif
   }
   comport.status = NOT_OPEN;
}


void send_command(const char *command)
{
   char tx_buf[32];
#ifdef ALLEGRO_WINDOWS
   DWORD bytes_written;
#endif
   
   sprintf(tx_buf, "%s\r", command);  // Append CR to the command
   
#ifdef ALLEGRO_WINDOWS
   PurgeComm(com_port, PURGE_TXCLEAR|PURGE_RXCLEAR);
   WriteFile(com_port, tx_buf, strlen(tx_buf), &bytes_written, 0);
   if (bytes_written != strlen(tx_buf))
   {
#ifdef LOG_COMMS
      log_comm("TX ERROR", tx_buf);  // Log transmission error
#endif
      return;
   }
#elif TERMIOS
//    printf("tx:'%s'\n",command);
	if( write(fdtty,tx_buf,strlen(tx_buf)) == -1 )
	{
	  perror("write tty");
	  close(fdtty);
	  fdtty = -1;
	}
	else
	{
        tcflush(fdtty, TCIFLUSH);
	}
#else
   comm_port_flush_output(com_port);
   comm_port_flush_input(com_port);
   comm_port_string_send(com_port, tx_buf);
#endif
   
#ifdef LOG_COMMS
   write_comm_log("TX", tx_buf);
#endif
}


int read_comport(char *response)
{
   char *prompt_pos = NULL;
#ifdef ALLEGRO_WINDOWS
   DWORD bytes_read = 0;
   DWORD errors;
   COMSTAT stat;
   int i, j;
   
   response[0] = '\0';
   ClearCommError(com_port, &errors, &stat);
   if (stat.cbInQue > 0)
      ReadFile(com_port, response, stat.cbInQue, &bytes_read, 0);
   
   // Remove extraneous 0s
   for (i = 0, j = 0; i < bytes_read; i++)
      if (response[i] > 0)
         response[j++] = response[i];
   response[j] = 0;
#elif TERMIOS
   int res=1;
   fd_set readfs;
   struct timeval timeout;
   char tmp[64];
   bzero(response,64);
   bzero(tmp,64);

   FD_ZERO(&readfs);
   FD_SET(fdtty, &readfs);
   while( res != 0 && fdtty != -1)
   {
       timeout.tv_usec = 200e3;  /* millisecondes */
       timeout.tv_sec  = 0;  /* secondes */
       res = select(fdtty+1, &readfs, NULL, NULL, &timeout);
       if( res != 0 )
       {
           if( read(fdtty, tmp, 64) != 0 )
           {
               strcat( response, tmp );
               bzero(tmp,64);
           }
       }
   }

//    printf("rx:'");
//    i=0;while(response[i]!=0) { printf("%c",response[i]>=32?response[i]:'.');i++;}
//    printf("'\n");

#else
   int i;
   char c;
   
   i = 0;
   while((c = comm_port_test(com_port)) >= 0) // while the serial buffer is not empty, read comport
      if (c > 0)
         response[i++] = c;
   response[i] = 0;
#endif
   
   prompt_pos = strchr(response, '>');
   if (prompt_pos != NULL)
   {
#ifdef LOG_COMMS
      write_comm_log("RX", response);
#endif
      *prompt_pos = '\0'; // erase ">"
      return PROMPT;      // command prompt detected
   }
   else if (strlen(response) == 0)  // if the string is empty,
      return EMPTY;
   else                         //otherwise,
   {
#ifdef LOG_COMMS
      write_comm_log("RX", response);
#endif
      return DATA;
   }
}


int find_valid_response(char *buf, char *response, const char *filter, char **stop)
{
   char *in_ptr = response;
   char *out_ptr = buf;

   buf[0] = 0;

   while (*in_ptr)
   {
      if (strncmp(in_ptr, filter, strlen(filter)) == 0)
      {
         while (*in_ptr && *in_ptr != SPECIAL_DELIMITER) // copy valid response into buf
         {
            *out_ptr = *in_ptr;
            in_ptr++;
            out_ptr++;
         }
         *out_ptr = 0;  // terminate string
         if (*in_ptr == SPECIAL_DELIMITER)
            in_ptr++;
         break;
      }
      else
      {
         // skip to the next delimiter
         while (*in_ptr && *in_ptr != SPECIAL_DELIMITER)
            in_ptr++;
         if (*in_ptr == SPECIAL_DELIMITER)  // skip the delimiter
            in_ptr++;
      }
   }

   if (stop)
      *stop = in_ptr;

   if (strlen(buf) > 0)
      return TRUE;
   else
      return FALSE;
}

// DO NOT TRANSLATE ANY STRINGS IN THIS FUNCTION!
int process_response(const char *cmd_sent, char *msg_received)
{
   int i = 0;
   char *msg = msg_received;
   int echo_on = TRUE; //echo status
   int is_hex_num = TRUE;
   char temp_buf[80];

   if (cmd_sent)
   {
      for(i = 0; cmd_sent[i]; i++)
      {
         if (cmd_sent[i] != *msg)    // if the characters are not the same,
         {
            echo_on = FALSE;  // say that echo is off
            break;            // break out of the loop
         }
         msg++;
      }

      if (echo_on == TRUE)  //if echo is on
      {
         send_command("ate0"); // turn off the echo
         start_serial_timer(AT_TIMEOUT);
         // wait for chip response or timeout
         while ((read_comport(temp_buf) != PROMPT) && !serial_time_out)
            ;
         stop_serial_timer();
         if (!serial_time_out)
         {
            send_command("atl0"); // turn off linefeeds
            start_serial_timer(AT_TIMEOUT);
            // wait for chip response or timeout
            while ((read_comport(temp_buf) != PROMPT) && !serial_time_out)
               ;
            stop_serial_timer();
         }
      }
      else //if echo is off
         msg = msg_received;
   }

   while(*msg && (*msg <= ' '))
      msg++;

   if (strncmp(msg, "SEARCHING...", 12) == 0)
      msg += 13;
   else if (strncmp(msg, "BUS INIT: OK", 12) == 0)
      msg += 13;
   else if (strncmp(msg, "BUS INIT: ...OK", 15) == 0)
      msg += 16;

   for(i = 0; *msg; msg++) //loop to copy data
   {
      if (*msg > ' ')  // if the character is not a special character or space
      {
         if (*msg == '<') // Detect <DATA_ERROR
         {
            if (strncmp(msg, "<DATA ERROR", 10) == 0)
               return DATA_ERROR2;
            else
               return RUBBISH;
         }
         msg_received[i] = *msg; // rewrite response
         if (!isxdigit(*msg) && *msg != ':')
            is_hex_num = FALSE;
         i++;
      }
      else if (((*msg == '\n') || (*msg == '\r')) && (msg_received[i-1] != SPECIAL_DELIMITER)) // if the character is a CR or LF
         msg_received[i++] = SPECIAL_DELIMITER; // replace CR with SPECIAL_DELIMITER
   }
   
   if (i > 0)
      if (msg_received[i-1] == SPECIAL_DELIMITER)
         i--;
   msg_received[i] = '\0'; // terminate the string

   if (is_hex_num)
      return HEX_DATA;

   if (strcmp(msg_received, "NODATA") == 0)
      return ERR_NO_DATA;
   if (strcmp(msg_received + strlen(msg_received) - 15, "UNABLETOCONNECT") == 0)
      return UNABLE_TO_CONNECT;
   if (strcmp(msg_received + strlen(msg_received) - 7, "BUSBUSY") == 0)
      return BUS_BUSY;
   if (strcmp(msg_received + strlen(msg_received) - 9, "DATAERROR") == 0)
      return DATA_ERROR;
   if (strcmp(msg_received + strlen(msg_received) - 8, "BUSERROR") == 0 ||
       strcmp(msg_received + strlen(msg_received) - 7, "FBERROR") == 0)
      return BUS_ERROR;
   if (strcmp(msg_received + strlen(msg_received) - 8, "CANERROR") == 0)
      return CAN_ERROR;
   if (strcmp(msg_received + strlen(msg_received) - 10, "BUFFERFULL") == 0)
      return BUFFER_FULL;
   if (strncmp(msg_received, "BUSINIT:", 8) == 0)
   {
      if (strcmp(msg_received + strlen(msg_received) - 5, "ERROR") == 0)
         return BUS_INIT_ERROR;
      else
         return SERIAL_ERROR;
   }
   if (strcmp(msg_received, "?") == 0)
      return UNKNOWN_CMD;
   if (strncmp(msg_received, "ELM320", 6) == 0)
      return INTERFACE_ELM320;
   if (strncmp(msg_received, "ELM322", 6) == 0)
      return INTERFACE_ELM322;
   if (strncmp(msg_received, "ELM323", 6) == 0)
      return INTERFACE_ELM323;
   if (strncmp(msg_received, "ELM327", 6) == 0)
      return INTERFACE_ELM327;
   if (strncmp(msg_received, "OBDLink", 7) == 0 ||
       strncmp(msg_received, "STN1000", 7) == 0 ||
       strncmp(msg_received, "STN11", 5) == 0)
      return INTERFACE_OBDLINK;
   if (strncmp(msg_received, "SCANTOOL.NET", 12) == 0)
      return STN_MFR_STRING;
   if (strcmp(msg_received, "OBDIItoRS232Interpreter") == 0)
      return ELM_MFR_STRING;
   
   return RUBBISH;
}


int display_error_message(int error, int retry)
{
   char buf[32];
   
   switch (error)
   {
      case BUS_ERROR:
         return alert("Bus Error: OBDII bus is shorted to Vbatt or Ground.", NULL, NULL, (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      case BUS_BUSY:
         return alert("OBD Bus Busy. Try again.", NULL, NULL, (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      case BUS_INIT_ERROR:
         return alert("OBD Bus Init Error. Check connection to the vehicle,", "make sure the vehicle is OBD-II compliant,", "and ignition is ON.", (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      case UNABLE_TO_CONNECT:
         return alert("Unable to connect to OBD bus.", "Check connection to the vehicle. Make sure", "the vehicle is OBD-II compliant, and ignition is ON.", (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      case CAN_ERROR:
         return alert("CAN Error.", "Check connection to the vehicle. Make sure", "the vehicle is OBD-II compliant, and ignition is ON.", (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      case DATA_ERROR:
      case DATA_ERROR2:
         return alert("Data Error: there has been a loss of data.", "You may have a bad connection to the vehicle,", "check the cable and try again.", (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      case BUFFER_FULL:
         return alert("Hardware data buffer overflow.", NULL, NULL, (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      case SERIAL_ERROR:
      case UNKNOWN_CMD:
      case RUBBISH:
         return alert("Serial Link Error: please check connection", "between computer and scan tool.", NULL, (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);

      default:
         sprintf(buf, "Unknown error occured: %i", error);
         return alert(buf, NULL, NULL, (retry) ? "Retry" : "OK", (retry) ? "Cancel" : NULL, 0, 0);
   }
}


const char *get_protocol_string(int interface_type, int protocol_id)
{
   switch (interface_type)
   {
      case INTERFACE_ELM320:
         return "SAE J1850 PWM (41.6 kBit/s)";
      case INTERFACE_ELM322:
         return "SAE J1850 VPW (10.4 kBit/s)";
      case INTERFACE_ELM323:
         return "ISO 9141-2 / ISO 14230-4 (KWP2000)";
      case INTERFACE_ELM327:
      case INTERFACE_OBDLINK:
         switch (protocol_id)
         {
            case 0:
               return "N/A";
            case 1:
               return "SAE J1850 PWM (41.6 kBit/s)";
            case 2:
               return "SAE J1850 VPW (10.4 kBit/s)";
            case 3:
               return "ISO 9141-2";
            case 4:
               return "ISO 14230-4 KWP2000 (5-baud init)";
            case 5:
               return "ISO 14230-4 KWP2000 (fast init)";
            case 6:
               return "ISO 15765-4 CAN (11-bit ID, 500 kBit/s)";
            case 7:
               return "ISO 15765-4 CAN (29-bit ID, 500 kBit/s)";
            case 8:
               return "ISO 15765-4 CAN (11-bit ID, 250 kBit/s)";
            case 9:
               return "ISO 15765-4 CAN (29-bit ID, 250 kBit/s)";
         }
   }
   
   return "unknown";
}
