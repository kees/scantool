#include <string.h>
#include "globals.h"
#include "serial.h"
#include "options.h"
#include "error_handlers.h"
#include "sensors.h"
#include "custom_gui.h"
#include "reset.h"

#define MSG_TOGGLE   MSG_USER
#define MSG_UPDATE   MSG_USER + 1
#define MSG_REFRESH  MSG_USER + 2

#define SENSORS_PER_PAGE      9
#define NUM_OF_RETRIES        2
#define SENSORS_TO_TIME_OUT   1 //number of sensors that need to time out before the warning will be issued
#define REFRESH_RATE_PRECISION 10 // how often the samples are taken, in milliseconds

// Sensor states:
#define SENSOR_OFF      0  // OFF,
#define SENSOR_ACTIVE   1  // ACTIVE (returned value), 
#define SENSOR_NA       2  // NA (displays "N/A")


typedef struct
{
   void (*formula)(int raw_data, char *buf);
   char label[32];
   char screen_buf[64];
   char pid[3];
   int enabled;
   int bytes; // number of data bytes expected from vehicle
} SENSOR;

static void load_sensor_states();
static void save_sensor_states();
static void fill_sensors(int page_number);

static int reset_chip_proc(int msg, DIALOG *d, int c);
static int options_proc(int msg, DIALOG *d, int c);
static int page_flipper_proc(int msg, DIALOG *d, int c);
static int sensor_proc(int msg, DIALOG *d, int c);
static int toggle_proc(int msg, DIALOG *d, int c);
static int toggle_all_proc(int msg, DIALOG *d, int c);
static int status_proc(int msg, DIALOG *d, int c);
static int page_number_proc(int msg, DIALOG *d, int c);
static int inst_refresh_rate_proc(int msg, DIALOG *d, int c);
static int avg_refresh_rate_proc(int msg, DIALOG *d, int c);
static int page_updn_handler_proc(int msg, DIALOG *d, int c);
static int genuine_proc(int msg, DIALOG *d, int c);

// Sensor formulae:
static void throttle_position_formula(int data, char *buf); // Page 1
static void engine_rpm_formula(int data, char *buf);
static void vehicle_speed_formula(int data, char *buf);
static void engine_load_formula(int data, char *buf);
static void timing_advance_formula(int data, char *buf);
static void intake_pressure_formula(int data, char *buf);
static void air_flow_rate_formula(int data, char *buf);
static void fuel_system1_status_formula(int data, char *buf);
static void fuel_system2_status_formula(int data, char *buf);
static void short_term_fuel_trim_formula(int data, char *buf); // Page 2
static void long_term_fuel_trim_formula(int data, char *buf);
static void intake_air_temp_formula(int data, char *buf);
static void coolant_temp_formula(int data, char *buf);
static void fuel_pressure_formula(int data, char *buf);
static void secondary_air_status_formula(int data, char *buf);
static void pto_status_formula(int data, char *buf);
static void o2_sensor_formula(int data, char *buf);
void obd_requirements_formula(int data, char *buf);
// added 1/2/2003
static void engine_run_time_formula(int data, char *buf);
static void mil_distance_formula(int data, char *buf);
static void frp_relative_formula(int data, char *buf);
static void frp_widerange_formula(int data, char *buf);
static void o2_sensor_wrv_formula(int data, char *buf);
static void commanded_egr_formula(int data, char *buf);
static void egr_error_formula(int data, char *buf);
static void evap_pct_formula(int data, char *buf);
static void fuel_level_formula(int data, char *buf);
static void warm_ups_formula(int data, char *buf);
static void clr_distance_formula(int data, char *buf);
static void evap_vp_formula(int data, char *buf);
static void baro_pressure_formula(int data, char *buf);
static void o2_sensor_wrc_formula(int data, char *buf);
static void cat_temp_formula(int data, char *buf);
static void ecu_voltage_formula(int data, char *buf);
static void abs_load_formula(int data, char *buf);
static void eq_ratio_formula(int data, char *buf);
static void relative_tp_formula(int data, char *buf);
static void amb_air_temp_formula(int data, char *buf);
static void abs_tp_formula(int data, char *buf);
static void tac_pct_formula(int data, char *buf);
static void mil_time_formula(int data, char *buf);
static void clr_time_formula(int data, char *buf);

// variables
static int device_connected = FALSE;
static int reset_hardware = FALSE;
static int num_of_sensors = 0;
static int num_of_disabled_sensors = 0;
static int sensors_on_page = 0;
static int current_page = 0;

static float inst_refresh_rate = -1; // instantaneous refresh rate
static float avg_refresh_rate = -1;  // average refresh rate

static volatile int refresh_time; // time between sensor updates

