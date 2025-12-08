#include <Arduino.h>
#include <M5StamPLC.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <screenHelper.cpp>
#include <typeHelpers/timeHM.h>
#include <chrono>
#include <DFRobot_GP8XXX.h>
#include "include/WiFiManager.h"
#include "include/WiFiLogger.h"
#include <LittleFS.h>
#include "include/FileLogger.h"
#include <WebServer.h>
using namespace std::chrono;

#define ONE_WIRE_BUS 4  //DS18B20 on Grove B corresponds to GPIO26 on ESP32
TaskHandle_t TaskCore0; //task for core 0 - low latency task

void TaskCore0Loop(void * pvParameters);
//parameters for the water
const float NORMAL_HOT_WATER_TEMPERATURE = 45;
const float DEFAULT_TEMPERATURE = 0;
const float BATH_WATER_TEMPERATURE = 60;
const float HEATER_TEMP_OFFSET = 10;
const float MAXIMUM_WATER_TEMP = 60;
const float HYSTERESIS_LOW = 4;
const float HYSTERESIS_HIGH = 1;
const int PRE_BATH_CIRC_TIME = 3;
const int BATH_TIME = 60;

const TimeHM CIRCULATION_TIME[] = {
    TimeHM(6, 0, 15),
    TimeHM(7, 30, 5),
    TimeHM(12, 30, 15),
    TimeHM(18, 00, 30)
}; //time in HH, MM, Duration

//for NTP time 
const long gmtOffset_sec = 3600;        // UTC+1
const int daylightOffset_sec = 3600;    // DST +1h in summer
const char* ntpServer = "pool.ntp.org";

DFRobot_GP8XXX_IIC GP8413(RESOLUTION_15_BIT, 0x59, &Wire); //DAC output
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature DS18B20(&oneWire);

// Set web server port number to 80
WebServer  server(80);
const char* logFilePath = "/log.txt";

//enumerator for screen choose
enum screen{
    MAIN_SCREEN,
    BATH_SCREEN,
    NORMAL_MODE_STARTED,
    STATUS_SCREEN_1,
    STATUS_SCREEN_2,
    STATUS_SCREEN_3
};
//enumerator for mode
enum mode{
    NORMAL = 0,
    CIRCULATION = 1,
    BATH = 2
    
};
/* relays and sensors: */
float temperature1; //temperature of water in tank [C]
float temperature2; //temperature of water coming from heater [C]
bool pumpHotWaterIsOn; // relay that turn on pump for heat water (it heat up water tank). 
bool pumpCirculationIsOn; //relay that turn on pump for hot water circulation( it circulate water in entire house). NO contact
bool floorHeatingIsOff; //relay that swith off floor heating. NC contact
bool hotWaterExternalVoltageIsOn; //relay that turn on external voltage for controling heater

screen actualScreen = MAIN_SCREEN; //current displayed screen
screen previousModeScreen = MAIN_SCREEN; //previous displayed screen in mode change
mode actualMode = NORMAL; //mode of operation
mode previousMode = NORMAL; //previous mode of operation
int bathStep = 0; //step in bath mode
String monitorstring = "";  //string for serial monitor output
String bathStepString = "";//string for bath step description
unsigned long secondsFromButtonPressed; //number of secconds that passed from last pressing button
void controlLogic(); // main method for controling relays
void setTemperature(float temp); //helper function for setting temperature output
void timeHelper(); // helper function for time handling
String modeToString(mode actualMode);//helper function to write mode to string
bool risingEdge100ms, risingEdge500ms, risingEdge1s; //single signal that is high for 1 cycle cyclically
bool triggerCirculation, hotWaterHeating, time1TimeSynchronised, dacInitFail;
int memoCirculationDuration, sensorsCount;
bool screenChanged, Shown = false;

float heaterSetTemperature, hotWaterTankSetTemperature, temperatureToSet, lastTemperatureSet;
time_point<steady_clock> helperTime100ms, helperTime500ms, helperTime1s, buttonPressedTime, circulationStartTime, bathTempStartTime, lastRelayChangeTime;//helper variables
time_point<steady_clock> debugTimerMemo, debugTimerNow;


