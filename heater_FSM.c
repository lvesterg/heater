// Heating system tracking & storage

// Hardware description
/* The circuit:
 
 * SD card attached to SPI bus as follows:
 ** CS - pin 10
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 
 Temperature sensors
 * DS18B20's
 * Onewire data bus on pin 8

 Display
 * Serial attached 16x2 red on black
 * databus on pin 2

I2C
 * SDA - pin A5
 * SCL - pin A4  
 * RTC 

*/

// includes 
#include <SoftwareSerial.h>  // For setting up serial coomunication with serial enabled 2x16 display
#include <OneWire.h> // OneWire library
#include <DallasTemperature.h> // Dallas DS18b20 sensor library
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>

// ** FSM states **************************************************************
#define S_BOOT 1
#define S_IDLE 2
#define S_TRIG_TEMP 3
#define S_READ_SENS 4
#define S_WRITE_SD 5

#define M_BOOT 10
#define M_PAGE1 11
#define M_PAGE2 12
#define M_PAGE3 13
#define M_PAGE4 14

static int menu_FSM = M_BOOT;
static int system_FSM = S_BOOT;
int menu_last_state;

// ** End FSM states ********************************************************** 
 
#define ONE_WIRE_BUS 8 


// ** Data structure ********************************************************** 
 typedef struct {
   float sensor1;	// Temperature reading
   float sensor2;	// Temperature reading
   float sensor3;	// Temperature reading
   float sensor4;	// Temperature reading
   float sensor5;	// Temperature reading
  } Payload;

// ** Display init ******************************************************** 
  // Attach the serial display's RX line to digital pin 2
  SoftwareSerial mySerial(3,2); // pin 2 = TX, pin 3 = RX (unused)
  int MenuDelayInMillis = 3000;
  unsigned long MenuShowTime;
 
// ** Global vars ********************************************************* 
  //Temperature chip i/o
  OneWire ds(ONE_WIRE_BUS); // on digital pin 8
    
  // Pass our oneWire reference to Dallas Temperature.
  DallasTemperature sensors(&ds);

// Needed for async reading of temp sensors
  int  resolution = 12;  unsigned long lastTempRequest = 0;
  int  delayInMillis = 0;
  float temperature = 0.0;
  
  Payload SensorData;

  DeviceAddress Sensor1 = { 0x28, 0x65, 0xDD, 0x66, 0x05, 0x00, 0x00, 0x28 };
  DeviceAddress Sensor2 = { 0x28, 0x77, 0x81, 0x95, 0x05, 0x00, 0x00, 0x50 };

// ** RTC library ************************************************************* 
RTC_DS1307 rtc;
char dateTimeString[40];

 
void setup() {

  // Init display
  mySerial.begin(9600); // set up serial port for 9600 baud
  delay(500); // wait for display to boot up
  


  // Setup DS1820 temp sensor

  sensors.begin();
  sensors.setResolution(Sensor1, 11);
  sensors.setResolution(Sensor2, 11);
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  delayInMillis = 750 / (1 << (12 - 11)); //750 for 12bit, 400 for 11bit, 220 for 10bit, 100 for 9bit
                        // calc by   delayInMillis = 750 / (1 << (12 - resolution)); 
  lastTempRequest = millis(); 


  // Set next state i FSM
  menu_FSM = M_PAGE1;
  menu_last_state = M_PAGE1;
  system_FSM = S_IDLE;
 
 
   // **************** Set up display *******************
  DisplayClear();
  MenuShowTime = millis();
 
  
  // **************** Set up RTC ***********************
  Wire.begin();
  rtc.begin();
  //TimeDate(rtc.now(),dateTimeString,1);

  //DateTime now = rtc.now();

 // write on display
  DisplayGoto(2,0);
  mySerial.print("Version 0.9B");

  
  // **************** Set up SD card *******************
  pinMode(10, OUTPUT);
  DisplayGoto(1,0);
  mySerial.write("Init SD -> "); // clear display + legends
 
  DisplayGoto(1,11);
  // see if the card is present and can be initialized:
  if (!SD.begin())
    mySerial.write("Fail");
  else
    mySerial.write("OK");
  delay(2000);
  
  // ***************** Clear display ********************
  DisplayClear();
   
  }

  
 