static SENSOR sensors[] =
{
   // formula                        // label            //screen_buffer  //pid  //enabled // bytes
   { throttle_position_formula,     "Absolute Throttle Position:",     "", "11",      0,    1 },
   { engine_rpm_formula,            "Engine RPM:",                     "", "0C",      -1,   2 },
   { vehicle_speed_formula,         "Vehicle Speed:",                  "", "0D",      0,    1 },
   { engine_load_formula,           "Calculated Load Value:",          "", "04",      0,    1 },
   { timing_advance_formula,        "Timing Advance (Cyl. #1):",       "", "0E",      0,    1 },
   { intake_pressure_formula,       "Intake Manifold Pressure:",       "", "0B",      0,    1 },
   { air_flow_rate_formula,         "Air Flow Rate (MAF sensor):",     "", "10",      0,    2 },
   { fuel_system1_status_formula,   "Fuel System 1 Status:",           "", "03",      0,    2 },
   { fuel_system2_status_formula,   "Fuel System 2 Status:",           "", "03",      0,    2 },
   // Page 2
   { short_term_fuel_trim_formula,  "Short Term Fuel Trim (Bank 1):",  "", "06",      0,    2 },
   { long_term_fuel_trim_formula,   "Long Term Fuel Trim (Bank 1):",   "", "07",      0,    2 },
   { short_term_fuel_trim_formula,  "Short Term Fuel Trim (Bank 2):",  "", "08",      0,    2 },
   { long_term_fuel_trim_formula,   "Long Term Fuel Trim (Bank 2):",   "", "09",      0,    2 },
   { intake_air_temp_formula,       "Intake Air Temperature:",         "", "0F",      0,    1 },
   { coolant_temp_formula,          "Coolant Temperature:",            "", "05",      -1,   1 },
   { fuel_pressure_formula,         "Fuel Pressure (gauge):",          "", "0A",      0,    1 },
   { secondary_air_status_formula,  "Secondary air status:",           "", "12",      0,    1 },
   { pto_status_formula,            "Power Take-Off Status:",          "", "1E",      0,    1 },
   // Page 3
   { o2_sensor_formula,             "O2 Sensor 1, Bank 1:",            "", "14",      0,    2 },
   { o2_sensor_formula,             "O2 Sensor 2, Bank 1:",            "", "15",      0,    2 },
   { o2_sensor_formula,             "O2 Sensor 3, Bank 1:",            "", "16",      0,    2 },
   { o2_sensor_formula,             "O2 Sensor 4, Bank 1:",            "", "17",      0,    2 },
   { o2_sensor_formula,             "O2 Sensor 1, Bank 2:",            "", "18",      0,    2 },
   { o2_sensor_formula,             "O2 Sensor 2, Bank 2:",            "", "19",      0,    2 },
   { o2_sensor_formula,             "O2 Sensor 3, Bank 2:",            "", "1A",      0,    2 },
   { o2_sensor_formula,             "O2 Sensor 4, Bank 2:",            "", "1B",      0,    2 },
   { obd_requirements_formula,      "OBD conforms to:",                "", "1C",      -1,   1 },
   // Page 4
   { o2_sensor_wrv_formula,         "O2 Sensor 1, Bank 1 (WR):",       "", "24",      0,    4 },    // o2 sensors (wide range), voltage
   { o2_sensor_wrv_formula,         "O2 Sensor 2, Bank 1 (WR):",       "", "25",      0,    4 },
   { o2_sensor_wrv_formula,         "O2 Sensor 3, Bank 1 (WR):",       "", "26",      0,    4 },
   { o2_sensor_wrv_formula,         "O2 Sensor 4, Bank 1 (WR):",       "", "27",      0,    4 },
   { o2_sensor_wrv_formula,         "O2 Sensor 1, Bank 2 (WR):",       "", "28",      0,    4 },
   { o2_sensor_wrv_formula,         "O2 Sensor 2, Bank 2 (WR):",       "", "29",      0,    4 },
   { o2_sensor_wrv_formula,         "O2 Sensor 3, Bank 2 (WR):",       "", "2A",      0,    4 },
   { o2_sensor_wrv_formula,         "O2 Sensor 4, Bank 2 (WR):",       "", "2B",      0,    4 },
   { engine_run_time_formula,       "Time Since Engine Start:",        "", "1F",      0,    2 },
   // Page 5
   { frp_relative_formula,          "FRP rel. to manifold vacuum:",    "", "22",      0,    2 },    // fuel rail pressure relative to manifold vacuum
   { frp_widerange_formula,         "Fuel Pressure (gauge):",          "", "23",      0,    2 },    // fuel rail pressure (gauge), wide range
   { commanded_egr_formula,         "Commanded EGR:",                  "", "2C",      0,    1 },
   { egr_error_formula,             "EGR Error:",                      "", "2D",      0,    1 },
   { evap_pct_formula,              "Commanded Evaporative Purge:",    "", "2E",      0,    1 },
   { fuel_level_formula,            "Fuel Level Input:",               "", "2F",      0,    1 },
   { warm_ups_formula,              "Warm-ups since ECU reset:",       "", "30",      0,    1 },
   { clr_distance_formula,          "Distance since ECU reset:",       "", "31",      0,    2 },
   { evap_vp_formula,               "Evap System Vapor Pressure:",     "", "32",      0,    2 },
   // Page 6
   { o2_sensor_wrc_formula,         "O2 Sensor 1, Bank 1 (WR):",       "", "34",      0,    4 },   // o2 sensors (wide range), current
   { o2_sensor_wrc_formula,         "O2 Sensor 2, Bank 1 (WR):",       "", "35",      0,    4 },
   { o2_sensor_wrc_formula,         "O2 Sensor 3, Bank 1 (WR):",       "", "36",      0,    4 },
   { o2_sensor_wrc_formula,         "O2 Sensor 4, Bank 1 (WR):",       "", "37",      0,    4 },
   { o2_sensor_wrc_formula,         "O2 Sensor 1, Bank 2 (WR):",       "", "38",      0,    4 },
   { o2_sensor_wrc_formula,         "O2 Sensor 2, Bank 2 (WR):",       "", "39",      0,    4 },
   { o2_sensor_wrc_formula,         "O2 Sensor 3, Bank 2 (WR):",       "", "3A",      0,    4 },
   { o2_sensor_wrc_formula,         "O2 Sensor 4, Bank 2 (WR):",       "", "3B",      0,    4 },
   { mil_distance_formula,          "Distance since MIL activated:",   "", "21",      0,    2 },
   // Page 7
   { baro_pressure_formula,         "Barometric Pressure (absolute):", "", "33",      0,    1 },
   { cat_temp_formula,              "CAT Temperature, B1S1:",          "", "3C",      0,    2 },
   { cat_temp_formula,              "CAT Temperature, B2S1:",          "", "3D",      0,    2 },
   { cat_temp_formula,              "CAT Temperature, B1S2:",          "", "3E",      0,    2 },
   { cat_temp_formula,              "CAT Temperature, B2S2:",          "", "3F",      0,    2 },
   { ecu_voltage_formula,           "ECU voltage:",                    "", "42",      0,    2 },
   { abs_load_formula,              "Absolute Engine Load:",           "", "43",      0,    2 },
   { eq_ratio_formula,              "Commanded Equivalence Ratio:",    "", "44",      0,    2 },
   { amb_air_temp_formula,          "Ambient Air Temperature:",        "", "46",      0,    1 },  // same scaling as $0F
   // Page 8
   { relative_tp_formula,           "Relative Throttle Position:",     "", "45",      0,    1 },
   { abs_tp_formula,                "Absolute Throttle Position B:",   "", "47",      0,    1 },
   { abs_tp_formula,                "Absolute Throttle Position C:",   "", "48",      0,    1 },
   { abs_tp_formula,                "Accelerator Pedal Position D:",   "", "49",      0,    1 },
   { abs_tp_formula,                "Accelerator Pedal Position E:",   "", "4A",      0,    1 },
   { abs_tp_formula,                "Accelerator Pedal Position F:",   "", "4B",      0,    1 },
   { tac_pct_formula,               "Comm. Throttle Actuator Cntrl:",  "", "4C",      0,    1 }, // commanded TAC
   { mil_time_formula,              "Engine running while MIL on:",    "", "4D",      0,    2 }, // minutes run by the engine while MIL activated
   { clr_time_formula,              "Time since DTCs cleared:",        "", "4E",      0,    2 },
   { NULL,                          "",                                "", "",        0,    0 }
};

