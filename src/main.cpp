/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
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


 //structure for time of day
 struct hourMinute
 {
   int hour;
   int minute;
 };
 
//parameters for the water
const float NORMAL_HOT_WATER_TEMPERATURE = 50;
const float FLOOR_HEATING_TEMPERATURE = 35;
const float BATH_WATER_TEMPERATURE = 65;
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

/*  Resolution of DS18B20 temperatur measurement = power-on-default = 12 bit = 0.0625 C
   *9-bit resolution --> 93.75 ms
   *10-bit resolution --> 187.5 ms
   *11-bit resolution --> 375 ms
   *12-bit resolution --> 750 ms
*/ 


String monitorstring = "";  
//enumerator for scren choose
enum screen{
    MAIN_SCREEN,
    OK_SCREEN,
    STOP_SCREEN,
    SENSORS_SCREEN
};
//enumerator for mode
enum mode{
    NORMAL,
    CIRCULATION,
    BATH
};
/* relays and sensors: */
float temperature1; //temperature of water in tank [C]
float temperature2; //temperature of water coming from heater [C]
bool pumpHotWaterIsOn; // relay that turn on pump for heat water (it heat up water tank). NO contact
bool pumpCirculationIsOn; //relay that turn on pump for hot water circulation( it circulate water in entire house). NO contact
bool powerOffFloorHeatingPump; //relay that cut down power on pumps for floor heating. NC contact

screen actualScreen = MAIN_SCREEN;
mode acualMode = NORMAL;
unsigned long secondsFromButtonPressed; //numbe of secconds that passed from last pressing button
void contolLogic(); // main method for controling relays
void setTemperature(float temp); //helper function for setting temperature output
void timeHelper(); // helper function for time handling
bool risingEdge100ms, risingEdge500ms, risingEdge1s; //single signal that is high for 1 cycle cyclically
bool pumpHotWaterIsOn, pumpCirculationIsOn, powerOffFloorHeatingPump, triggerCirculation, hotWaterHeatingActive, triggerBath, preBathCirculationDone, preBathCirculationTriggered, beforeBathCirculationDone;
int memoCirculationDuration;

float heaterSetTemperature, hotWaterTankSetTemperature, lastTemperatureSet;
time_point<steady_clock> helperTime100ms, helperTime500ms, helperTime1s, buttonPressedTime, circtulationStartTime, bathTempStartTime, lastRelayChangeTime;//helper variables



