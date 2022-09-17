/*
(C) HANCO WIESEMANN 2022
hanco.hw@gmail.com
*/
#include "gsm_mqtt.h"
#include <ArduinoQueue.h>

gsm_mqtt::gsm_mqtt(String server,String sub_topic,void (*subscribe_callback)(String topic,String message)){
  Subscribe_Callback = subscribe_callback;
  state = state_next = STATES::POWER_CYCLE;
  Server = server;
  Sub_topic = sub_topic;
  pub_messages = new ArduinoQueue<Message> (10);
  gsm_serial = new UART(0,1);
  pinMode(14,OUTPUT);
}

gsm_mqtt::~gsm_mqtt(){
  delete pub_messages;
  delete gsm_serial;
}

bool OK(String response){
  if(response.indexOf("OK") != -1) return true;
  return false;
}

enum CYCLE{TURN_OFF,TURN_ON,TURN_SETUP};
CYCLE cycle = CYCLE::TURN_OFF;
unsigned long cycle_timer = 0;
void gsm_mqtt::power_cycle(){
  if(timeout(cycle_timer)){
    switch(cycle){
      case CYCLE::TURN_OFF:{
        cycle_timer = set_time(5000);
        cycle = CYCLE::TURN_ON;
        digitalWrite(14,LOW);
        Serial.println("Shut down");
        gsm_serial->end();
        break;
      }
      case CYCLE::TURN_ON:{
        cycle_timer = set_time(5000);
        cycle = CYCLE::TURN_SETUP;
        digitalWrite(14,HIGH);
        Serial.println("Turn on");
        break;
      }
      case CYCLE::TURN_SETUP:{
        cycle = CYCLE::TURN_OFF;
        Serial.println("restarted");
        gsm_serial->begin(115200);
        while(!gsm_serial);
        state_next = STATES::SET_ECHO;
        break;
      }
    }
  }
}

void gsm_mqtt::gsm_mqtt_loop(){
  switch (state){
    case STATES::POWER_CYCLE:{
      power_cycle();
      break;
    }
    case STATES::SET_ECHO:{
      String response = write_data("ATE0");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::TURNOFF_RF;
        }
      }
      break;
    }
    case STATES::TURNOFF_RF:{
      String response = write_data("AT+CFUN=0");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::SETAPN;
        }
      }
      break;
    }
    case STATES::SETAPN:{
      String response = write_data("AT*MCGDEFCONT=\"IP\",\"flickswitch\"");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::TURNON_RF;
        }
      }
      break;
    }
    case STATES::TURNON_RF:{
      String response = write_data("AT+CFUN=1");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::SIGNAL_STRENGTH;
        }
      }
      break;
    }
    case STATES::SIGNAL_STRENGTH:{
      String response = write_data("AT+CSQ");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::APN_CONNECTING_INFO;
        }
      }
      break;
    }
    case STATES::APN_CONNECTING_INFO:{
      apn_connecting_info();
      break;
    }
    case STATES::NEW_MQTT_INSTANCE:{
      String response = write_data("AT+CMQNEW=\""+Server+"\",1883,12000,1024");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::MQTT_CONNECTING;
        }else{
          state_next = STATES::MQTT_DISCONNECT;
        }
      }
      break;
    }
    case STATES::MQTT_CONNECTING:{
      String response = write_data("AT+CMQCON=0,3,\""+String(random(1000000))+"\",600,1,0");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::MQTT_SUBSCRIBING;
        }else{
          state_next = STATES::MQTT_DISCONNECT;
        }
      }
      break;
    }
    case STATES::MQTT_SUBSCRIBING:{
      String response = write_data("AT+CMQSUB=0,\""+Sub_topic+"\",1");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::PUB;
        }else{
          state_next = STATES::MQTT_DISCONNECT;
        }
      }
      break;
    }
    case STATES::MQTT_DISCONNECT:{
      String response = write_data("AT+CMQDISCON=0");
      if(response != ""){
        if(OK(response)){
          state_next = STATES::APN_CONNECTING_INFO;
        }else{
          state_next = STATES::APN_CONNECTING_INFO;
        }
      }
      break;
    }
    case STATES::PUB:{
      pub_state();
      break;
    }
    case STATES::SUB:{
      if(!pub_messages->isEmpty()){
        state_next = STATES::PUB;
      }
      poll();
      break;
    }
  }
  if(state != state_next){
    print_heading(states[state]+" -> "+states[state_next]);
    state = state_next;
  }
  blink(state+1);
}