static DIALOG sensor_dialog[] =
{
   /* (proc)                 (x)  (y)  (w)  (h) (fg)     (bg)           (key) (flags) (d1) (d2) (dp)                (dp2) (dp3) */
   { d_clear_proc,           0,   0,   0,   0,  0,       C_WHITE,       0,    0,      0,   0,   NULL,               NULL, NULL },
   { page_updn_handler_proc, 0,   0,   0,   0,  0,       0,             0,    0,      0,   0,   0,                  NULL, NULL },
   { d_shadow_box_proc,      40,  20,  560, 56, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { d_rtext_proc,           50,  25,  96,  20, C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Port Status:",     NULL, NULL },
   { status_proc,            172, 25,  288, 20, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { reset_chip_proc,        464, 28,  126, 40, C_BLACK, C_GREEN,       'r',  D_EXIT, 0,   0,   "&Reset Interface", NULL, NULL },
   { d_rtext_proc,           50,  51,  96,  20, C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Refresh rate:",    NULL, NULL },
   { inst_refresh_rate_proc, 154, 51,  174, 20, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { avg_refresh_rate_proc,  328, 51,  132, 20, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  87,  560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  91,  45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 0,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  94,  504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  123, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  127, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 1,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  130, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      1,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  159, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  163, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 2,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  166, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      2,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  195, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  199, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 3,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  202, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      3,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  231, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  235, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 4,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  238, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      4,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  267, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  271, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 5,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  274, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      5,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  303, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  307, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 6,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  310, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      6,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  339, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  343, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 7,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  346, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      7,   0,   NULL,               NULL, NULL },
   { d_box_proc,             40,  375, 560, 32, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { toggle_proc,            45,  379, 45,  24, C_BLACK, C_WHITE,       0,    D_EXIT, 8,   0,   NULL,               NULL, NULL },
   { sensor_proc,            95,  382, 504, 24, C_BLACK, C_LIGHT_GRAY,  0,    0,      8,   0,   NULL,               NULL, NULL },
   { toggle_all_proc,        40,  420, 100, 40, C_BLACK, C_DARK_YELLOW, 'a',  D_EXIT, 0,   0,   "&All ON",          NULL, NULL },
   { options_proc,           150, 420, 100, 40, C_BLACK, C_GREEN,       'o',  D_EXIT, 0,   0,   "&Options",         NULL, NULL },
   { d_shadow_box_proc,      260, 420, 230, 40, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { d_button_proc,          500, 420, 100, 40, C_BLACK, C_DARK_YELLOW, 'm',  D_EXIT, 0,   0,   "&Main Menu",       NULL, NULL },
   { st_ctext_proc,          300, 422, 38,  20, C_BLACK, C_TRANSP,      0,    0,      0,   0,   "Page",             NULL, NULL },
   { page_flipper_proc,      340, 425, 75,  30, C_BLACK, C_DARK_YELLOW, 'p',  D_EXIT, -1,  0,   "&Previous",        NULL, NULL },
   { page_flipper_proc,      425, 425, 55,  30, C_BLACK, C_GREEN,       'x',  D_EXIT, 1,   0,   "Ne&xt",            NULL, NULL },
   { page_number_proc,       300, 440, 36,  18, C_BLACK, C_LIGHT_GRAY,  0,    0,      0,   0,   NULL,               NULL, NULL },
   { genuine_proc,           0,   0,   0,   0,  0,       0,             0,    0,      0,   0,   NULL,               NULL, NULL },
   { d_yield_proc,           0,   0,   0,   0,  0,       0,             0,    0,      0,   0,   NULL,               NULL, NULL },
   { NULL,                   0,   0,   0,   0,  0,       0,             0,    0,      0,   0,   NULL,               NULL, NULL }
};


void inc_refresh_time(void)
{
   refresh_time++;
}
END_OF_FUNCTION(inc_refresh_time);


int display_sensor_dialog(int reset)
{
   int i;
   int ret;
   
   for (i = 0; sensors[i].formula; i++);
   num_of_sensors = i;
   
   current_page = 0;
   if (reset)
      reset_hardware = TRUE;
   
   load_sensor_states();
   fill_sensors(0);

   LOCK_VARIABLE(refresh_time);
   LOCK_FUNCTION(inc_refresh_time);
   
   ret = do_dialog(sensor_dialog, -1);
   save_sensor_states();

   return ret;
}


static void calculate_refresh_rate(int sensor_state)
{
   static int initialization_occured = FALSE;
   static int num_of_sensors_off = 0;
   static int reset_on_all_off_occured = FALSE;
   static int sensors_on_counter = 0;
   static float avg_refresh_rate_accumulator = 0;
   
   if (!initialization_occured) // we received our first ">", initialize...
   {
      if (sensor_state == SENSOR_ACTIVE) // if we received HEX data
      {
         refresh_time = 0; // reset the time
         install_int(inc_refresh_time, REFRESH_RATE_PRECISION); // install handler
         initialization_occured = TRUE;
      }
   }
   else  // if this is not our first ">"
   {
      if ((num_of_sensors_off >= sensors_on_page) && !reset_on_all_off_occured) // if all sensors on page are OFF
      {
        inst_refresh_rate = -1;
         avg_refresh_rate = -1;
         reset_on_all_off_occured = TRUE;
         broadcast_dialog_message(MSG_REFRESH, 0);
      }
      else  // if num_of_sensors_off < sensors_on_page
      {
         if (sensor_state != SENSOR_OFF)
         {
            reset_on_all_off_occured = FALSE;
            inst_refresh_rate = 1/(refresh_time*REFRESH_RATE_PRECISION*0.001);

            if (sensors_on_counter < (sensors_on_page - num_of_disabled_sensors))
            {
               sensors_on_counter++;
               avg_refresh_rate_accumulator += inst_refresh_rate;
            }
            else
            {
               avg_refresh_rate = avg_refresh_rate_accumulator/sensors_on_counter;
               avg_refresh_rate_accumulator = 0;
               sensors_on_counter = 0;
            }

            if (sensor_state == SENSOR_ACTIVE) // if we got response from ECU
               refresh_time = 0; // reset time
            broadcast_dialog_message(MSG_REFRESH, 0);
         }
      }
   }
}


int reset_chip_proc(int msg, DIALOG *d, int c)
{
   int ret = d_button_proc(msg, d, c);
   
   if (ret == D_CLOSE)
   {
      if (comport.status == READY)
         reset_hardware = TRUE;
      else
         alert("Port is not ready.", NULL, NULL, "OK", NULL, 0, 0);
      ret = D_REDRAWME;
   }
   
   return ret;
}


int options_proc(int msg, DIALOG *d, int c)
{
   int old_port;
   int ret = d_button_proc(msg, d, c);

   if (ret == D_CLOSE)
   {
      old_port = comport.number;
      display_options();
      if (comport.number != old_port)
         reset_hardware = TRUE;
      ret = D_REDRAWME;
   }

   return ret;
}


int page_updn_handler_proc(int msg, DIALOG *d, int c)
{
   if ((msg == MSG_XCHAR) && ((c>>8) == KEY_PGUP || (c>>8) == KEY_PGDN))
   {
      if ((c>>8) == KEY_PGUP)
         simulate_keypress('p');
      if ((c>>8) == KEY_PGDN)
         simulate_keypress('x');

      return D_USED_CHAR;
   }

   return D_O_K;
}


int inst_refresh_rate_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         if ((d->dp = calloc(64, sizeof(char))) == NULL)
            fatal_error("Could not allocate enough memory for instantaneous refresh rate control");
         // Fall through

      case MSG_REFRESH:
      case MSG_UPDATE:
         if (inst_refresh_rate >= 0)
           sprintf(d->dp, "Instantaneous: %.2fHz", inst_refresh_rate);
         else
            sprintf(d->dp, "Instantaneous: N/A");
         d->flags |= D_DIRTY;
         break;

      case MSG_DRAW:
         rectfill(screen, d->x, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the element
         break;

      case MSG_END:
         free(d->dp);
         d->dp = NULL;
         break;
   }

   return d_text_proc(msg, d, c);
}


int avg_refresh_rate_proc(int msg, DIALOG *d, int c)
{
	switch (msg)
   {
      case MSG_START:
         if ((d->dp = calloc(64, sizeof(char))) == NULL)
            fatal_error("Could not allocate enough memory for average refresh rate control");
         // Fall through

      case MSG_REFRESH:
      case MSG_UPDATE:
         if (avg_refresh_rate >= 0)
           sprintf(d->dp, "Average: %.2fHz", avg_refresh_rate);
         else
            sprintf(d->dp, "Average: N/A");
         d->flags |= D_DIRTY;
         break;

      case MSG_DRAW:
         rectfill(screen, d->x, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the element
         break;

      case MSG_END:
         free(d->dp);
         d->dp = NULL;
         break;
   }

   return d_text_proc(msg, d, c);
}

/* ---- DO NOT TRANSLATE FROM HERE ---- */
void load_sensor_states()
{
   int i;
   int setting;
   char temp_buf[64];
   
   for (i = 0; sensors[i].formula; i++)
   {
      sprintf(temp_buf, "sensor%i", i);
      setting = get_config_int("sensors", temp_buf, 0x12345678);
      if (setting != 0x12345678)
         sensors[i].enabled = setting;
   }
}


void save_sensor_states()
{
   int i;
   char temp_buf[64];
   
   for (i = 0; sensors[i].formula; i++)
   {
      sprintf(temp_buf, "sensor%i", i);
      set_config_int("sensors", temp_buf, sensors[i].enabled);
   }
}
/* ---- TO HERE ---- */

void fill_sensors(int page_number)
{
   int i;
   int index = 0;
   int sensor_index;
   const char *def_str;
   
   for (i = 0; sensor_dialog[i].proc; i++)
   {
      if (sensor_dialog[i].proc == sensor_proc)
      {
         sensor_index = index + page_number * SENSORS_PER_PAGE;
         if (sensors[sensor_index].formula)
         {
            if (sensors[sensor_index].enabled)
               def_str = "N/A";
            else
               def_str = "not monitoring";
            strcpy(sensors[sensor_index].screen_buf, def_str);
            sensor_dialog[i].dp3 = &sensors[sensor_index];
            index++;
         }
         else
         {
            sensor_dialog[i].dp3 = NULL;
            sensor_dialog[i].flags |= D_DIRTY;
         }
      }
   }
   
   sensors_on_page = index;
}


int page_number_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         if ((d->dp = calloc(64, sizeof(char))) == NULL)
            fatal_error("Could not allocate enough memory for page number control"); // do not translate
      case MSG_UPDATE:
         sprintf(d->dp, "%i of %i", current_page + 1, ((num_of_sensors % SENSORS_PER_PAGE) ? num_of_sensors/SENSORS_PER_PAGE + 1 : num_of_sensors/SENSORS_PER_PAGE));
         d->flags |= D_DIRTY;
         break;
         
      case MSG_DRAW:
         rectfill(screen, d->x-d->w, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the element
         break;

      case MSG_END:
         free(d->dp);
         d->dp = NULL;
         break;
   }
   
   return st_ctext_proc(msg, d, c);
}


int page_flipper_proc(int msg, DIALOG *d, int c)
{
   int ret;
   int last_page = (num_of_sensors % SENSORS_PER_PAGE) ? num_of_sensors/SENSORS_PER_PAGE : num_of_sensors/SENSORS_PER_PAGE - 1;

   switch (msg)
   {
      case MSG_START:
      case MSG_UPDATE:
         if (d->d1 == -1)
         {
            if (current_page <= 0)
               d->flags |= D_DISABLED;
            else
               d->flags &= ~D_DISABLED;
         }
         else if (d->d1 == 1)
         {
            if (current_page >= last_page)
               d->flags |= D_DISABLED;
            else
               d->flags &= ~D_DISABLED;
         }
         d->flags |= D_DIRTY;
         break;
   }

   ret = d_button_proc(msg, d, c);

   if (ret == D_CLOSE)
   {
      if (d->d1 == -1)
         current_page--;
      else if (d->d1 == 1)
         current_page++;
         
      if (current_page < 0)
         current_page = 0;
      else if (current_page > last_page)
         current_page = last_page;
         
      fill_sensors(current_page);
      inst_refresh_rate = -1;
      avg_refresh_rate = -1;
      broadcast_dialog_message(MSG_UPDATE, 0);

      ret = D_REDRAWME;
   }
   
   return ret;
}


int toggle_proc(int msg, DIALOG *d, int c)
{
   int ret;
   
   switch (msg)
   {
      case MSG_START:
         d->d2 = 1;
         if ((d->dp = calloc(4, sizeof(char))) == NULL)
         {
            // don't translate!
            sprintf(temp_error_buf, "Could not allocate enough memory for toggle button caption: %i", d->d1); 
            fatal_error(temp_error_buf);
         }
         strcpy(d->dp, "ON");
         d->bg = C_GREEN;
         // Fall through

      case MSG_UPDATE:
         if (d->d1 >= sensors_on_page)
         {
            d->flags |= D_DISABLED;
            d->bg = C_LIGHT_GRAY;
            strcpy(d->dp, empty_string);
            if (d->d2 == 0)
               num_of_disabled_sensors--;
         }
         else
         {
            if ((d->flags & D_DISABLED) && (d->d2 == 0))
               num_of_disabled_sensors++;
            d->flags &= ~D_DISABLED;
            if (d->d2 == 1)
            {
               d->bg = C_GREEN;
               strcpy(d->dp, "ON");
            }
            else
            {
               d->bg = C_DARK_YELLOW;
               strcpy(d->dp, "OFF");
            }
         }
         d->flags |= D_DIRTY;
         break;
   
      case MSG_TOGGLE:
         if (((c == d->d1) || ((c == -2) && (d->d2 == 1)) || ((c == -1) && (d->d2 == 0))) && !(d->flags & D_DISABLED))
         {
            if (d->d2 == 0)
            {
               d->d2 = 1;
               strcpy(d->dp, "ON");
               d->bg = C_GREEN;
            }
            else
            {
               d->d2 = 0;
               strcpy(d->dp, "OFF");
               d->bg = C_DARK_YELLOW;
            }
      
            return D_REDRAWME;
         }
         break;
   
      case MSG_END:
         free(d->dp);
         d->dp = NULL;
         break;
   }
   
   ret = d_button_proc(msg, d, c);
   
   if (ret == D_CLOSE)
   {
      broadcast_dialog_message(MSG_TOGGLE, d->d1);
      return D_O_K;
   }
   
   return ret;
}


int toggle_all_proc(int msg, DIALOG *d, int c)
{
   int ret;

   switch (msg)
   {
      case MSG_START:
         d->d2 = 1;
         if ((d->dp = calloc(64, sizeof(char))) == NULL)
            fatal_error("Could not allocate enough memory for toggle_all button caption"); // do not translate
         strcpy(d->dp, "All OFF");
         // Fall through
         
      case MSG_UPDATE:
         if (sensors_on_page == 0)
         {
            d->flags |= D_DISABLED;
            d->bg = C_LIGHT_GRAY;
         }
         else
         {
            d->flags &= ~D_DISABLED;
            if (d->bg == C_LIGHT_GRAY)
               d->bg = C_DARK_YELLOW;
            object_message(d, MSG_TOGGLE, 0);
         }
         d->flags |= D_DIRTY;
         break;
         
      case MSG_TOGGLE:
         if ((c == -1) || ((c >= 0) && (num_of_disabled_sensors <= 0)))
         {
            d->d2 = 1;
            strcpy(d->dp, "All OFF");
            d->bg = C_DARK_YELLOW;
         }
         else if ((c == -2) || ((c >= 0) && (num_of_disabled_sensors >= sensors_on_page)))
         {
            d->d2 = 0;
            strcpy(d->dp, "All ON");
            d->bg = C_GREEN;
            inst_refresh_rate = -1;
            avg_refresh_rate = -1;
            broadcast_dialog_message(MSG_REFRESH, 0);
         }
         d->flags |= D_DIRTY;
         break;

      case MSG_END:
         free(d->dp);
         d->dp = NULL;
         break;
   }

   ret = d_button_proc(msg, d, c);

   if (ret == D_CLOSE)
   {
      if (d->d2 == 0)
         broadcast_dialog_message(MSG_TOGGLE, -1);
      else
         broadcast_dialog_message(MSG_TOGGLE, -2);

      ret = D_REDRAWME;
   }

   return ret;
}


int status_proc(int msg, DIALOG *d, int c)
{
   switch (msg)
   {
      case MSG_START:
         if((d->dp = calloc(48, sizeof(char))) == NULL)
            fatal_error("Could not allocate enough memory for status control (sensors)");
         d->d1 = device_connected;
         d->d2 = -1;
         object_message(d, MSG_IDLE, 0);
         break;
   
      case MSG_END:
         free(d->dp);
         d->dp = NULL;
         break;
         
      case MSG_DRAW:
      {
         int circle_color;

         if ((comport.status == READY) && device_connected)
            circle_color = C_GREEN;
         else if ((comport.status == NOT_OPEN) || (comport.status == USER_IGNORED))
            circle_color = C_RED;
         else 
            circle_color = C_DARK_YELLOW;
         
         circlefill(screen, d->x-d->h/2, d->y+d->h/2-3, d->h/2-2, circle_color);
         circle(screen, d->x-d->h/2, d->y+d->h/2-3, d->h/2-2, C_BLACK);
         
         rectfill(screen, d->x, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the text area
         break;
      }
      case MSG_IDLE:      
         if ((d->d1 != device_connected) || (d->d2 != comport.status))
         {
            d->d1 = device_connected;
            d->d2 = comport.status;
         
            if (comport.status == READY)
            {
               if (device_connected)
                  sprintf(d->dp, " COM%i ready (device connected)", comport.number + 1);
               else
                  sprintf(d->dp, " COM%i ready (device not found)", comport.number + 1);
            }
            else
               sprintf(d->dp, " COM%i could not be opened", comport.number + 1);
         
            return D_REDRAWME;
         }
   }

   return d_text_proc(msg, d, c);
}


#define SENSOR_LABEL_MARGIN   245
#define SENSOR_VALUE_INDENT   8

int sensor_proc(int msg, DIALOG *d, int c)
{
   static int current_sensor = 0; // sensor we're working on
   static int new_page = FALSE;
   static int receiving_response = FALSE; // flag: receiving or sending
   static int num_of_sensors_timed_out = 0;
   static int ignore_device_not_connected = FALSE;
   static int retry_attempts = NUM_OF_RETRIES;
   static int active_sensor_found = FALSE;
   static char vehicle_response[1024];
   char buf[256];
   char cmd[8];
   int response_status = EMPTY; //status of the response: EMPTY, DATA, PROMPT
   int response_type;
   int ret = 0;
   SENSOR *sensor = (SENSOR *)d->dp3; // create a pointer to SENSOR structure in dp3 (vm)

   if ((msg == MSG_IDLE) && reset_hardware && comport.status == READY) // if user hit "Reset Chip" button, and we're doing nothing
   {
      reset_hardware = FALSE;	 
      receiving_response = FALSE;
      device_connected = FALSE;
      
      reset_chip();
      return D_O_K;
   }

   if (!sensor && (msg != MSG_DRAW))  // don't wanna handle any messages for an empty sensor line except MSG_DRAW
      return D_O_K;

   switch (msg)
   {
      case MSG_START:
         if (d->d1 == 0)
         {
            stop_serial_timer();
            receiving_response = FALSE;
            device_connected = FALSE;
            ignore_device_not_connected = FALSE;
            num_of_disabled_sensors = 0;
         }
         d->d2 = 0;
         d->flags &= ~D_DISABLED;
         // fall through

      case MSG_UPDATE:
         if (d->d1 == 0)  // if it's the first sensors in sensor_dialog
         {
            current_sensor = 0;
            num_of_sensors_timed_out = 0;
            active_sensor_found = FALSE;
            new_page = TRUE;
            // page was flipped, reset refresh rate variables
            inst_refresh_rate = 0;
            avg_refresh_rate = 0;
            refresh_time = 0;
         }
         if ((sensor->enabled && d->flags & D_DISABLED) || (!sensor->enabled && !(d->flags & D_DISABLED)))
            d->d2 = 1;
         d->flags |= D_DIRTY;
         break;

      case MSG_END:
         if (d->d1 == sensors_on_page - 1)
            stop_serial_timer();
         d->dp3 = NULL;
         break;
   
      case MSG_TOGGLE:
         if((d->d1 == c) || ((c == -2) && !(d->flags & D_DISABLED)) || ((c == -1) && (d->flags & D_DISABLED)))
         {
            if (d->flags & D_DISABLED)
            {
               d->flags &= ~D_DISABLED;
               sensor->enabled = TRUE;
               num_of_disabled_sensors--;
               strcpy(sensor->screen_buf, "N/A");
            }
            else
            {
               d->flags |= D_DISABLED;
               sensor->enabled = FALSE;
               num_of_disabled_sensors++;
               strcpy(sensor->screen_buf, "not monitoring");
               if (d->d1 == current_sensor)
               {
                  receiving_response = FALSE;
                  stop_serial_timer();
               }
               if (num_of_disabled_sensors == sensors_on_page) // if all of the sensors are disabled
                  num_of_sensors_timed_out = 0; // reset timeout counter
            }

            return D_REDRAWME;
         }
         break;

      case MSG_DRAW:
         if (d->d2)
         {
            d->d2 = 0;
            broadcast_dialog_message(MSG_TOGGLE, d->d1);
         }
         rectfill(screen, d->x, d->y, d->x+d->w-1, d->y+d->h-1, d->bg);  // clear the element
         if (sensor)
         {
            gui_textout_ex(screen, sensor->label, d->x + SENSOR_LABEL_MARGIN - gui_strlen(sensor->label), d->y, d->fg, d->bg, FALSE);
            gui_textout_ex(screen, sensor->screen_buf, d->x + SENSOR_LABEL_MARGIN + SENSOR_VALUE_INDENT, d->y, ((d->flags & D_DISABLED) ? gui_mg_color : d->fg), d->bg, FALSE);
         }
         return D_O_K;

      case MSG_IDLE:
         if (d->d1 == current_sensor)
         {
            if (comport.status == READY)
            {
               if (d->flags & D_DISABLED)
                  calculate_refresh_rate(SENSOR_OFF); // calculate instantaneous/average refresh rates

               if (!receiving_response)
               {
                  if (d->flags & D_DISABLED) // if the sensor is disabled
                  {
                     current_sensor = (current_sensor == sensors_on_page - 1) ? 0 : current_sensor + 1; // go to the next sensor
                     return D_O_K;
                  }

                  sprintf(cmd, "01%s", sensor->pid);
                  send_command(cmd); // send command for that particular sensor
                  new_page = FALSE;
                  receiving_response = TRUE; // now we're waiting for response
                  start_serial_timer(OBD_REQUEST_TIMEOUT); // start the timer
                  vehicle_response[0] = '\0'; // clear string
               }
               else  // if we are receiving response
               {
                  response_status = read_comport(buf);  // read comport
                  
                  if (d->flags & D_DISABLED) // if the sensor is disabled
                  {
                     if (response_status == PROMPT)
                        receiving_response = FALSE;
                     current_sensor = (current_sensor == sensors_on_page - 1) ? 0 : current_sensor + 1; // go to the next sensor
                     return D_O_K;
                  }
                  
                  if (response_status == DATA) // if data detected in com port buffer
                  {
                     strcat(vehicle_response, buf); // append contents of buf to vehicle_response
                  }
                  else if (response_status == PROMPT) // if '>' detected
                  {
                     device_connected = TRUE;
                     num_of_sensors_timed_out = 0;
                     receiving_response = FALSE; // we're not waiting for response any more
                     stop_serial_timer();

                     if (new_page)
                        break;
                     
                     strcat(vehicle_response, buf); // append contents of buf to vehicle_response
                     sprintf(cmd, "01%s", sensor->pid);
                     response_type = process_response(cmd, vehicle_response);

                     if (response_type == HEX_DATA)  // HEX_DATA received
                     {
                        sprintf(cmd, "41%s", sensor->pid);
                        if (find_valid_response(buf, vehicle_response, cmd, NULL))
                        {
                           active_sensor_found = TRUE;
                           calculate_refresh_rate(SENSOR_ACTIVE); // calculate instantaneous/average refresh rates
                           buf[4 + sensor->bytes * 2] = 0;  // solves problem where response is padded with zeroes (i.e., '41 05 7C 00 00 00')
                           sensor->formula((int)strtol(buf + 4, NULL, 16), buf); //plug the value into formula
                           strcpy(sensor->screen_buf, buf);  // and copy result in screen buffer
                           /* if current_sensor is the last sensor, set it to 0; otherwise current_sensor++ */
                           current_sensor = (current_sensor == sensors_on_page - 1) ? 0 : current_sensor + 1;
                           retry_attempts = NUM_OF_RETRIES;

                           return D_REDRAWME;
                        }
                        else
                           response_type = ERR_NO_DATA;
                     }
                     
                     strcpy(sensor->screen_buf, "N/A");

                     if (active_sensor_found)
                        calculate_refresh_rate(SENSOR_NA); // calculate instantaneous/average refresh rates

                     if (response_type == ERR_NO_DATA) // if we received "NO DATA", "N/A" will be printed
                     {
                        current_sensor = (current_sensor == sensors_on_page - 1) ? 0 : current_sensor + 1; // next time poll next sensor
                        retry_attempts = NUM_OF_RETRIES;
                     }
                     else if (response_type == BUS_ERROR || response_type == UNABLE_TO_CONNECT || response_type == BUS_INIT_ERROR)
                     {
                        display_error_message(response_type, FALSE);
                        retry_attempts = NUM_OF_RETRIES;
                     }
                     else // for other errors, try to re-send the request, do nothing if successful and alert user if failed
                     {
                        if (retry_attempts > 0)
                        {
                           retry_attempts--;
                           return D_O_K;
                        }
                        else
                        {
                           display_error_message(response_type, FALSE);
                           retry_attempts = NUM_OF_RETRIES; // reset the number of retry attempts
                        }
                     }

                     return D_REDRAWME;
                  }
                  else if (serial_time_out) // if timeout occured,
                  {
                     receiving_response = FALSE; // we're not waiting for a response any more
                     strcpy(sensor->screen_buf, "N/A");

                     if (num_of_sensors_timed_out >= SENSORS_TO_TIME_OUT)
                     {
                        num_of_sensors_timed_out = 0;
                        device_connected = FALSE;
                        if  (!ignore_device_not_connected)
                        {
                           ret = alert3("Device is not responding.", "Please check that it is connected", "and the port settings are correct", "&OK", "&Configure Port", "&Ignore", 'o', 'c', 'i');
                           if (ret == 2)
                              display_options();   // let the user choose correct settings
                           else if (ret == 3)
                              ignore_device_not_connected = TRUE;
                        }
                     }
                     else
                        num_of_sensors_timed_out++;

                     while (comport.status == NOT_OPEN)
                     {
                        if (alert("Port is not ready.", "Please check that you specified the correct port", "and that no other application is using it", "&Configure Port", "&Ignore", 'c', 'i') == 1)
                           display_options(); // let the user choose correct settings
                        else
                           comport.status = USER_IGNORED;
                     }

                     stop_serial_timer();

                     /* if current_sensor is the last sensor, set it to 0; otherwise current_sensor++ */
                     current_sensor = (current_sensor == sensors_on_page - 1) ? 0 : current_sensor + 1;

                     return D_REDRAWME;
                  }
               }
            }
         }
         break;
   } // end of switch (msg)

   return D_O_K;
}


void engine_rpm_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i r/min", (int)((float)data/4));
   else   // if the system is IMPERIAL
      sprintf(buf, "%i rpm", (int)((float)data/4));
}


void engine_load_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data*100/255);
}


void coolant_temp_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i%c C", data-40, 0xB0);
   else   // if the system is IMPERIAL
      sprintf(buf, "%i%c F", (int)(((float)data-40)*9/5 + 32), 0xB0);
}


void fuel_system_status_formula(int data, char *buf)
{
   if (data == 0)
      sprintf(buf, "unused");
   else if (data == 0x01)
      sprintf(buf, "open loop");
   else if (data == 0x02)
      sprintf(buf, "closed loop");
   else if (data == 0x04)
      sprintf(buf, "open loop (driving conditions)");
   else if (data == 0x08)
      sprintf(buf, "open loop (system fault)");
   else if (data == 0x10)
      sprintf(buf, "closed loop, O2 sensor fault");
   else
      sprintf(buf, "unknown: 0x%02X", data);
}


void fuel_system1_status_formula(int data, char *buf)
{
   fuel_system_status_formula((data >> 8) & 0xFF, buf);  // Fuel System 1 status: Data A
}


void fuel_system2_status_formula(int data, char *buf)
{
   fuel_system_status_formula(data & 0xFF, buf);  // Fuel System 2 status: Data B
}


void vehicle_speed_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i km/h", data);
   else   // if the system is IMPERIAL
      sprintf(buf, "%i mph", (int)(data/1.609));
}


void intake_pressure_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i kPa", data);
   else
      sprintf(buf, "%.1f inHg", data/3.386389);
}


void timing_advance_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%c", ((float)data-128)/2, 0xB0);
}


