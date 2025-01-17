/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef __HWMSEN_CUSTOM_H__ 
#define __HWMSEN_CUSTOM_H__
/* lenovo-sh molg1 add for phaeton 20131202 begin */
#ifdef CUSTOM_KERNEL_ACCELEROMETER
	#define ACCELEROMETER 		"BMA056 3-axis Accelerometer"
	#define ACCELEROMETER_VENDER 	"Bosch"
#endif

#ifdef CUSTOM_KERNEL_ALSPS
	#define PROXIMITY 		"APDS9930 Proximity Sensor"
	#define PROXIMITY_VENDER 	"AVAGO"
	#define LIGHT 			"APDS9930 Light Sensor"
	#define LIGHT_VENDER 		"AVAGO"
#endif

#ifdef CUSTOM_KERNEL_MAGNETOMETER
	#define MAGNETOMETER 		"BMM056 3-axis Magnetic Field sensor"
	#define MAGNETOMETER_VENDER 	"Bosch"
	#define ORIENTATION 		"BMM056 Orientation sensor"
	#define ORIENTATION_VENDER 	"Bosch"
#endif

//#ifdef CUSTOM_KERNEL_GYROSCOPE
	#define GYROSCOPE 		"BMM056 software  gyroscope Sensor"
	#define GYROSCOPE_VENDER 	"Bosch"

#ifdef CUSTOM_KERNEL_BAROMETER
	#define PRESSURE 		"BMA050"
	#define PRESSURE_VENDER		"BMA050"
#endif

#ifdef CUSTOM_KERNEL_TEMPURATURE
	#define TEMPURATURE 		"BMA050"
	#define TEMPURATURE_VENDER	"BMA050"
#endif
/* lenovo-sh molg1 add for phaeton 20131202 end */
#endif

