/*
(C) HANCO WIESEMANN 2022
hanco.hw@gmail.com
*/
#ifndef GSM_MQTT_H
#define GSM_MQTT_H
#define TRIESOUT 30
#include <Arduino.h>
#include <hardware/uart.h>
#include <ArduinoQueue.h>

#define endpointtoken UCDIWQfdBr
#define applicationversion ccfccnadblapdp7slkg0-v1
class gsm_mqtt{
    public:
        gsm_mqtt(String server,String port,String topic,void (*subscribe_callback)(String topic,String message));
        ~gsm_mqtt();
        void gsm_mqtt_loop();
        bool pub(String topic,String data);
        bool timeout(unsigned long timer);
        unsigned long set_time(unsigned long time);
    private:
        void (*Subscribe_Callback)(String topic,String message);
        String Server;
        String Port;
        String Topic;
        typedef struct Message{
            String topic;
            String message;
        }Message;
        ArduinoQueue<Message> *pub_messages;
        enum STATES{
        POWER_CYCLE,
        SET_ECHO,
        TURNOFF_RF,
        SETAPN,
        TURNON_RF,
        SIGNAL_STRENGTH,
        APN_CONNECTING_INFO,
        NEW_MQTT_INSTANCE,
        MQTT_CONNECTING,
        MQTT_SUBSCRIBING,
        MQTT_DISCONNECT,
        PUB,
        SUB} state,state_next;
        String states[13] = {
        "POWER_CYCLE",
        "SET_ECHO",
        "TURNOFF_RF",
        "SETAPN",
        "TURNON_RF",
        "SIGNAL_STRENGTH",
        "APN_CONNECTING_INFO",
        "NEW_MQTT_INSTANCE",
        "MQTT_CONNECTING",
        "MQTT_SUBSCRIBING",
        "MQTT_DISCONNECT",
        "PUB",
        "SUB"};

        UART* gsm_serial = NULL;
        void sub_handler(String sub_raw);
        String to_hex(String string);
        void print_heading(String title,bool sub_heading = false);
        String toascii(String hex);
        String write_data(String data);
        String poll();
        void pub_state();
        void apn_connecting_info();
        void power_cycle();
        void blink(int Amount);
        
};

#endif //GS_MQTT_H