void intake_air_temp_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i%c C", data-40, 0xB0);
   else   // if the system is IMPERIAL
      sprintf(buf, "%i%c F", (int)(((float)data-40)*9/5 + 32), 0xB0);
}


void air_flow_rate_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%.2f g/s", data*0.01);
   else
      sprintf(buf, "%.1f lb/min", data*0.0013227736);
}


void throttle_position_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data*100/255);
}


// **** New formulae added 3/11/2003: ****

// Fuel Pressure (guage): PID 0A
void fuel_pressure_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i kPa", data*3);
   else   // if the system is IMPERIAL
      sprintf(buf, "%.1f psi", data*3*0.145037738);
}


// Fuel Trim statuses: PID 06-09
void short_term_fuel_trim_formula(int data, char *buf)
{
   if (data > 0xFF)  // we're only showing bank 1 and 2 FT
      data >>= 8;

   sprintf(buf, (data == 128) ? "0.0%%" : "%+.1f%%", ((float)data - 128)*100/128);
}


void long_term_fuel_trim_formula(int data, char *buf)
{
   if (data > 0xFF)  // we're only showing bank 1 and 2 FT
      data >>= 8;

   sprintf(buf, (data == 128) ? "0.0%%" : "%+.1f%%", ((float)data - 128)*100/128);
}


