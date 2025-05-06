///////// ESP32-C3 SuperMINI inverted Pendulum ////////
/////////////// Practice2 BLE pcocessing //////////////
////////////////// 2025/04/28 by hir. /////////////////

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// LED_PIN
#define LED1_PIN 1
#define LED2_PIN 8

////////////////////////////////////////////////////////////////
////////// BLE 処理関連 //////////
#define DEVICENAME "ESP32C3_name"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
boolean isrequested = false;
bool led_bool = false;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    //Serial.println("** device connected");
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    //Serial.println("** device disconnected");
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      // //Display on serial monitor for debug
      // Serial.println("*********");
      // Serial.print("Received Value: ");
      // //rxValue.trim();
      // Serial.println(rxValue.c_str());
      // Serial.println("*********");
      // //Reply as is
      pTxCharacteristic->setValue(rxValue.c_str());
      pTxCharacteristic->notify();
      delay(10);

      ///////////////////////////////////////////////////////////
      //////////////////////// status 管理 //////////////////////
      if (rxValue.find("start") == 0) {       // BLE start
        isrequested = true;
      } else if (rxValue.find("quit") == 0) { // BLE finished
        isrequested = false;
      }
    }
  }
};

////////// timer intrrupt 関連 //////////
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
u_int32_t isrCounter = 0; // timer内処理完結なので vola要りません
volatile bool first_timer = false, second_timer = false, third_timer = false;

////////// Timer 実行処理 //////////
void ARDUINO_ISR_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  isrCounter++;
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);

  // Depending on the count, you can safely set a flag here.
  if (isrCounter %   50 == 0){ // 50msc timer
    first_timer = true;
  }
  if (isrCounter %  200 == 0){ // 200msc timer
    second_timer = true;
  }
  if (isrCounter % 1000 == 0){ // 1000msc timer
    third_timer = true;
  }
}

////////// LED 処理 //////////
void led_blink(u8_t pin){
  digitalWrite(pin, !digitalRead(pin));
}

void setup() {
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  // Create the BLE Device
  BLEDevice::init(DEVICENAME); //BLE Device Name scaned and found by clients

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    //BLECharacteristic::PROPERTY_NOTIFY );
                    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE ); // 両方のプロパティを設定

  // pTxCharacteristic に BLE2902 を追加。BLE Client の Notify (Indicate) を有効化
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_RX,
                    BLECharacteristic::PROPERTY_WRITE );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  //Serial.println("start advertising");
  //Serial.println("Waiting a client connection to notify...");

  //The ESP32S3 bluetoth 5.0 requires security settings.
  //Without it, an error will occur when trying to pair with other devices.
  //Using a 6-digit PIN as the authentication method seems to work.
  //This PIN allows the device to be paired with an Client device.
  //Client device users will be prompted to key in a 6-digit PIN, '123456'.
  BLESecurity *pSecurity = new BLESecurity();
  //pSecurity->setStaticPIN(123456);
  //Setting ESP_LE_AUTH_REQ_SC_ONLY instead of the PIN setting eliminates the need for PIN input during pairing.
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);

  ////////// timer count setting //////////
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();
  timer = timerBegin(0, 80, true);    // prescaler (1usec, increment => 80)
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true); // 1000usec timer
  delay(50);
  timerAlarmEnable(timer);            // timer start!!
}

void loop() {
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE){
    // Read the interrupt count and time
    portENTER_CRITICAL(&timerMux);
    portEXIT_CRITICAL(&timerMux);
  }

  ////////////////////////////////////////////////////
  if ( first_timer == true ){
    led_blink(LED1_PIN);
    first_timer = false;
  }
  ////////////////////////////////////////////////////
  if ( second_timer == true ){
    //led_blink(LED1_PIN);

    if (deviceConnected) {
      if (isrequested) {
        char string0[64]; // up to 256

        if (digitalRead(LED2_PIN)){
          //sprintf(string0, "LED2_PIN_8 = ON\r\n");
          sprintf(string0, "0123456789012345678901234567890123456789\r\n");
        } else {
          sprintf(string0, "LED2_PIN_8 = OFF\r\n");
        }

        pTxCharacteristic->setValue(string0);
        //pTxCharacteristic->notify();
        pTxCharacteristic->indicate();
      }
    } else {
      isrequested = false;
    }

    second_timer = false;
  }
  ////////////////////////////////////////////////////
  if ( third_timer == true ){
    led_blink(LED2_PIN);
    third_timer = false;
  }
}