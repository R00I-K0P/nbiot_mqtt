#include <Arduino.h>
#include "gsm_mqtt.h"

gsm_mqtt *test;

bool light = false;
void mqtt_callback(String topic, String message){
    Serial.println(topic);
    Serial.println(message);
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    test = new gsm_mqtt("test.mosquitto.org","commands",mqtt_callback);
}

unsigned long int timerr = 0;
void loop() {
    test->gsm_mqtt_loop();
    if(test->timeout(timerr)){
        timerr = test->set_time(60000*5);
        test->pub("Hello world","data");
    }else{
        return;
    }
} 

