#include <Arduino.h>
#include <CorsairLightingProtocol.h>
#include <CustomCorsairLightingFirmwareStorageEEPROM.h>
#include <CustomFastLEDControllerStorageEEPROM.h>
#include <FastLED.h>
#include <I2C_eeprom.h>
#include <RP2040_PWM.h>
#include <Wire.h>

// Pin Definitions
#define I2C0_SDA_PIN 0
#define I2C0_SCL_PIN 1
#define I2C0_SLAVE_ADDR 0x71
#define EEPROM_I2C_ADDR 0x50

#define I2C1_SDA_PIN 10
#define I2C1_SCL_PIN 11

#define PWM_CH_1_PIN 21
#define PWM_CH_2_PIN 20
#define PWM_CH_3_PIN 19
#define PWM_CH_4_PIN 18
#define PWM_CH_5_PIN 17

#define ENABLE_PIN 15
#define COOLING_FAN_PWM_PIN 12
#define COOLING_FAN_TAC_PIN 13
#define ARGB_CH1_PIN 8
#define ARGB_CH2_PIN 9
#define ARGB_CH3_PIN 7

#ifdef PWM_DEBUG
#define PWM_DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define PWM_DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define PWM_DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define PWM_DEBUG_PRINT(...)
#define PWM_DEBUG_PRINTLN(...)
#define PWM_DEBUG_PRINTF(...)
#endif

typedef enum { REQ_DUTY, REQ_FREQ, REQ_NONE } req_mode_t;

req_mode_t reqMode = REQ_NONE;

// Global Variables
CRGB ledsChannel1[96];
CRGB ledsChannel2[96];

corsair_product_enum_t PRODUCT = CORSAIR_LIGHTING_NODE_PRO;
I2C_eeprom ee(EEPROM_I2C_ADDR, I2C_DEVICESIZE_24LC256);
CustomCorsairLightingFirmwareStorageEEPROM firmwareStorage(ee);

CorsairLightingFirmware *firmware = nullptr;
CustomFastLEDControllerStorageEEPROM *storage = nullptr;
FastLEDController *ledController = nullptr;
CorsairLightingProtocolController *cLP = nullptr;
CorsairLightingProtocolTinyUSBHID *cHID = nullptr;

#define DEFAULT_PWM_DUTY 50

uint8_t PWM_DUTIES[5] = {DEFAULT_PWM_DUTY, DEFAULT_PWM_DUTY, DEFAULT_PWM_DUTY,
                         DEFAULT_PWM_DUTY, DEFAULT_PWM_DUTY};
const int PWM_PINS[5] = {PWM_CH_1_PIN, PWM_CH_2_PIN, PWM_CH_3_PIN, PWM_CH_4_PIN,
                         PWM_CH_5_PIN};

#define DEFAULT_PWM_FREQ 25000
int PWM_FREQ = DEFAULT_PWM_FREQ;

RP2040_PWM *pwmCh1;
RP2040_PWM *pwmCh2;
RP2040_PWM *pwmCh3;
RP2040_PWM *pwmCh4;
RP2040_PWM *pwmCh5;

volatile bool updatePWM = false;
volatile int updateChannel = -1;
volatile bool initPWM = false;

// Command Definitions
#define CMD_SET_DUTY_CH1 0x10
#define CMD_SET_DUTY_CH2 0x11
#define CMD_SET_DUTY_CH3 0x12
#define CMD_SET_DUTY_CH4 0x13
#define CMD_SET_DUTY_CH5 0x14

#define CMD_BEGIN_PWM 0x01
#define CMD_DISABLE_PWM 0x05
#define CMD_ENABLE_PWM 0x06

#define CMD_SET_FREQUENCY 0x02
#define CMD_GET_DUTY_CYCLE 0x03
#define CMD_GET_FREQUENCY 0x04

// Function Prototypes
void initPWMChannels();
void handleI2CReceive(int howMany);
void handleI2CRequest();
void setDutyCycle(int channel, int duty);
void setFrequency(int freq);
int getDutyCycle(int channel);
int getFrequency();
void enablePWM();
void disablePWM();
void initI2CSlave();

