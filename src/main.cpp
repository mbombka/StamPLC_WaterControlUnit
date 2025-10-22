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

 #define ONE_WIRE_BUS 4  //DS18B20 on Grove B corresponds to GPIO26 on ESP32

//parameters for the water
const float MAXIMUM_WATER_TEMP = 80;
const float HYSTERESIS = 8;
const float NORMAL_WATER_TEMPERATURE = 40;
const float BATH_WATER_TEMPERATURE = 60;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

/*  Resolution of DS18B20 temperatur measurement = power-on-default = 12 bit = 0.0625 C
   *9-bit resolution --> 93.75 ms
   *10-bit resolution --> 187.5 ms
   *11-bit resolution --> 375 ms
   *12-bit resolution --> 750 ms
*/ 
String monitorstring = "";  
//enumarator for scren choose
enum screen{
    MAIN_SCREEN,
    OK_SCREEN,
    STOP_SCREEN,
    SENSORS_SCREEN
};
//enumerator for mode
enum mode{
    OFF,
    CIRCULATION,
    BATH
};

screen actualScreen = MAIN_SCREEN;
mode acualMode = OFF;
void contolLogic(float temperature1, float temperature2, bool& pumpHotWaterIsOn, bool& pumpCirculationIsOn, bool& pumpFloorHeatingIsOn);

void setup()
{
    /* Init M5StamPLC */
    M5StamPLC.begin();
}

void loop()
{ 
    float celsius, temperature1, temperature2;
    bool pumpHotWaterIsOn, pumpCirculationIsOn, pumpFloorHeatingIsOn;
    monitorstring = "Date;Time";  // change to realtime date and time of M5Stack (later)
    
    DS18B20.begin();
    int count = DS18B20.getDS18Count();  //check for number of connected sensors
    //read status of relays 
    pumpHotWaterIsOn =  M5StamPLC.readPlcRelay(0);
    pumpCirculationIsOn =  M5StamPLC.readPlcRelay(1);
    pumpFloorHeatingIsOn =  M5StamPLC.readPlcRelay(2);

    //read temperature
    if (count > 2) {
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
        actualScreen = OK_SCREEN;
    } else if (M5.BtnB.wasClicked()) {
        acualMode = OFF;
        actualScreen = STOP_SCREEN;
    } else if (M5.BtnC.wasClicked()) {
        actualScreen = SENSORS_SCREEN;           
    }
    //actual logic
    contolLogic(temperature1, temperature2, pumpHotWaterIsOn, pumpCirculationIsOn, pumpFloorHeatingIsOn);

    //display screen
    switch (actualScreen)
    {
    case MAIN_SCREEN:
        showMenu();
        break;
    case OK_SCREEN:
        showHeatingStarted();
        break;
    case STOP_SCREEN:
        showHeatingStopped();
        break;
    case SENSORS_SCREEN:
        showSensorStatus(temperature1, temperature2, pumpHotWaterIsOn, pumpCirculationIsOn, pumpFloorHeatingIsOn);
        break;
    default:
        break;
    }
    
    //Write relays
    M5StamPLC.writePlcRelay(0, pumpHotWaterIsOn);
    M5StamPLC.writePlcRelay(1, pumpCirculationIsOn);
    M5StamPLC.writePlcRelay(2, pumpFloorHeatingIsOn);    
    
    delay(500); //Measuremnt interval

} 

void contolLogic(float temperature1, float temperature2, bool& pumpHotWaterIsOn, bool& pumpCirculationIsOn, bool& pumpFloorHeatingIsOn){
//logic
     switch (acualMode)
    {
    case OFF:

        break;
    case CIRCULATION:
        break;
    case BATH:
        break;
    default:
        break;
    }
    

    }
};