// Commanded secondary air status: PID 12
void secondary_air_status_formula(int data, char *buf)
{
   data = data & 0x0700; // mask bits 0, 1, and 2

   if (data == 0x0100)
      sprintf(buf, "upstream of 1st cat. conv.");
   else if (data == 0x0200)
      sprintf(buf, "downstream of 1st cat. conv.");
   else if (data == 0x0400)
      sprintf(buf, "atmosphere / off");
   else
      sprintf(buf, "Not supported");
}

// Oxygen sensor voltages & short term fuel trims: PID 14-1B
// Format is bankX_sensor

void o2_sensor_formula(int data, char *buf)
{
   if ((data & 0xFF) == 0xFF)  // if the sensor is not used in fuel trim calculation,
      sprintf(buf, "%.3f V", (data >> 8)*0.005);
   else
      sprintf(buf, ((data & 0xFF) == 128) ? "%.3f V @ 0.0%% s.t. fuel trim" : "%.3f V @ %+.1f%% s.t. fuel trim", (data >> 8)*0.005, ((float)(data & 0xFF) - 128)*100/128);
}


//Power Take-Off Status: PID 1E
void pto_status_formula(int data, char *buf)
{
   if ((data & 0x01) == 0x01)
      sprintf(buf, "active");
   else
      sprintf(buf, "not active");	
}