void setup() {
#ifdef PWM_DEBUG
  Serial.begin(115200);
  Serial.println("Initializing PWM Generator...");
#endif

  initI2CSlave();
  initPWMChannels();

  // Initialize secondary I2C (for EEPROM)
  Wire1.setSDA(I2C1_SDA_PIN);
  Wire1.setSCL(I2C1_SCL_PIN);
  ee.begin();

  delay(200);

  // Initialize Corsair Lighting Protocol
  firmware = new CorsairLightingFirmware(PRODUCT, &firmwareStorage);
  storage = new CustomFastLEDControllerStorageEEPROM(ee);
  ledController = new FastLEDController(storage);
  cLP = new CorsairLightingProtocolController(ledController, firmware);
  cHID = new CorsairLightingProtocolTinyUSBHID(cLP);
  cHID->setup();

  // Initialize FastLED
  FastLED.addLeds<WS2812B, ARGB_CH1_PIN, GRB>(ledsChannel1, 96);
  FastLED.addLeds<WS2812B, ARGB_CH2_PIN, GRB>(ledsChannel2, 96);
  ledController->addLEDs(0, ledsChannel1, 96);
  ledController->addLEDs(1, ledsChannel2, 96);

  PWM_DEBUG_PRINTLN("PWM Generator Ready.");
}

void printPWMInfo(RP2040_PWM *PWM_Instance) {
  // Serial.println(dashLine);
  PWM_DEBUG_PRINT("Actual data: pin = ");
  PWM_DEBUG_PRINT(PWM_Instance->getPin());
  PWM_DEBUG_PRINT(", PWM DutyCycle % = ");
  PWM_DEBUG_PRINT(PWM_Instance->getActualDutyCycle() / 1000.0f);
  PWM_DEBUG_PRINT(", PWMPeriod = ");
  PWM_DEBUG_PRINT(PWM_Instance->get_TOP());
  PWM_DEBUG_PRINT(", PWM Freq (Hz) = ");
  PWM_DEBUG_PRINTLN(PWM_Instance->getActualFreq(), 4);
  // Serial.println(dashLine);
}

void handleUpdatePWM() {
  if (updatePWM) {
    switch (updateChannel) {
    case 0:
      pwmCh1->setPWM(PWM_CH_1_PIN, PWM_FREQ, PWM_DUTIES[0]);
      break;
    case 1:
      pwmCh2->setPWM(PWM_CH_2_PIN, PWM_FREQ, PWM_DUTIES[1]);
      break;
    case 2:
      pwmCh3->setPWM(PWM_CH_3_PIN, PWM_FREQ, PWM_DUTIES[2]);
      break;
    case 3:
      pwmCh4->setPWM(PWM_CH_4_PIN, PWM_FREQ, PWM_DUTIES[3]);
      break;
    case 4:
      pwmCh5->setPWM(PWM_CH_5_PIN, PWM_FREQ, PWM_DUTIES[4]);
      break;
    default:
      break;
    }
    PWM_DEBUG_PRINTF("PWM Updated ch %d, duty %d\n", updateChannel, PWM_DUTIES[updateChannel]);
    updatePWM = false;
    updateChannel = -1;
  }
}

uint16_t prevMillis = 0;

void loop() {
  // Update PWM if needed
  handleUpdatePWM();

  // Update Corsair Lighting Protocol
  if (cHID) {
    cHID->update();
    if (ledController->updateLEDs()) {
      FastLED.show();
    }
  }
}

void initI2CSlave() {
  Wire.setSDA(I2C0_SDA_PIN);
  Wire.setSCL(I2C0_SCL_PIN);
  Wire.setClock(100000);
  Wire.begin(I2C0_SLAVE_ADDR);
  Wire.onReceive(handleI2CReceive);
  Wire.onRequest(handleI2CRequest);
}

void initPWMChannels() {
  if (initPWM) {
    return;
  }

  pwmCh1 = new RP2040_PWM(PWM_CH_1_PIN, PWM_FREQ, PWM_DUTIES[0]);
  pwmCh2 = new RP2040_PWM(PWM_CH_2_PIN, PWM_FREQ, PWM_DUTIES[1]);
  pwmCh3 = new RP2040_PWM(PWM_CH_3_PIN, PWM_FREQ, PWM_DUTIES[2]);
  pwmCh4 = new RP2040_PWM(PWM_CH_4_PIN, PWM_FREQ, PWM_DUTIES[3]);
  pwmCh5 = new RP2040_PWM(PWM_CH_5_PIN, PWM_FREQ, PWM_DUTIES[4]);

  pwmCh1->setPWM();
  pwmCh2->setPWM();
  pwmCh3->setPWM();
  pwmCh4->setPWM();
  pwmCh5->setPWM();

  initPWM = true;
}