void setup()
{
    /* Init M5StamPLC */
    M5StamPLC.begin();
    Serial.begin(9600);

    //init time helpers
    helperTime100ms = steady_clock::now();
    helperTime500ms = steady_clock::now();
    helperTime1s = steady_clock::now();

    heaterSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
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
        monitorstring = "Sensor0: " + String(temperature1,1) + ", Sensor1: " + String(temperature2,1); 
    }
    else {
        monitorstring = "Problem, number of found sensors: " + String(count);
    }
    Serial.println(monitorstring);  // 1 line for all measurements on serial monitor

   /* Check if button was clicked */
    if (M5.BtnA.wasClicked()) {
        acualMode = BATH;
        bathStep = 0;
        actualScreen = OK_SCREEN;
        Serial.println("Button A pressed");
        buttonPressedTime = steady_clock::now();
    } else if (M5.BtnB.wasClicked()) {
        acualMode = NORMAL;
        actualScreen = STOP_SCREEN;
        Serial.println("Button B pressed");
        buttonPressedTime = steady_clock::now();
    } else if (M5.BtnC.wasClicked()) {
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
    case OK_SCREEN:
         showHeatingStarted(risingEdge100ms);
        if (secondsFromButtonPressed > 5) {
            actualScreen = MAIN_SCREEN;
        };
        break;
    case STOP_SCREEN:
        showHeatingStopped(risingEdge100ms);
        if (secondsFromButtonPressed > 5) {
            actualScreen = MAIN_SCREEN;
        };
        break;
    case SENSORS_SCREEN:
        showSensorStatus(temperature1, temperature2, pumpHotWaterIsOn, pumpCirculationIsOn, powerOffFloorHeatingPump, risingEdge100ms);
        if (secondsFromButtonPressed > 5) {
            actualScreen = MAIN_SCREEN;
        };
        break;
    default:
        break;
    }
    
    //Write relays
    M5StamPLC.writePlcRelay(0, pumpHotWaterIsOn);
    M5StamPLC.writePlcRelay(1, pumpCirculationIsOn);
    M5StamPLC.writePlcRelay(2, powerOffFloorHeatingPump);    
    
    //update status on serial
    Serial.println("Actual Screen:" + String(actualScreen));  
    if (secondsFromButtonPressed > 0) {
         Serial.println("Seconds from button press:" + String(secondsFromButtonPressed));  
    }
    Serial.println("Actual mode: " + String(acualMode)); 
   
    delay(500); //Measuremnt interval

} 

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
    } elsif (temperature1 > hotWaterTankSetTemperature + HYSTERESIS){
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
                triggerCirculation = true;
                Serial.println("Circulation started");
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
            Serial.println("Bath mode started");
            bathStep ++;
            break;
        case 1: //turn on circulation when temperature is reached
            if((temperature1 > NORMAL_HOT_WATER_TEMPERATURE - HYSTERESIS) && (temperature2 > NORMAL_HOT_WATER_TEMPERATURE- HYSTERESIS)){
                umpCirculationIsOn = true;
                circtulationStartTime = steady_clock::now();
                bathStep ++;
            }
            break;
                      
        case 2: //wait for pre bath circulation done 
            if(duration_cast<minutes>(now - circtulationStartTime).count() >= PRE_BATH_CIRC_TIME) {
                pumpCirculationIsOn = false;
                bathStep ++;
            }
            break;

        case 3: //set bath temperature and start time            
            heaterSetTemperature = BATH_WATER_TEMPERATURE;
            hotWaterTankSetTemperature = BATH_WATER_TEMPERATURE;
            setTemperature(heaterSetTemperature);
            bathTempStartTime = steady_clock::now();
            bathStep ++;
            break;

        case 4: //wait for the bath end
             if(duration_cast<minutes>(now - bathTempStartTime).count() >= BATH_TIME) {
                heaterSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
                hotWaterTankSetTemperature = NORMAL_HOT_WATER_TEMPERATURE;
                setTemperature(heaterSetTemperature);
                acualMode = NORMAL;
                Serial.println("Bath mode ended");
            }
            break;
        default:
            break;
        }
        //turn on circulation valve only when temperature is ok
        if (preBathCirculationTriggered && !preBathCirculationDone &&
            (temperature1 > NORMAL_HOT_WATER_TEMPERATURE - HYSTERESIS) && (temperature2 > NORMAL_HOT_WATER_TEMPERATURE- HYSTERESIS)) {
            pumpCirculationIsOn = true;
            circtulationStartTime = steady_clock::now();
            triggerCirculation = false;
        })
        //end circulation after given time
        if(preBathCirculationTriggered && duration_cast<minutes>(now - circtulationStartTime).count() >= 15) {
            
        }
            pumpCirculationIsOn = false;
            acualMode = NORMAL;

        heaterSetTemperature = BATH_WATER_TEMPERATURE;
        hotWaterTankSetTemperature = BATH_WATER_TEMPERATURE;
        setTemperature(heaterSetTemperature);
        break;
    default:
        break;
    }    

    //control pumpHotWaterIsOn separately from other
    //


    if(hotWaterHeatingActive && temperature2 > hotWaterTankSetTemperature) {
        pumpHotWaterIsOn = true;
        Serial.println("Started pumpHotWaterIsOn");  
    } elsif (!hotWaterHeatingActive || temperature2 < hotWaterTankSetTemperature - HYSTERESIS){
        pumpHotWaterIsOn = false;
        Serial.println("Stopped pumpHotWaterIsOn");  
    }

    //logic for cutting power to heating pump:
    powerOffFloorHeatingPump = pumpHotWaterIsOn;
};


// helper function for time handling
void timeHelper() {
 /* Elapsed time handling*/
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

        Serial.println("temperature was set to:  %0.2f " + String(temp));   
    }
    
}