// ----- WEB SERVER HANDLERS -----
void handleRoot() {
    server.send(200, "text/html", FileLogger::getLogHTML());
}

void handleAddLog() {
    if(server.hasArg("msg")) {
        String msg = server.arg("msg");
        FileLogger::addLog(msg);
        server.send(200, "text/plain", "Log added");
    } else {
        server.send(400, "text/plain", "Missing 'msg' argument");
    }
}

void setup()
{   
     Serial.begin(9600);
    
    // Init M5StamPLC
    M5StamPLC.begin();
   
    
    // Initialize LittleFS
    if(!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return;
    }

    //start wifi
    MyDevice::setupWiFi();
    WiFiLogger::begin(23);  // default port 23
    server.begin();


    // Setup web server
    server.on("/", handleRoot);
    server.on("/add", handleAddLog);
    server.begin();
    Serial.println("HTTP server started");

    //init time helpers
    helperTime100ms = steady_clock::now();
    helperTime500ms = steady_clock::now();
    helperTime1s = steady_clock::now();

    heaterSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
   
       //start IE2C for DAC
    Wire.end();
    bool i2cStartOK = Wire.begin(2,1); //SDA, SCL, frequency
    WiFiLogger::println("I2C init status: " + String(i2cStartOK));
    FileLogger::addLog("I2C init status: " + String(i2cStartOK));
    if (GP8413.begin() != 0) {
        WiFiLogger::println("Init of DAC2 Fail!");
         FileLogger::addLog("Init of DAC2 Fail!");
        dacInitFail = true;
        delay(1000);
    }
    DS18B20.begin();
    sensorsCount = DS18B20.getDS18Count();  //check for number of connected sensors
       //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
    xTaskCreatePinnedToCore(
                    TaskCore0Loop, /* Function to implement the task */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    0,           /* priority of the task */
                    &TaskCore0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */  

}


//Buttons are running on separate task for better user experience
void TaskCore0Loop(void * pvParameters) {
    for(;;){
        //update devices 
        M5.update();
        // Button handling
        if (M5.BtnA.wasSingleClicked()) {                
            actualScreen = MAIN_SCREEN; //Bath screen
            screenChanged = true;
            WiFiLogger::println("Button A pressed");
            FileLogger::addLog("Button A pressed");
            buttonPressedTime = steady_clock::now();
           /* if(actualMode != BATH){
                actualMode = BATH;         
                WiFiLogger::println("Mode set by button to " + String(actualMode));
                bathStep = 0; 
            }    */   
        } else if (M5.BtnB.wasSingleClicked()) {
            actualMode = NORMAL;
            bathStep = 0;
            actualScreen = NORMAL_MODE_STARTED;
            screenChanged = true;
            WiFiLogger::println("Mode set by button to " + String(actualMode));
            WiFiLogger::println("Button B pressed");
            FileLogger::addLog("Button B pressed");
            buttonPressedTime = steady_clock::now();
        } else if (M5.BtnC.wasSingleClicked()) {
            if (actualScreen < STATUS_SCREEN_1 )
            {
                actualScreen = STATUS_SCREEN_1;
            } else if (actualScreen < STATUS_SCREEN_2)
            {
                actualScreen = STATUS_SCREEN_2;
            } else if (actualScreen >= STATUS_SCREEN_2)
            {
                actualScreen = MAIN_SCREEN;
            }
            
            WiFiLogger::println("Button C pressed");     
            FileLogger::addLog("Button C pressed");
            buttonPressedTime = steady_clock::now();    
        }
         vTaskDelay(pdMS_TO_TICKS(10)); // give scheduler breathing room
  } 
}


void loop()
{           
    timeHelper();
    MyDevice::handleWiFi();
    WiFiLogger::handle();
   
    //set time from NTP - 1 time
    if (!time1TimeSynchronised && WiFi.status() == WL_CONNECTED) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        time1TimeSynchronised = true;
        WiFiLogger::println("Time setup from NTP done");
    }

     server.handleClient();

    //read status of relays 
    pumpHotWaterIsOn =  M5StamPLC.readPlcRelay(0);
    pumpCirculationIsOn =  M5StamPLC.readPlcRelay(1);
    floorHeatingIsOff = M5StamPLC.readPlcRelay(2);
    hotWaterExternalVoltageIsOn = M5StamPLC.readPlcRelay(3);

    //read temperature
    if (sensorsCount >= 2 && risingEdge1s) {
        DS18B20.requestTemperatures();
        float reading1 = DS18B20.getTempCByIndex(0);
        float reading2 = DS18B20.getTempCByIndex(1);
        temperature1 = (reading1 > 0) ? reading1 : temperature1; //limit false reading
        temperature2 = (reading2 > 0) ? reading2 : temperature2;
       // monitorstring = "Sensor0: " + String(temperature1,1) + ", Sensor1: " + String(temperature2,1); 
    }
    else if  (sensorsCount < 2 && risingEdge1s)   {
        monitorstring = "Problem, number of found sensors: " + String(sensorsCount);
        FileLogger::addLog("DS18B20 sensors problem - not found");
    }
  
    
    //actual logic
    controlLogic();

    //display screen
    switch (actualScreen)
    {
    case MAIN_SCREEN:
        if(screenChanged) {
            screenChanged = false;
             showMenu();
        }
       
        break;
    case BATH_SCREEN:  
        if(screenChanged || risingEdge1s) {
            screenChanged = false;
             showHeatingWithText(heaterSetTemperature, temperature1, bathStepString);  
        }       
        
        if (actualMode == NORMAL) {
            actualScreen = MAIN_SCREEN;
            WiFiLogger::println("Back to main screen"); 
        };
        break;
    case NORMAL_MODE_STARTED:
        if(screenChanged || risingEdge1s) {
            screenChanged = false;
             showNormalModeStarted();
        }
        
        if (secondsFromButtonPressed > 5) {
            actualScreen = MAIN_SCREEN;
        };
        break;
    case STATUS_SCREEN_1:
        if(screenChanged || risingEdge1s) {
            screenChanged = false;
             showStatus1(heaterSetTemperature, temperature1, temperature2, pumpHotWaterIsOn, pumpCirculationIsOn, floorHeatingIsOff, hotWaterExternalVoltageIsOn);
        }               
        break;
    case STATUS_SCREEN_2:
        if(screenChanged || risingEdge1s) {
            screenChanged = false;
             showStatus2(modeToString(actualMode), bathStep, dacInitFail, (WiFi.status() == WL_CONNECTED));
        }                
        break;
    default:
        break;
    }
    
    //Write relays
    M5StamPLC.writePlcRelay(0, pumpHotWaterIsOn);
    M5StamPLC.writePlcRelay(1, pumpCirculationIsOn); 
    M5StamPLC.writePlcRelay(2, floorHeatingIsOff); 
    M5StamPLC.writePlcRelay(3, hotWaterExternalVoltageIsOn); 
       
    if(previousMode != actualMode){
        WiFiLogger::println("Mode changed from " + String(previousMode) + " to " + String(actualMode));         
        previousMode = actualMode;
    }
    if(previousModeScreen != actualScreen){
        screenChanged = true;
         WiFiLogger::println("Screen changed from " + String(previousModeScreen) + " to " + String(actualScreen));
         FileLogger::addLog("Screen changed from " + String(previousModeScreen) + " to " + String(actualScreen));
        previousModeScreen = actualScreen;
    }

    
    vTaskDelay(pdMS_TO_TICKS(10)); // give scheduler breathing room
} 

