#include <Arduino.h>
#include <M5StamPLC.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <screenHelper.cpp>
#include <typeHelpers/timeHM.h>
#include <chrono>
#include <DFRobot_GP8XXX.h>

using namespace std::chrono;

#define ONE_WIRE_BUS 4  //DS18B20 on Grove B corresponds to GPIO26 on ESP32
 
//parameters for the water
const float NORMAL_HOT_WATER_TEMPERATURE = 25;
const float FLOOR_HEATING_TEMPERATURE = 35;
const float BATH_WATER_TEMPERATURE = 27;
const float MAXIMUM_WATER_TEMP = 80;
const float HYSTERESIS = 4;
const int PRE_BATH_CIRC_TIME = 3;
const int BATH_TIME = 60;

const TimeHM CIRCULATION_TIME[] = {
    TimeHM(5, 30, 15),
    TimeHM(6, 45, 15),
    TimeHM(12, 0, 15),
    TimeHM(18, 00, 30)
}; //time in HH, MM, Duration


DFRobot_GP8XXX_IIC GP8413(RESOLUTION_15_BIT, 0x59, &Wire); //DAC output
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature DS18B20(&oneWire);


//enumerator for scren choose
enum screen{
    MAIN_SCREEN,
    BATH_MODE_STARTED,
    NORMAL_MODE_STARTED,
    SENSORS_SCREEN
};
//enumerator for mode
enum mode{
    NORMAL = 0,
    BATH = 1,
    CIRCULATION = 2
    
};
/* relays and sensors: */
float temperature1; //temperature of water in tank [C]
float temperature2; //temperature of water coming from heater [C]
bool pumpHotWaterIsOn; // relay that turn on pump for heat water (it heat up water tank). NO contact
bool pumpCirculationIsOn; //relay that turn on pump for hot water circulation( it circulate water in entire house). NO contact
bool powerOffFloorHeatingPump; //relay that cut down power on pumps for floor heating. NC contact

screen actualScreen = MAIN_SCREEN; //current displayed screen
mode acualMode; //mode of operation
mode previousMode; //previous mode of operation
int bathStep = 0; //step in bath mode
String monitorstring = "";  //string for serial monitor output
String bathStepString = "";//string for bath step description
unsigned long secondsFromButtonPressed; //numbe of secconds that passed from last pressing button
void contolLogic(); // main method for controling relays
void setTemperature(float temp); //helper function for setting temperature output
void timeHelper(); // helper function for time handling
bool risingEdge100ms, risingEdge500ms, risingEdge1s; //single signal that is high for 1 cycle cyclically
bool triggerCirculation, hotWaterHeatingActive;
int memoCirculationDuration;

float heaterSetTemperature, hotWaterTankSetTemperature, lastTemperatureSet;
time_point<steady_clock> helperTime100ms, helperTime500ms, helperTime1s, buttonPressedTime, circtulationStartTime, bathTempStartTime, lastRelayChangeTime;//helper variables


void setup()
{
    // Init M5StamPLC
    M5StamPLC.begin();
    Serial.begin(9600);

    //init time helpers
    helperTime100ms = steady_clock::now();
    helperTime500ms = steady_clock::now();
    helperTime1s = steady_clock::now();

    heaterSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;

    //start IE2C for DAC
    Wire.end();
    Wire.begin(2,1); //SDA, SCL, frequency

    if (GP8413.begin() != 0) {
        Serial.println("Init of DAC2 Fail!");
        delay(1000);
    }
}