void handleI2CReceive(int howMany) {
  if (Wire.available()) {
    byte command = Wire.read();
    if (command == CMD_SET_DUTY_CH1) {
      PWM_DEBUG_PRINTF("Received command: 0x%02X\n", command);
    }
    switch (command) {
    case CMD_SET_DUTY_CH1:
      if (Wire.available()) {
        uint8_t duty = Wire.read();
        setDutyCycle(0, duty);
      }
      break;
    case CMD_SET_DUTY_CH2:
      if (Wire.available()) {
        uint8_t duty = Wire.read();
        setDutyCycle(1, duty);
      }
      break;
    case CMD_SET_DUTY_CH3:
      if (Wire.available()) {
        uint8_t duty = Wire.read();
        setDutyCycle(2, duty);
      }
      break;
    case CMD_SET_DUTY_CH4:
      if (Wire.available()) {
        uint8_t duty = Wire.read();
        setDutyCycle(3, duty);
      }
      break;
    case CMD_SET_DUTY_CH5:
      if (Wire.available()) {
        uint8_t duty = Wire.read();
        setDutyCycle(4, duty);
      }
      break;
    case CMD_SET_FREQUENCY:
      if (Wire.available()) {
        int freq = Wire.read() * 1000;
        setFrequency(freq);
      }
      break;
    case CMD_GET_DUTY_CYCLE:
      reqMode = REQ_DUTY;
      break;
    case CMD_GET_FREQUENCY:
      reqMode = REQ_FREQ;
      break;
    case CMD_BEGIN_PWM:
      // initPWMChannels();
      break;
    case CMD_ENABLE_PWM:
      enablePWM();
      break;
    case CMD_DISABLE_PWM:
      disablePWM();
      break;
    default:
      PWM_DEBUG_PRINTF("Error: Unknown command (%d) received.\n", command);
      break;
    }
  }
}

void handleI2CRequest() {
  if (reqMode == REQ_NONE) {
    Wire.write(0);
    return;
  }
  if (reqMode == REQ_DUTY) {
    Wire.write(PWM_DUTIES, 5);
    reqMode = REQ_NONE;
    return;
  } else if (reqMode == REQ_FREQ) {
    Wire.write(PWM_FREQ);
    reqMode = REQ_NONE;
    return;
  }
}

void setDutyCycle(int channel, int duty) {
  if (channel >= 0 && channel < 5 && duty >= 0 && duty <= 100) {
    PWM_DUTIES[channel] = duty;
    if (!initPWM) {
      PWM_DEBUG_PRINTLN("SET DUTY WITHOUT INIT");
      return;
    }
    updatePWM = true;
    updateChannel = channel;
  } else {
    PWM_DEBUG_PRINTLN("SET_DUTY_CYCLE: Error: Invalid channel or duty cycle.");
  }
}

void setFrequency(int freq) {
  if (freq > 0) {
    PWM_FREQ = freq;
    for (int i = 0; i < 5; i++) {
      updatePWM = true;
      updateChannel = i;
    }
    PWM_DEBUG_PRINTF("SET_FREQUENCY: Frequency set to %d Hz\n", freq);
  } else {
    PWM_DEBUG_PRINTLN("SET_FREQUENCY: Error: Invalid frequency.");
  }
}

void enablePWM() {
  if (!initPWM) {
    return;
  }
  pwmCh1->enablePWM();
  pwmCh2->enablePWM();
  pwmCh3->enablePWM();
  pwmCh4->enablePWM();
  pwmCh5->enablePWM();
}

void disablePWM() {
  if (!initPWM) {
    return;
  }
  pwmCh1->disablePWM();
  pwmCh2->disablePWM();
  pwmCh3->disablePWM();
  pwmCh4->disablePWM();
  pwmCh5->disablePWM();
}

int getFrequency() { return PWM_FREQ; }