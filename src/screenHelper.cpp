#include <M5StamPLC.h>

static void showMenu(){
   
    M5StamPLC.Display.clear(TFT_BLACK);
    M5StamPLC.Display.setCursor(0, 10);
    M5StamPLC.Display.setTextColor(TFT_WHITE);
    M5StamPLC.Display.setTextSize(3);   
    M5StamPLC.Display.println(" A: Podgrzej\n");
    M5StamPLC.Display.println(" B: Zatrzymaj\n");
    M5StamPLC.Display.println(" C: Stan ");
    
   
}

static void showStatus1(float heaterSetTemperature, float temperature1, float temperature2, bool pumpHotWaterIsOn, bool pumpCirculationIsOn, bool floorHeatingIsOff, bool hotWaterExternalVoltageIsOn){
   
    M5StamPLC.Display.clear(TFT_BLACK);
    M5StamPLC.Display.setCursor(0, 10);
    M5StamPLC.Display.setTextColor(TFT_WHITE);
    M5StamPLC.Display.setTextSize(2);   
    M5StamPLC.Display.printf(" T. zadana:%.2fC\n", heaterSetTemperature);
    M5StamPLC.Display.printf(" T. zbiornika:%.2fC\n", temperature1);
    M5StamPLC.Display.printf(" T. sprzegla:%.2fC\n", temperature2);
    M5StamPLC.Display.printf(" Pompa CWU: %s\n", pumpHotWaterIsOn? "ON":"OFF");
    M5StamPLC.Display.printf(" Pompa cyrk.: %s\n", pumpCirculationIsOn? "ON":"OFF");
    M5StamPLC.Display.printf(" Ogrz. Podl.: %s\n", floorHeatingIsOff? "ON":"OFF");
    M5StamPLC.Display.printf(" Zewn. nap. : %s\n", hotWaterExternalVoltageIsOn? "ON":"OFF");
    
}

static void showStatus2(String actualMode, int bathStep, bool dacInitFail, bool wifiConnected ){
   
    M5StamPLC.Display.clear(TFT_BLACK);
    M5StamPLC.Display.setCursor(0, 10);
    M5StamPLC.Display.setTextColor(TFT_WHITE);
    M5StamPLC.Display.setTextSize(2);   
    M5StamPLC.Display.printf(" Tryb: %s\n", actualMode);
    M5StamPLC.Display.printf(" Bath step: %d\n", bathStep);
    M5StamPLC.Display.printf(" DAC init : %s\n", dacInitFail ? "FAILED" : "OK");
    M5StamPLC.Display.printf(" Wifi connected: %s\n", wifiConnected ? "yes" : "no");
    time_t timestamp;
    time(&timestamp);
    M5StamPLC.Display.printf(" %s", ctime(&timestamp));

}

static void showHeatingStarted(){
   
    M5StamPLC.Display.clear(TFT_BLACK);
    M5StamPLC.Display.setCursor(0, 10);
    M5StamPLC.Display.setTextColor(TFT_WHITE);
    M5StamPLC.Display.setTextSize(2);   
    M5StamPLC.Display.println(" TRYB KAPIELI");
    M5StamPLC.Display.println(" WLACZONY");

}
static void showCirculationStarted(){
   
    M5StamPLC.Display.clear(TFT_BLACK);
    M5StamPLC.Display.setCursor(0, 10);
    M5StamPLC.Display.setTextColor(TFT_WHITE);
    M5StamPLC.Display.setTextSize(2);   
    M5StamPLC.Display.println(" TRYB CYRKULACJI");
    M5StamPLC.Display.println(" WLACZONY");

}
static void showHeatingWithText(float heaterSetTemperature, float temperature1, String text){
   
    M5StamPLC.Display.clear(TFT_BLACK);
    M5StamPLC.Display.setCursor(0, 10);
    M5StamPLC.Display.setTextColor(TFT_WHITE);
    M5StamPLC.Display.setTextSize(2);   
    M5StamPLC.Display.println(" TRYB KAPIELI");
    M5StamPLC.Display.printf(" T. zadana:%.2fC\n", heaterSetTemperature);
    M5StamPLC.Display.printf(" T. zbiornika:%.2fC\n", temperature1);
    M5StamPLC.Display.println(text);

}
static void showNormalModeStarted(){
   
    M5StamPLC.Display.clear(TFT_BLACK);
    M5StamPLC.Display.setCursor(0, 10);
    M5StamPLC.Display.setTextColor(TFT_WHITE);
    M5StamPLC.Display.setTextSize(2);   
    M5StamPLC.Display.println(" TRYB NORMALNY");
    M5StamPLC.Display.println(" WLACZANY");

}