void loop()
{ 
    monitorstring = "Date;Time";  // change to realtime date and time of M5Stack (later)

    M5.update();
    DS18B20.begin();
    int count = DS18B20.getDS18Count();  //check for number of connected sensors
    timeHelper();

    //read status of relays 
    pumpHotWaterIsOn =  M5StamPLC.readPlcRelay(0);
    pumpCirculationIsOn =  M5StamPLC.readPlcRelay(1);
    powerOffFloorHeatingPump =  M5StamPLC.readPlcRelay(2);

    //read temperature
    if (count >= 2 && risingEdge1s) {
        DS18B20.requestTemperatures();
        temperature1 = DS18B20.getTempCByIndex(0);
        temperature2 = DS18B20.getTempCByIndex(1);
       // monitorstring = "Sensor0: " + String(temperature1,1) + ", Sensor1: " + String(temperature2,1); 
    }
    else if  (count < 2 && risingEdge1s)   {
        monitorstring = "Problem, number of found sensors: " + String(count);
    }

   // Button handling
    if (M5.BtnA.isPressed()) {         
        actualScreen = BATH_MODE_STARTED;
        Serial.println("Button A pressed");
        buttonPressedTime = steady_clock::now();
        if(acualMode != BATH){
          acualMode = BATH;
          Serial.println("Mode set by button to " + String(acualMode));
          bathStep = 0;
        }       
    } else if (M5.BtnB.isPressed()) {
        acualMode = NORMAL;
        actualScreen = NORMAL_MODE_STARTED;
        Serial.println("Mode set by button to " + String(acualMode));
        Serial.println("Button B pressed");
        buttonPressedTime = steady_clock::now();
    } else if (M5.BtnC.isPressed()) {
        actualScreen = SENSORS_SCREEN;  
        Serial.println("Button C pressed");     
        buttonPressedTime = steady_clock::now();    
    }
    
    //actual logic
    contolLogic();

    //display screen
    switch (actualScreen)
    {
    case MAIN_SCREEN:
        showMenu(risingEdge100ms);
        break;
    case BATH_MODE_STARTED:         
        showHeatingWithText(risingEdge100ms, heaterSetTemperature, temperature1, bathStepString);  
        if (acualMode == NORMAL) {
            actualScreen = MAIN_SCREEN;
            Serial.println("Back to main screen"); 
        };
        break;
    case NORMAL_MODE_STARTED:
        showNormalModeStarted(risingEdge100ms);
        if (secondsFromButtonPressed > 5) {
            actualScreen = MAIN_SCREEN;
        };
        break;
    case SENSORS_SCREEN:
        showSensorStatus(heaterSetTemperature, temperature1, temperature2, pumpHotWaterIsOn, pumpCirculationIsOn, powerOffFloorHeatingPump, risingEdge100ms, static_cast<int>(acualMode), bathStep);
        if (secondsFromButtonPressed > 10) {
            actualScreen =  (acualMode == NORMAL) ? MAIN_SCREEN : BATH_MODE_STARTED;
        };
        break;
    default:
        break;
    }
    
    //Write relays
    M5StamPLC.writePlcRelay(0, pumpHotWaterIsOn);
    M5StamPLC.writePlcRelay(1, pumpCirculationIsOn);
    M5StamPLC.writePlcRelay(2, powerOffFloorHeatingPump);    
       
    if(previousMode != acualMode){
        Serial.println("Mode changed from " + String(previousMode) + " to " + String(acualMode));
        previousMode = acualMode;
    }
} 

/* END OF LOOP*/


