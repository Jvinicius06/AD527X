#include <Wire.h>
#include "AD527X.h"

AD527X my_AD5272(10, 20, ADDRESS_GND);
#define input A0

void setup(){
    Wire.begin();
    Serial.begin(115200);
    pinMode(input, INPUT);
    my_AD5272.begin();
    my_AD5272.setSettingValue(0);
    delay(1000);
}

int val = 0;
float voltage;
float analogval;
float math;
void loop(){

while(val < 1){
    for(int32_t i = 0; i <= 20000; i = i +10){
        my_AD5272.setResistance(i);
        analogval = analogRead(input);
        voltage = 5*(analogval/1024); 
        Serial.print("Value: ");
        Serial.print(my_AD5272.readRDAC());
        Serial.print(" | ");
        Serial.print("Input: ");
        Serial.print(i);
        Serial.print(" | ");
        Serial.print("Resistance: ");
        Serial.print(my_AD5272.getResistance());
        Serial.print(" | Voltage: ");
        Serial.println(voltage); 
        delay(1);
        }
        val++;
        Serial.println("done");
    }
}