// OBD requirement to which vehicle is designed: PID 1C
void obd_requirements_formula(int data, char *buf)
{
   switch (data)
   {
      case 0x01:
         sprintf(buf, "OBD-II (California ARB)");
         break;
      case 0x02:
         sprintf(buf, "OBD (Federal EPA)");
         break;
      case 0x03:
         sprintf(buf, "OBD and OBD-II");
         break;
      case 0x04:
         sprintf(buf, "OBD-I");
         break;
      case 0x05:
         sprintf(buf, "Not OBD compliant");
         break;
      case 0x06:
         sprintf(buf, "EOBD (Europe)");
         break;
      case 0x07:
         sprintf(buf, "EOBD and OBD-II");
         break;
      case 0x08:
         sprintf(buf, "EOBD and OBD");
         break;
      case 0x09:
         sprintf(buf, "EOBD, OBD and OBD-II");
         break;
      case 0x0A:
         sprintf(buf, "JOBD (Japan)");
         break;
      case 0x0B:
         sprintf(buf, "JOBD and OBD-II");
         break;
      case 0x0C:
         sprintf(buf, "JOBD and EOBD");
         break;
      case 0x0D:
         sprintf(buf, "JOBD, EOBD, and OBD-II");
         break;
      default:
         sprintf(buf, "Unknown: 0x%02X", data);
   }
}