//control logic for relays and temperature
void contolLogic(){
     auto now = steady_clock::now();

     //time for hours minute comparison
    time_t actualTime = time(nullptr);
    tm* t = localtime(&actualTime);
    int actualHour = t->tm_hour;
    int actualMinute = t->tm_min;

    //hot water tank logic should be independent
    if (temperature1 < hotWaterTankSetTemperature - HYSTERESIS) {
         hotWaterHeatingActive = true;
    } else if (temperature1 > hotWaterTankSetTemperature + HYSTERESIS){
        hotWaterHeatingActive = false;
    }

    //main logic
    switch (acualMode)
    {
    case NORMAL: 
        
         hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
         
        //set normal temperature temperature
        if (hotWaterHeatingActive) {
            heaterSetTemperature = hotWaterTankSetTemperature;           
            setTemperature(heaterSetTemperature);
        } else {
            heaterSetTemperature = FLOOR_HEATING_TEMPERATURE;
            setTemperature(heaterSetTemperature);
        }       

       //activate circulation at given time
        for(TimeHM circulation : CIRCULATION_TIME){
            if (circulation == (actualHour, actualMinute)){
                memoCirculationDuration = circulation.duration;
                acualMode = CIRCULATION;
                Serial.println("Mode set by circulation start to " + String(BATH));
                triggerCirculation = true;
                Serial.println("Circulation started");  
                circulation.print();
            }
        }        
        break;

    case CIRCULATION: 
        //start only if water is heated
        if ((temperature1 > NORMAL_HOT_WATER_TEMPERATURE - HYSTERESIS) && (temperature2 > NORMAL_HOT_WATER_TEMPERATURE- HYSTERESIS) && triggerCirculation) {
            pumpCirculationIsOn = true;
            circtulationStartTime = steady_clock::now();
            triggerCirculation = false;
        }
        //end circulation after given time
        if(duration_cast<minutes>(now - circtulationStartTime).count() >= memoCirculationDuration) {
            pumpCirculationIsOn = false;
            acualMode = NORMAL;
            Serial.println("Mode set by circulation end to " + String(BATH));
            Serial.println("Circulation ended");
        }
        heaterSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
        hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
        setTemperature(heaterSetTemperature); 
        break;

    case BATH:
         //start pre bath circulation 
        switch (bathStep)
        {
            //set normal temperature for pre bath circulation
        case 0:
            heaterSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
            hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
            setTemperature(heaterSetTemperature);
            Serial.println("Bath mode started. BathStep " + String(bathStep));            
            bathStepString = " Zadano temp cyrk.\n";
            bathStep ++;
            break; 
        case 1: //turn on circulation when temperature is reached
            if((temperature1 > NORMAL_HOT_WATER_TEMPERATURE - HYSTERESIS) && (temperature2 > NORMAL_HOT_WATER_TEMPERATURE- HYSTERESIS)){
                pumpCirculationIsOn = true;
                circtulationStartTime = steady_clock::now();
                bathStepString =bathStepString + " Cyrk. w toku...\n";
                Serial.println("BathStep " + String(bathStep)); 
                bathStep ++;
            }
            break;
                      
        case 2: //wait for pre bath circulation done 
            if(duration_cast<minutes>(now - circtulationStartTime).count() >= PRE_BATH_CIRC_TIME) {
                pumpCirculationIsOn = false;
                bathStepString = bathStepString + " Cyrk. zakonczona.\n";
                Serial.println("BathStep " + String(bathStep)); 
                bathStep ++;
            }
            break;

        case 3: //set bath temperature and start time            
            heaterSetTemperature = BATH_WATER_TEMPERATURE;
            hotWaterTankSetTemperature = BATH_WATER_TEMPERATURE;
            setTemperature(heaterSetTemperature);
            bathTempStartTime = steady_clock::now();
            bathStepString = bathStepString + " Zadano T= " + String(heaterSetTemperature) + " C\n";
            Serial.println("BathStep " + String(bathStep)); 
            bathStep ++;
            break;

        case 4: //wait for the bath end
             if(duration_cast<minutes>(now - bathTempStartTime).count() >= BATH_TIME) {
                heaterSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
                hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
                setTemperature(heaterSetTemperature);
                acualMode = NORMAL;
                bathStepString = bathStepString + " Tryb kąpieli zakończony.\n";
                Serial.println("Bath mode ended");
            }
            break; 
        default:
            break;
        }
        break;
    default:
        break;
    }    

    //control pumpHotWaterIsOn separately from rest of logic
    if(hotWaterHeatingActive && temperature2 > hotWaterTankSetTemperature && !pumpHotWaterIsOn){ 
        pumpHotWaterIsOn = true;
        Serial.println("Started pumpHotWaterIsOn");  
    } else if ((!hotWaterHeatingActive || temperature2 < hotWaterTankSetTemperature - HYSTERESIS) && pumpHotWaterIsOn) {
        pumpHotWaterIsOn = false;
        Serial.println("Stopped pumpHotWaterIsOn");  
    }

    //logic for cutting power to heating pump:
    powerOffFloorHeatingPump = pumpHotWaterIsOn;
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
void setTemperature(float temp) {
    //check if temperature is different from last
    if (abs(temp - lastTemperatureSet)> 0.1 ){
        lastTemperatureSet = temp;
        uint16_t setting_vol = 0;
        if (temp > MAXIMUM_WATER_TEMP) {
            temp = MAXIMUM_WATER_TEMP;
        }
        setting_vol = (int16_t)(temp / 100.0f * 32767.0f);

        //limit output to 80C (8V)
        if (setting_vol > 32767.0f * 0.8) {
            setting_vol = 32767.0f * 0.8;
        }
        GP8413.setDACOutVoltage(setting_vol, 0);  

        Serial.println("temperature was set to:  " + String(temp));   
    }
    
}