/* END OF LOOP*/


//control logic for relays and temperature
void controlLogic(){
     auto now = steady_clock::now();

     //time for hours minute comparison
    time_t actualTime;
    time(&actualTime);
    tm* t = localtime(&actualTime);
    int actualHour = t->tm_hour;
    int actualMinute = t->tm_min;

    //hot water tank logic is independent. temperature1 - tank temperature
    if ((temperature1 < hotWaterTankSetTemperature - HYSTERESIS_LOW) && !hotWaterHeating) {
         hotWaterHeating = true;
         heaterSetTemperature = hotWaterTankSetTemperature;  
        hotWaterExternalVoltageIsOn = true;
        setTemperature(heaterSetTemperature);
        FileLogger::addLog("Hot water logic set heating ON.");
        FileLogger::addLog("hotWaterExternalVoltageIsOn set to 1");
        floorHeatingIsOff = true;
        FileLogger::addLog("floorHeatingIsOff set to 1");
    } else if ((temperature1 > hotWaterTankSetTemperature + HYSTERESIS_HIGH) && hotWaterHeating){
        hotWaterHeating = false;
        heaterSetTemperature = DEFAULT_TEMPERATURE;
        hotWaterExternalVoltageIsOn = false;
        setTemperature(heaterSetTemperature);
        FileLogger::addLog("Hot water logic set heating OFF.");
        FileLogger::addLog("hotWaterExternalVoltageIsOn set to 0");
        floorHeatingIsOff = false;
        FileLogger::addLog("floorHeatingIsOff set to 0");
    }    
    

    //control pumpHotWaterIsOn is independent. temperature 2 - water clutch temperature
    if(hotWaterHeating && temperature2 > hotWaterTankSetTemperature && !pumpHotWaterIsOn){ 
        pumpHotWaterIsOn = true;
        WiFiLogger::println("Started pumpHotWaterIsOn");  
        FileLogger::addLog("pumpHotWaterIsOn set to 1");
    } else if ((!hotWaterHeating || temperature2 < (hotWaterTankSetTemperature - HYSTERESIS_LOW)) && pumpHotWaterIsOn) {
        pumpHotWaterIsOn = false;
        WiFiLogger::println("Stopped pumpHotWaterIsOn"); 
        FileLogger::addLog("pumpHotWaterIsOn set to 0"); 
    }

    //main logic
    switch (actualMode)
    {
    case NORMAL: 
        
         hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;

       //activate circulation at given time
        for(TimeHM circulation : CIRCULATION_TIME){
            if ((circulation == TimeHM(actualHour, actualMinute)) && actualMode != CIRCULATION) {
                memoCirculationDuration = circulation.duration;
                actualMode = CIRCULATION;
                WiFiLogger::println("Mode set by circulation start to " + String(actualMode));
                triggerCirculation = true;
                circulation.print();
                WiFiLogger::println("Circulation started");  
                WiFiLogger::println("ActualHour:" + String(actualHour));  
                WiFiLogger::println("actualMinute: " + String(actualMinute)); 
                FileLogger::addLog("Circulation started by timer"); 
                FileLogger::addLog("ActualHour:" + String(actualHour) + "actualMinute: " + String(actualMinute));  
                FileLogger::addLog("CirculationHour:" + String(circulation.hour) + "CirculationMinute: " + String(circulation.minute)+ "CirculationDuration: " + String(circulation.duration)); 
                
            }
        }        
        break;

    case CIRCULATION: 

        hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;

        //start only if water is heated
        if ((temperature1 >= NORMAL_HOT_WATER_TEMPERATURE - HYSTERESIS_LOW)  && triggerCirculation) {
            pumpCirculationIsOn = true;
            circulationStartTime = steady_clock::now();
            triggerCirculation = false;
             FileLogger::addLog("pumpCirculationIsOn set to 1"); 
        }
        //end circulation after given time
        if(duration_cast<minutes>(now - circulationStartTime).count() >= memoCirculationDuration) {
            pumpCirculationIsOn = false;
            actualMode = NORMAL;

            WiFiLogger::println("Mode set by circulation end to " + String(actualMode));
            WiFiLogger::println("Circulation ended");
            FileLogger::addLog("Circulation ended");
            FileLogger::addLog("pumpCirculationIsOn set to 0"); 
        }
        
        break;
        /*
    case BATH:
         //start pre bath circulation 
        switch (bathStep)
        {
            //set normal temperature for pre bath circulation
        case 0:
            hotWaterTankSetTemperature = BATH_WATER_TEMPERATURE;
            WiFiLogger::println("Bath mode started. BathStep " + String(bathStep));            
            bathStepString = " Zadano temp kapieli.\n";
            bathStep ++;
            break; 
        case 1: //turn on circulation when temperature is reached
            if((temperature1 > BATH_WATER_TEMPERATURE - HYSTERESIS) && (temperature2 > BATH_WATER_TEMPERATURE- HYSTERESIS)){
                pumpCirculationIsOn = true;
                circulationStartTime = steady_clock::now();
                bathStepString =bathStepString + " Cyrk. w toku...\n";
                WiFiLogger::println("BathStep " + String(bathStep)); 
                bathStep ++;
            }
            break;
                      
        case 2: //wait for pre bath circulation done 
            if(duration_cast<minutes>(now - circulationStartTime).count() >= PRE_BATH_CIRC_TIME) {
                pumpCirculationIsOn = false;
                bathStepString = bathStepString + " Cyrk. zakonczona.\n";
                WiFiLogger::println("BathStep " + String(bathStep)); 
                bathStep ++;
            }
            break;

        case 3: //start time      
            bathTempStartTime = steady_clock::now();
            bathStepString = bathStepString + " Start kapieli. \n";
            WiFiLogger::println("BathStep " + String(bathStep)); 
            bathStep ++;
            break;

        case 4: //wait for the bath end
             if(duration_cast<minutes>(now - bathTempStartTime).count() >= BATH_TIME) {
                hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
                actualMode = NORMAL;
                bathStepString = bathStepString + " Tryb kąpieli zakończony.\n";
                WiFiLogger::println("Bath mode ended");
            }
            break; 
        default:
            break;
        }
        break; 
    */
    default:
        break;
    }    

};


