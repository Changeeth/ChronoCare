//include libraries
#include <DHTesp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <sys/time.h>

//declare OLED parameter
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

//declare wifi time parameters
const char* ssid = "Wokwi-GUEST";
const char* password = "";
int wifiChannel = 6;

//define for healthy condition for medicine
#define TEMP_HIGH 32
#define TEMP_LOW 24
#define HUMIDITY_HIGH 80
#define HUMIDITY_LOW 65

//declare buzzer parameters
int melodyNotes[] = {131, 147, 165, 175, 196, 220, 247, 262}; // C, D, E, F, G, A, B, C (one octave lower)
int totalNotes = 8;

//declare pins
#define DHT_PIN 14
#define LED_PIN 5
#define BUZZER_PIN 4
#define PB_CANCEL 33
#define PB_OK 32
#define PB_UP 35
#define PB_DOWN 34

//declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

//declare global variables

  //related with time
  const tm timeInfo = 
  {
    12, // hour
    0,  // minute
    0   // second
  };

  const char* NTP_SERVER = "pool.ntp.org";
  String UTC_OFFSET="IST-5:30"; //defaulty set to srilanka
  String utcOffsets[] = {"UTC-12:00","UTC-11:00","UTC-10:00","UTC-09:30","UTC-09:00","UTC-08:00","UTC-07:00", "UTC-06:00","UTC-05:30", "UTC-04:30", "UTC-04:00", "UTC-03:30", "UTC-03:00","UTC-02:00","UTC-01:00", "UTC 00:00","UTC+01:00","UTC+02:00", "UTC+03:00", "UTC+03:30", "UTC+04:00", "UTC+04:30","UTC+05:00","UTC+05:30", "UTC+05:45","UTC+06:00","UTC+06:30","UTC+07:00","UTC+08:00","UTC+08:45", "UTC+09:00","UTC+09:30","UTC+10:00","UTC+10:30","UTC+11:00","UTC+12:00","UTC+12:45","UTC+13:00","UTC+14:00"};
  int numUtcOffsets = sizeof(utcOffsets)/sizeof(utcOffsets[0]);

  int seconds;
  int minutes;
  int hours;
  int day;
  int month;
  int year;

  //related with alarm
  bool alarmEnabled_1 = false;
  bool alarmEnabled_2 = false;

  int numAlarms = 2;
  int alarmHours[] = {0,0};
  int alarmMinutes[] = {0,0};
  bool alarmTriggered[] = {false,false};
  int lastAlarmDay = 0; // Track the day when alarms were last triggered

  //related with menue
  int currentMode = 0; //store current options in our menue
  String menuOptions[] = {"1 - Setup Timezone", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Disable Alarm 1","5 - Disable Alarm 2", "6 - Debug Alarms"};
  int maxModes = sizeof(menuOptions)/sizeof(menuOptions[0]); //store maximum number of in our menue
  
//declare global functions

// Displays text on the OLED screen with options for positioning and formatting
void print_line(String text, String displayClearStatus="n", int text_size=1, int column=0, int row=0)
{
  if (displayClearStatus=="y")  //n-do not clear display y-clear display
  {
    display.clearDisplay();
  }
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE); 
 
  display.setCursor(column,row); 
  display.println(text);
  display.display();
}

// Gets current time and date from NTP server and updates global time and date variables
void update_time()
{
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) 
  {
    print_line("Time update failed","y");
    return;
  }

  // Updating global time variables
  seconds = timeInfo.tm_sec;
  minutes = timeInfo.tm_min;
  hours = timeInfo.tm_hour;

  // Updating global date variables
   day = timeInfo.tm_mday;   
   month = timeInfo.tm_mon + 1;  
   year = timeInfo.tm_year + 1900;  
}

// Displays current time on the OLED screen
void print_time_now()
{
  update_time();
  print_line("Time-"+String(hours)+":"+String(minutes)+":"+String(seconds),"n",1,25,30);
  print_line("Date: " + String(day) + "/" + String(month) + "/" + String(year), "y", 1, 25, 10);
}

// Generates sound through the buzzer at specified frequency
void myTone(int pin, int frequency) 
{
  tone(pin, frequency);  // Use tone() for generating sound
}

// Stops sound generation on the buzzer
void myNoTone(int pin) 
{
  noTone(pin);  // Use noTone() to stop sound
}

// Displays medication alert and sounds alarm until canceled
void ring_alarm()
{
  print_line("Take","y",2,7,10);
  print_line("Medicine","n",2,9,35);
  
  bool break_happened=false;

  while (break_happened==false && digitalRead(PB_CANCEL)==HIGH)
  {
    //ring the buzzer
    for(int i=0;i<totalNotes;i++)
    {
      if (digitalRead(PB_CANCEL)==LOW)
      {
        break_happened=true;
        delay(200);
        break;
      }
      myTone(BUZZER_PIN,melodyNotes[i]);
      digitalWrite(LED_PIN, HIGH);
      delay(200);
      myNoTone(BUZZER_PIN);
      digitalWrite(LED_PIN, LOW);
      delay(10);
    }
  }
  digitalWrite(LED_PIN, LOW);
  myNoTone(BUZZER_PIN);
}

