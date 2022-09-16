#include <Arduino.h>
#include "gsm_mqtt.h"

gsm_mqtt *gsm_module;

void mqtt_callback(String topic, String message){
    Serial.println(topic);
    Serial.println(message);
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    gsm_module = new gsm_mqtt("test.mosquitto.org","commands",mqtt_callback);
}

unsigned long int timerr = 0;
void loop() {
    gsm_module->gsm_mqtt_loop();
    if(gsm_module->timeout(timerr)){
        timerr = gsm_module->set_time(60000*5);
        gsm_module->pub("Hello world","data");
    }else{
        return;
    }
} 

