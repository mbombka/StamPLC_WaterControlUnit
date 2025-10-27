#include <M5StamPLC.h>

static void showMenu(bool refreshTick){
    if (refreshTick) {
         M5StamPLC.Display.clear(TFT_BLACK);
        M5StamPLC.Display.setCursor(0, 10);
        M5StamPLC.Display.setTextColor(TFT_WHITE);
        M5StamPLC.Display.setTextSize(3);   
        M5StamPLC.Display.println(" A: Podgrzej\n");
        M5StamPLC.Display.println(" B: Zatrzymaj\n");
        M5StamPLC.Display.println(" C: Stan ");
    }
   
}

static void showSensorStatus(float heaterSetTemperature, float temperature1, float temperature2, bool pumpHotWaterIsOn, bool pumpCirculationIsOn, bool pumpFloorHeatingIsOn, bool refreshTick, int acualMode, int bathStep){
    if (refreshTick) {
        M5StamPLC.Display.clear(TFT_BLACK);
        M5StamPLC.Display.setCursor(0, 10);
        M5StamPLC.Display.setTextColor(TFT_WHITE);
        M5StamPLC.Display.setTextSize(2);   
        M5StamPLC.Display.printf(" T. zadana:%.2fC\n", heaterSetTemperature);
        M5StamPLC.Display.printf(" T. zbiornika:%.2fC\n", temperature1);
        M5StamPLC.Display.printf(" T. sprzegla:%.2fC\n", temperature2);
        M5StamPLC.Display.printf(" Pompa CWU: %s\n", pumpHotWaterIsOn? "ON":"OFF");
        M5StamPLC.Display.printf(" Pompa cyrk.: %s\n", pumpCirculationIsOn? "ON":"OFF");
        M5StamPLC.Display.printf(" Pompa CO: %s\n", pumpFloorHeatingIsOn? "ON":"OFF");
        M5StamPLC.Display.printf(" Tryb: %d\n", acualMode);
        M5StamPLC.Display.printf(" Bath step: %d\n", bathStep);

    }
}

static void showHeatingStarted(bool refreshTick){
    if (refreshTick) {
        M5StamPLC.Display.clear(TFT_BLACK);
        M5StamPLC.Display.setCursor(0, 10);
        M5StamPLC.Display.setTextColor(TFT_WHITE);
        M5StamPLC.Display.setTextSize(2);   
        M5StamPLC.Display.println(" TRYB KAPIELI");
        M5StamPLC.Display.println(" WLACZONY");
    }
}
static void showHeatingWithText(bool refreshTick, float heaterSetTemperature, float temperature1, String text){
    if (refreshTick) {
        M5StamPLC.Display.clear(TFT_BLACK);
        M5StamPLC.Display.setCursor(0, 10);
        M5StamPLC.Display.setTextColor(TFT_WHITE);
        M5StamPLC.Display.setTextSize(2);   
        M5StamPLC.Display.println(" TRYB KAPIELI");
        M5StamPLC.Display.printf(" T. zadana:%.2fC\n", heaterSetTemperature);
        M5StamPLC.Display.printf(" T. zbiornika:%.2fC\n", temperature1);
        M5StamPLC.Display.println(text);
    }
}
static void showNormalModeStarted(bool refreshTick){
    if (refreshTick) {
        M5StamPLC.Display.clear(TFT_BLACK);
        M5StamPLC.Display.setCursor(0, 10);
        M5StamPLC.Display.setTextColor(TFT_WHITE);
        M5StamPLC.Display.setTextSize(2);   
        M5StamPLC.Display.println(" TRYB NORMALNY");
        M5StamPLC.Display.println(" WLACZANY");
    }
}