void loop() {
	
  File dataFile;
  //String dataString;
  //char dataToSD[40];
  //String timeString;
  char tempstring[10]; // create string arrays
  //float temp;
  static int WaitingForTemp = 0;
  //int readCount;
  DateTime now; // = rtc.now();
  static int nowriting = 0;
  
 delay(1000);
 
  switch(system_FSM) {
    case S_IDLE:
      //Read time from RTC
      now = rtc.now();
      
      if(now.minute() % 11 == 0)
        nowriting = 0;
      
      //Trigger state change
      if(WaitingForTemp == 1){
        if (millis() - lastTempRequest >= delayInMillis) // waited long enough??
		  system_FSM = S_READ_SENS;
	    }
      if(WaitingForTemp == 0) {
		system_FSM = S_TRIG_TEMP;
		if(now.minute() % 10 == 0 || nowriting == 0)
		  system_FSM = S_WRITE_SD;
		  nowriting = 1;
	    }
    break;
    case S_TRIG_TEMP:
      // Trig async reading of DS1820 temp sensor
      sensors.requestTemperatures(); 
      
      // remember time of trigger
      lastTempRequest = millis();
      WaitingForTemp = 1;
      system_FSM = S_IDLE; 
    break;
    case S_READ_SENS:
      //read value of sensors
      SensorData.sensor1 = sensors.getTempC(Sensor1);
      SensorData.sensor2 = sensors.getTempC(Sensor2);
      //SensorData.sensor1 = sensors.getTempC(Sensor1);
      //SensorData.sensor1 = sensors.getTempC(Sensor1);
      //SensorData.sensor1 = sensors.getTempC(Sensor1);
      WaitingForTemp = 0;

      system_FSM = S_IDLE;
/*
      if(readCount == 0)
        system_FSM = S_WRITE_SD;
        else
          system_FSM = S_IDLE;
      readCount++;
      */
    break;
    case S_WRITE_SD:
      // Read time and date from RTC
      TimeDate(rtc.now(),dateTimeString,1);

      // Open datafile
      dataFile = SD.open("templog.txt", FILE_WRITE);
      
      // Write values to SD card
      if (dataFile) {
        dataFile.print(dateTimeString);
        dataFile.print(";");
        dataFile.print(SensorData.sensor1);
        dataFile.print(";");
        dataFile.println(SensorData.sensor2);
        dataFile.close();
        }  
        
      // state change to IDLE
      system_FSM = S_IDLE;
    break;               
    default:
      system_FSM = S_IDLE;
  }
 
  switch(menu_FSM) {
    case M_PAGE1:
      if(menu_FSM !=menu_last_state) {
        DisplayClear();
        mySerial.write("Time"); // clear display + legends
        MenuShowTime = millis();
      }  
      now = rtc.now();
      DisplayGoto(1,0);
      mySerial.write("Time"); // clear display + legends

      DisplayGoto(1,8);
      TimeDate(now,dateTimeString,2);
      mySerial.print(dateTimeString);

      DisplayGoto(2,6);
      TimeDate(now,dateTimeString,3);
      mySerial.print(dateTimeString);

      //reserved for showing time and SD card status
      menu_last_state = M_PAGE1;

      if(millis() - MenuShowTime >= MenuDelayInMillis)
        menu_FSM = M_PAGE2;
      
      
      
    break;
    case M_PAGE2:
      if(menu_FSM !=menu_last_state) {
        DisplayClear();
        mySerial.write("Sens 1:        C"); 
        mySerial.write("Sens 2:        C"); 
        DisplayGoto(1,14);
        mySerial.write(223); 
        DisplayGoto(2,14);
        mySerial.write(223); 
        MenuShowTime = millis();
      }  

      DisplayGoto(1,10);
      mySerial.write(dtostrf(SensorData.sensor1,4,1,tempstring)); // write out the RPM value

      DisplayGoto(2,10);
      mySerial.write(dtostrf(SensorData.sensor2,4,1,tempstring)); // write out the TEMP value
      //mySerial.write(dtostrf(system_FSM,4,1,tempstring)); DEBUG
      
      menu_last_state = M_PAGE2;

      if(millis() - MenuShowTime >= MenuDelayInMillis)
        menu_FSM = M_PAGE3;
      
    break;
    case M_PAGE3:
      if(menu_FSM !=menu_last_state) {
		DisplayClear();
        mySerial.write("Sens 3:  N/A   C"); // clear display + legends
        mySerial.write("Sens 4:  N/A   C"); 
        DisplayGoto(1,14);
        mySerial.write(223); 
        DisplayGoto(2,14);
        mySerial.write(223);
        MenuShowTime = millis();
      }  

      DisplayGoto(1,7);
      
//    mySerial.write(dtostrf(SensorData.sensor1,4,1,tempstring)); // write out the RPM value

      DisplayGoto(2,7);

      //mySerial.write(dtostrf(MenuShowTime,4,1,tempstring)); // write out the TEMP value
      menu_last_state = M_PAGE3;

      if(millis() - MenuShowTime >= MenuDelayInMillis)
        menu_FSM = M_PAGE1;
      
    break;
    case M_PAGE4:
    break;
    default:
      menu_FSM = M_PAGE1;
  }
 }

void TimeDate(DateTime time,char *dateTimeString,int format)
{
  switch(format){
    case 1:
      sprintf(dateTimeString, "%02d/%02d/%04d %02d:%02d:%02d",
       time.day(), time.month(), time.year(),
       time.hour(), time.minute(), time.second());
    break;
    case 2:
      sprintf(dateTimeString, "%02d:%02d:%02d",
       time.hour(), time.minute(), time.second());
    break;
    case 3:
      sprintf(dateTimeString, "%02d/%02d/%04d",
       time.day(), time.month(), time.year());
    break;    
  }

}


void DisplayGoto(int line ,int pos)
{
  mySerial.write(0xFE); //command flag
  if(line==1)
    mySerial.write(128+pos);
  if(line==2)
    mySerial.write(128+64+pos);
}
void DisplayClear()
{
    //clears the screen, you will use this a lot!
  mySerial.write(0xFE);
  mySerial.write(0x01); 	
}