/* Sensors added 1/2/2003: */

void engine_run_time_formula(int data, char *buf)
{
   int sec, min, hrs;
   
   hrs = data / 3600;  // get hours
   min = (data % 3600) / 60;  // get minutes
   sec = data % 60;  // get seconds

   sprintf(buf, "%02i:%02i:%02i", hrs, min, sec);
}


void mil_distance_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i km", data);
   else   // if the system is IMPERIAL
      sprintf(buf, "%i miles", (int)(data/1.609));
}


void frp_relative_formula(int data, char *buf)
{
   float kpa, psi;
   
   kpa = data*0.079;
   psi = kpa*0.145037738;

   if (system_of_measurements == METRIC)
      sprintf(buf, "%.3f kPa", kpa);
   else   // if the system is IMPERIAL
      sprintf(buf, "%.1f PSI", psi);
}


void frp_widerange_formula(int data, char *buf)
{
   int kpa;
   float psi;

   kpa = data*10;
   psi = kpa*0.145037738;

   if (system_of_measurements == METRIC)
      sprintf(buf, "%i kPa", kpa);
   else
      sprintf(buf, "%.1f PSI", psi);
}


void o2_sensor_wrv_formula(int data, char *buf)
{
   float eq_ratio, o2_voltage; // equivalence ratio and sensor voltage
   
   eq_ratio = (float)(data >> 16)*0.0000305;  // data bytes A,B
   o2_voltage = (float)(data & 0xFFFF)*0.000122; // data bytes C,D
   
   sprintf(buf, "%.3f V, Eq. ratio: %.3f", o2_voltage, eq_ratio);
}


