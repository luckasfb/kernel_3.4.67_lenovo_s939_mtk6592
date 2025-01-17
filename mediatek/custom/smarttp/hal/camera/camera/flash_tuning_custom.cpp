/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/


#include "camera_custom_nvram.h"
#include "camera_custom_types.h"

#include "camera_custom_AEPlinetable.h"
#ifdef WIN32
#else
#include "camera_custom_nvram.h"
#endif

#include <cutils/xlog.h>
#include "flash_feature.h"
#include "flash_param.h"
#include "flash_tuning_custom.h"
#include <kd_camera_feature.h>
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int cust_isNeedAFLamp(int flashMode, int afLampMode, int isBvHigherTriger)
{
	if(flashMode==FLASHLIGHT_FORCE_OFF)
		return 0;
	if(flashMode==FLASHLIGHT_FORCE_ON)
		return 1;
	//if(afLampMode==AF_LAMP_OFF)
	//	return 0;
	//if(afLampMode==AF_LAMP_ON)
	//	return 1;
	if(isBvHigherTriger==1)
		return 1;
	else
		return 0;
	// #flash mode
	// FLASHLIGHT_AUTO
    // FLASHLIGHT_FORCE_ON,
    // FLASHLIGHT_FORCE_OFF,
    // FLASHLIGHT_REDEYE,
    // FLASHLIGHT_TORCH,

	// #AF lamp mode
	// AF_LAMP_OFF
    // AF_LAMP_ON,
    // AF_LAMP_AUTO,
    // AF_LAMP_FLASH,
}


void copyTuningPara(FLASH_TUNING_PARA* p, NVRAM_FLASH_TUNING_PARA* nv_p)
{
	XLOGD("copyTuningPara ytar =%d %d", p->yTar ,nv_p->yTar);
	p->yTar = nv_p->yTar;
	p->antiIsoLevel = nv_p->antiIsoLevel;
	p->antiExpLevel = nv_p->antiExpLevel;
	p->antiStrobeLevel = nv_p->antiStrobeLevel;
	p->antiUnderLevel = nv_p->antiUnderLevel;
	p->antiOverLevel = nv_p->antiOverLevel;
	p->foregroundLevel = nv_p->foregroundLevel;
	p->isRefAfDistance = nv_p->isRefAfDistance;
    p->accuracyLevel = nv_p->accuracyLevel;
}




