#include <WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_HTS221.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "mbedtls/aes.h"

#define NUMPIXELS 16
//-------------------------------------------------------------------------Wi-Fi
const char* ssid = "....";      // WiFi name
const char* password = "....";  // WiFi password
//-------------------------------------------------------------------------NETPIE 
WiFiClient espClient;
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
PubSubClient client(espClient);
char msg[200];
const char* mqtt_client = "....";     // Cient id
const char* mqtt_username = "....";   // Token
const char* mqtt_password = "....";   // Secret
//-------------------------------------------------------------------------NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String formattedDate;
String dayStamp;
String timeStamp;
int ntpDisplayDelay = 1000;
//------------------------------------------------------------------------- Sensor
const int SCLpin = 40;
const int SDApin = 41;
Adafruit_BMP280 bmp;
Adafruit_HTS221 hts;
Adafruit_MPU6050 mpu;
int tempAndHumiDelay = 1000;  //every 5 sec display
int pressureDelay = 5000;     //every 25 sec display
int angularDelay = 10000;     //every 50 sec display
//------------------------------------------------------------------------- En/De
char key[17] = "abcdefghijklmnop";
unsigned char cipherTextOutput[16];
unsigned char decipheredTextOutput[16];
//------------------------------------------------------------------------- LED colour
Adafruit_NeoPixel pixels(NUMPIXELS, 18, NEO_GRB + NEO_KHZ800);
//------------------------------------------------------------------------- function
  //************************************************************************** function for callback
String byteToString(byte* payload, unsigned int length_payload){ char buffer_payload[length_payload+1] = {0};
  memcpy(buffer_payload, (char*)payload, length_payload);
  return String(buffer_payload);
}

String charStarToString(char* payload){ 
  String buffer=payload;
  return buffer;
}

String constCharStarToString(const char* payload){ 
  String buffer =payload;
  return buffer;
}
  //************************************************************************** callback(payload from subscribe topic)
void callback(char* topic, byte* payload, unsigned int length) {
  //à¹à¸à¹‰à¹„à¸Ÿà¸¥à¹Œ PubSubClient.h à¸•à¸£à¸‡ #define MQTT_MAX_PACKET_SIZE 256 à¹€à¸›à¹‡à¸™ 512
  String ms=byteToString(payload, length);
  String t=charStarToString(topic);
  //ms="\"" + ms + "\"";
  Serial.println(ms);

  int preT = ms.indexOf("previousTemp");
  int curT = ms.indexOf("currentTemp");
  int preHu = ms.indexOf("previousHumi");

  if (preT !=-1 && curT !=-1 && preHu !=-1){
    String preTempData = ms.substring(preT + 15, curT - 3);
    String curTempData = ms.substring(curT + 14, preHu - 3);
    Serial.println(preTempData);
    Serial.println(curTempData);
    unsigned char output[16];
    for(int i=0; i<16; i++){
      String hexStr = preTempData.substring(i*2, i*2+2);
      output[i] = (unsigned char) strtol(hexStr.c_str(), NULL, 16);
    }  

    String buffer2="";
    decrypt(output, key, decipheredTextOutput);
    for(int i=0; i<16; i++){
      buffer2 +=String((char)decipheredTextOutput[i]);
    }
    Serial.println("decrypted previous temperature is "+ buffer2);

    unsigned char output2[16];
    for(int i=0; i<16; i++){
      String hexStr2 = curTempData.substring(i*2, i*2+2);
      output2[i] = (unsigned char) strtol(hexStr2.c_str(), NULL, 16);
    }  

    String buffer3="";
    decrypt(output2, key, decipheredTextOutput);
    for(int i=0; i<16; i++){
      buffer3 +=String((char)decipheredTextOutput[i]);
    }
    Serial.println("decrypted current temperature is "+ buffer3);

    String t1 = buffer2.substring(0, 5);
    String t2 = buffer3.substring(0, 5);
    
    float t1_float = t1.toFloat();
    float t2_float = t2.toFloat();
    float diff = abs(t1_float-t2_float);
    if(diff>=0.2){
      pixels.setPixelColor(0, pixels.Color(150, 0, 0));
      pixels.show();
    }
    else{
      pixels.setPixelColor(0, pixels.Color(0, 150, 0));
      pixels.show();
    }
  }
}
  //************************************************************************** En/De function
void encrypt(char * plainText, char * key, unsigned char * outputBuffer){
  mbedtls_aes_context aes;
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_enc( &aes, (const unsigned char*) key, strlen(key) * 8 );
  mbedtls_aes_crypt_ecb( &aes, MBEDTLS_AES_ENCRYPT, (const unsigned char*)plainText, outputBuffer);
  mbedtls_aes_free( &aes );
}

