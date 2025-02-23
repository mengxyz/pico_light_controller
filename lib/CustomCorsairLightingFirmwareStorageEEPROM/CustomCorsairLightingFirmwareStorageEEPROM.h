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
#pragma once

#include "CorsairLightingFirmware.h"
#include <I2C_eeprom.h>

#ifndef EEPROM_ADDRESS_DEVICE_ID
#define EEPROM_ADDRESS_DEVICE_ID 0
#endif

class CustomCorsairLightingFirmwareStorageEEPROM : public CorsairLightingFirmwareStorage {
public:
	virtual void loadDeviceID(DeviceID& deviceID) const override;
	virtual void saveDeviceID(const DeviceID& deviceID) override;
   CustomCorsairLightingFirmwareStorageEEPROM(I2C_eeprom &ee) : ee(ee) {}
private:
   I2C_eeprom &ee;
};