#include <Servo.h>
#include <Conceptinetics.h>       // https://sourceforge.net/projects/dmxlibraryforar/files/
//#include <LiquidCrystal.h>       // https://www.dfrobot.com/wiki/index.php/Arduino_LCD_KeyPad_Shield_(SKU:_DFR0009)
#include <EEPROM.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_pinIO.h>

#define BUTTON_PIN A0
#define SERVO_PIN A5
#define BACKLIGHT_PIN 10

#define DMX_TIMEOUT 5
#define DMX_SLAVE_CHANNELS   512

#define btnNONE   0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnRIGHT  4
#define btnSELECT 5

/*
 * (used by example from bperrybap)
 * Macros to safely turn on the backlight even with back BL hardware
 * These assume that the BL pin is not touched or used after RESET other
 * that by these macros.
 */
#define enableBacklight(pin) pinMode(pin, INPUT)
#define disableBacklight(pin) pinMode(pin, OUTPUT)


/*
 * GLOBALS
 */

Servo servo;
DMX_Slave dmx_slave (DMX_SLAVE_CHANNELS);
hd44780_pinIO lcd( 8, 9, 4, 5, 6, 7 );
//hd44780_pinIO lcd( 8, 9, 11, 5, 6, 7 );       // May need to move the connector from pin 4 to pin 11 to avoid conflict

int dmx_channel = 1;
int dmx_level = 0;
int dmx_scale_input = 1;               // Scale DMX input range (0-255) to match servo (0-180)
int default_on_signal_loss = 1;
unsigned long dmx_last_millis = 0;     // last time a dmx frame was received

int default_servo_position = 0;
int servo_position = 0;
int last_servo_position = -1;
int demo_mode_active = 0;

long int backlight_time = 0;





/************************************************************
 * Save and Restore Configuration
 ************************************************************/

// end with btnNONE instead of NULL
const int konami_sequence[] = { btnUP, btnUP, btnDOWN, btnDOWN, btnLEFT, btnRIGHT, btnLEFT, btnRIGHT, btnSELECT, btnNONE };
int check_konami(int keypress) {
  static int konami_progress = 0;
  if (keypress == btnNONE) return 0;

  if ( keypress == konami_sequence[konami_progress] ) {
    konami_progress++;
  } else {
    konami_progress = 0;
  }
  if ( konami_sequence[konami_progress] == btnNONE ) {
    konami_progress = 0;
    return 1;
  }
  return 0;
}

const char *secret[] = {
  "This is a test  " , "of the emergency",
  "easter egg      " , "system. In the  ",
  "event of a real " , "easter egg,     ",
  "this message    " , "would contain   ",
  NULL, NULL
};

void konami_message() {
  char **ptr;
  ptr = (char **)secret;
  while (*ptr != 0) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(*ptr++);
    lcd.setCursor(0,1);
    lcd.print(*ptr++);
    delay(2600);
  }
  delay(2500);
  lcd.setCursor(0,0);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(15,1);
}

void dmx_secret_message() {
  if (dmx_channel == 232) {
    lcd.write("Happy Birthday!");
    delay(10000);
  } else if (dmx_channel == 255) {
    lcd.write("Happy");
    lcd.setCursor(0,1);
    lcd.write("towel day!!");
    delay(10000);
  }
}




/************************************************************
 * Save and Restore Configuration
 ************************************************************/

int check_range(int val, int min, int max, int dfl) {
  // Checks to see if a number is within the allowed range
  // Returns the number if it is valid, or returns default.
  if (val < min) return min;
  if (val > max) return min;
  return val;
}

void write_eeprom() {
  EEPROM.put(0, dmx_channel);
  EEPROM.put(4, default_servo_position);
  EEPROM.put(8, backlight_time);
  EEPROM.put(12, default_on_signal_loss);
  EEPROM.put(16, dmx_scale_input);
}

void read_eeprom() {
  // Read saved config from eeprom and apply defaults if data is invalid
  EEPROM.get(0, dmx_channel);
  EEPROM.get(4, default_servo_position);
  EEPROM.get(8, backlight_time);
  EEPROM.get(12, default_on_signal_loss);
  EEPROM.get(16, dmx_scale_input);
  dmx_channel = check_range(dmx_channel, 0, 255, 0);
  default_servo_position = check_range(default_servo_position, 0, 180, 0);
  backlight_time = check_range(backlight_time, 0, 15, 5);
  default_on_signal_loss = check_range(default_on_signal_loss, 0, 1, 1);
  dmx_scale_input = check_range(dmx_scale_input, 0, 1, 1);
}



/************************************************************
 * Timers and Support
 ************************************************************/