void decrypt(unsigned char * chipherText, char * key, unsigned char * outputBuffer){
  mbedtls_aes_context aes;
  mbedtls_aes_init( &aes );
  mbedtls_aes_setkey_dec( &aes, (const unsigned char*) key, strlen(key) * 8 );
  mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const unsigned char*)chipherText, outputBuffer);
  mbedtls_aes_free( &aes );
}

String encryptString(float sensorData){
  String Text = String(sensorData);
  char plainText[17];
  Text.toCharArray(plainText, Text.length()+1);
  encrypt(plainText, key, cipherTextOutput);
  String buffer = "";
  char result[16];
  for(int i=0; i<16; i++){
    sprintf(result,"%02x",cipherTextOutput[i]);
    buffer += String(result);
    //buffer += String(cipherTextOutput[i], HEX);
  }
  String buffer2 ="";
  decrypt(cipherTextOutput, key, decipheredTextOutput);
  for(int i=0; i<16; i++){
    buffer2 +=String((char)decipheredTextOutput[i]);
  }
  return buffer;
}

//------------------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  pixels.begin();
  //************************************************************************* setup Sensor
  Wire.begin(SDApin, SCLpin);
  if (bmp.begin(0x76)){
    Serial.println("BMP280 sensor ready");
  }
  if (hts.begin_I2C()){
    Serial.println("HTS221 sensor ready");
  }
  if (mpu.begin()){
    Serial.println("MPU6050 sensor ready");
  }
  //************************************************************************* setup Pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  //************************************************************************* setup wifi
  //connect wifi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  client.setServer(mqtt_server, mqtt_port);
  client.connect(mqtt_client, mqtt_username, mqtt_password);
  //************************************************************************* setup NTP
  timeClient.begin();
  timeClient.update();
  timeClient.setTimeOffset(3600*7);
  Serial.println("Setup complete");
  //************************************************************************* setup Netpie
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.connect(mqtt_client, mqtt_username, mqtt_password);
  client.subscribe("@shadow/data/updated");
  client.publish("@msg/testText","hello netpie"); //@shadow/data/update
  //*************************************************************************  
  //task setup
  xTaskCreate(
  displayNTP, /* Task function. */
  "displayNTP", /* name of task. */
  10000, /* Stack size of task */
  NULL, /* parameter of the task */
  1, /* priority of the task */
  NULL); /* Task handle to keep track of created task */

  xTaskCreate(
  temp_Humudity_Data, /* Task function. */
  "temp_Humudity_Data", /* name of task. */
  10000, /* Stack size of task */
  NULL, /* parameter of the task */
  2, /* priority of the task */
  NULL); /* Task handle to keep track of created task */

  xTaskCreate(
  pressure_Data, /* Task function. */
  "pressure_Data", /* name of task. */
  10000, /* Stack size of task */
  NULL, /* parameter of the task */
  3, /* priority of the task */
  NULL); /* Task handle to keep track of created task */

  xTaskCreate(
  gyroscope_Data, /* Task function. */
  "gyroscope_Data", /* name of task. */
  10000, /* Stack size of task */
  NULL, /* parameter of the task */
  4, /* priority of the task */
  NULL); /* Task handle to keep track of created task */
}

void loop() {
  if(client.connected()) {
    client.loop(); 
  }
  else {
    if(WiFi.status() == WL_CONNECTED) {
      client.disconnect();
      client.connect(mqtt_client, mqtt_username, mqtt_password);
      client.subscribe("@shadow/data/get");
      Serial.println("reconnected to Netpie");
    } else {
      WiFi.disconnect();
      WiFi.begin(ssid, password);
      Serial.println("reconnected to WiFi");
    }
  }
}

//------------------------------------------------------------------------- task
  //************************************************************************** displayNTP
void displayNTP( void * parameter ){ // task localTime
  for(;;){
    timeClient.update();
    Serial.println(timeClient.getFormattedTime());
    vTaskDelay(ntpDisplayDelay); 
  }
}
  //************************************************************************** temp_Humudity_Data