// Checks if a value is outside acceptable thresholds and displays warning messages
void show_warning(float value, float threshold_low, float threshold_high, int print_row, String DisplayStatus, String message)
{
  if (value < threshold_low)
  {
    message=message+"LOW";
    print_line(message,DisplayStatus,1,10,print_row);
  }

  if (value > threshold_high)
  {
    message=message+"HIGH";
    print_line(message,DisplayStatus,1,10,print_row);
  }
}

// Monitors environment conditions and triggers warnings if outside safe ranges
void check_temp_and_humidity()
{
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  int n=0;

  while(data.temperature<TEMP_LOW || data.temperature>TEMP_HIGH || data.humidity < HUMIDITY_LOW || data.humidity > HUMIDITY_HIGH )
  {
    show_warning(data.temperature, TEMP_LOW, TEMP_HIGH,10 ,"y","TEMPERATURE ");
    show_warning(data.humidity, HUMIDITY_LOW, HUMIDITY_HIGH,30 ,"n","HUMIDITY ");

    n++;
    
    if (n%2==0)
    {
      digitalWrite(LED_PIN, HIGH);
      myTone(BUZZER_PIN,melodyNotes[4]);
      delay(1000);
    }

    else
    {
      digitalWrite(LED_PIN, LOW);
      myNoTone(BUZZER_PIN);
      delay(500);
    }

    data = dhtSensor.getTempAndHumidity();
  }

  digitalWrite(LED_PIN, LOW);
  myNoTone(BUZZER_PIN);
}

// Debug function to help troubleshoot alarms
void debug_alarms() {
  display.clearDisplay();
  print_line("Alarm Debug", "y");
  print_line("A1: " + String(alarmEnabled_1 ? "ON" : "OFF") + " " + 
             String(alarmHours[0]) + ":" + String(alarmMinutes[0]), "n", 1, 0, 15);
  print_line("A2: " + String(alarmEnabled_2 ? "ON" : "OFF") + " " + 
             String(alarmHours[1]) + ":" + String(alarmMinutes[1]), "n", 1, 0, 25);
  print_line("Triggered: " + String(alarmTriggered[0]) + "," + String(alarmTriggered[1]), "n", 1, 0, 35);
  print_line("Day: " + String(day) + " Last: " + String(lastAlarmDay), "n", 1, 0, 45);
  delay(5000);
}

// Main monitoring function that updates time, checks alarms, and monitors environment
void update_time_with_check_alarm_and_check_warning()
{
  update_time(); // Make sure time is updated first
  print_time_now();

  // Reset alarm triggers if it's a new day
  if (day != lastAlarmDay) {
    for (int i = 0; i < numAlarms; i++) {
      alarmTriggered[i] = false;
    }
    lastAlarmDay = day;
  }

  // Check if any alarm should trigger
  for (int i = 0; i < numAlarms; i++) {
    bool isEnabled = (i == 0) ? alarmEnabled_1 : alarmEnabled_2;
    
    // Only proceed if the alarm is enabled
    if (isEnabled) {
      // Check if it's time to trigger the alarm and it hasn't been triggered today
      if (!alarmTriggered[i] && 
          hours == alarmHours[i] && 
          minutes == alarmMinutes[i] && 
          seconds < 3) { // 3-second window to trigger
        
        // Trigger the alarm
        ring_alarm();
        alarmTriggered[i] = true;
      }
    }
  }
  
  check_temp_and_humidity();
}

// Waits for and returns button press ID
int wait_for_button_press()
{
  int buttons[] = {PB_UP, PB_DOWN, PB_OK, PB_CANCEL};
  int numButtons = sizeof(buttons)/sizeof(buttons[0]);

  while(true)
  {
    for(int i=0; i<numButtons; i++)
    {
      if(digitalRead(buttons[i])==LOW)
      {
        delay(200);
        return buttons[i];
      }
    }
  }
}

// UI function for setting time values with up/down buttons and OK confirmation
void set_time_unit(int &unit, int max_value, String message)
{
  int temp_unit = unit;

  while (true)
  {
    print_line(message + String(temp_unit), "y");

    int pressed = wait_for_button_press();

    delay(200);

    switch(pressed)
    {
      case PB_UP:
        temp_unit = (temp_unit + 1) % max_value;
        break;
      case PB_DOWN:
        temp_unit = (temp_unit - 1 + max_value) % max_value;
        break;
      case PB_OK:
        unit = temp_unit;
        return;
      case PB_CANCEL:
        return;
    }
  }
}