// helper function for time handling
void timeHelper() {
    auto now = steady_clock::now();
    //100ms
    if (duration_cast<milliseconds>(now - helperTime100ms).count() >= 100) {
        helperTime100ms = steady_clock::now();
        risingEdge100ms = true;
    } else {
        risingEdge100ms = false;
    }
    //500ms
    if (duration_cast<milliseconds>(now - helperTime500ms).count() >= 500) {
        helperTime500ms = steady_clock::now();
        risingEdge500ms = true;
    } else {
        risingEdge500ms = false;
    }
    //1s
    if (duration_cast<milliseconds>(now - helperTime1s).count() >= 1000) {
        helperTime1s = steady_clock::now();
        risingEdge1s = true;
    } else {
        risingEdge1s = false;
    }

    //time from button press
    secondsFromButtonPressed = duration_cast<seconds>(now - buttonPressedTime).count() ;
}

//set DAC from temperature.
//100C = 10.0 V = 32767.0f
// First set to 0, then to desired
void setTemperature(float temp) {
    //check if temperature is different from last
    if (abs(temp - lastTemperatureSet)> 0.1 ){
        lastTemperatureSet = temp;
        uint16_t setting_vol = 0;
        if (temp > MAXIMUM_WATER_TEMP) {
            temp = MAXIMUM_WATER_TEMP;
        }
        setting_vol = (int16_t)((temp + HEATER_TEMP_OFFSET) / 100.0f * 32767.0f);

        //limit output to 80C (8V)
        if (setting_vol > 32767.0f * 0.8) {
            setting_vol = 32767.0f * 0.8;
        }
        GP8413.setDACOutVoltage(setting_vol, 0);  
        String message = "temperature was set to: " + String(temp) + ", + offset: " + String(HEATER_TEMP_OFFSET);
        WiFiLogger::println(message);   
    }
    
}
//Write mode in string way for display
String modeToString(mode actualMode){
    
    switch (actualMode)
        {
         case NORMAL: 
             return  "Normalny";
             break;
        case   BATH :
             return  "Kąpiel";
             break;
        case  CIRCULATION :
             return  "Cyrkulacja";
             break;
        default:
            return "";
            break;
        }
}