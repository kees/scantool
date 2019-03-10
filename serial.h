#ifndef SERIAL_H
#define SERIAL_H

#ifdef ALLEGRO_WINDOWS
   #define BAUD_RATE_9600    9600
   #define BAUD_RATE_38400   38400
   #define BAUD_RATE_115200  115200
   #define BAUD_RATE_230400  230400
#elif TERMIOS
   #include <termios.h>
   #define BAUD_RATE_9600    B9600
   #define BAUD_RATE_38400   B38400
   #define BAUD_RATE_115200  B115200
   #define BAUD_RATE_230400  B230400
   char *getTtyName(int *idx);
#else
   #define DZCOMM_SECONDARY_INCLUDE
   #include <dzcomm.h>
   #define BAUD_RATE_9600    _9600
   #define BAUD_RATE_38400   _38400
   #define BAUD_RATE_115200  _115200
#endif

//read_comport returned data type
#define EMPTY    0
#define DATA     1
#define PROMPT   2

#define SPECIAL_DELIMITER   '\t'

//comport status
#define READY          0
#define NOT_OPEN       1
#define USER_IGNORED   2

//process_response return values
#define HEX_DATA           0
#define BUS_BUSY           1
#define BUS_ERROR          2
#define BUS_INIT_ERROR     3
#define UNABLE_TO_CONNECT  4
#define CAN_ERROR          5
#define DATA_ERROR         6
#define DATA_ERROR2        7
#define ERR_NO_DATA        8
#define BUFFER_FULL        9
#define SERIAL_ERROR       10
#define UNKNOWN_CMD        11
#define RUBBISH            12

#define INTERFACE_ID       13
#define INTERFACE_ELM320   13
#define INTERFACE_ELM322   14
#define INTERFACE_ELM323   15
#define INTERFACE_ELM327   16
#define INTERFACE_OBDLINK  17
#define STN_MFR_STRING     18
#define ELM_MFR_STRING     19

// timeouts
#define OBD_REQUEST_TIMEOUT   12000
#define ATZ_TIMEOUT           2000
#define AT_TIMEOUT            1000
#define ECU_TIMEOUT           5000

// function prototypes
void serial_module_init();
void serial_module_shutdown();
int open_comport();
void close_comport();
void send_command(const char *command);
int read_comport(char *response);
void start_serial_timer(int delay);
void stop_serial_timer();
int process_response(const char *cmd_sent, char *msg_received);
int find_valid_response(char *buf, char *response, const char *filter, char **stop);
const char *get_protocol_string(int interface_type, int protocol_id);
int display_error_message(int error, int retry);

// variables
volatile int serial_time_out;
volatile int serial_timer_running;

struct COMPORT {
   int number;
   int baud_rate;
   int status;    // READY, NOT_OPEN, USER_IGNORED
} comport;

#endif
