/*
   Copyright 2021 Leon Kiefer

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "CustomFastLEDControllerStorageEEPROM.h"

bool CustomFastLEDControllerStorageEEPROM::load(const int index, LEDChannel &channel)
{
   ee.readBlock(EEPROM_ADDRESS + (sizeof(LEDChannel) * index), (uint8_t *)&channel, sizeof(LEDChannel));
   return true;
}

bool CustomFastLEDControllerStorageEEPROM::save(const int index, const LEDChannel &channel)
{
   CLP_LOG(3, F("Save to EEPROM.\r\n"));
#ifdef DEBUG
   Serial.println(F("Save to EEPROM."));
#endif
   ee.writeBlock(EEPROM_ADDRESS + (sizeof(LEDChannel) * index), (uint8_t *)&channel, sizeof(LEDChannel));
   return true;
}

size_t CustomFastLEDControllerStorageEEPROM::getEEPROMSize() { return sizeof(LEDChannel) * CHANNEL_NUM; }