// Configures an alarm by setting hours and minutes
void set_alarm(int alarm)
{
  set_time_unit(alarmHours[alarm], 24, "Set hour: ");
  set_time_unit(alarmMinutes[alarm], 60, "Set minute: ");

  // Reset trigger status when alarm is set
  alarmTriggered[alarm] = false;
  
  // Set the appropriate alarm enable flag
  if (alarm == 0) {
    alarmEnabled_1 = true;
  } else if (alarm == 1) {
    alarmEnabled_2 = true;
  }

  print_line("Alarm " + String(alarm+1) + " set", "y");
  print_line("Time: " + String(alarmHours[alarm]) + ":" + String(alarmMinutes[alarm]), "n", 1, 0, 20);
  delay(2000);
}

// Disables all alarms in the system
void disable_alarm_1()
{
  alarmEnabled_1 = false;
  print_line("Alarm 1 Disabled", "y");
  delay(1000);
}

void disable_alarm_2()
{
  alarmEnabled_2 = false;
  print_line("Alarm 2 Disabled", "y");
  delay(1000);
}

// UI for selecting a timezone from the predefined list
void set_time_zone()
{
  int index = 8; //default utc_offset
  int index_max=numUtcOffsets;

  while (true)
  {
    print_line("Select timezone:", "y");
    print_line(utcOffsets[index] , "n",1,0,15);

    int pressed = wait_for_button_press();

    delay(200);

    if (pressed == PB_UP)
      index=(index+1)%index_max;

    else if (pressed == PB_DOWN)
      index=(index- 1+index_max)%index_max;

    else if (pressed == PB_OK)
    {
      UTC_OFFSET = utcOffsets[index];
      configTzTime(UTC_OFFSET.c_str(), NTP_SERVER);
      print_line("Timezone updated", "y",1,10,30);
      delay(1000);
      break;
    }
    else if (pressed == PB_CANCEL)
      break;
  }
}

// Executes different functionalities based on menu selection
void run_mode(int mode)
{
  switch(mode)
  {
    case 0:
      set_time_zone();
      break;
    case 1:
      set_alarm(0); // Note: This is alarm index 0 (Alarm 1)
      break;
    case 2:
      set_alarm(1); // Note: This is alarm index 1 (Alarm 2)
      break;
    case 3:
      disable_alarm_1();
      break;
    case 4:
      disable_alarm_2();
      break;
    case 5: // New debug option
      debug_alarms();
      break;
  }
}

// Displays and manages the menu interface for system configuration
void go_to_menue()
{
  while(digitalRead(PB_CANCEL)==HIGH)
  {
    print_line(menuOptions[currentMode],"y",1,0,30);

    int pressed = wait_for_button_press();
    delay(200);

    switch(pressed)
    {
      case PB_UP:
        currentMode = (currentMode + 1) % maxModes;
        break;
      case PB_DOWN:
        currentMode = (currentMode - 1 + maxModes) % maxModes;
        break;
      case PB_OK:
        run_mode(currentMode);
        break;
      case PB_CANCEL:
        return;
    }
  }
}

//main code

// Initializes hardware, connects to WiFi, and synchronizes time
void setup() 
{
  //initialize pins mode
  pinMode(DHT_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  //Initialize the DHT22 sensor
  dhtSensor.setup(DHT_PIN,DHTesp::DHT22);

  //SSD1306_SWITCHAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC,SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 init failed"));
    for(;;); //don't proceed loop forever
  }

  //show initial display buffer contents on the screen
  //the library initiallizes this with an adafruit splash screen
  display.display(); 
  delay(1000); //pause for 1 sec

  //clear the buffer
  display.clearDisplay();

  //connect to Wi-Fi
  WiFi.begin(ssid, password, wifiChannel);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(250);
    print_line("Connecting to WiFi","y",1,0,5);
  }

  print_line("WiFi connected","n",1,0,20);
  delay(2000);

  print_line("Syncing time...","n",1,0,35);

  // Set the system time using configTzTime
  configTzTime("IST-5:30", "pool.ntp.org");

  //waiting for time to be set
  while (time(nullptr) < 1510644967) 
  {
    delay(500);
  }

  print_line("Time synchronized","n",1,0,50);
  delay(1000);
  display.clearDisplay();
  delay(1000);

  // Initialize lastAlarmDay with current day
  update_time();
  lastAlarmDay = day;

  //show welcome message
  print_line("MediBox Ready!","n",1.5,10,30);
  delay(1000);
  display.clearDisplay();

  delay(2000);
}

// Main program loop - monitors time/environment and checks for menu button press
void loop() 
{
  update_time_with_check_alarm_and_check_warning();
  if (digitalRead(PB_OK)==LOW)
  {
    delay(200);
    go_to_menue();
  }
}