bool handleBacklightTimer(int keypress) {
  /* Manage timing for the LCD backlight;
   * returns true if the backlight gets turned on.
   * returns false if the backlight is already on or if it gets turned off
   * By convention, call with -1 to just extend the timer.
   */
  static unsigned long currentMillis = 0;
  static unsigned long backlightOffMillis = 10000;  // default value == time from reset to backlight off
  static bool backlight_enabled = true;

  if (backlight_time <= 0) return false;   // Always on
  
  currentMillis = millis();
  if (keypress != btnNONE) {
    // user pressed a key - make sure backlight is on
    backlightOffMillis = currentMillis + backlight_time*1000;
    if (!backlight_enabled) {
      enableBacklight(BACKLIGHT_PIN);
      backlight_enabled = true;
      return true;
    }
  } else {
    // no key pressed - turn backlight off if enough time has passed
    if (backlight_enabled && currentMillis > backlightOffMillis) {
      disableBacklight(BACKLIGHT_PIN);
      backlight_enabled = false;
    }
  }
  return false;
}


void OnFrameReceiveComplete (unsigned short channelsReceived) {
  /* Callback function that fires every time a full DMX frame is received */
  dmx_last_millis = millis();
  dmx_level = dmx_slave.getChannelValue(dmx_channel);
}

int get_keypress() {
    /* Observed raw values:
     *     Right  = 002         None   = 102
     *     Up     = 141         Down   = 325
     *     Left   = 502         Select = 739
     *     
     */
    static int previous_state = btnNONE;
    int current_state;
    int analog_value;

    analog_value = analogRead(BUTTON_PIN);
    if (analog_value > 900) current_state = btnNONE;
    else if (analog_value > 600) current_state = btnSELECT;
    else if (analog_value > 450) current_state = btnLEFT;
    else if (analog_value > 285) current_state = btnDOWN;
    else if (analog_value > 110) current_state = btnUP;
    else current_state = btnRIGHT;

    if (previous_state == current_state) return btnNONE;
    else {
      previous_state = current_state;
      return current_state; 
    }
}



/************************************************************
 * Menu Handlers
 ************************************************************/

typedef struct {
  int order;
  const char *title;
  bool (*handler)(int);
  bool should_write_eeprom;
} menu_descriptor;

menu_descriptor menus[] = {
  { 0, "               ", NULL,                     false  },
  { 1, "DMX Channel    ", menu_dmx_channel,          true  },
  { 2, "DMX Scaling    ", menu_dmx_input_scaling,    true  },
  { 3, "Start position ", menu_start_position,       true  },
  { 4, "On Signal Loss ", menu_dmx_signal_loss,      true  },
  { 5, "Backlight Time ", menu_backlight_time,       true  },
  { 6, "Demo mode      ", menu_demo_mode,            false },
  { 0 , NULL, NULL, false }
};



bool menu_dmx_channel(int keypress) {
  lcd.setCursor(0,1);
  lcd.print("Channel=" + String(dmx_channel) + "  ");
  if      (keypress == btnUP)    dmx_channel++;
  else if (keypress == btnDOWN)  dmx_channel--;
  else if (keypress == btnLEFT)  dmx_channel -= 10;
  else if (keypress == btnRIGHT) dmx_channel += 10;
  if (dmx_channel >= 512) dmx_channel = dmx_channel - 512;
  if (dmx_channel <= 0) dmx_channel = dmx_channel + 512;
  return false;
}


bool menu_dmx_input_scaling(int keypress) {
  if (keypress == btnUP || keypress == btnDOWN){
    dmx_scale_input = 1 - dmx_scale_input;
  }
  lcd.setCursor(0,1);
  if (dmx_scale_input) {
    lcd.print("DMX In Scale = Y");
  } else {
    lcd.print("DMX In Scale = N");
  }
  return false;
}

bool menu_dmx_signal_loss(int keypress) {
  if (keypress == btnUP || keypress == btnDOWN) {
    default_on_signal_loss = 1 - default_on_signal_loss;
  }
  lcd.setCursor(0,1);
  if (default_on_signal_loss) {
    lcd.print("Goto default   ");
  } else {
    lcd.print("Hold position  ");
  }
  return false;
}

bool menu_start_position(int keypress) {
  lcd.setCursor(0,1);
  lcd.print("Position=" + String(default_servo_position) + "  ");
  servo.write(default_servo_position);
  if      (keypress == btnUP)    default_servo_position++;
  else if (keypress == btnDOWN)  default_servo_position--;
  else if (keypress == btnLEFT)  default_servo_position -= 10;
  else if (keypress == btnRIGHT) default_servo_position += 10;
  if (default_servo_position >= 180) default_servo_position = default_servo_position - 180;
  if (default_servo_position < 0) default_servo_position = default_servo_position + 180;
  return false;
}