//Commanded EGR status: PID 2C
void commanded_egr_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data*100/255);
}

//EGR error: PID 2D
void egr_error_formula(int data, char *buf)
{
   sprintf(buf, (data == 128) ? "0.0%%" : "%+.1f%%", (float)(data-128)/255*100);
}


void evap_pct_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data/255*100);
}


void fuel_level_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data/255*100);
}


void warm_ups_formula(int data, char *buf)
{
   sprintf(buf, "%i", data);
}


void clr_distance_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i km", data);
   else
      sprintf(buf, "%i miles", (int)(data/1.609));
}


void evap_vp_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%.2f Pa", data*0.25);
   else
      sprintf(buf, "%.3f in H2O", data*0.25/249.088908);
}


void baro_pressure_formula(int data, char *buf)
{
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i kPa", data);
   else
      sprintf(buf, "%.1f inHg", data*0.2953);
}


void o2_sensor_wrc_formula(int data, char *buf)
{
   float eq_ratio, o2_ma; // equivalence ratio and sensor current

   eq_ratio = (float)(data >> 16)*0.0000305;  // data bytes A,B
   o2_ma = ((float)(data & 0xFFFF) - 0x8000)*0.00390625; // data bytes C,D

   sprintf(buf, "%.3f mA, Eq. ratio: %.3f", o2_ma, eq_ratio);
}


void cat_temp_formula(int data, char *buf)
{
   float c, f;
   
   c = data*0.1 - 40; // degrees Celcius
   f = c*9/5 + 32;  // degrees Fahrenheit
   
   if (system_of_measurements == METRIC)
      sprintf(buf, "%.1f%c C", c, 0xB0);
   else
      sprintf(buf, "%.1f%c F", f, 0xB0);
}


void ecu_voltage_formula(int data, char *buf)
{
   sprintf(buf, "%.3f V", data*0.001);
}


void abs_load_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data*100/255);
}


void eq_ratio_formula(int data, char *buf)
{
   sprintf(buf, "%.3f", data*0.0000305);
}


void relative_tp_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data*100/255);
}


void amb_air_temp_formula(int data, char *buf)
{
   int c, f;
   
   c = data-40; // degrees Celcius
   f = (float)c*9/5 + 32;  // degrees Fahrenheit
   
   if (system_of_measurements == METRIC)
      sprintf(buf, "%i%c C", c, 0xB0);
   else
      sprintf(buf, "%i%c F", f, 0xB0);
}


void abs_tp_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data*100/255);
}


void tac_pct_formula(int data, char *buf)
{
   sprintf(buf, "%.1f%%", (float)data*100/255);
}


void mil_time_formula(int data, char *buf)
{
   sprintf(buf, "%i hrs %i min", data/60, data%60);
}


void clr_time_formula(int data, char *buf)
{
   sprintf(buf, "%i hrs %i min", data/60, data%60);
}


int genuine_proc(int msg, DIALOG *d, int c)
{
   if (msg == MSG_IDLE)
      if (is_not_genuine_scan_tool == TRUE)
         return D_CLOSE;
   
   return D_O_K;
}