FLASH_PROJECT_PARA& cust_getFlashProjectPara(int aeMode, NVRAM_CAMERA_STROBE_STRUCT* nvrame)
{
	static FLASH_PROJECT_PARA para;

	para.dutyNum = 16;
	para.stepNum = 1;
	//tuning



	if(nvrame!=0)
	{
		XLOGD("cust_getFlashProjectPara nvrame->tuningPara[0].yTar=%d", nvrame->tuningPara[0].yTar);
		switch(aeMode)
		{
			case LIB3A_AE_MODE_OFF:
			case LIB3A_AE_MODE_AUTO:
				copyTuningPara(&para.tuningPara, &nvrame->tuningPara[0]);
			break;
			case LIB3A_AE_MODE_NIGHT:
			case LIB3A_AE_MODE_CANDLELIGHT:
			case LIB3A_AE_MODE_FIREWORKS:
			case LIB3A_AE_MODE_NIGHT_PORTRAIT:
				copyTuningPara(&para.tuningPara, &nvrame->tuningPara[1]);
			break;
			case LIB3A_AE_MODE_ACTION:
			case LIB3A_AE_MODE_SPORTS:
			case LIB3A_AE_MODE_ISO_ANTI_SHAKE:
				copyTuningPara(&para.tuningPara, &nvrame->tuningPara[2]);
			break;
			case LIB3A_AE_MODE_PORTRAIT:
				copyTuningPara(&para.tuningPara, &nvrame->tuningPara[3]);
			break;
			default:
				copyTuningPara(&para.tuningPara, &nvrame->tuningPara[0]);
			break;
		}
	}
	//--------------------
	//eng level
	//index mode
	//torch
	para.engLevel.torchEngMode = ENUM_FLASH_ENG_INDEX_MODE;
	para.engLevel.torchDuty = 0;
	para.engLevel.torchStep = 0;

	//af
	para.engLevel.afEngMode = ENUM_FLASH_ENG_INDEX_MODE;
	para.engLevel.afDuty = 0;
	para.engLevel.afStep = 0;

	//pf, mf, normal
	para.engLevel.pmfEngMode = ENUM_FLASH_ENG_INDEX_MODE;
	para.engLevel.pfDuty = 0;
	para.engLevel.mfDutyMax = 15;
	para.engLevel.mfDutyMin = 0;
	para.engLevel.pmfStep = 0;

	//low bat
	para.engLevel.IChangeByVBatEn=0;
	para.engLevel.vBatL = 3400;	//mv
	para.engLevel.pfDutyL = 0;
	para.engLevel.mfDutyMaxL = 2;
	para.engLevel.mfDutyMinL = 2;
	para.engLevel.pmfStepL = 0;

	//burst setting
	para.engLevel.IChangeByBurstEn=1;
	para.engLevel.pfDutyB = 0;
	para.engLevel.mfDutyMaxB = 0;//lenovo.sw 20140410 wangsx3 set max duty=0,in burstmode(continuous mode)
	para.engLevel.mfDutyMinB = 0;
	para.engLevel.pmfStepB = 0;

	//--------------------
	//cooling delay para
	para.coolTimeOutPara.tabMode = ENUM_FLASH_ENG_INDEX_MODE;
	para.coolTimeOutPara.tabNum = 5;
	para.coolTimeOutPara.tabId[0]=0;
	para.coolTimeOutPara.tabId[1]=5;
	para.coolTimeOutPara.tabId[2]=9;
	para.coolTimeOutPara.tabId[3]=12;
	para.coolTimeOutPara.tabId[4]=15;	
	para.coolTimeOutPara.coolingTM[0]=0;
	para.coolTimeOutPara.coolingTM[1]=0;
	para.coolTimeOutPara.coolingTM[2]=1;
	para.coolTimeOutPara.coolingTM[3]=2;
	para.coolTimeOutPara.coolingTM[4]=5;

	para.coolTimeOutPara.timOutMs[0]=ENUM_FLASH_TIME_NO_TIME_OUT;
	para.coolTimeOutPara.timOutMs[1]=ENUM_FLASH_TIME_NO_TIME_OUT;
	para.coolTimeOutPara.timOutMs[2]=1000;
	para.coolTimeOutPara.timOutMs[3]=500;
	para.coolTimeOutPara.timOutMs[4]=500;


	para.maxCapExpTimeUs=100000;
	para.pfExpFollowPline=0;
	//para.maxPfAfe=3000;
	para.engLevel.mfAveIMax = 600;

	//92 new
	para.engLevel.IChangeByBurstEn=1;
	para.yTargetWeight=0;
	para.lowReflectanceThreshold=16;
	para.flashReflectanceWeight = 0;

	
	if(nvrame!=0)
	{
		if(nvrame->isTorchEngUpdate)
		{
			para.engLevel.torchEngMode = nvrame->engLevel.torchEngMode;
			para.engLevel.torchPeakI = nvrame->engLevel.torchPeakI;
		    para.engLevel.torchAveI = nvrame->engLevel.torchAveI;
			para.engLevel.torchDuty = nvrame->engLevel.torchDuty;
			para.engLevel.torchStep = nvrame->engLevel.torchStep;
		}
		if(nvrame->isAfEngUpdate)
		{
			para.engLevel.afEngMode = nvrame->engLevel.afEngMode;
			para.engLevel.afPeakI = nvrame->engLevel.afPeakI;
		    para.engLevel.afAveI = nvrame->engLevel.afAveI;
			para.engLevel.afDuty = nvrame->engLevel.afDuty;
			para.engLevel.afStep = nvrame->engLevel.afStep;
		}
		if(nvrame->isNormaEnglUpdate)
		{
			para.engLevel.pfAveI = nvrame->engLevel.pfAveI;
			para.engLevel.mfAveIMax = nvrame->engLevel.mfAveIMax;
			para.engLevel.mfAveIMin = nvrame->engLevel.mfAveIMin;
			para.engLevel.pmfPeakI = nvrame->engLevel.pmfPeakI;
			para.engLevel.pfDuty = nvrame->engLevel.pfDuty;
			para.engLevel.mfDutyMax = nvrame->engLevel.mfDutyMax;
			para.engLevel.mfDutyMin = nvrame->engLevel.mfDutyMin;
			para.engLevel.pmfStep = nvrame->engLevel.pmfStep;
		}
		if(nvrame->isLowBatEngUpdate)
		{
			para.engLevel.IChangeByVBatEn = nvrame->engLevel.IChangeByVBatEn;
			para.engLevel.vBatL = nvrame->engLevel.vBatL;
			para.engLevel.pfAveIL = nvrame->engLevel.pfAveIL;
			para.engLevel.mfAveIMaxL = nvrame->engLevel.mfAveIMaxL;
			para.engLevel.mfAveIMinL = nvrame->engLevel.mfAveIMinL;
			para.engLevel.pmfPeakIL = nvrame->engLevel.pmfPeakIL;
			para.engLevel.pfDutyL = nvrame->engLevel.pfDutyL;
			para.engLevel.mfDutyMaxL = nvrame->engLevel.mfDutyMaxL;
			para.engLevel.mfDutyMinL = nvrame->engLevel.mfDutyMinL;
			para.engLevel.pmfStepL = nvrame->engLevel.pmfStepL;
		}
		if(nvrame->isBurstEngUpdate)
		{
			para.engLevel.IChangeByBurstEn = nvrame->engLevel.IChangeByBurstEn;
			para.engLevel.pfAveIB = nvrame->engLevel.pfAveIB;
			para.engLevel.mfAveIMaxB = nvrame->engLevel.mfAveIMaxB;
			para.engLevel.mfAveIMaxB = nvrame->engLevel.mfAveIMaxB;
			para.engLevel.mfAveIMinB = nvrame->engLevel.mfAveIMinB;
			para.engLevel.pmfPeakIB = nvrame->engLevel.pmfPeakIB;
			para.engLevel.pfDutyB = nvrame->engLevel.pfDutyB;
			para.engLevel.mfDutyMaxB = nvrame->engLevel.mfDutyMaxB;
			para.engLevel.mfDutyMinB = nvrame->engLevel.mfDutyMinB;
			para.engLevel.pmfStepB = nvrame->engLevel.pmfStepB;
		}
	}
	return para;
}