unsigned long connection_counter = 0;
void gsm_mqtt::apn_connecting_info(){
  String response = write_data("AT+CGCONTRDP");
  if(response != ""){
    if(response.indexOf("192.168") != -1){
      state_next = STATES::NEW_MQTT_INSTANCE;
    }else{
      if(connection_counter++ > TRIESOUT){
        state_next = STATES::POWER_CYCLE;
        connection_counter = 0;
      }else{
        state_next = STATES::SIGNAL_STRENGTH;
      }
    }
  }
}
void gsm_mqtt::pub_state(){

  if(pub_messages->isEmpty()){
    state_next = STATES::SUB;
    return;
  }
  Message message = pub_messages->getHead();
  String msg = to_hex(message.message);
  String topic = message.topic;
  String response = write_data("AT+CMQPUB=0,\""+topic+"\",1,0,0,"+String(msg.length())+",\""+msg+"\"");
  if(response != ""){
    if(!OK(response)){
      state_next = STATES::MQTT_DISCONNECT;
    }else
    {
      pub_messages->dequeue();
    }
  }
}
bool gsm_mqtt::pub(String message,String topic){
  Message msg = {topic,message};
  return pub_messages->enqueue(msg);
}

void gsm_mqtt::sub_handler(String sub_raw){
  sub_raw.remove(0,sub_raw.indexOf("\"")+1);
  String topic = sub_raw.substring(0,sub_raw.indexOf("\""));
  sub_raw.remove(0,sub_raw.indexOf("\"")+1);
  sub_raw.remove(0,sub_raw.indexOf("\"")+1);
  String message_hex = sub_raw.substring(0,sub_raw.indexOf("\""));
  Subscribe_Callback(topic,toascii(message_hex));
}

String gsm_mqtt::to_hex(String string){
  char* hexStr = (char*)malloc(sizeof(char)*string.length()*2+1);

  for (int i = 0; i < string.length(); i++)
      sprintf(hexStr + i*2, "%X", string[i]);

  hexStr[string.length()*2] = '\0';
  String new_string(hexStr);
  delete hexStr;
  return new_string;
}

String gsm_mqtt::toascii(String hex){
  Serial.println(hex);
   String ascii = "";
   for (size_t i = 0; i < hex.length(); i += 2){
      String part = hex.substring(i, i+2);
      char ch = (char)strtol(part.c_str(), NULL, 16);
      ascii+= ch;
   }
   return ascii;
}

unsigned long write_timer = 0;
bool sent = false;
String gsm_mqtt::write_data(String data){
  if(timeout(write_timer)){
    if(!sent){
      print_heading("Data",true);
      Serial.println(data);
      gsm_serial->println(data);
      write_timer = set_time(60000);
      sent = true;
    }else{
      state_next = STATES::POWER_CYCLE;
      sent = false;
    }
  }
  String response = poll();
  if(response != ""){
    write_timer = 0;
    print_heading("Response",true);
    Serial.println("\""+response+"\"");
    write_timer = set_time(5000);
    sent = false;
  }
  return response;
}

String gsm_mqtt::poll(){
  if(gsm_serial->available() > 0){
    String response = gsm_serial->readString();

    if(response.indexOf("+CMQPUB: ") != -1){
      sub_handler(response);
      if(response.indexOf("\r\nOK\r\n") != -1)
        return "\r\nOK\r\n";
      if(response.indexOf("\r\nERROR\r\n") != -1)
        return "\r\nERROR\r\n";
    }else if(response.indexOf("+CLTS:") != -1){
      if(response.indexOf("\r\nOK\r\n") != -1)
        return "\r\nOK\r\n";
      if(response.indexOf("\r\nERROR\r\n") != -1)
        return "\r\nERROR\r\n";
      return "";
    }else{
      return response;
    }
  }
  return "";
}
void gsm_mqtt::print_heading(String title,bool sub_heading){
  int length = 60;
  int title_len = title.length();
  String heading;
  heading.reserve(length);

  String head = "=";
  if(sub_heading)head = "-";

  for(int i =0;i<length/2-title_len/2;i++){
    heading += head;
  }
  heading += title;
  for(int i =0;i<length/2-title_len/2;i++){
    heading += head;
  }
  Serial.println(heading);
}
bool gsm_mqtt::timeout(unsigned long timer){
  if(timer < millis())return true;
  return false;
}
unsigned long gsm_mqtt::set_time(unsigned long time){
  return millis() + time;
}


enum BLINK{ON,OFF,DELAY};
BLINK Blink = BLINK::DELAY;
int blink_time = 0;
int left = 0;
int amount = 0;
void gsm_mqtt::blink(int Amount){
  if(timeout(blink_time)){
    switch(Blink){
      case BLINK::ON:{
        Blink = BLINK::OFF;
        digitalWrite(25,HIGH);
        blink_time = set_time(200);
        break;
      }
      case BLINK::OFF:{
        digitalWrite(25,LOW);
        Blink = BLINK::ON;
        blink_time = set_time(200);
        left++;
        if(left >= amount)
          Blink = BLINK::DELAY;
        break;
      }
      case BLINK::DELAY:{
        left = 0;
        amount = Amount;
        blink_time = set_time(1000);
        Blink = BLINK::ON;
        break;
      }
    }
  }
}