#include "Arduino.h"
#include "DevKitMQTTClient.h"
#include "AudioClassV2.h"
#include "RGB_LED.h"
#include "Sensor.h"
#include "HTS221Sensor.h"
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "RingBuffer.h"
#include "SystemTickCounter.h"
#include "config.h"
#include "utility.h"

const char *deviceId = "Chariot-001";

static bool hasWifi = false;
static bool hasIoTHub = false;
static bool isConnected = false;
int messageCount = 1;
int sentMessageCount = 0;
static bool messageSending = true;
static uint64_t send_interval_ms;


RGB_LED rgbLed;
DevI2C *ext_i2c;
HTS221Sensor *ht_sensor;
AudioClass &Audio = AudioClass::getInstance();
int MESSAGE_MAX_LEN = 256;

float temperature = 0;
char temperatureUnit = 'C';
float record = 0;
char volumeUnit[] = "dB"; 

void setup() {
  Screen.print(0, "Chariot Office");
  initSensors();
  InitWiFi();
  
  DevKitMQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "DevKit-Chariot");
  DevKitMQTTClient_Init(true);

  //DevKitMQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  //DevKitMQTTClient_SetMessageCallback(MessageCallback);
  //DevKitMQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  //DevKitMQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);

  send_interval_ms = SystemTickCounterRead();

}

void initSensors()
{
  ext_i2c = new DevI2C(D14, D15);
  
  ht_sensor = new HTS221Sensor(*ext_i2c);
  ht_sensor->init(NULL);
}

void InitWiFi()
{
  char wifiBuff[128];
  Screen.print(1,"WiFi \r\nConnecting...");
  
  if(WiFi.begin() == WL_CONNECTED)
  {
    
    IPAddress ip = WiFi.localIP();
    sprintf(wifiBuff, ip.get_address());
    Screen.print(3,wifiBuff);
    isConnected = true;
    hasWifi = true;
  }
  else
  {
    sprintf(wifiBuff, "No connection");
    Screen.print(3,wifiBuff);
  }
}

void getTemperature() {
  try
  {
    ht_sensor->reset();
    ht_sensor->getTemperature(&temperature);
    
    char buff[16];
    sprintf(buff, "Temp: %.2f%c",temperature,temperatureUnit);
    Screen.print(1, buff);
  }
  catch(int error)
  {
    LogError("An error has occured while reading temperature: %d",error);
  }
  
}

void getNoise()
{
  try
  {
    char audioBuff[128];
    char buff[16];
    record = Audio.startRecord(audioBuff, 128);
    delay(500);
    record = Audio.convertToMono(audioBuff, 128, 128);
    sprintf(buff, "Sound: %d%c", record,volumeUnit);
    Screen.print(2, buff);
    
  }
  catch(int error)
  {
    LogError("An error has occured while accessing the microphone: %d",error);
  }
  
}

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    sentMessageCount++;
    blinkGreen();
  }
  char line1[20];
  sprintf(line1, "Count: %d/%d",sentMessageCount, messageCount); 
  Screen.print(2, line1);

  messageCount++;
}


static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  //parseTwinMessage(updateState, temp);
  free(temp);
}

static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending temperature and humidity data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending temperature and humidity data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}

void blinkGreen()
{
  rgbLed.setColor(0, 255, 0);
  delay(250);
  rgbLed.turnOff();
  delay(50);
  rgbLed.setColor(0, 255, 0);
  delay(250);
  rgbLed.turnOff();
}

void blinkIdle()
{
  rgbLed.setColor(0, 0,50);
  delay(500);
  rgbLed.turnOff();
  delay(500);
}

void blinkError()
{
  for(int i = 10; i > 0; i--)
  {
    rgbLed.setColor(100, 0,0);
    delay(100);
    rgbLed.turnOff();
    delay(100);
  }
}
void sendData()
{
  try
  {
    char sensorData[100];
    sprintf_s(sensorData, sizeof(sensorData), "{\"DeviceId\":%s,\"Temperature\":%s,\"Noise\":%s}", deviceId, f2s(temperature, 1), f2s(record, 1));
    DevKitMQTTClient_SendEvent(sensorData);
    blinkGreen();
  } catch(exception e){
    blinkError();
  }
  
}

void loop() {
  sendData();
  //delay for 15 seconds, so only 4 messages sent per minute 
  for(int i=0; i<15; i++)
  {
    blinkIdle();
    getNoise();
    getTemperature();
  }
}