void temp_Humudity_Data( void * parameter ){ // task localTime
  vTaskDelay(10);
  int counter = 0;
  float currentTemp =0;
  float previousTemp =0;
  float currentHumi =0;
  float previousHumi =0;
  float tempData[5];
  float humiData[5];
  for(;;){
    //Serial.println("from task temp_Humudity_Data");
    sensors_event_t temp, humid;
    hts.getEvent(&humid, &temp);
    float t = temp.temperature;
    float h = humid.relative_humidity;
    if (counter==5){ // already collected 5 data
      Serial.println("from task temp_Humudity_Data");
      counter = 0;
      float avgTemp = 0;
      float avgHumi = 0;
      for (int i=0; i<5; i++){
        avgTemp += tempData[i];
        avgHumi += humiData[i];
      }
      avgTemp /= 5;
      avgHumi /= 5;

      if(currentTemp != 0){
        previousTemp = currentTemp;
      }
      currentTemp = avgTemp;

      if(currentHumi != 0){
        previousHumi = currentHumi;
      }
      currentHumi = avgHumi;

      String buffer_1 =encryptString(previousTemp);
      String buffer_2 =encryptString(currentTemp);

      String payload = "{\"data\": {";
      payload.concat("\"previousTemp\":\"" + buffer_1);
      payload.concat("\", ");
      payload.concat("\"currentTemp\":\"" + buffer_2);
      payload.concat("\", ");
      payload.concat("\"previousHumi\":\"" + String(previousHumi,2));
      payload.concat("\", ");
      payload.concat("\"currentHumi\":\"" + String(currentHumi,2));
      payload.concat("\"}}");
      //Serial.println(payload);
      payload.toCharArray(msg, (payload.length()+1)); 
      client.publish("@shadow/data/update", msg);
      Serial.println("previousTemp = " + String(previousTemp) + ", " + "currentTemp = " + String(currentTemp));
      Serial.println("previousHumi = " + String(previousHumi) + ", " + "currentHumi = " + String(currentHumi));
    }
    else {
      tempData[counter] = t;
      humiData[counter] = h;
      counter++;
    }
    vTaskDelay(tempAndHumiDelay); 
  }
}
  //************************************************************************** pressure_Data
void pressure_Data( void * parameter ){ // task localTime
  vTaskDelay(20);
  int counter = 0;
  float currentPressure =0;
  float previousPressure =0;
  float pressureData[5];
  for(;;){
    //Serial.println("from task pressure_Data");
    float p = bmp.readPressure() / 1000;
    if (counter==5){ // already collected 5 data
      Serial.println("from task pressure_Data");
      counter = 0;
      float avgPressure = 0;
      for (int i=0; i<5; i++){
        avgPressure += pressureData[i];
      }
      avgPressure /= 5;
      if(currentPressure != 0){
        previousPressure = currentPressure;
      }
      currentPressure = avgPressure;

      String payload = "{\"data\": {";
      payload.concat("\"previousPressure\":\"" + String(previousPressure,2));
      payload.concat("\", ");
      payload.concat("\"currentPressure\":\"" + String(currentPressure,2));
      payload.concat("\"}}");
      payload.toCharArray(msg, (payload.length()+1)); 
      client.publish("@shadow/data/update", msg);

      Serial.println("previousPressure = "+ String(previousPressure) + ", " + "currentPressure = "+String(currentPressure));
    }
    else {
      pressureData[counter] = p;
      counter++;
    }
    vTaskDelay(pressureDelay); 
  }
}
  //************************************************************************** gyroscope_Data
void gyroscope_Data( void * parameter ){ // task localTime
  vTaskDelay(30);
  int counter = 0;
  float currentGyro =0;
  float previousGyro =0;
  float gyroData[5];
  for(;;){
    //Serial.println("from task gyroscope_Data");
    sensors_event_t a, g, buffer;
    mpu.getEvent(&a, &g, &buffer);
    //float ax = a.acceleration.x;
    float gx = g.gyro.x;
    if (counter==5){ // already collected 5 data
      Serial.println("from task gyroscope_Data");
      counter = 0;
      float avgGyro = 0;
      for (int i=0; i<5; i++){
        avgGyro += gyroData[i];
      }
      avgGyro /= 5;
      if(currentGyro != 0){
        previousGyro = currentGyro;
      }
      currentGyro = avgGyro;

      String payload = "{\"data\": {";
      payload.concat("\"previousGyro\":\"" + String(previousGyro,2));
      payload.concat("\", ");
      payload.concat("\"currentGyro\":\"" + String(currentGyro,2));
      payload.concat("\"}}");
      payload.toCharArray(msg, (payload.length()+1)); 
      client.publish("@shadow/data/update", msg);

      Serial.println("previousGyro = "+ String(previousGyro) + ", "+ "currentGyro = " + String(currentGyro));
    }
    else {
      gyroData[counter] = gx;
      counter++;
    }
    vTaskDelay(angularDelay); 
  }
}
