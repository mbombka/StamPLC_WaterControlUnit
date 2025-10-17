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

  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature DS18B20(&oneWire);

/*  Resolution of DS18B20 temperatur measurement = power-on-default = 12 bit = 0.0625 C
   *9-bit resolution --> 93.75 ms
   *10-bit resolution --> 187.5 ms
   *11-bit resolution --> 375 ms
   *12-bit resolution --> 750 ms
*/ 
  String monitorstring = "";  

void setup()
{
    /* Init M5StamPLC */
    M5StamPLC.begin();
}


void loop()
{ 
     float celsius;
    float fahrenheit;

    monitorstring = "Date;Time";  // change to realtime date and time of M5Stack (later)
    
    DS18B20.begin();
    int count = DS18B20.getDS18Count();  //check for number of connected sensors

//    M5.Lcd.clear();    //clearing causes flickering of Lcd-display, looks nicer without
    M5.Lcd.setCursor(0,0);
    M5.Lcd.print("Devices found: ");
    M5.Lcd.print(count);
    M5.Lcd.println(" DS18B20");

    if (count > 0) {
    DS18B20.requestTemperatures();
    for (int i=0; i < count; i++) {
      String m5stackstring = "Sensor ";
 
      celsius = DS18B20.getTempCByIndex(i);

      m5stackstring = m5stackstring + String(i) + String(": ") + String(celsius,4) + String(" C     ");  
      M5.Lcd.println(m5stackstring);  // 1 line per sensor on M5Stack Lcd
    
      monitorstring = monitorstring + String(";") + String(celsius,4);  // ";" is Excel compatibel separator
 
    }
    Serial.println(monitorstring);  // 1 line for all measurements on serial monitor
    }
    delay(500); //Measuremnt interval
//      /* Show menu */
//     showMenu();

    
//    /* Check if button was clicked */
//     if (M5.BtnA.wasClicked()) {
//         showHeatingStarted();
//     } else if (M5.BtnB.wasClicked()) {
//         showHeatingStopped();;
//     } else if (M5.BtnC.wasClicked()) {
      
//             //showSensorStatus(45.5, 38.2, true, false, true, false);
//     }

//     delay(100);

} 