bool menu_backlight_time(int keypress) {
    if (keypress == btnUP) {
      backlight_time++;
      if (backlight_time == 16)      backlight_time = 30;
      else if (backlight_time == 31) backlight_time = 45;
      else if (backlight_time == 46) backlight_time = 60;
      else if (backlight_time > 60)  backlight_time = 0;
      else if (backlight_time == 1)  backlight_time = 5;
      handleBacklightTimer(-1);    
    } else if (keypress == btnDOWN) {
      backlight_time--;
      if (backlight_time < 0)        backlight_time = 60;
      else if (backlight_time == 59) backlight_time = 45;
      else if (backlight_time == 44) backlight_time = 30;
      else if (backlight_time == 29) backlight_time = 15;
      else if (backlight_time == 4)  backlight_time = 0;
      handleBacklightTimer(-1);    
    }
    lcd.setCursor(0,1);
    if (backlight_time > 0) {
      lcd.print(String(backlight_time) + "s        ");
    } else {
      lcd.print("Always on");
    }
    return false;
}


bool menu_demo_mode(int keypress) {
  static int demo_direction = 1;
  static int demo_position = 0;

  if (keypress == btnLEFT || keypress == btnRIGHT || keypress == btnUP || keypress == btnDOWN) {
    demo_mode_active = 1 - demo_mode_active;
  }
  if (!demo_mode_active) {
    lcd.setCursor(0,1);
    lcd.print("<demo inactive> ");
    return false;
  } else {
    // The demo is running.
    handleBacklightTimer(-1); // keep backlight always-on during demo
    demo_position = demo_position + demo_direction;
    if (demo_position == 180 || demo_position == 0) {
      demo_direction = 0 - demo_direction;
    }
    servo.write(demo_position);
    for (int i = 0; i < 16; i++) {
      lcd.setCursor(i,1);
      if ((demo_position*16)/180 > i) lcd.write("-");
      else lcd.write(" ");
    }
    delay(10);
    return true;
  }
}





/************************************************************
 * Core Arduino Functions
 ************************************************************/

void setup() {
  uint8_t heart[8] = {0x0,0xa,0x1f,0x1f,0xe,0x4,0x0};

  dmx_slave.enable();
  dmx_slave.setStartAddress(1);
  dmx_slave.onReceiveComplete(OnFrameReceiveComplete);
  
  read_eeprom();
  servo.attach(SERVO_PIN);

  pinMode(3, INPUT);              // DMX RX-io
  pinMode(BUTTON_PIN, INPUT);     // keypad's input
  lcd.begin(16,2);
  lcd.createChar(0, heart);       // load character to the LCD
  
  handleBacklightTimer(-1);
  dmx_secret_message();
  lcd.setCursor(0,0);
}



void loop() {
  unsigned long currentMillis = 0;
  static int current_menu = 0;
  bool dmx_signal;
  int skip_dmx;
  int keypress;  

  /* Handle Display and Config Menus */
  keypress = get_keypress();

  /* Check DMX Signal Status */
  currentMillis = millis();
  dmx_signal = (dmx_last_millis + DMX_TIMEOUT*1000 > currentMillis) ? true: false;

  /* Konami Code is important! */
  if (check_konami(keypress)) konami_message();
  
  // Suppress the key's normal function if it turned on the backlight
  if (handleBacklightTimer(keypress)) return;

  // Handle menu screens.
  // Either change to another menu or call the current menu's handler.
  if (keypress == btnSELECT) {
    // User pressed select - move to the next menu in the list
    if (menus[current_menu].should_write_eeprom) write_eeprom();
    demo_mode_active = 0;
    current_menu++;
    lcd.clear();
    if (menus[current_menu].handler == NULL) {
      current_menu = 0;
      return;
    } else {
      lcd.setCursor(0,0);
      lcd.print(menus[current_menu].title);
      lcd.setCursor(15,0);
      lcd.print("*");
      lcd.setCursor(0,1);
    }
  } else {
    // We are in a menu... let the menu handler deal with this.
    if (current_menu != 0) {
      skip_dmx = menus[current_menu].handler(keypress);
    }
  } 
  
  if (current_menu == 0) {
    // This is the normal display - just process and display DMX
    lcd.setCursor(0,0);
    lcd.print("DMX Channel=" + String(dmx_channel) + "  ");
    lcd.setCursor(0,1);
    if (!dmx_signal) {
      lcd.print("No DMX Signal   ");
    } else {
      lcd.print("DMX Level=" + String(dmx_level) + "  ");
    }
    lcd.setCursor(15,0);
    lcd.print(" ");
  }

  // Some menus will have handled DMX output themselves.
  // If a menu returned true above, skip_dmx will be set and we won't touch the servo here.
  if (skip_dmx) return;

  /* Routine I/O Processing */
  if (dmx_signal) {
    servo_position = dmx_level;
    if (dmx_scale_input) servo_position = servo_position * (180.0/255.0);
    if (servo_position > 180) servo_position = 180;
  } else if (default_on_signal_loss) {
    // We may or may not want to reset the position when DMX signal drops
    servo_position = default_servo_position;
  }
  
  if (servo_position != last_servo_position) {
    // Don't try to move the servo unless it actually needs to move.
    servo.write(servo_position);
    last_servo_position = servo_position;
  }
}

