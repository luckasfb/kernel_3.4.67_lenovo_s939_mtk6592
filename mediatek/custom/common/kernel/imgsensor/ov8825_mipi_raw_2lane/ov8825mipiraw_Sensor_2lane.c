/*******************************************************************************************/
      

/*******************************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>    
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov8825mipiraw_Sensor_2lane.h"
#include "ov8825mipiraw_Camera_Sensor_para_2lane.h"
#include "ov8825mipiraw_CameraCustomized_2lane.h"
static DEFINE_SPINLOCK(ov8825mipiraw_drv_lock);

#define OV8825_DEBUG
//#define OV8825_DEBUG_SOFIA

#ifdef OV8825_DEBUG
	#define OV8825DB(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[OV8825MIPI]" , fmt, ##arg)
#else
	#define OV8825DB(x,...)
#endif

#ifdef OV8825_DEBUG_SOFIA
	#define OV8825DBSOFIA(fmt, arg...) xlog_printk(ANDROID_LOG_DEBUG, "[OV8825MIPI]", fmt, ##arg)
#else
	#define OV8825DBSOFIA(x,...)
#endif

#define mDELAY(ms)  mdelay(ms)

kal_uint32 OV8825_FeatureControl_PERIOD_PixelNum=OV8825_PV_PERIOD_PIXEL_NUMS;
kal_uint32 OV8825_FeatureControl_PERIOD_LineNum=OV8825_PV_PERIOD_LINE_NUMS;

UINT16 VIDEO_MODE_TARGET_FPS = 30;
static BOOL ReEnteyCamera = KAL_FALSE;


MSDK_SENSOR_CONFIG_STRUCT OV8825SensorConfigData;

kal_uint32 OV8825_FAC_SENSOR_REG;

MSDK_SCENARIO_ID_ENUM OV8825CurrentScenarioId = MSDK_SCENARIO_ID_CAMERA_PREVIEW;

/* FIXME: old factors and DIDNOT use now. s*/
SENSOR_REG_STRUCT OV8825SensorCCT[]=CAMERA_SENSOR_CCT_DEFAULT_VALUE;
SENSOR_REG_STRUCT OV8825SensorReg[ENGINEER_END]=CAMERA_SENSOR_REG_DEFAULT_VALUE;
/* FIXME: old factors and DIDNOT use now. e*/

static OV8825_PARA_STRUCT ov8825;

extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);

#define OV8825_write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1, OV8825MIPI_WRITE_ID)

kal_uint16 OV8825_read_cmos_sensor(kal_uint32 addr)
{
kal_uint16 get_byte=0;
    iReadReg((u16) addr ,(u8*)&get_byte,OV8825MIPI_WRITE_ID);
    return get_byte;
}

#define Sleep(ms) mdelay(ms)

void OV8825_write_shutter(kal_uint32 shutter)
{
	kal_uint32 min_framelength = OV8825_PV_PERIOD_PIXEL_NUMS, max_shutter=0;
	kal_uint32 extra_lines = 0;
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;
	unsigned long flags;

	OV8825DBSOFIA("!!shutter=%d!!!!!\n", shutter);

	

	if(ov8825.OV8825AutoFlickerMode == KAL_TRUE)
	{
		if ( SENSOR_MODE_PREVIEW == ov8825.sensorMode )  //(g_iOV8825_Mode == OV8825_MODE_PREVIEW)	//SXGA size output
		{
			line_length = OV8825_PV_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
			max_shutter = OV8825_PV_PERIOD_LINE_NUMS + ov8825.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == ov8825.sensorMode ) 
		{
			line_length = OV8825_VIDEO_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
			max_shutter = OV8825_VIDEO_PERIOD_LINE_NUMS + ov8825.DummyLines ;
		}
		else
		{
			line_length = OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
			max_shutter = OV8825_FULL_PERIOD_LINE_NUMS + ov8825.DummyLines ;
		}

		switch(OV8825CurrentScenarioId)
		{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				OV8825DBSOFIA("AutoFlickerMode!!! MSDK_SCENARIO_ID_CAMERA_ZSD  0!!\n");
				#if defined(ZSD15FPS)
				min_framelength = (ov8825.capPclk*10000) /(OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/148*10 ;
				#else
				min_framelength = (ov8825.capPclk*10000) /(OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/130*10 ;//13fps
				#endif
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				if(VIDEO_MODE_TARGET_FPS==30)
				{
					min_framelength = (ov8825.videoPclk*10000) /(OV8825_VIDEO_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/306*10 ;
				}
				else if(VIDEO_MODE_TARGET_FPS==15)
				{
					min_framelength = (ov8825.videoPclk*10000) /(OV8825_VIDEO_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/148*10 ;
				}
				else
				{
					min_framelength = max_shutter;
				}
				break;
			default:
				min_framelength = (ov8825.pvPclk*10000) /(OV8825_PV_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/296*10 ;
    			break;
		}

		OV8825DBSOFIA("AutoFlickerMode!!! min_framelength for AutoFlickerMode = %d (0x%x)\n",min_framelength,min_framelength);
		OV8825DBSOFIA("max framerate(10 base) autofilker = %d\n",(ov8825.pvPclk*10000)*10 /line_length/min_framelength);

		if (shutter < 3)
			shutter = 3;

		if (shutter > max_shutter-4)
			extra_lines = shutter - max_shutter + 4;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == ov8825.sensorMode )	//SXGA size output
		{
			frame_length = OV8825_PV_PERIOD_LINE_NUMS+ ov8825.DummyLines + extra_lines ;
		}
		else if(SENSOR_MODE_VIDEO == ov8825.sensorMode)
		{
			frame_length = OV8825_VIDEO_PERIOD_LINE_NUMS+ ov8825.DummyLines + extra_lines ;
		}
		else				//QSXGA size output
		{
			frame_length = OV8825_FULL_PERIOD_LINE_NUMS + ov8825.DummyLines + extra_lines ;
		}
		OV8825DBSOFIA("frame_length 0= %d\n",frame_length);

		if (frame_length < min_framelength)
		{
			//shutter = min_framelength - 4;

			switch(OV8825CurrentScenarioId)
			{
        	case MSDK_SCENARIO_ID_CAMERA_ZSD:
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				extra_lines = min_framelength- (OV8825_FULL_PERIOD_LINE_NUMS+ ov8825.DummyLines);
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				extra_lines = min_framelength- (OV8825_VIDEO_PERIOD_LINE_NUMS+ ov8825.DummyLines);
				break;
			default:
				extra_lines = min_framelength- (OV8825_PV_PERIOD_LINE_NUMS+ ov8825.DummyLines);
    			break;
			}
			frame_length = min_framelength;
		}

		OV8825DBSOFIA("frame_length 1= %d\n",frame_length);

		ASSERT(line_length < OV8825_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < OV8825_MAX_FRAME_LENGTH); 	//0xFFFF

		//Set total frame length
		OV8825_write_cmos_sensor(0x380e, (frame_length >> 8) & 0xFF);
		OV8825_write_cmos_sensor(0x380f, frame_length & 0xFF);
		spin_lock_irqsave(&ov8825mipiraw_drv_lock,flags);
		ov8825.maxExposureLines = frame_length;
		OV8825_FeatureControl_PERIOD_PixelNum = line_length;
		OV8825_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&ov8825mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		OV8825_write_cmos_sensor(0x3500, (shutter>>12) & 0x0F);
		OV8825_write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
		OV8825_write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);	/* Don't use the fraction part. */

		OV8825DBSOFIA("frame_length 2= %d\n",frame_length);
		//OV8825DB("framerate(10 base) = %d\n",(ov8825.pvPclk*10000)*10 /line_length/frame_length);

		OV8825DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);

	}
	else
	{
		if ( SENSOR_MODE_PREVIEW == ov8825.sensorMode )  //(g_iOV8825_Mode == OV8825_MODE_PREVIEW)	//SXGA size output
		{
			max_shutter = OV8825_PV_PERIOD_LINE_NUMS + ov8825.DummyLines ;
		}
		else if( SENSOR_MODE_VIDEO == ov8825.sensorMode ) //add for video_6M setting
		{
			max_shutter = OV8825_VIDEO_PERIOD_LINE_NUMS + ov8825.DummyLines ;
		}
		else
		{
			max_shutter = OV8825_FULL_PERIOD_LINE_NUMS + ov8825.DummyLines ;
		}

		if (shutter < 3)
			shutter = 3;

		if (shutter > max_shutter-4)
			extra_lines = shutter - max_shutter + 4;
		else
			extra_lines = 0;

		if ( SENSOR_MODE_PREVIEW == ov8825.sensorMode )	//SXGA size output
		{
			line_length = OV8825_PV_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
			frame_length = OV8825_PV_PERIOD_LINE_NUMS+ ov8825.DummyLines + extra_lines ;
		}
		else if( SENSOR_MODE_VIDEO == ov8825.sensorMode )
		{
			line_length = OV8825_VIDEO_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
			frame_length = OV8825_VIDEO_PERIOD_LINE_NUMS + ov8825.DummyLines + extra_lines ;
		}
		else				//QSXGA size output
		{
			line_length = OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
			frame_length = OV8825_FULL_PERIOD_LINE_NUMS + ov8825.DummyLines + extra_lines ;
		}

		ASSERT(line_length < OV8825_MAX_LINE_LENGTH);		//0xCCCC
		ASSERT(frame_length < OV8825_MAX_FRAME_LENGTH); 	//0xFFFF

		//Set total frame length
		OV8825_write_cmos_sensor(0x380e, (frame_length >> 8) & 0xFF);
		OV8825_write_cmos_sensor(0x380f, frame_length & 0xFF);
		spin_lock_irqsave(&ov8825mipiraw_drv_lock,flags);
		ov8825.maxExposureLines = frame_length -4;
		OV8825_FeatureControl_PERIOD_PixelNum = line_length;
		OV8825_FeatureControl_PERIOD_LineNum = frame_length;
		spin_unlock_irqrestore(&ov8825mipiraw_drv_lock,flags);

		//Set shutter (Coarse integration time, uint: lines.)
		OV8825_write_cmos_sensor(0x3500, (shutter>>12) & 0x0F);
		OV8825_write_cmos_sensor(0x3501, (shutter>>4) & 0xFF);
		OV8825_write_cmos_sensor(0x3502, (shutter<<4) & 0xF0);	/* Don't use the fraction part. */

		//OV8825DB("framerate(10 base) = %d\n",(ov8825.pvPclk*10000)*10 /line_length/frame_length);

		OV8825DB("shutter=%d, extra_lines=%d, line_length=%d, frame_length=%d\n", shutter, extra_lines, line_length, frame_length);
	}

}   /* write_OV8825_shutter */

/*******************************************************************************
*
********************************************************************************/
static kal_uint16 OV8825Reg2Gain(const kal_uint16 iReg)
{
    kal_uint8 iI;
    kal_uint16 iGain = ov8825.ispBaseGain;    // 1x-gain base

    // Range: 1x to 32x
    // Gain = (GAIN[7] + 1) * (GAIN[6] + 1) * (GAIN[5] + 1) * (GAIN[4] + 1) * (1 + GAIN[3:0] / 16)
    for (iI = 8; iI >= 4; iI--) {
        iGain *= (((iReg >> iI) & 0x01) + 1);
    }
	OV8825DBSOFIA("[OV8825Reg2Gain]real gain= %d\n",(iGain +  (iGain * (iReg & 0x0F)) / 16));
    return iGain +  (iGain * (iReg & 0x0F)) / 16; //ov8825.realGain
}

/*******************************************************************************
*
********************************************************************************/
static kal_uint16 OV8825Gain2Reg(const kal_uint16 Gain)
{
    kal_uint16 iReg = 0x0000;
	kal_uint16 iGain=Gain;
	if(iGain <  ov8825.ispBaseGain) {
		iReg =0;
	}    else if (iGain < 2 * ov8825.ispBaseGain) {
        iReg = 16 * iGain / ov8825.ispBaseGain - 16;
    }else if (iGain < 4 * ov8825.ispBaseGain) {
        iReg |= 0x10;
        iReg |= (8 *iGain / ov8825.ispBaseGain - 16);
    }else if (iGain < 8 * ov8825.ispBaseGain) {
        iReg |= 0x30;
        iReg |= (4 * iGain / ov8825.ispBaseGain - 16);
    }else if (iGain < 16 * ov8825.ispBaseGain) {
        iReg |= 0x70;
        iReg |= (2 * iGain /ov8825.ispBaseGain - 16);
    }else if(iGain < 32 * ov8825.ispBaseGain) {
        iReg |= 0xF0;
        iReg |= (iGain /ov8825.ispBaseGain - 16);
    }else if(iGain <= 62 * ov8825.ispBaseGain) {
    	iReg |= 0x1F0;
        iReg |= (iGain /ov8825.ispBaseGain/2 - 16);
    }
	else
	{
		OV8825DB("out of range!\n");
	}
	OV8825DBSOFIA("[OV8825Gain2Reg]: isp gain:%d,sensor gain:0x%x\n",iGain,iReg);

    return iReg;//ov8825. sensorGlobalGain

}


void write_OV8825_gain(kal_uint16 gain)
{

	OV8825_write_cmos_sensor(0x350a,(gain>>8));
	OV8825_write_cmos_sensor(0x350b,(gain&0xff));

	return;
}

/*************************************************************************
* FUNCTION
*    OV8825_SetGain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    gain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8825_SetGain(UINT16 iGain)
{
	unsigned long flags;

	
	OV8825DB("noise debug iGain=%d",iGain);
	
	spin_lock_irqsave(&ov8825mipiraw_drv_lock,flags);
	ov8825.realGain = iGain;
	ov8825.sensorGlobalGain = OV8825Gain2Reg(iGain);
	spin_unlock_irqrestore(&ov8825mipiraw_drv_lock,flags);

	write_OV8825_gain(ov8825.sensorGlobalGain);
	OV8825DB("[OV8825_SetGain]ov8825.sensorGlobalGain=0x%x,ov8825.realGain=%d\n",ov8825.sensorGlobalGain,ov8825.realGain);

}   /*  OV8825_SetGain_SetGain  */


/*************************************************************************
* FUNCTION
*    read_OV8825_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_uint16 read_OV8825_gain(void)
{
	kal_uint16 read_gain=0;

	read_gain=(((OV8825_read_cmos_sensor(0x350a)&0x01) << 8) | OV8825_read_cmos_sensor(0x350b));

	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.sensorGlobalGain = read_gain;
	ov8825.realGain = OV8825Reg2Gain(ov8825.sensorGlobalGain);
	spin_unlock(&ov8825mipiraw_drv_lock);

	OV8825DB("ov8825.sensorGlobalGain=0x%x,ov8825.realGain=%d\n",ov8825.sensorGlobalGain,ov8825.realGain);

	return ov8825.sensorGlobalGain;
}  /* read_OV8825_gain */


void OV8825_camera_para_to_sensor(void)
{
    kal_uint32    i;
    for(i=0; 0xFFFFFFFF!=OV8825SensorReg[i].Addr; i++)
    {
        OV8825_write_cmos_sensor(OV8825SensorReg[i].Addr, OV8825SensorReg[i].Para);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=OV8825SensorReg[i].Addr; i++)
    {
        OV8825_write_cmos_sensor(OV8825SensorReg[i].Addr, OV8825SensorReg[i].Para);
    }
    for(i=FACTORY_START_ADDR; i<FACTORY_END_ADDR; i++)
    {
        OV8825_write_cmos_sensor(OV8825SensorCCT[i].Addr, OV8825SensorCCT[i].Para);
    }
}


/*************************************************************************
* FUNCTION
*    OV8825_sensor_to_camera_para
*
* DESCRIPTION
*    // update camera_para from sensor register
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8825_sensor_to_camera_para(void)
{
    kal_uint32    i, temp_data;
    for(i=0; 0xFFFFFFFF!=OV8825SensorReg[i].Addr; i++)
    {
         temp_data = OV8825_read_cmos_sensor(OV8825SensorReg[i].Addr);
		 spin_lock(&ov8825mipiraw_drv_lock);
		 OV8825SensorReg[i].Para =temp_data;
		 spin_unlock(&ov8825mipiraw_drv_lock);
    }
    for(i=ENGINEER_START_ADDR; 0xFFFFFFFF!=OV8825SensorReg[i].Addr; i++)
    {
        temp_data = OV8825_read_cmos_sensor(OV8825SensorReg[i].Addr);
		spin_lock(&ov8825mipiraw_drv_lock);
		OV8825SensorReg[i].Para = temp_data;
		spin_unlock(&ov8825mipiraw_drv_lock);
    }
}

/*************************************************************************
* FUNCTION
*    OV8825_get_sensor_group_count
*
* DESCRIPTION
*    //
*
* PARAMETERS
*    None
*
* RETURNS
*    gain : sensor global gain(base: 0x40)
*
* GLOBALS AFFECTED
*
*************************************************************************/
kal_int32  OV8825_get_sensor_group_count(void)
{
    return GROUP_TOTAL_NUMS;
}

void OV8825_get_sensor_group_info(kal_uint16 group_idx, kal_int8* group_name_ptr, kal_int32* item_count_ptr)
{
   switch (group_idx)
   {
        case PRE_GAIN:
            sprintf((char *)group_name_ptr, "CCT");
            *item_count_ptr = 2;
            break;
        case CMMCLK_CURRENT:
            sprintf((char *)group_name_ptr, "CMMCLK Current");
            *item_count_ptr = 1;
            break;
        case FRAME_RATE_LIMITATION:
            sprintf((char *)group_name_ptr, "Frame Rate Limitation");
            *item_count_ptr = 2;
            break;
        case REGISTER_EDITOR:
            sprintf((char *)group_name_ptr, "Register Editor");
            *item_count_ptr = 2;
            break;
        default:
            ASSERT(0);
}
}

void OV8825_get_sensor_item_info(kal_uint16 group_idx,kal_uint16 item_idx, MSDK_SENSOR_ITEM_INFO_STRUCT* info_ptr)
{
    kal_int16 temp_reg=0;
    kal_uint16 temp_gain=0, temp_addr=0, temp_para=0;

    switch (group_idx)
    {
        case PRE_GAIN:
           switch (item_idx)
          {
              case 0:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-R");
                  temp_addr = PRE_GAIN_R_INDEX;
              break;
              case 1:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gr");
                  temp_addr = PRE_GAIN_Gr_INDEX;
              break;
              case 2:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-Gb");
                  temp_addr = PRE_GAIN_Gb_INDEX;
              break;
              case 3:
                sprintf((char *)info_ptr->ItemNamePtr,"Pregain-B");
                  temp_addr = PRE_GAIN_B_INDEX;
              break;
              case 4:
                 sprintf((char *)info_ptr->ItemNamePtr,"SENSOR_BASEGAIN");
                 temp_addr = SENSOR_BASEGAIN;
              break;
              default:
                 ASSERT(0);
          }

            temp_para= OV8825SensorCCT[temp_addr].Para;
			//temp_gain= (temp_para/ov8825.sensorBaseGain) * 1000;

            info_ptr->ItemValue=temp_gain;
            info_ptr->IsTrueFalse=KAL_FALSE;
            info_ptr->IsReadOnly=KAL_FALSE;
            info_ptr->IsNeedRestart=KAL_FALSE;
            info_ptr->Min= OV8825_MIN_ANALOG_GAIN * 1000;
            info_ptr->Max= OV8825_MAX_ANALOG_GAIN * 1000;
            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Drv Cur[2,4,6,8]mA");

                    //temp_reg=MT9P017SensorReg[CMMCLK_CURRENT_INDEX].Para;
                    temp_reg = ISP_DRIVING_2MA;
                    if(temp_reg==ISP_DRIVING_2MA)
                    {
                        info_ptr->ItemValue=2;
                    }
                    else if(temp_reg==ISP_DRIVING_4MA)
                    {
                        info_ptr->ItemValue=4;
                    }
                    else if(temp_reg==ISP_DRIVING_6MA)
                    {
                        info_ptr->ItemValue=6;
                    }
                    else if(temp_reg==ISP_DRIVING_8MA)
                    {
                        info_ptr->ItemValue=8;
                    }

                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_TRUE;
                    info_ptr->Min=2;
                    info_ptr->Max=8;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"Max Exposure Lines");
                    info_ptr->ItemValue=    111;  //MT9P017_MAX_EXPOSURE_LINES;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"Min Frame Rate");
                    info_ptr->ItemValue=12;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_TRUE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0;
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Addr.");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                case 1:
                    sprintf((char *)info_ptr->ItemNamePtr,"REG Value");
                    info_ptr->ItemValue=0;
                    info_ptr->IsTrueFalse=KAL_FALSE;
                    info_ptr->IsReadOnly=KAL_FALSE;
                    info_ptr->IsNeedRestart=KAL_FALSE;
                    info_ptr->Min=0;
                    info_ptr->Max=0xFFFF;
                    break;
                default:
                ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
}



kal_bool OV8825_set_sensor_item_info(kal_uint16 group_idx, kal_uint16 item_idx, kal_int32 ItemValue)
{
//   kal_int16 temp_reg;
   kal_uint16  temp_gain=0,temp_addr=0, temp_para=0;

   switch (group_idx)
    {
        case PRE_GAIN:
            switch (item_idx)
            {
              case 0:
                temp_addr = PRE_GAIN_R_INDEX;
              break;
              case 1:
                temp_addr = PRE_GAIN_Gr_INDEX;
              break;
              case 2:
                temp_addr = PRE_GAIN_Gb_INDEX;
              break;
              case 3:
                temp_addr = PRE_GAIN_B_INDEX;
              break;
              case 4:
                temp_addr = SENSOR_BASEGAIN;
              break;
              default:
                 ASSERT(0);
          }

		 temp_gain=((ItemValue*BASEGAIN+500)/1000);			//+500:get closed integer value

		  if(temp_gain>=1*BASEGAIN && temp_gain<=16*BASEGAIN)
          {
//             temp_para=(temp_gain * ov8825.sensorBaseGain + BASEGAIN/2)/BASEGAIN;
          }
          else
			  ASSERT(0);

			 OV8825DBSOFIA("OV8825????????????????????? :\n ");
		  spin_lock(&ov8825mipiraw_drv_lock);
          OV8825SensorCCT[temp_addr].Para = temp_para;
		  spin_unlock(&ov8825mipiraw_drv_lock);
          OV8825_write_cmos_sensor(OV8825SensorCCT[temp_addr].Addr,temp_para);

            break;
        case CMMCLK_CURRENT:
            switch (item_idx)
            {
                case 0:
                    //no need to apply this item for driving current
                    break;
                default:
                    ASSERT(0);
            }
            break;
        case FRAME_RATE_LIMITATION:
            ASSERT(0);
            break;
        case REGISTER_EDITOR:
            switch (item_idx)
            {
                case 0:
					spin_lock(&ov8825mipiraw_drv_lock);
                    OV8825_FAC_SENSOR_REG=ItemValue;
					spin_unlock(&ov8825mipiraw_drv_lock);
                    break;
                case 1:
                    OV8825_write_cmos_sensor(OV8825_FAC_SENSOR_REG,ItemValue);
                    break;
                default:
                    ASSERT(0);
            }
            break;
        default:
            ASSERT(0);
    }
    return KAL_TRUE;
}

#define OV8825_OTP
#ifdef OV8825_OTP

#define OV8825_SHADING_ADD 0x5000
#define OV8825_SHADING_VALUE_ADD 0x5800
#define OV8825_SHADING_OFF_VALUE 0x06
#define OV8825_SHADING_ON_VALUE 0X86
#define BANK_SIZE 32
#define SHADING_EMPTY 0
#define SHADING_VALID 2
#define SHADING_INVALID 1

//*******golden sample value: module vendor provide********//
#define GOLDEN_RG_RATIO 0x5b  //R/G 85.95  =86=0x56
#define GOLDEN_BG_RATIO 0x5a  //B/G  88.67 =89=0x59
//*******golden sample value: module vendor provide********//

#define M_ID 0
#define LENS_ID 0


kal_bool OV8825OTPWriteSensor(kal_uint16 address, kal_uint16 para)
{
	 OV8825_write_cmos_sensor(address, para);	
}
	
kal_uint16 OV8825OTPReadSensor(kal_uint16 address)
{
	kal_uint16 reVal;
	reVal = OV8825_read_cmos_sensor(address);	
	OV8825DB("!!!!!adress=0x%x, reVal= 0x%x!!!!!\n", address, reVal);
	return reVal;
}
	

u16 OV8825_update_awb_gain(u16 R_gain, u16 G_gain, u16 B_gain)
{
	OV8825DB("OV8825_update_awb_gain ENTER :\n ");

	OV8825DB("OV8825_update_awb_gain R_gain =0x%x, G_gain=0x%x, B_gain=0x%x\n",R_gain, G_gain, B_gain);
	
	if (R_gain>0x400) {
		OV8825_write_cmos_sensor(0x3400, R_gain>>8);
		OV8825_write_cmos_sensor(0x3401, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
		OV8825_write_cmos_sensor(0x3402, G_gain>>8);
		OV8825_write_cmos_sensor(0x3403, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
		OV8825_write_cmos_sensor(0x3404, B_gain>>8);
		OV8825_write_cmos_sensor(0x3405, B_gain & 0x00ff);
	}
	return 0;
}



u8 OV8825_update_otp_wb(u8 Cur_RG_ratio,u8 Cur_BG_ratio)
{
u32 R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
u32 rg = Cur_RG_ratio;
u32 bg = Cur_BG_ratio;

OV8825DB("OV8825_update_otp_wb rg=0x%x,bg=0x%x\n", Cur_RG_ratio,Cur_BG_ratio);

//calculate G gain
//0x400 = 1x gain
	if(bg < GOLDEN_BG_RATIO) {
		if (rg< GOLDEN_RG_RATIO) {
	// current_otp.bg_ratio < BG_Ratio_typical &&
	// current_otp.rg_ratio < RG_Ratio_typical
		G_gain = 0x400;
		B_gain = (0x400 * GOLDEN_BG_RATIO) / bg;
		R_gain = (0x400 * GOLDEN_RG_RATIO) / rg;
		}
		else {
	// current_otp.bg_ratio < BG_Ratio_typical &&
	// current_otp.rg_ratio >= RG_Ratio_typical
		R_gain = 0x400;
		G_gain = (0x400 * rg) / GOLDEN_RG_RATIO;
		B_gain = (G_gain * GOLDEN_BG_RATIO) /bg;
		}
	}
	else {
		if (rg < GOLDEN_RG_RATIO) {
	// current_otp.bg_ratio >= BG_Ratio_typical &&
	// current_otp.rg_ratio < RG_Ratio_typical
		B_gain = 0x400;
		G_gain = (0x400 * bg)/ GOLDEN_BG_RATIO;
		R_gain = (G_gain * GOLDEN_RG_RATIO)/ rg;
		}
		else {
	// current_otp.bg_ratio >= BG_Ratio_typical &&
	// current_otp.rg_ratio >= RG_Ratio_typical
		G_gain_B = (0x400 * bg) / GOLDEN_BG_RATIO;
		G_gain_R = (0x400 * rg) / GOLDEN_RG_RATIO;
		
		if(G_gain_B > G_gain_R ) {
		B_gain = 0x400;
		G_gain = G_gain_B;
		R_gain = (G_gain * GOLDEN_RG_RATIO) /rg;
		}
		else {
		R_gain = 0x400;
		G_gain = G_gain_R;
		B_gain = (G_gain * GOLDEN_BG_RATIO) / bg;
		}
		}
	}
	
	OV8825_update_awb_gain(R_gain, G_gain, B_gain);
	return 0;
}


//read data from the address of bank and save the data in iBuffer
kal_bool OV8825_ReadOtp(u16 Bank,u16 address,unsigned char*iBuffer,u16 buffersize)
{
kal_bool res = KAL_TRUE;
u16 reVal;
u8 tempBank = (u8)Bank;
u16 tempAddress = 0x3d00+address;
u16 j = 0;
u16 i=0,k=0;

	OV8825DB("OV8825_ReadOtp ENTER :\n ");

	while (i<buffersize)
	{
		j=0;
		tempBank = ((tempBank & 0x07) | 0x08);
		
		OV8825DB("OV8825_ReadOtp tempBank=%d\n", tempBank);
		OV8825OTPWriteSensor(0x3d84, tempBank);//select bank
		OV8825OTPWriteSensor(0x3d81, 0x01);//select bank
		Sleep(20);//10 -> 100
		OV8825OTPWriteSensor(0x3d81,0x00);//select bank
		while (j<32)
		{
			reVal= OV8825OTPReadSensor(tempAddress);
			*(iBuffer+i) =(u8)reVal;
			i++;
			j++;
			
			if (i>=buffersize)
			{
				break;
			}
			tempAddress++;
			if (tempAddress>0x3d1f)
			{
				tempAddress = 0x3d00;
				break;
			}
		}
		
		tempAddress = 0x3d00;
		for(k=0;k<32;k++)
		{
			OV8825OTPWriteSensor(tempAddress, 0);
			tempAddress++;
		}
		tempAddress = 0x3d00;
		tempBank++;
		Sleep(20);
	}
	return KAL_TRUE;
}


//check the flag of address is valid or not
kal_uint8 check_OV8825_otp_flag(u16 address)
{
	u16 reVal;
	u16 bank = address/BANK_SIZE;//one bank range:00~1f
	u16 tempAddress = address%BANK_SIZE;
	u8 tempBuf;
	
	OV8825_ReadOtp(bank,tempAddress,&tempBuf,1);//flag of group 1λ
	reVal = tempBuf;

	OV8825DB("check_OV8825_otp_flag reVal=0x%x,bank=0x%x,tempAddress=0x%x,address=0x%x\n", reVal,bank,tempAddress,address);

	if (!reVal)
	{
		return SHADING_EMPTY; //
	}
	
	reVal &= 0x80;  //get bit[7] Flag of group
	if (reVal == 0x80) //bit7 == 1?, yes, OTP data is invalid; No, OTP data is valid 
	{
		return SHADING_INVALID;
	}
	else
	{
		return SHADING_VALID;
	}

	return 0;
}
//read LSC parameter
kal_bool OV8825OTPShading(void)
{
	u16 lscGroupAddr[] = {0x20,0x60,0xa0};
	static unsigned char lscDecode[100]={0};
	static u16 otp_shading_ready = 0;
	static u8 otp_shading_index = 3;
	u16 LSCWriteStartAddr = lscGroupAddr[0];
	u8 i,temp1;
	u8 pTemp[4]={0};
	u8 index = 3;

	OV8825DB("OV8825OTPShading ENTER :\n ");
	//check which group is valid
	if (!otp_shading_ready) {
		for(i=0;i<3;i++)
		{
			temp1 = check_OV8825_otp_flag(lscGroupAddr[i]);
			if (temp1==SHADING_VALID)
			{
				otp_shading_index = i; //store shading otp index
				index = i;
				break;
			}
		}
	} else {
		OV8825DB("[OV8825OTP]Shading OTP index aimed: %d\n", otp_shading_index);
		index = otp_shading_index;
	}
	
	if (index == 3)
	{
		OV8825DB("[OV8825OTP]No Shading OTP Data\n");
		return KAL_FALSE;
	}
	
	//read shading from otp
	LSCWriteStartAddr = lscGroupAddr[index];
	int tempBank = (LSCWriteStartAddr+1)/BANK_SIZE;
	int tempAddr = (LSCWriteStartAddr+1)%BANK_SIZE;

	if (!otp_shading_ready) {
		OV8825_ReadOtp(tempBank,tempAddr,lscDecode,62);
		otp_shading_ready = 1;
		OV8825DB("[OV8825OTP]Shading OTP Data Ready\n");
	}

	//write shading parameter to sensor
	OV8825OTPWriteSensor(OV8825_SHADING_ADD,OV8825_SHADING_OFF_VALUE);
	for(i=0;i<62;i++)
	{
		OV8825OTPWriteSensor(OV8825_SHADING_VALUE_ADD+i,lscDecode[i]);
		OV8825DB("OV8825 Write Shading OTP ADRESS:=0x%x, PARA:=0x%x\n ",OV8825_SHADING_VALUE_ADD+i,lscDecode[i]);
	}
	OV8825OTPWriteSensor(OV8825_SHADING_ADD,OV8825_SHADING_ON_VALUE);
	
	Sleep(50); //1000-->200
	return KAL_TRUE;
}
//read AWB data
kal_bool OV8825OTPWbPregain(void)
{
	u16 awbGroupAddr[] = {0x05,0x0e,0x17};
	u16 AWBWriteStartAddr = awbGroupAddr[0];
	u16 i,temp1;
	static unsigned char pTemp[4]={0};
	static u8 otp_awb_ready = 0;
	static u8 otp_awb_index = 3;
	u16 index = 3;
	u16 RoverG_dec,BoverG_dec;

	OV8825DB("OV8825OTPWbPregain ENTER :\n ");

	if (!otp_awb_ready) {
		//check which group is valid
		for(i=0;i<3;i++)
		{
			temp1 = check_OV8825_otp_flag(awbGroupAddr[i]);
			if (temp1==SHADING_VALID)
			{
				otp_awb_index = i;//store awb otp index
				index = i;
				break;
			}
		}
	} else {
		OV8825DB("[OV8825OTP]AWB OTP index aimed: %d\n", otp_awb_index);
		index = otp_awb_index;
	}
	
	if (index == 3)
	{
		OV8825DB("No AWB OTP Data\n");
		return KAL_FALSE;
	}
	
	//read awb from otp
	AWBWriteStartAddr = awbGroupAddr[index];
	int tempBank = (AWBWriteStartAddr+1)/BANK_SIZE;
	int tempAddr = (AWBWriteStartAddr+1)%BANK_SIZE;

	OV8825DB("AWBWriteStartAddr = 0x%x\n", AWBWriteStartAddr);

	if (!otp_awb_ready) {
		OV8825_ReadOtp(tempBank,tempAddr,pTemp,3);
		otp_awb_ready = 1;
	} 

	/*
	if (pTemp[0] != MID || pTemp[0] != LENS_ID)
	{
	OV8825DB("[OV8825OTP]LENS is not match with OTP data\n");
	return KAL_FALSE;
	}
	*/

	OV8825_update_otp_wb(pTemp[1],pTemp[2]);

	return KAL_TRUE;
}

//main function to call
static void OnReadOTPOV8825(void)
{
	 OV8825DB("OnReadOTPOV8825 ENTER :\n ");
//read AWB data
	if(OV8825OTPWbPregain())
	{
		OV8825DB("ov8825 awb otp ok!!\n");
	}else{
		OV8825DB("ov8825 awb otp fail!!\n");
	}
//read LSC parameter
	if(OV8825OTPShading())
	{
		OV8825DB("ov8825 lenc otp ok!!\n");
	}else{
		OV8825DB("ov8825 lenc otp fail!!\n");
	}
}

#endif

static void OV8825_SetDummy( const kal_uint32 iPixels, const kal_uint32 iLines )
{
	kal_uint32 line_length = 0;
	kal_uint32 frame_length = 0;

	if ( SENSOR_MODE_PREVIEW == ov8825.sensorMode )	//SXGA size output
	{
		line_length = OV8825_PV_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV8825_PV_PERIOD_LINE_NUMS + iLines;
	}
	else if( SENSOR_MODE_VIDEO== ov8825.sensorMode )	
	{
		line_length = OV8825_VIDEO_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV8825_VIDEO_PERIOD_LINE_NUMS + iLines;
	}
	else//QSXGA size output
	{
		line_length = OV8825_FULL_PERIOD_PIXEL_NUMS + iPixels;
		frame_length = OV8825_FULL_PERIOD_LINE_NUMS + iLines;
	}

	//if(ov8825.maxExposureLines > frame_length -4 )
	//	return;

	//ASSERT(line_length < OV8825_MAX_LINE_LENGTH);		//0xCCCC
	//ASSERT(frame_length < OV8825_MAX_FRAME_LENGTH);	//0xFFFF

	//Set total frame length
	OV8825_write_cmos_sensor(0x380e, (frame_length >> 8) & 0xFF);
	OV8825_write_cmos_sensor(0x380f, frame_length & 0xFF);

	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.maxExposureLines = frame_length -4;
	OV8825_FeatureControl_PERIOD_PixelNum = line_length;
	OV8825_FeatureControl_PERIOD_LineNum = frame_length;
	spin_unlock(&ov8825mipiraw_drv_lock);

	//Set total line length
	OV8825_write_cmos_sensor(0x380c, (line_length >> 8) & 0xFF);
	OV8825_write_cmos_sensor(0x380d, line_length & 0xFF);

}   /*  OV8825_SetDummy */

void OV8825PreviewSettingOTP(void)
{	
	OV8825DB("OV8825PreviewSettingOTP enter :\n ");
	
	//;//OV8830_1632*1224_setting_2lanes_520Mbps/lane_72.22MSCLK30fps
	//;//Base_on_OV8825_APP_R1.0
	//;//2012_2_27
	//;//Tony Li
	//;;;;;;;;;;;;;Any modify please inform to OV FAE;;;;;;;;;;;;;;;

	OV8825_write_cmos_sensor(0x0100,0x00);			//sleep
	
	OV8825_write_cmos_sensor(0x3003,0xce);			//PLL_CTRL0 			 
	OV8825_write_cmos_sensor(0x3004,0xE3);			//PLL_CTRL1 			 
	OV8825_write_cmos_sensor(0x3005,0x00);			//PLL_CTRL2 			 
	OV8825_write_cmos_sensor(0x3006,0x10);//0x50);//10		//PLL_CTRL3 			 
	OV8825_write_cmos_sensor(0x3007,0x43);//0x2b);//3b		//PLL_CTRL4 			 
	OV8825_write_cmos_sensor(0x3011,0x01);			//MIPI_Lane_4_Lane		 
	OV8825_write_cmos_sensor(0x3012,0x81);			//SC_PLL CTRL_S0		 
	OV8825_write_cmos_sensor(0x3013,0x39);			//SC_PLL CTRL_S1		 
	OV8825_write_cmos_sensor(0x3104,0x20);			//SCCB_PLL				 
	OV8825_write_cmos_sensor(0x3106,0x11);//0x11);//15		//SRB_CTRL	  
	//OV8825_write_cmos_sensor(0x3501,0x4e);
	//OV8825_write_cmos_sensor(0x3502,0xa0);
	//OV8825_write_cmos_sensor(0x350b,0x3f);
	OV8825_write_cmos_sensor(0x3600,0x07);//ANACTRL0				 
	OV8825_write_cmos_sensor(0x3601,0x33);//ANACTRL1				 
	OV8825_write_cmos_sensor(0x3700,0x10);//SENCTROL0 Sensor control 
	OV8825_write_cmos_sensor(0x3702,0x28);//SENCTROL2 Sensor control 
	OV8825_write_cmos_sensor(0x3703,0x6c);//SENCTROL3 Sensor control 
	OV8825_write_cmos_sensor(0x3704,0x8d);//SENCTROL4 Sensor control 
	OV8825_write_cmos_sensor(0x3705,0x0a);//SENCTROL5 Sensor control 
	OV8825_write_cmos_sensor(0x3706,0x27);//SENCTROL6 Sensor control 
	OV8825_write_cmos_sensor(0x3707,0x63);//SENCTROL7 Sensor control 
	OV8825_write_cmos_sensor(0x3708,0x40);//SENCTROL8 Sensor control 
	OV8825_write_cmos_sensor(0x3709,0x20);//CTROL9 Sensor control 
	OV8825_write_cmos_sensor(0x370a,0x12);//SENCTROLA Sensor control 
	OV8825_write_cmos_sensor(0x370e,0x08);//SENCTROLE Sensor control 
	OV8825_write_cmos_sensor(0x3711,0x07);//SENCTROL11 Sensor control
	OV8825_write_cmos_sensor(0x3712,0x4e);//SENCTROL12 Sensor control
	OV8825_write_cmos_sensor(0x3724,0x00);//Reserved				 
	OV8825_write_cmos_sensor(0x3725,0xd4);//Reserved				 
	OV8825_write_cmos_sensor(0x3726,0x00);//Reserved				 
	OV8825_write_cmos_sensor(0x3727,0xe1);//Reserved  

	/////////////////////////////////////////////////////////
	OV8825_write_cmos_sensor(0x3800,0x00);//HS(HREF start High) 	 
	OV8825_write_cmos_sensor(0x3801,0x00);//HS(HREF start Low)		 
	OV8825_write_cmos_sensor(0x3802,0x00);//VS(Vertical start High)  
	OV8825_write_cmos_sensor(0x3803,0x00);//VS(Vertical start Low)	 
	OV8825_write_cmos_sensor(0x3804,0x0c);//HW =  3295				 
	OV8825_write_cmos_sensor(0x3805,0xdf);//HW						 
	OV8825_write_cmos_sensor(0x3806,0x09);//VH =  2459				 
	OV8825_write_cmos_sensor(0x3807,0x9b);//VH	  
	
	OV8825_write_cmos_sensor(0x3808,0x06);//ISPHO = 1632			 
	OV8825_write_cmos_sensor(0x3809,0x60);//ISPHO					 
	OV8825_write_cmos_sensor(0x380a,0x04);//ISPVO = 1224			 
	OV8825_write_cmos_sensor(0x380b,0xc8);//VO	
	
	OV8825_write_cmos_sensor(0x380c,0x0d);//HTS = 3516				 
	OV8825_write_cmos_sensor(0x380d,0xbc);//HTS 	   
	OV8825_write_cmos_sensor(0x380e,0x05);
	OV8825_write_cmos_sensor(0x380f,0x1e);
	OV8825_write_cmos_sensor(0x3810,0x00);//HOFF = 8				 
	OV8825_write_cmos_sensor(0x3811,0x08);//HOFF					 
	OV8825_write_cmos_sensor(0x3812,0x00);//VOFF = 4				 
	OV8825_write_cmos_sensor(0x3813,0x04);//VOFF					 
	OV8825_write_cmos_sensor(0x3814,0x31);//X INC					 
	OV8825_write_cmos_sensor(0x3815,0x31);//Y INC  
	
	//chenglong correct the direction
	//OV8825_write_cmos_sensor(0x3820,0x80);//Timing Reg20:Vflip		 
	//OV8825_write_cmos_sensor(0x3821,0x17);//ming Reg21:Hmirror	
	OV8825_write_cmos_sensor(0x3820,0x80);//Timing Reg20:Vflip		 
	OV8825_write_cmos_sensor(0x3821,0x17);//ming Reg21:Hmirror	   
	
	OV8825_write_cmos_sensor(0x3f00,0x00);//RAM Ctrl0			   
	OV8825_write_cmos_sensor(0x3f01,0xfc);//RAM Ctrl1			   
	OV8825_write_cmos_sensor(0x3f05,0x10);//RAM Ctrl5			   
	OV8825_write_cmos_sensor(0x4600,0x04);//FO Ctrl0			  
	OV8825_write_cmos_sensor(0x4601,0x00);//IFO Read ST High	   
	OV8825_write_cmos_sensor(0x4602,0x78);//IFO Read ST Low 	   
	OV8825_write_cmos_sensor(0x4837,/*0x18*/0x1e);//0x0F);//0x28			//PI PCLK PERIOD		 
	OV8825_write_cmos_sensor(0x5068,0x00);//CALE_CTRL			   
	OV8825_write_cmos_sensor(0x506a,0x00);//CALE_CTRL			   
	OV8825_write_cmos_sensor(0x5c00,0x80);//LC CTRL00			   
	OV8825_write_cmos_sensor(0x5c01,0x00);//LC CTRL01			   
	OV8825_write_cmos_sensor(0x5c02,0x00);//PBLC CTRL02 			 
	OV8825_write_cmos_sensor(0x5c03,0x00);//PBLC CTRL03 			 
	OV8825_write_cmos_sensor(0x5c04,0x00);//PBLC CTRL04 			 
	OV8825_write_cmos_sensor(0x5c08,0x10);//PBLC CTRL08 			 
	OV8825_write_cmos_sensor(0x6900,0x61);//CADC CTRL00  
	OV8825_write_cmos_sensor(0x3602,0xc2);////////////////////add by tony
	OV8825_write_cmos_sensor(0x0100,0x01);// wake up
	//mDELAY(5);
	
	OV8825DB("OV8825PreviewSettingOTP exit :\n ");
}

void OV8825PreviewSetting(void)
{

	OV8825DB("OV8825PreviewSetting enter_2lane :\n ");

    if(ReEnteyCamera == KAL_TRUE)
    {
		OV8825_write_cmos_sensor(0x0100, 0x00);
    }
	else
	{
	    OV8825_write_cmos_sensor(0x301a,0x71);
	}
	
	OV8825_write_cmos_sensor(0x3003,0xce);
	OV8825_write_cmos_sensor(0x3004,0xd4);
	OV8825_write_cmos_sensor(0x3005,0x00);
	OV8825_write_cmos_sensor(0x3006,0x10);
	OV8825_write_cmos_sensor(0x3007,0x43);
	OV8825_write_cmos_sensor(0x3011,0x01);
	OV8825_write_cmos_sensor(0x3012,0x80); 
	OV8825_write_cmos_sensor(0x3013,0x39);
	OV8825_write_cmos_sensor(0x3020,0x01);
	OV8825_write_cmos_sensor(0x3104,0x20);
	OV8825_write_cmos_sensor(0x3106,0x15);
	//OV8825_write_cmos_sensor(0x3501,0x4e);
	//OV8825_write_cmos_sensor(0x3502,0xa0);
	//OV8825_write_cmos_sensor(0x350b,0x3f);
	OV8825_write_cmos_sensor(0x3600,0x06);
	OV8825_write_cmos_sensor(0x3601,0x34);
	OV8825_write_cmos_sensor(0x3602,0xc2);
	OV8825_write_cmos_sensor(0x3700,0x20);
	OV8825_write_cmos_sensor(0x3702,0x50);
	OV8825_write_cmos_sensor(0x3703,0xcc);
	OV8825_write_cmos_sensor(0x3704,0x19);
	OV8825_write_cmos_sensor(0x3705,0x32);
	OV8825_write_cmos_sensor(0x3706,0x4b);
	OV8825_write_cmos_sensor(0x3707,0x63);
	OV8825_write_cmos_sensor(0x3708,0x84);
	OV8825_write_cmos_sensor(0x3709,0x40);
	OV8825_write_cmos_sensor(0x370a,0x33);
	OV8825_write_cmos_sensor(0x370d,0x0c);
	OV8825_write_cmos_sensor(0x370e,0x00);
	OV8825_write_cmos_sensor(0x3711,0x0f);
	OV8825_write_cmos_sensor(0x3712,0x9c);
	OV8825_write_cmos_sensor(0x3724,0x01);
	OV8825_write_cmos_sensor(0x3725,0x92);
	OV8825_write_cmos_sensor(0x3726,0x01);
	OV8825_write_cmos_sensor(0x3727,0xc7);
	OV8825_write_cmos_sensor(0x3800,0x00);
	OV8825_write_cmos_sensor(0x3801,0x00);
	OV8825_write_cmos_sensor(0x3802,0x00);
	OV8825_write_cmos_sensor(0x3803,0x00);
	OV8825_write_cmos_sensor(0x3804,0x0c);
	OV8825_write_cmos_sensor(0x3805,0xdf);
	OV8825_write_cmos_sensor(0x3806,0x09);
	OV8825_write_cmos_sensor(0x3807,0x9b);
	OV8825_write_cmos_sensor(0x3808,0x06);
	OV8825_write_cmos_sensor(0x3809,0x60);
	OV8825_write_cmos_sensor(0x380a,0x04);
	OV8825_write_cmos_sensor(0x380b,0xc8);
	OV8825_write_cmos_sensor(0x380c,0x0d);
	OV8825_write_cmos_sensor(0x380d,0xbc);
	OV8825_write_cmos_sensor(0x380e,0x05);
	OV8825_write_cmos_sensor(0x380f,0x1e);
	OV8825_write_cmos_sensor(0x3810,0x00);
	OV8825_write_cmos_sensor(0x3811,0x08);
	OV8825_write_cmos_sensor(0x3812,0x00);
	OV8825_write_cmos_sensor(0x3813,0x04);
	OV8825_write_cmos_sensor(0x3814,0x31);
	OV8825_write_cmos_sensor(0x3815,0x31);
	/*Lenovo-sw chenglong1 modify for display order issue*/
	//OV8825_write_cmos_sensor(0x3820,0x81);
	//OV8825_write_cmos_sensor(0x3821,0x17);
	OV8825_write_cmos_sensor(0x3820,0x87);
	OV8825_write_cmos_sensor(0x3821,0x11);
	/*Lenovo-sw modify end*/
	OV8825_write_cmos_sensor(0x3f00,0x00);
	OV8825_write_cmos_sensor(0x3f01,0xfc);
	OV8825_write_cmos_sensor(0x3f05,0x10);
	OV8825_write_cmos_sensor(0x4005,0x18);
	OV8825_write_cmos_sensor(0x4600,0x04);
	OV8825_write_cmos_sensor(0x4601,0x00);
	OV8825_write_cmos_sensor(0x4602,0x30);
	OV8825_write_cmos_sensor(0x4837,0x1e);
	OV8825_write_cmos_sensor(0x5068,0x00);
	OV8825_write_cmos_sensor(0x506a,0x00);
	OV8825_write_cmos_sensor(0x5c00,0x80);
	OV8825_write_cmos_sensor(0x5c01,0x00);
	OV8825_write_cmos_sensor(0x5c02,0x00);
	OV8825_write_cmos_sensor(0x5c03,0x00);
	OV8825_write_cmos_sensor(0x5c04,0x00);
	OV8825_write_cmos_sensor(0x5c08,0x10);
	OV8825_write_cmos_sensor(0x6900,0x60);

    if(ReEnteyCamera == KAL_TRUE)
    {
		OV8825_write_cmos_sensor(0x0100, 0x01);// wake up
    }
	else
	{
		OV8825_write_cmos_sensor(0x4003,0x82);//
		OV8825_write_cmos_sensor(0x4003,0x02);//
		OV8825_write_cmos_sensor(0x301a,0x70);//stream on
	}
	ReEnteyCamera = KAL_FALSE;

}
	
void OV8825VideoSetting(void)
{

	OV8825DB("OV8825VideoSetting/enter_2lane OB:\n ");

    if(ReEnteyCamera == KAL_TRUE)
    {
		OV8825_write_cmos_sensor(0x0100, 0x00);
    }
	else
	{
	    OV8825_write_cmos_sensor(0x301a,0x71);
	}

	OV8825_write_cmos_sensor(0x3003,0xce);// ;//;//PLL_CTRL0
	OV8825_write_cmos_sensor(0x3004,0xd8);//;bf ;//0xe0 ;//0xd8 ;//;//PLL_CTRL1, 
	OV8825_write_cmos_sensor(0x3005,0x00);//;10 ;//;//PLL_CTRL2
	OV8825_write_cmos_sensor(0x3006,0x00);// ;//(0x3006 50 ;//0x20 ;//0x10 ;//;//PLL_CTRL3
	OV8825_write_cmos_sensor(0x3007,0x3b);//;3b ;//(0x3007 2b ;//0x3b ;//;//PLL_CTRL4	
	OV8825_write_cmos_sensor(0x3011,0x01);//;02 ;//;//MIPI_Lane_2_Lane
	OV8825_write_cmos_sensor(0x3012,0x80);// ;//;//SC_PLL CTRL_S0
	OV8825_write_cmos_sensor(0x3013,0x39);// ;//;//SC_PLL CTRL_S1
	OV8825_write_cmos_sensor(0x3020,0x01);//
	OV8825_write_cmos_sensor(0x3104,0x20);// ;//;//SCCB_PLL

	OV8825_write_cmos_sensor(0x3106,0x15);// ;//;//SRB_CTRL


	OV8825_write_cmos_sensor(0x3600,0x06);// ;//ANACTRL0
	OV8825_write_cmos_sensor(0x3601,0x34);// ;//ANACTRL1
	OV8825_write_cmos_sensor(0x3602,0x42);// ;//;//;//;//;//;//;//;//;//;//add by tony
	OV8825_write_cmos_sensor(0x3700,0x20);// ;SENCTROL0 Sensor control 
	OV8825_write_cmos_sensor(0x3702,0x50);// ;SENCTROL2 Sensor control 
	OV8825_write_cmos_sensor(0x3703,0xcc);// ;SENCTROL3 Sensor control 
	OV8825_write_cmos_sensor(0x3704,0x19);// ;SENCTROL4 Sensor control 
	OV8825_write_cmos_sensor(0x3705,0x32);// ;SENCTROL5 Sensor control 
	OV8825_write_cmos_sensor(0x3706,0x4b);// ;SENCTROL6 Sensor control 
	OV8825_write_cmos_sensor(0x3707,0x63);// ;SENCTROL7 Sensor control 
	OV8825_write_cmos_sensor(0x3708,0x84);// ;SENCTROL8 Sensor control 
	OV8825_write_cmos_sensor(0x3709,0x40);// ;SENCTROL9 Sensor control 
	OV8825_write_cmos_sensor(0x370a,0x31);// ;SENCTROLA Sensor control 
	OV8825_write_cmos_sensor(0x370d,0x00);// 
	OV8825_write_cmos_sensor(0x370e,0x00);// ;SENCTROLE Sensor control 
	OV8825_write_cmos_sensor(0x3711,0x0f);// ;SENCTROL11 Sensor control
	OV8825_write_cmos_sensor(0x3712,0x9c);// ;SENCTROL12 Sensor control
	OV8825_write_cmos_sensor(0x3724,0x01);// ;Reserved                 
	OV8825_write_cmos_sensor(0x3725,0x92);// ;Reserved                 
	OV8825_write_cmos_sensor(0x3726,0x01);// ;Reserved                 
	OV8825_write_cmos_sensor(0x3727,0xc7);// ;Reserved                 

	OV8825_write_cmos_sensor(0x3800,0x02);// ;//HS(HREF start High)
	OV8825_write_cmos_sensor(0x3801,0x28);// ;//HS(HREF start Low)
	OV8825_write_cmos_sensor(0x3802,0x01);// ;//VS(Vertical start High)
	OV8825_write_cmos_sensor(0x3803,0x9c);//;//VS(Vertical start Low)
	OV8825_write_cmos_sensor(0x3804,0x0a);// ;//HW
	OV8825_write_cmos_sensor(0x3805,0xb7);// ;//HW
	OV8825_write_cmos_sensor(0x3806,0x07);// ;//VH,
	OV8825_write_cmos_sensor(0x3807,0xfb);// ;//VH, 

	OV8825_write_cmos_sensor(0x3808,0x08);// ;//ISPHO
	OV8825_write_cmos_sensor(0x3809,0x70);// ;//ISPHO
	OV8825_write_cmos_sensor(0x380a,0x06);// ;//ISPVO
	OV8825_write_cmos_sensor(0x380b,0x54);// ;//ISPVO

	OV8825_write_cmos_sensor(0x380c,0x0f);// ;//HTS 
	OV8825_write_cmos_sensor(0x380d,0x30);// ;//HTS

	OV8825_write_cmos_sensor(0x380e,0x07);// ;//VTS
	OV8825_write_cmos_sensor(0x380f,0x40);// ;//VTS

	OV8825_write_cmos_sensor(0x3810,0x00);// ;//HOFF
	OV8825_write_cmos_sensor(0x3811,0x10);// ;//HOFF,
	OV8825_write_cmos_sensor(0x3812,0x00);// ;//VOFF, 
	OV8825_write_cmos_sensor(0x3813,0x06);// ;//VOFF

	OV8825_write_cmos_sensor(0x3814,0x11);// ;//X INC
	OV8825_write_cmos_sensor(0x3815,0x11);// ;//Y INC
	
	/*Lenovo-sw chenglong1 modify for display order issue*/
	//OV8825_write_cmos_sensor(0x3820,0x80);// ;//Timing Reg20:Vflip
	//OV8825_write_cmos_sensor(0x3821,0x16);// ;//Timing Reg21:Hmirror
	OV8825_write_cmos_sensor(0x3820,0x86);// ;//Timing Reg20:Vflip
	OV8825_write_cmos_sensor(0x3821,0x10);// ;//Timing Reg21:Hmirror
	/*Lenovo-sw modify end*/

	OV8825_write_cmos_sensor(0x3f00,0x02);// ;//PSRAM Ctrl0
	OV8825_write_cmos_sensor(0x3f01,0xfc);// ;//PSRAM Ctrl1
	OV8825_write_cmos_sensor(0x3f05,0x10);// ;//PSRAM Ctrl5
	OV8825_write_cmos_sensor(0x4600,0x04);// ;//VFIFO Ctrl0
	OV8825_write_cmos_sensor(0x4601,0x00);// ;//VFIFO Read
	OV8825_write_cmos_sensor(0x4602,0x78);// ;//VFIFO Read
	OV8825_write_cmos_sensor(0x4837,0x15);// ;//MIPI PCLK  18;28;
	OV8825_write_cmos_sensor(0x5068,0x00);// ;//HSCALE_CTRL
	OV8825_write_cmos_sensor(0x506a,0x00);// ;//VSCALE_CTRL
	OV8825_write_cmos_sensor(0x5c00,0x80);// ;//PBLC CTRL00
	OV8825_write_cmos_sensor(0x5c01,0x00);// ;//PBLC CTRL01
	OV8825_write_cmos_sensor(0x5c02,0x00);// ;//PBLC CTRL02
	OV8825_write_cmos_sensor(0x5c03,0x00);// ;//PBLC CTRL03
	OV8825_write_cmos_sensor(0x5c04,0x00);// ;//PBLC CTRL04
	OV8825_write_cmos_sensor(0x5c08,0x10);// ;//PBLC CTRL08
	OV8825_write_cmos_sensor(0x6900,0x60);// ;//CADC CTRL00
	
    if(ReEnteyCamera == KAL_TRUE)
    {
		OV8825_write_cmos_sensor(0x0100, 0x01);
    }
	else
	{
		OV8825_write_cmos_sensor(0x4003,0x82);
		OV8825_write_cmos_sensor(0x4003,0x02);
		OV8825_write_cmos_sensor(0x301a,0x70);
	}
	ReEnteyCamera = KAL_FALSE;

}

void OV8825CaptureSetting(void)
{

	OV8825DB("OV8825CaptureSetting_2lane_OB:\n ");
    if(ReEnteyCamera == KAL_TRUE)
    {
		OV8825_write_cmos_sensor(0x0100, 0x00);
    }
	else
	{
	    OV8825_write_cmos_sensor(0x301a,0x71);
	}
	
	OV8825_write_cmos_sensor(0x3003,0xce);// ;//PLL_CTRL0              
	OV8825_write_cmos_sensor(0x3004,0xd8);//;dc;d8 ;//PLL_CTRL1//tony_5_8             
	OV8825_write_cmos_sensor(0x3005,0x00);// ;//PLL_CTRL2              
	OV8825_write_cmos_sensor(0x3006,0x00);//;10;50;10 ;//PLL_CTRL3////////////////tony              
	OV8825_write_cmos_sensor(0x3007,0x3b);//;A3;2b;3b ;//PLL_CTRL4///////////////tony              
	OV8825_write_cmos_sensor(0x3011,0x01);// ;//MIPI_Lane_4_Lane       
	OV8825_write_cmos_sensor(0x3012,0x80);//;;//SC_PLL CTRL_S0        
	OV8825_write_cmos_sensor(0x3013,0x39);// ;//SC_PLL CTRL_S1      
	OV8825_write_cmos_sensor(0x3020,0x81);//   
	OV8825_write_cmos_sensor(0x3104,0x20);// ;//SCCB_PLL               
	OV8825_write_cmos_sensor(0x3106,0x15);// ;//SRB_CTRL               
	//OV8825_write_cmos_sensor(0x3501,0x9a);//;//AEC_HIGH               
	//OV8825_write_cmos_sensor(0x3502,0xa0);//;//AEC_LOW                
	//OV8825_write_cmos_sensor(0x350b,0x3f);//;//AGC                    
	OV8825_write_cmos_sensor(0x3600,0x06);// ;ANACTRL0                 
	OV8825_write_cmos_sensor(0x3601,0x34);// ;ANACTRL1   
	OV8825_write_cmos_sensor(0x3602,0x42);//;add by tony_5_8                   
	OV8825_write_cmos_sensor(0x3700,0x10);// ;SENCTROL0 Sensor control 
	OV8825_write_cmos_sensor(0x3702,0x28);// ;SENCTROL2 Sensor control 
	OV8825_write_cmos_sensor(0x3703,0x6c);// ;SENCTROL3 Sensor control 
	OV8825_write_cmos_sensor(0x3704,0x40);// ;SENCTROL4 Sensor control 
	OV8825_write_cmos_sensor(0x3705,0x19);// ;SENCTROL5 Sensor control 
	OV8825_write_cmos_sensor(0x3706,0x27);// ;SENCTROL6 Sensor control 
	OV8825_write_cmos_sensor(0x3707,0x63);// ;SENCTROL7 Sensor control 
	OV8825_write_cmos_sensor(0x3708,0x48);// ;SENCTROL8 Sensor control 
	OV8825_write_cmos_sensor(0x3709,0x20);// ;SENCTROL9 Sensor control 
	OV8825_write_cmos_sensor(0x370a,0x31);// ;SENCTROLA Sensor control 
	OV8825_write_cmos_sensor(0x370d,0x00);// ;SENCTROLE Sensor control
	OV8825_write_cmos_sensor(0x370e,0x00);// ;SENCTROLE Sensor control 
	OV8825_write_cmos_sensor(0x3711,0x07);// ;SENCTROL11 Sensor control
	OV8825_write_cmos_sensor(0x3712,0x4e);// ;SENCTROL12 Sensor control
	OV8825_write_cmos_sensor(0x3724,0x00);// ;Reserved                 
	OV8825_write_cmos_sensor(0x3725,0xd4);// ;Reserved                 
	OV8825_write_cmos_sensor(0x3726,0x00);// ;Reserved                 
	OV8825_write_cmos_sensor(0x3727,0xf0);// ;Reserved                 
	OV8825_write_cmos_sensor(0x3800,0x00);// ;HS(HREF start High)      
	OV8825_write_cmos_sensor(0x3801,0x00);// ;HS(HREF start Low)       
	OV8825_write_cmos_sensor(0x3802,0x00);// ;VS(Vertical start High)  
	OV8825_write_cmos_sensor(0x3803,0x00);// ;VS(Vertical start Low)   
	OV8825_write_cmos_sensor(0x3804,0x0c);// ;HW =  3295               
	OV8825_write_cmos_sensor(0x3805,0xdf);// ;HW                       
	OV8825_write_cmos_sensor(0x3806,0x09);// ;VH =  2459              
	OV8825_write_cmos_sensor(0x3807,0x9b);// ;VH                       
	OV8825_write_cmos_sensor(0x3808,0x0c);// ;ISPHO = 3264             
	OV8825_write_cmos_sensor(0x3809,0xc0);// ;ISPHO                    
	OV8825_write_cmos_sensor(0x380a,0x09);// ;ISPVO = 2448             
	OV8825_write_cmos_sensor(0x380b,0x90);// ;ISPVO              
	
	OV8825_write_cmos_sensor(0x380c,0x16);//;0e ;HTS = 3584
	OV8825_write_cmos_sensor(0x380d,0xc0);//;30;00 ;HTS   Tony_5_8                   
	OV8825_write_cmos_sensor(0x380e,0x09);//;0a;09 ;VTS = 2572/////////////tony          
	OV8825_write_cmos_sensor(0x380f,0xb0);//;0c;b0 ;VTS     
	
	OV8825_write_cmos_sensor(0x3810,0x00);// ;HOFF = 16                
	OV8825_write_cmos_sensor(0x3811,0x10);// ;HOFF                     
	OV8825_write_cmos_sensor(0x3812,0x00);// ;VOFF = 6                 
	OV8825_write_cmos_sensor(0x3813,0x06);// ;VOFF                     
	OV8825_write_cmos_sensor(0x3814,0x11);// ;X INC                    
	OV8825_write_cmos_sensor(0x3815,0x11);// ;Y INC       
	/*Lenovo-sw chenglong1 modify for display order issue*/             
	//OV8825_write_cmos_sensor(0x3820,0x80);// ;Timing Reg20:Vflip       
	//OV8825_write_cmos_sensor(0x3821,0x16);// ;Timing Reg21:Hmirror     
	OV8825_write_cmos_sensor(0x3820,0x86);// ;Timing Reg20:Vflip       
	OV8825_write_cmos_sensor(0x3821,0x10);// ;Timing Reg21:Hmirror     
	/*Lenovo-sw modify end*/
	OV8825_write_cmos_sensor(0x3f00,0x02);// ;PSRAM Ctrl0              
	OV8825_write_cmos_sensor(0x3f01,0xfc);// ;PSRAM Ctrl1              
	OV8825_write_cmos_sensor(0x3f05,0x10);// ;PSRAM Ctrl5  
	OV8825_write_cmos_sensor(0x4005,0x18);//    modify BLC shake for ZSD zhangjiano         
	OV8825_write_cmos_sensor(0x4600,0x04);// ;VFIFO Ctrl0              
	OV8825_write_cmos_sensor(0x4601,0x00);// ;VFIFO Read ST High       
	OV8825_write_cmos_sensor(0x4602,0x20);// ;VFIFO Read ST Low        
	OV8825_write_cmos_sensor(0x4837,0x19);//;//18;16;28;;MIPI PCLK PERIOD/////////tony_5_8        nick_0531
	OV8825_write_cmos_sensor(0x5068,0x00);// ;HSCALE_CTRL              
	OV8825_write_cmos_sensor(0x506a,0x00);// ;VSCALE_CTRL              
	OV8825_write_cmos_sensor(0x5c00,0x80);// ;PBLC CTRL00              
	OV8825_write_cmos_sensor(0x5c01,0x00);// ;PBLC CTRL01              
	OV8825_write_cmos_sensor(0x5c02,0x00);// ;PBLC CTRL02              
	OV8825_write_cmos_sensor(0x5c03,0x00);// ;PBLC CTRL03              
	OV8825_write_cmos_sensor(0x5c04,0x00);// ;PBLC CTRL04              
	OV8825_write_cmos_sensor(0x5c08,0x10);// ;PBLC CTRL08              
	OV8825_write_cmos_sensor(0x6900,0x60);// ;CADC CTRL00 

    if(ReEnteyCamera == KAL_TRUE)
    {
		OV8825_write_cmos_sensor(0x0100, 0x01);
    }
	else
	{
		OV8825_write_cmos_sensor(0x4003,0x82);
		OV8825_write_cmos_sensor(0x4003,0x02);
		OV8825_write_cmos_sensor(0x301a,0x70);
	}
	ReEnteyCamera = KAL_FALSE;
}	

static void OV8825_Sensor_Init(void)
{
	OV8825DB("OV8825_Sensor_Init enter_2lane OB:\n ");	
    ReEnteyCamera = KAL_TRUE;
		
	OV8825_write_cmos_sensor(0x0103,0x01);//software reset
	Sleep(5);
	OV8825_write_cmos_sensor(0x3000,0x16);//; strobe disable, frex disable, vsync disable
	OV8825_write_cmos_sensor(0x3001,0x00);//
	OV8825_write_cmos_sensor(0x3002,0x6c);//; SCCB ID = 0x6c
	OV8825_write_cmos_sensor(0x3003,0xce);//
	OV8825_write_cmos_sensor(0x3004,0xd4);//
	OV8825_write_cmos_sensor(0x3005,0x00);//
	OV8825_write_cmos_sensor(0x3006,0x10);//
	OV8825_write_cmos_sensor(0x3007,0x43);//		 
	OV8825_write_cmos_sensor(0x300d,0x00);//; PLL2
	OV8825_write_cmos_sensor(0x3011,0x01);//
	OV8825_write_cmos_sensor(0x3012,0x80);// 
	OV8825_write_cmos_sensor(0x3013,0x39);//	  
	OV8825_write_cmos_sensor(0x301f,0x09);//; frex_mask_mipi, frex_mask_mipi_phy
	OV8825_write_cmos_sensor(0x3010,0x00);//; strobe, sda, frex, vsync, shutter GPIO unselected
	OV8825_write_cmos_sensor(0x3018,0x00);//; clear PHY HS TX power down and PHY LP RX power down
	OV8825_write_cmos_sensor(0x3020,0x01);//
	OV8825_write_cmos_sensor(0x3104,0x20);//;//SCCB_PLL 			  
	OV8825_write_cmos_sensor(0x3106,0x15);//;//SRB_CTRL 		 
	OV8825_write_cmos_sensor(0x3300,0x00);//
	OV8825_write_cmos_sensor(0x3500,0x00);//; exposure[19:16] = 0
	OV8825_write_cmos_sensor(0x3503,0x07);//; Gain has no delay, VTS manual, AGC manual, AEC manual
	OV8825_write_cmos_sensor(0x3509,0x00);//; use sensor gain
	OV8825_write_cmos_sensor(0x3600,0x06);//
	OV8825_write_cmos_sensor(0x3601,0x34);//
	OV8825_write_cmos_sensor(0x3602,0xc2);//
	OV8825_write_cmos_sensor(0x3603,0x5c);//; analog control
	OV8825_write_cmos_sensor(0x3604,0x98);//; analog control
	OV8825_write_cmos_sensor(0x3605,0xf5);//; analog control
	OV8825_write_cmos_sensor(0x3609,0xb4);//; analog control
	OV8825_write_cmos_sensor(0x360a,0x7c);//; analog control
	OV8825_write_cmos_sensor(0x360b,0xc9);//; analog control
	OV8825_write_cmos_sensor(0x360c,0x0b);//; analog control
	OV8825_write_cmos_sensor(0x3612,0x00);//; pad drive 1x, analog control
	OV8825_write_cmos_sensor(0x3613,0x02);//; analog control
	OV8825_write_cmos_sensor(0x3614,0x0f);//; analog control
	OV8825_write_cmos_sensor(0x3615,0x00);//; analog control
	OV8825_write_cmos_sensor(0x3616,0x03);//; analog control
	OV8825_write_cmos_sensor(0x3617,0xa1);//; analog control
	OV8825_write_cmos_sensor(0x3618,0x00);//; VCM position & slew rate, slew rate = 0
	OV8825_write_cmos_sensor(0x3619,0x00);//; VCM position = 0
	OV8825_write_cmos_sensor(0x361a,0xB0);//; VCM clock divider, VCM clock = 24000000/0x4b0 = 20000
	OV8825_write_cmos_sensor(0x361b,0x04);//; VCM clock divider
	OV8825_write_cmos_sensor(0x3700,0x20);//
	OV8825_write_cmos_sensor(0x3701,0x44);//; sensor control
	OV8825_write_cmos_sensor(0x3702,0x50);//
	OV8825_write_cmos_sensor(0x3703,0xcc);//
	OV8825_write_cmos_sensor(0x3704,0x19);//
	OV8825_write_cmos_sensor(0x3705,0x32);//
	OV8825_write_cmos_sensor(0x3706,0x4b);//
	OV8825_write_cmos_sensor(0x3707,0x63);//
	OV8825_write_cmos_sensor(0x3708,0x84);//
	OV8825_write_cmos_sensor(0x3709,0x40);//
	OV8825_write_cmos_sensor(0x370a,0x33);//
	OV8825_write_cmos_sensor(0x370b,0x01);//; sensor control
	OV8825_write_cmos_sensor(0x370c,0x50);//; sensor control
	OV8825_write_cmos_sensor(0x370d,0x0c);//; sensor control
	OV8825_write_cmos_sensor(0x370e,0x00);//
	OV8825_write_cmos_sensor(0x3711,0x0f);//
	OV8825_write_cmos_sensor(0x3712,0x9c);//
	OV8825_write_cmos_sensor(0x3724,0x01);//
	OV8825_write_cmos_sensor(0x3725,0x92);//
	OV8825_write_cmos_sensor(0x3726,0x01);//
	OV8825_write_cmos_sensor(0x3727,0xc7);//
	OV8825_write_cmos_sensor(0x3800,0x00);//
	OV8825_write_cmos_sensor(0x3801,0x00);//
	OV8825_write_cmos_sensor(0x3802,0x00);//
	OV8825_write_cmos_sensor(0x3803,0x00);//
	OV8825_write_cmos_sensor(0x3804,0x0c);//
	OV8825_write_cmos_sensor(0x3805,0xdf);//
	OV8825_write_cmos_sensor(0x3806,0x09);//
	OV8825_write_cmos_sensor(0x3807,0x9b);//
	OV8825_write_cmos_sensor(0x3808,0x06);//
	OV8825_write_cmos_sensor(0x3809,0x60);//
	OV8825_write_cmos_sensor(0x380a,0x04);//
	OV8825_write_cmos_sensor(0x380b,0xc8);//
	OV8825_write_cmos_sensor(0x380c,0x0d);//
	OV8825_write_cmos_sensor(0x380d,0xbc);//
	OV8825_write_cmos_sensor(0x380e,0x05);//04
	OV8825_write_cmos_sensor(0x380f,0x1e);//f0
	OV8825_write_cmos_sensor(0x3810,0x00);//
	OV8825_write_cmos_sensor(0x3811,0x08);//
	OV8825_write_cmos_sensor(0x3812,0x00);//
	OV8825_write_cmos_sensor(0x3813,0x04);//
	OV8825_write_cmos_sensor(0x3814,0x31);//
	OV8825_write_cmos_sensor(0x3815,0x31);//
	OV8825_write_cmos_sensor(0x3816,0x02);//; Hsync start H
	OV8825_write_cmos_sensor(0x3817,0x40);//; Hsync start L
	OV8825_write_cmos_sensor(0x3818,0x00);//; Hsync end H
	OV8825_write_cmos_sensor(0x3819,0x40);//; Hsync end L
	/*Lenovo-sw chenglong1 modify for display order issue*/
	//OV8825_write_cmos_sensor(0x3820,0x81);//
	//OV8825_write_cmos_sensor(0x3821,0x17);//
	OV8825_write_cmos_sensor(0x3820,0x87);//
	OV8825_write_cmos_sensor(0x3821,0x11);//
	/*Lenovo-sw modify end*/
	OV8825_write_cmos_sensor(0x3b1f,0x00);//; Frex conrol
	//clear OTP data buffer
	OV8825_write_cmos_sensor(0x3d00,0x00);
	OV8825_write_cmos_sensor(0x3d01,0x00);
	OV8825_write_cmos_sensor(0x3d02,0x00);
	OV8825_write_cmos_sensor(0x3d03,0x00);
	OV8825_write_cmos_sensor(0x3d04,0x00);
	OV8825_write_cmos_sensor(0x3d05,0x00);
	OV8825_write_cmos_sensor(0x3d06,0x00);
	OV8825_write_cmos_sensor(0x3d07,0x00);
	OV8825_write_cmos_sensor(0x3d08,0x00);
	OV8825_write_cmos_sensor(0x3d09,0x00);
	OV8825_write_cmos_sensor(0x3d0a,0x00);
	OV8825_write_cmos_sensor(0x3d0b,0x00);
	OV8825_write_cmos_sensor(0x3d0c,0x00);
	OV8825_write_cmos_sensor(0x3d0d,0x00);
	OV8825_write_cmos_sensor(0x3d0e,0x00);
	OV8825_write_cmos_sensor(0x3d0f,0x00);
	OV8825_write_cmos_sensor(0x3d10,0x00);
	OV8825_write_cmos_sensor(0x3d11,0x00);
	OV8825_write_cmos_sensor(0x3d12,0x00);
	OV8825_write_cmos_sensor(0x3d13,0x00);
	OV8825_write_cmos_sensor(0x3d14,0x00);
	OV8825_write_cmos_sensor(0x3d15,0x00);
	OV8825_write_cmos_sensor(0x3d16,0x00);
	OV8825_write_cmos_sensor(0x3d17,0x00);
	OV8825_write_cmos_sensor(0x3d18,0x00);
	OV8825_write_cmos_sensor(0x3d19,0x00);
	OV8825_write_cmos_sensor(0x3d1a,0x00);
	OV8825_write_cmos_sensor(0x3d1b,0x00);
	OV8825_write_cmos_sensor(0x3d1c,0x00);
	OV8825_write_cmos_sensor(0x3d1d,0x00);
	OV8825_write_cmos_sensor(0x3d1e,0x00);
	OV8825_write_cmos_sensor(0x3d1f,0x00);
	OV8825_write_cmos_sensor(0x3d80,0x00);
	OV8825_write_cmos_sensor(0x3d81,0x00);
	OV8825_write_cmos_sensor(0x3d84,0x00);
	OV8825_write_cmos_sensor(0x3f00,0x00);
	OV8825_write_cmos_sensor(0x3f01,0xfc);
	OV8825_write_cmos_sensor(0x3f05,0x10);
	OV8825_write_cmos_sensor(0x3f06,0x00);
	OV8825_write_cmos_sensor(0x3f07,0x00);
	//BLC
	OV8825_write_cmos_sensor(0x4000,0x29);//
	OV8825_write_cmos_sensor(0x4001,0x02);//; BLC start line
	OV8825_write_cmos_sensor(0x4002,0x45);//; BLC auto, reset 5 frames
	OV8825_write_cmos_sensor(0x4003,0x08);//; BLC redo at 8 frames
	OV8825_write_cmos_sensor(0x4004,0x04);//; 4 black lines are used for BLC
	OV8825_write_cmos_sensor(0x4005,0x18);//; no black line output, apply one channel offiset (0x400c, 0x400d) to all manual BLC channels
	OV8825_write_cmos_sensor(0x404e,0x37);//
	OV8825_write_cmos_sensor(0x404f,0x8f);//
	OV8825_write_cmos_sensor(0x4300,0xff);//; max
	OV8825_write_cmos_sensor(0x4303,0x00);//; format control
	OV8825_write_cmos_sensor(0x4304,0x08);//; output {data[7:0], data[9:8]}
	OV8825_write_cmos_sensor(0x4307,0x00);//; embeded control
	OV8825_write_cmos_sensor(0x4600,0x04);//
	OV8825_write_cmos_sensor(0x4601,0x00);//
	OV8825_write_cmos_sensor(0x4602,0x30);//
	//MIPI
	OV8825_write_cmos_sensor(0x4800,0x14);//04
	OV8825_write_cmos_sensor(0x4801,0x0f);//; ECC configure
	OV8825_write_cmos_sensor(0x4837,0x1e);//28
	OV8825_write_cmos_sensor(0x4843,0x02);//; manual set pclk divider
	//ISP
	OV8825_write_cmos_sensor(0x5000,0x86);// ; LENC off, BPC on, WPC on
	OV8825_write_cmos_sensor(0x5001,0x00);// ; MWB off
	OV8825_write_cmos_sensor(0x5002,0x00);//
	OV8825_write_cmos_sensor(0x501f,0x00);// ; enable ISP
	OV8825_write_cmos_sensor(0x5068,0x00);//
	OV8825_write_cmos_sensor(0x506a,0x00);//
	OV8825_write_cmos_sensor(0x5780,0xfc);//
	OV8825_write_cmos_sensor(0x5c00,0x80);//
	OV8825_write_cmos_sensor(0x5c01,0x00);//
	OV8825_write_cmos_sensor(0x5c02,0x00);//
	OV8825_write_cmos_sensor(0x5c03,0x00);//
	OV8825_write_cmos_sensor(0x5c04,0x00);//
	OV8825_write_cmos_sensor(0x5c05,0x00);// ; pre BLC
	OV8825_write_cmos_sensor(0x5c06,0x00);// ; pre BLC
	OV8825_write_cmos_sensor(0x5c07,0x80);// ; pre BLC
	OV8825_write_cmos_sensor(0x5c08,0x10);//
	//temperature sensor
	OV8825_write_cmos_sensor(0x6700,0x05);//
	OV8825_write_cmos_sensor(0x6701,0x19);//
	OV8825_write_cmos_sensor(0x6702,0xfd);//
	OV8825_write_cmos_sensor(0x6703,0xd7);//
	OV8825_write_cmos_sensor(0x6704,0xff);//
	OV8825_write_cmos_sensor(0x6705,0xff);//
	OV8825_write_cmos_sensor(0x6800,0x10);//
	OV8825_write_cmos_sensor(0x6801,0x02);//
	OV8825_write_cmos_sensor(0x6802,0x90);//
	OV8825_write_cmos_sensor(0x6803,0x10);//
	OV8825_write_cmos_sensor(0x6804,0x59);//
	OV8825_write_cmos_sensor(0x6900,0x60);//
	OV8825_write_cmos_sensor(0x6901,0x04);//; CADC control
	//Lens Control
	OV8825_write_cmos_sensor(0x5800,0x0f);//
	OV8825_write_cmos_sensor(0x5801,0x0d);//
	OV8825_write_cmos_sensor(0x5802,0x09);//
	OV8825_write_cmos_sensor(0x5803,0x0a);//
	OV8825_write_cmos_sensor(0x5804,0x0d);//
	OV8825_write_cmos_sensor(0x5805,0x14);//
	OV8825_write_cmos_sensor(0x5806,0x0a);//
	OV8825_write_cmos_sensor(0x5807,0x04);//
	OV8825_write_cmos_sensor(0x5808,0x03);//
	OV8825_write_cmos_sensor(0x5809,0x03);//
	OV8825_write_cmos_sensor(0x580a,0x05);//
	OV8825_write_cmos_sensor(0x580b,0x0a);//
	OV8825_write_cmos_sensor(0x580c,0x05);//
	OV8825_write_cmos_sensor(0x580d,0x02);//
	OV8825_write_cmos_sensor(0x580e,0x00);//
	OV8825_write_cmos_sensor(0x580f,0x00);//
	OV8825_write_cmos_sensor(0x5810,0x03);//
	OV8825_write_cmos_sensor(0x5811,0x05);//
	OV8825_write_cmos_sensor(0x5812,0x09);//
	OV8825_write_cmos_sensor(0x5813,0x03);//
	OV8825_write_cmos_sensor(0x5814,0x01);//
	OV8825_write_cmos_sensor(0x5815,0x01);//
	OV8825_write_cmos_sensor(0x5816,0x04);//
	OV8825_write_cmos_sensor(0x5817,0x09);//
	OV8825_write_cmos_sensor(0x5818,0x09);//
	OV8825_write_cmos_sensor(0x5819,0x08);//
	OV8825_write_cmos_sensor(0x581a,0x06);//
	OV8825_write_cmos_sensor(0x581b,0x06);//
	OV8825_write_cmos_sensor(0x581c,0x08);//
	OV8825_write_cmos_sensor(0x581d,0x06);//
	OV8825_write_cmos_sensor(0x581e,0x33);//
	OV8825_write_cmos_sensor(0x581f,0x11);//
	OV8825_write_cmos_sensor(0x5820,0x0e);//
	OV8825_write_cmos_sensor(0x5821,0x0f);//
	OV8825_write_cmos_sensor(0x5822,0x11);//
	OV8825_write_cmos_sensor(0x5823,0x3f);//
	OV8825_write_cmos_sensor(0x5824,0x08);//
	OV8825_write_cmos_sensor(0x5825,0x46);//
	OV8825_write_cmos_sensor(0x5826,0x46);//
	OV8825_write_cmos_sensor(0x5827,0x46);//
	OV8825_write_cmos_sensor(0x5828,0x46);//
	OV8825_write_cmos_sensor(0x5829,0x46);//
	OV8825_write_cmos_sensor(0x582a,0x42);//
	OV8825_write_cmos_sensor(0x582b,0x42);//
	OV8825_write_cmos_sensor(0x582c,0x44);//
	OV8825_write_cmos_sensor(0x582d,0x46);//
	OV8825_write_cmos_sensor(0x582e,0x46);//
	OV8825_write_cmos_sensor(0x582f,0x60);//
	OV8825_write_cmos_sensor(0x5830,0x62);//
	OV8825_write_cmos_sensor(0x5831,0x42);//
	OV8825_write_cmos_sensor(0x5832,0x46);//
	OV8825_write_cmos_sensor(0x5833,0x46);//
	OV8825_write_cmos_sensor(0x5834,0x44);//
	OV8825_write_cmos_sensor(0x5835,0x44);//
	OV8825_write_cmos_sensor(0x5836,0x44);//
	OV8825_write_cmos_sensor(0x5837,0x48);//
	OV8825_write_cmos_sensor(0x5838,0x28);//
	OV8825_write_cmos_sensor(0x5839,0x46);//
	OV8825_write_cmos_sensor(0x583a,0x48);//
	OV8825_write_cmos_sensor(0x583b,0x68);//
	OV8825_write_cmos_sensor(0x583c,0x28);//
	OV8825_write_cmos_sensor(0x583d,0xae);//
	OV8825_write_cmos_sensor(0x5842,0x00);//
	OV8825_write_cmos_sensor(0x5843,0xef);//
	OV8825_write_cmos_sensor(0x5844,0x01);//
	OV8825_write_cmos_sensor(0x5845,0x3f);//
	OV8825_write_cmos_sensor(0x5846,0x01);//
	OV8825_write_cmos_sensor(0x5847,0x3f);//
	OV8825_write_cmos_sensor(0x5848,0x00);//
	OV8825_write_cmos_sensor(0x5849,0xd5);//
	//Exposure
	OV8825_write_cmos_sensor(0x3503,0x07);//; Gain has no delay, VTS manual, AGC manual, AEC manual
	OV8825_write_cmos_sensor(0x3500,0x00);//; expo[19:16] = lines/16
	OV8825_write_cmos_sensor(0x3501,0x27);//; expo[15:8]
	OV8825_write_cmos_sensor(0x3502,0x00);//; expo[7:0]
	OV8825_write_cmos_sensor(0x350b,0xff);//; gain
	//MWB
	OV8825_write_cmos_sensor(0x3400,0x04);//	; red h
	OV8825_write_cmos_sensor(0x3401,0x00);//	; red l
	OV8825_write_cmos_sensor(0x3402,0x04);//	; green h
	OV8825_write_cmos_sensor(0x3403,0x00);//	; green l
	OV8825_write_cmos_sensor(0x3404,0x04);//	; blue h
	OV8825_write_cmos_sensor(0x3405,0x00);//	; blue l
	OV8825_write_cmos_sensor(0x3406,0x01);//	; MWB manual
	//ISP								
	OV8825_write_cmos_sensor(0x5001,0x01);//	; MWB on
	OV8825_write_cmos_sensor(0x5000,0x86);//	; LENC off, BPC on, WPC on

	OV8825PreviewSettingOTP();

#ifdef OV8825_OTP
	#if 0
	u8 i;
	unsigned char pTemp[4]={0};
	u16 awbGroupAddr[] = {0x05,0x0e,0x17};
	u16 temp_adress;
	for(i=0;i<3;i++)
	{
		OV8825_ReadOtp(0,(awbGroupAddr[i]+1),pTemp,3);
		temp_adress=awbGroupAddr[i]+1;
	
		OV8825DB("[OV8825OTP]adress = 0x%x,pTemp[1]=0x%x,pTemp[2]=0x%x\n",temp_adress,pTemp[1],pTemp[2]);	
	}
	#else
	OnReadOTPOV8825();
	#endif
#endif	
	
	OV8825DB("OV8825_Sensor_Init exit :\n ");
}   /*  OV8825_Sensor_Init  */

/*************************************************************************
* FUNCTION
*   OV8825Open
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/

UINT32 OV8825Open(void)
{

	volatile signed int i;
	kal_uint16 sensor_id = 0;

	OV8825DB("OV8825Open enter :\n ");
	OV8825_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(2);

	//  Read sensor ID to adjust I2C is OK?
	for(i=0;i<3;i++)
	{
		sensor_id = (OV8825_read_cmos_sensor(0x300A)<<8)|OV8825_read_cmos_sensor(0x300B);
		OV8825DB("OOV8825 READ ID :%x",sensor_id);
		if(sensor_id != OV8825_SENSOR_ID)
		{
			return ERROR_SENSOR_CONNECT_FAIL;
		}else
			break;
	}
	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.sensorMode = SENSOR_MODE_INIT;
	ov8825.OV8825AutoFlickerMode = KAL_FALSE;
	ov8825.OV8825VideoMode = KAL_FALSE;
	spin_unlock(&ov8825mipiraw_drv_lock);
	OV8825_Sensor_Init();

	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.DummyLines= 0;
	ov8825.DummyPixels= 0;

	ov8825.pvPclk =  (13867);   //Clk all check with 4lane setting
	ov8825.videoPclk = (21667); //3.4M video  4:3
	
	spin_unlock(&ov8825mipiraw_drv_lock);

	//#if defined(MT6575)||defined(MT6577)
    	switch(OV8825CurrentScenarioId)
		{
			case MSDK_SCENARIO_ID_CAMERA_ZSD:
				#if defined(ZSD15FPS)
				spin_lock(&ov8825mipiraw_drv_lock);
				ov8825.capPclk = (21667);//15fps
				spin_unlock(&ov8825mipiraw_drv_lock);
				#else
				spin_lock(&ov8825mipiraw_drv_lock);
				ov8825.capPclk = (21667);//13fps
				spin_unlock(&ov8825mipiraw_drv_lock);
				#endif
				break;
        	default:
				spin_lock(&ov8825mipiraw_drv_lock);
				ov8825.capPclk = (21667);
				spin_unlock(&ov8825mipiraw_drv_lock);
				break;
          }
	//#else
	//	spin_lock(&ov8825mipiraw_drv_lock);
	//	ov8825.capPclk = (13867);
	//	spin_unlock(&ov8825mipiraw_drv_lock);
	//#endif
	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.shutter = 0x4EA;
	ov8825.pvShutter = 0x4EA;
	ov8825.maxExposureLines =OV8825_PV_PERIOD_LINE_NUMS -4;

	ov8825.ispBaseGain = BASEGAIN;//0x40
	ov8825.sensorGlobalGain = 0x1f;//sensor gain read from 0x350a 0x350b; 0x1f as 3.875x
	ov8825.pvGain = 0x1f;
	ov8825.realGain = OV8825Reg2Gain(0x1f);//ispBaseGain as 1x
	spin_unlock(&ov8825mipiraw_drv_lock);
	//OV8825DB("OV8825Reg2Gain(0x1f)=%x :\n ",OV8825Reg2Gain(0x1f));

	OV8825DB("OV8825Open exit :\n ");

    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*   OV8825GetSensorID
*
* DESCRIPTION
*   This function get the sensor ID
*
* PARAMETERS
*   *sensorID : return the sensor ID
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8825GetSensorID(UINT32 *sensorID)
{
    int  retry = 1;

	OV8825DB("OV8825GetSensorID enter :\n ");
	OV8825_write_cmos_sensor(0x0103,0x01);// Reset sensor
    mDELAY(10);

    // check if sensor ID correct
    do {
        *sensorID = (OV8825_read_cmos_sensor(0x300A)<<8)|OV8825_read_cmos_sensor(0x300B);
        if (*sensorID == OV8825_SENSOR_ID)
        	{
        		OV8825DB("Sensor ID = 0x%04x\n", *sensorID);
            	break;
        	}
        OV8825DB("Read Sensor ID Fail = 0x%04x\n", *sensorID);
        retry--;
    } while (retry > 0);

    if (*sensorID != OV8825_SENSOR_ID) {
        *sensorID = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   OV8825_SetShutter
*
* DESCRIPTION
*   This function set e-shutter of OV8825 to change exposure time.
*
* PARAMETERS
*   shutter : exposured lines
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8825_SetShutter(kal_uint32 iShutter)
{
	if(MSDK_SCENARIO_ID_CAMERA_ZSD == OV8825CurrentScenarioId )
	{
		//OV8825DB("always UPDATE SHUTTER when ov8825.sensorMode == SENSOR_MODE_CAPTURE\n");
	}
	else{
		if(ov8825.sensorMode == SENSOR_MODE_CAPTURE)
		{
			//OV8825DB("capture!!DONT UPDATE SHUTTER!!\n");
			//return;
		}
	}
	if(ov8825.shutter == iShutter)
		return;
   spin_lock(&ov8825mipiraw_drv_lock);
   ov8825.shutter= iShutter;
   spin_unlock(&ov8825mipiraw_drv_lock);
   OV8825_write_shutter(iShutter);
   return;
}   /*  OV8825_SetShutter   */



/*************************************************************************
* FUNCTION
*   OV8825_read_shutter
*
* DESCRIPTION
*   This function to  Get exposure time.
*
* PARAMETERS
*   None
*
* RETURNS
*   shutter : exposured lines
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8825_read_shutter(void)
{

	kal_uint16 temp_reg1, temp_reg2 ,temp_reg3;
	UINT32 shutter =0;
	temp_reg1 = OV8825_read_cmos_sensor(0x3500);    // AEC[b19~b16]
	temp_reg2 = OV8825_read_cmos_sensor(0x3501);    // AEC[b15~b8]
	temp_reg3 = OV8825_read_cmos_sensor(0x3502);    // AEC[b7~b0]
	//read out register value and divide 16;
	shutter  = (temp_reg1 <<12)| (temp_reg2<<4)|(temp_reg3>>4);

	return shutter;
}

/*************************************************************************
* FUNCTION
*   OV8825_night_mode
*
* DESCRIPTION
*   This function night mode of OV8825.
*
* PARAMETERS
*   none
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
void OV8825_NightMode(kal_bool bEnable)
{
}/*	OV8825_NightMode */



/*************************************************************************
* FUNCTION
*   OV8825Close
*
* DESCRIPTION
*   This function is to turn off sensor module power.
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8825Close(void)
{
    //  CISModulePowerOn(FALSE);
    //s_porting
    //  DRV_I2CClose(OV8825hDrvI2C);
    //e_porting
    ReEnteyCamera = KAL_FALSE;
    return ERROR_NONE;
}	/* OV8825Close() */

void OV8825SetFlipMirror(kal_int32 imgMirror)
{
	kal_int16 mirror=0,flip=0;
	mirror= OV8825_read_cmos_sensor(0x3820);
	flip  = OV8825_read_cmos_sensor(0x3821);
    switch (imgMirror)
    {
        case IMAGE_H_MIRROR://IMAGE_NORMAL:
            OV8825_write_cmos_sensor(0x3820, (mirror |(0x06)));	//Set mirror & flip
            OV8825_write_cmos_sensor(0x3821, (flip |(0x06)));	//Set mirror & flip	    
            break;
        case IMAGE_NORMAL://IMAGE_V_MIRROR:
            OV8825_write_cmos_sensor(0x3820, (mirror |(0x06)));	//Set mirror
            OV8825_write_cmos_sensor(0x3821, (flip & (0xF9)));	//Set mirror
            break;
        case IMAGE_HV_MIRROR://IMAGE_H_MIRROR:
            OV8825_write_cmos_sensor(0x3820, (mirror & (0xF9)));//Set flip
            OV8825_write_cmos_sensor(0x3821, (flip | (0x06)));	//Set flip
            break;
        case IMAGE_V_MIRROR://IMAGE_HV_MIRROR:
            OV8825_write_cmos_sensor(0x3820, (mirror & (0xF9)));//Set normal
            OV8825_write_cmos_sensor(0x3821, (flip & (0xF9)));	//Set normal
            break;
    }
}


/*************************************************************************
* FUNCTION
*   OV8825Preview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 OV8825Preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	OV8825DB("OV8825Preview enter:");

	// preview size
	if(ov8825.sensorMode == SENSOR_MODE_PREVIEW)
	{
		// do nothing
		// FOR CCT PREVIEW
	}
	else
	{
		//OV8825DB("OV8825Preview setting!!\n");
		OV8825PreviewSetting();
	}
	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.sensorMode = SENSOR_MODE_PREVIEW; // Need set preview setting after capture mode
	ov8825.DummyPixels = 0;//define dummy pixels and lines
	ov8825.DummyLines = 0 ;
	OV8825_FeatureControl_PERIOD_PixelNum=OV8825_PV_PERIOD_PIXEL_NUMS+ ov8825.DummyPixels;
	OV8825_FeatureControl_PERIOD_LineNum=OV8825_PV_PERIOD_LINE_NUMS+ov8825.DummyLines;
	spin_unlock(&ov8825mipiraw_drv_lock);

	//OV8825_write_shutter(ov8825.shutter);
	//write_OV8825_gain(ov8825.pvGain);

	//set mirror & flip
	//OV8825DB("[OV8825Preview] mirror&flip: %d \n",sensor_config_data->SensorImageMirror);
	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&ov8825mipiraw_drv_lock);
	//OV8825SetFlipMirror(sensor_config_data->SensorImageMirror);
	OV8825SetFlipMirror(IMAGE_HV_MIRROR);

	OV8825DBSOFIA("[OV8825Preview]frame_len=%x\n", ((OV8825_read_cmos_sensor(0x380e)<<8)+OV8825_read_cmos_sensor(0x380f)));
    mDELAY(40);
	OV8825DB("OV8825Preview exit:\n");
    return ERROR_NONE;
}	/* OV8825Preview() */



UINT32 OV8825Video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	OV8825DB("OV8825Video enter:");

	if(ov8825.sensorMode == SENSOR_MODE_VIDEO)
	{
		// do nothing
	}
	else
	{
		OV8825VideoSetting();

	}
	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.sensorMode = SENSOR_MODE_VIDEO;
	OV8825_FeatureControl_PERIOD_PixelNum=OV8825_VIDEO_PERIOD_PIXEL_NUMS+ ov8825.DummyPixels;
	OV8825_FeatureControl_PERIOD_LineNum=OV8825_VIDEO_PERIOD_LINE_NUMS+ov8825.DummyLines;
	spin_unlock(&ov8825mipiraw_drv_lock);

	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.imgMirror = sensor_config_data->SensorImageMirror;
	spin_unlock(&ov8825mipiraw_drv_lock);
	//OV8825SetFlipMirror(sensor_config_data->SensorImageMirror);
	OV8825SetFlipMirror(IMAGE_HV_MIRROR);

	OV8825DBSOFIA("[OV8825Video]frame_len=%x\n", ((OV8825_read_cmos_sensor(0x380e)<<8)+OV8825_read_cmos_sensor(0x380f)));
    mDELAY(40);
	OV8825DB("OV8825Video exit:\n");
    return ERROR_NONE;
}


UINT32 OV8825Capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                                                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

 	kal_uint32 shutter = ov8825.shutter;
	kal_uint32 temp_data;
	//kal_uint32 pv_line_length , cap_line_length,

	if( SENSOR_MODE_CAPTURE== ov8825.sensorMode)
	{
		OV8825DB("OV8825Capture BusrtShot!!!\n");
	}else{
	OV8825DB("OV8825Capture enter:\n");

	//Record Preview shutter & gain
	shutter=OV8825_read_shutter();
	temp_data =  read_OV8825_gain();
	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.pvShutter =shutter;
	ov8825.sensorGlobalGain = temp_data;
	ov8825.pvGain =ov8825.sensorGlobalGain;
	ov8825.sensorMode = SENSOR_MODE_CAPTURE;	
	spin_unlock(&ov8825mipiraw_drv_lock);

	OV8825DB("[OV8825Capture]ov8825.shutter=%d, read_pv_shutter=%d, read_pv_gain = 0x%x\n",ov8825.shutter, shutter,ov8825.sensorGlobalGain);

	// Full size setting
	OV8825CaptureSetting();
    mDELAY(40);
    //rewrite pixel number to Register ,for mt6589 line start/end;
	OV8825_SetDummy(ov8825.DummyPixels,ov8825.DummyLines);

	spin_lock(&ov8825mipiraw_drv_lock);

	ov8825.imgMirror = sensor_config_data->SensorImageMirror;
	ov8825.DummyPixels = 0;//define dummy pixels and lines
	ov8825.DummyLines = 0 ;
	OV8825_FeatureControl_PERIOD_PixelNum = OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
	OV8825_FeatureControl_PERIOD_LineNum = OV8825_FULL_PERIOD_LINE_NUMS + ov8825.DummyLines;

	spin_unlock(&ov8825mipiraw_drv_lock);

	//OV8825DB("[OV8825Capture] mirror&flip: %d\n",sensor_config_data->SensorImageMirror);
	//OV8825SetFlipMirror(sensor_config_data->SensorImageMirror);
	OV8825SetFlipMirror(IMAGE_HV_MIRROR);

	//#if defined(MT6575)||defined(MT6577)
    if(OV8825CurrentScenarioId==MSDK_SCENARIO_ID_CAMERA_ZSD)
    {
		OV8825DB("OV8825Capture exit ZSD!!\n");
		return ERROR_NONE;
    }
	//#endif   

	#if 0 //no need to calculate shutter from mt6589
	//calculate shutter
	pv_line_length = OV8825_PV_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;
	cap_line_length = OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels;

	OV8825DB("[OV8825Capture]pv_line_length =%d,cap_line_length =%d\n",pv_line_length,cap_line_length);
	OV8825DB("[OV8825Capture]pv_shutter =%d\n",shutter );

	shutter =  shutter * pv_line_length / cap_line_length;
	shutter = shutter *ov8825.capPclk / ov8825.pvPclk;
	shutter *= 2; //preview bining///////////////////////////////////////

	if(shutter < 3)
	    shutter = 3;

	OV8825_write_shutter(shutter);

	gain = read_OV8825_gain();

	OV8825DB("[OV8825Capture]cap_shutter =%d , cap_read gain = 0x%x\n",shutter,read_OV8825_gain());
	//write_OV8825_gain(ov8825.sensorGlobalGain);
   #endif

	OV8825DB("OV8825Capture exit:\n");
	}

    return ERROR_NONE;
}	/* OV8825Capture() */

UINT32 OV8825GetResolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{

    OV8825DB("OV8825GetResolution!!\n");

	pSensorResolution->SensorPreviewWidth	= OV8825_IMAGE_SENSOR_PV_WIDTH;
    pSensorResolution->SensorPreviewHeight	= OV8825_IMAGE_SENSOR_PV_HEIGHT;
	
    pSensorResolution->SensorFullWidth		= OV8825_IMAGE_SENSOR_FULL_WIDTH;
    pSensorResolution->SensorFullHeight		= OV8825_IMAGE_SENSOR_FULL_HEIGHT;
	
    pSensorResolution->SensorVideoWidth		= OV8825_IMAGE_SENSOR_VIDEO_WIDTH;
    pSensorResolution->SensorVideoHeight    = OV8825_IMAGE_SENSOR_VIDEO_HEIGHT;
//    OV8825DB("SensorPreviewWidth:  %d.\n", pSensorResolution->SensorPreviewWidth);
//    OV8825DB("SensorPreviewHeight: %d.\n", pSensorResolution->SensorPreviewHeight);
//    OV8825DB("SensorFullWidth:  %d.\n", pSensorResolution->SensorFullWidth);
//    OV8825DB("SensorFullHeight: %d.\n", pSensorResolution->SensorFullHeight);
    return ERROR_NONE;
}   /* OV8825GetResolution() */

UINT32 OV8825GetInfo(MSDK_SCENARIO_ID_ENUM ScenarioId,
                                                MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{

	pSensorInfo->SensorPreviewResolutionX= OV8825_IMAGE_SENSOR_PV_WIDTH;
	pSensorInfo->SensorPreviewResolutionY= OV8825_IMAGE_SENSOR_PV_HEIGHT;

	pSensorInfo->SensorFullResolutionX= OV8825_IMAGE_SENSOR_FULL_WIDTH;
    pSensorInfo->SensorFullResolutionY= OV8825_IMAGE_SENSOR_FULL_HEIGHT;

	spin_lock(&ov8825mipiraw_drv_lock);
	ov8825.imgMirror = pSensorConfigData->SensorImageMirror ;
	spin_unlock(&ov8825mipiraw_drv_lock);

   	pSensorInfo->SensorOutputDataFormat= SENSOR_OUTPUT_FORMAT_RAW_B;
    pSensorInfo->SensorClockPolarity =SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorClockFallingPolarity=SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    pSensorInfo->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;

    pSensorInfo->SensroInterfaceType=SENSOR_INTERFACE_TYPE_MIPI;
    pSensorInfo->MIPIsensorType = MIPI_OPHY_CSI2; 

    pSensorInfo->CaptureDelayFrame = 3;//1;
    pSensorInfo->PreviewDelayFrame = 1;
    pSensorInfo->VideoDelayFrame = 4;//2;

    pSensorInfo->SensorDrivingCurrent = ISP_DRIVING_8MA;
    pSensorInfo->AEShutDelayFrame = 0;//0;		    /* The frame of setting shutter default 0 for TG int */
    pSensorInfo->AESensorGainDelayFrame = 0 ;//0;     /* The frame of setting sensor gain */
    pSensorInfo->AEISPGainDelayFrame = 2;

    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            pSensorInfo->SensorClockFreq=26;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV8825_PV_X_START;
            pSensorInfo->SensorGrabStartY = OV8825_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            pSensorInfo->SensorClockFreq=26;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV8825_VIDEO_X_START;
            pSensorInfo->SensorGrabStartY = OV8825_VIDEO_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            pSensorInfo->SensorClockFreq=26;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV8825_FULL_X_START;	//2*OV8825_IMAGE_SENSOR_PV_STARTX;
            pSensorInfo->SensorGrabStartY = OV8825_FULL_Y_START;	//2*OV8825_IMAGE_SENSOR_PV_STARTY;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 0x20;
            pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
        default:
            pSensorInfo->SensorClockFreq=26;
            pSensorInfo->SensorClockRisingCount= 0;

            pSensorInfo->SensorGrabStartX = OV8825_PV_X_START;
            pSensorInfo->SensorGrabStartY = OV8825_PV_Y_START;

            pSensorInfo->SensorMIPILaneNumber = SENSOR_MIPI_2_LANE;
            pSensorInfo->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	     	pSensorInfo->MIPIDataLowPwr2HighSpeedSettleDelayCount = 14;
	    	pSensorInfo->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
            pSensorInfo->SensorPacketECCOrder = 1;
            break;
    }

    memcpy(pSensorConfigData, &OV8825SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));

    return ERROR_NONE;
}   /* OV8825GetInfo() */


UINT32 OV8825Control(MSDK_SCENARIO_ID_ENUM ScenarioId, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
                                                MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
		spin_lock(&ov8825mipiraw_drv_lock);
		OV8825CurrentScenarioId = ScenarioId;
		spin_unlock(&ov8825mipiraw_drv_lock);
		//OV8825DB("ScenarioId=%d\n",ScenarioId);
		OV8825DB("OV8825CurrentScenarioId=%d\n",OV8825CurrentScenarioId);
    switch (ScenarioId)
    {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            OV8825Preview(pImageWindow, pSensorConfigData);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			OV8825Video(pImageWindow, pSensorConfigData);
			break;   
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
            OV8825Capture(pImageWindow, pSensorConfigData);
            break;

        default:
            return ERROR_INVALID_SCENARIO_ID;

    }
    return ERROR_NONE;
} /* OV8825Control() */


UINT32 OV8825SetVideoMode(UINT16 u2FrameRate)
{

    kal_uint32 MIN_Frame_length =0,frameRate=0,extralines=0;
    OV8825DB("[OV8825SetVideoMode] frame rate = %d\n", u2FrameRate);
	
	spin_lock(&ov8825mipiraw_drv_lock);
	VIDEO_MODE_TARGET_FPS=u2FrameRate;
	spin_unlock(&ov8825mipiraw_drv_lock);

	if(u2FrameRate==0)
	{
		OV8825DB("Disable Video Mode or dynimac fps\n");
		return KAL_TRUE;
	}
	if(u2FrameRate >30 || u2FrameRate <5)
	    OV8825DB("error frame rate seting\n");

    if(ov8825.sensorMode == SENSOR_MODE_VIDEO)//video ScenarioId recording
    {
    	if(ov8825.OV8825AutoFlickerMode == KAL_TRUE)
    	{
    		if (u2FrameRate==30)
				frameRate= 306;
			else if(u2FrameRate==15)
				frameRate= 148;//148;
			else
				frameRate=u2FrameRate*10;

			MIN_Frame_length = (ov8825.videoPclk*10000)/(OV8825_VIDEO_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (ov8825.videoPclk*10000) /(OV8825_VIDEO_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=OV8825_VIDEO_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = OV8825_VIDEO_PERIOD_LINE_NUMS;
		}
		extralines = MIN_Frame_length - OV8825_VIDEO_PERIOD_LINE_NUMS;

		spin_lock(&ov8825mipiraw_drv_lock);
		ov8825.DummyPixels = 0;//define dummy pixels and lines
		ov8825.DummyLines = extralines ;
		spin_unlock(&ov8825mipiraw_drv_lock);

		OV8825_SetDummy(ov8825.DummyPixels,extralines);
    }
	else if(ov8825.sensorMode == SENSOR_MODE_CAPTURE)
	{
		OV8825DB("-------[OV8825SetVideoMode]ZSD???---------\n");
		if(ov8825.OV8825AutoFlickerMode == KAL_TRUE)
    	{
			#if defined(ZSD15FPS)
    		if (u2FrameRate==15)
			    frameRate= 148;
			#else
    		if (u2FrameRate==13)
				frameRate= 130;
			#endif
			else
				frameRate=u2FrameRate*10;
			
			MIN_Frame_length = (ov8825.capPclk*10000) /(OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/frameRate*10;
    	}
		else
			MIN_Frame_length = (ov8825.capPclk*10000) /(OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/u2FrameRate;

		if((MIN_Frame_length <=OV8825_FULL_PERIOD_LINE_NUMS))
		{
			MIN_Frame_length = OV8825_FULL_PERIOD_LINE_NUMS;
			OV8825DB("[OV8825SetVideoMode]current fps = %d\n", (ov8825.capPclk*10000) /(OV8825_FULL_PERIOD_PIXEL_NUMS)/OV8825_FULL_PERIOD_LINE_NUMS);

		}
		OV8825DB("[OV8825SetVideoMode]current fps (10 base)= %d\n", (ov8825.pvPclk*10000)*10/(OV8825_FULL_PERIOD_PIXEL_NUMS + ov8825.DummyPixels)/MIN_Frame_length);

		extralines = MIN_Frame_length - OV8825_FULL_PERIOD_LINE_NUMS;

		spin_lock(&ov8825mipiraw_drv_lock);
		ov8825.DummyPixels = 0;//define dummy pixels and lines
		ov8825.DummyLines = extralines ;
		spin_unlock(&ov8825mipiraw_drv_lock);

		OV8825_SetDummy(ov8825.DummyPixels,extralines);
	}
	OV8825DB("[OV8825SetVideoMode]MIN_Frame_length=%d,ov8825.DummyLines=%d\n",MIN_Frame_length,ov8825.DummyLines);

    return KAL_TRUE;
}

UINT32 OV8825SetAutoFlickerMode(kal_bool bEnable, UINT16 u2FrameRate)
{
	//return ERROR_NONE;

    OV8825DB("[OV8825SetAutoFlickerMode] frame rate(10base) = %d %d\n", bEnable, u2FrameRate);
	if(bEnable) {   // enable auto flicker
		spin_lock(&ov8825mipiraw_drv_lock);
		ov8825.OV8825AutoFlickerMode = KAL_TRUE;
		spin_unlock(&ov8825mipiraw_drv_lock);
    } else {
    	spin_lock(&ov8825mipiraw_drv_lock);
        ov8825.OV8825AutoFlickerMode = KAL_FALSE;
		spin_unlock(&ov8825mipiraw_drv_lock);
        OV8825DB("Disable Auto flicker\n");
    }

    return ERROR_NONE;
}

UINT32 OV8825SetTestPatternMode(kal_bool bEnable)
{
    OV8825DB("[OV8825SetTestPatternMode] Test pattern enable:%d\n", bEnable);

    return TRUE;
}

UINT32 OV8825MIPISetMaxFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 frameRate) 
{
	kal_uint32 pclk;
	kal_int16 dummyLine;
	kal_uint16 lineLength,frameHeight;
		
	OV8825DB("OV8825MIPISetMaxFramerateByScenario: scenarioId = %d, frame rate = %d\n",scenarioId,frameRate);
	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			pclk = 138670000;
			lineLength = OV8825_PV_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV8825_PV_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&ov8825mipiraw_drv_lock);
			ov8825.sensorMode = SENSOR_MODE_PREVIEW;
			spin_unlock(&ov8825mipiraw_drv_lock);
			OV8825_SetDummy(0, dummyLine);			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			pclk = 147330000;
			lineLength = OV8825_VIDEO_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV8825_VIDEO_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&ov8825mipiraw_drv_lock);
			ov8825.sensorMode = SENSOR_MODE_VIDEO;
			spin_unlock(&ov8825mipiraw_drv_lock);
			OV8825_SetDummy(0, dummyLine);			
			break;			
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			pclk = 138670000;
			lineLength = OV8825_FULL_PERIOD_PIXEL_NUMS;
			frameHeight = (10 * pclk)/frameRate/lineLength;
			dummyLine = frameHeight - OV8825_FULL_PERIOD_LINE_NUMS;
			if(dummyLine<0)
				dummyLine = 0;
			spin_lock(&ov8825mipiraw_drv_lock);
			ov8825.sensorMode = SENSOR_MODE_CAPTURE;
			spin_unlock(&ov8825mipiraw_drv_lock);
			OV8825_SetDummy(0, dummyLine);			
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
            break;
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
			break;
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			break;		
		default:
			break;
	}	
	return ERROR_NONE;
}


UINT32 OV8825MIPIGetDefaultFramerateByScenario(MSDK_SCENARIO_ID_ENUM scenarioId, MUINT32 *pframeRate) 
{

	switch (scenarioId) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			 *pframeRate = 300;
			 break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		case MSDK_SCENARIO_ID_CAMERA_ZSD:
			 *pframeRate = 150;
			break;		
        case MSDK_SCENARIO_ID_CAMERA_3D_PREVIEW: //added
        case MSDK_SCENARIO_ID_CAMERA_3D_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_3D_CAPTURE: //added   
			 *pframeRate = 300;
			break;		
		default:
			break;
	}

	return ERROR_NONE;
}



UINT32 OV8825FeatureControl(MSDK_SENSOR_FEATURE_ENUM FeatureId,
                                                                UINT8 *pFeaturePara,UINT32 *pFeatureParaLen)
{
    UINT16 *pFeatureReturnPara16=(UINT16 *) pFeaturePara;
    UINT16 *pFeatureData16=(UINT16 *) pFeaturePara;
    UINT32 *pFeatureReturnPara32=(UINT32 *) pFeaturePara;
    UINT32 *pFeatureData32=(UINT32 *) pFeaturePara;
    UINT32 SensorRegNumber;
    UINT32 i;
    PNVRAM_SENSOR_DATA_STRUCT pSensorDefaultData=(PNVRAM_SENSOR_DATA_STRUCT) pFeaturePara;
    MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData=(MSDK_SENSOR_CONFIG_STRUCT *) pFeaturePara;
    MSDK_SENSOR_REG_INFO_STRUCT *pSensorRegData=(MSDK_SENSOR_REG_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_GROUP_INFO_STRUCT *pSensorGroupInfo=(MSDK_SENSOR_GROUP_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ITEM_INFO_STRUCT *pSensorItemInfo=(MSDK_SENSOR_ITEM_INFO_STRUCT *) pFeaturePara;
    MSDK_SENSOR_ENG_INFO_STRUCT	*pSensorEngInfo=(MSDK_SENSOR_ENG_INFO_STRUCT *) pFeaturePara;

    switch (FeatureId)
    {
        case SENSOR_FEATURE_GET_RESOLUTION:
            *pFeatureReturnPara16++= OV8825_IMAGE_SENSOR_FULL_WIDTH;
            *pFeatureReturnPara16= OV8825_IMAGE_SENSOR_FULL_HEIGHT;
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_PERIOD:
				*pFeatureReturnPara16++= OV8825_FeatureControl_PERIOD_PixelNum;
				*pFeatureReturnPara16= OV8825_FeatureControl_PERIOD_LineNum;
				*pFeatureParaLen=4;
				break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			switch(OV8825CurrentScenarioId)
			{
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*pFeatureReturnPara32 = 138670000;
					*pFeatureParaLen=4;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*pFeatureReturnPara32 = 216670000;
					*pFeatureParaLen=4;
					break;	 
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_CAMERA_ZSD:
					*pFeatureReturnPara32 = 216670000;
					*pFeatureParaLen=4;
					break;
				default:
					*pFeatureReturnPara32 = 216670000;
					*pFeatureParaLen=4;
					break;
			}
		    break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            OV8825_SetShutter(*pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            OV8825_NightMode((BOOL) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            OV8825_SetGain((UINT16) *pFeatureData16);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            //OV8825_isp_master_clock=*pFeatureData32;
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            OV8825_write_cmos_sensor(pSensorRegData->RegAddr, pSensorRegData->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            pSensorRegData->RegData = OV8825_read_cmos_sensor(pSensorRegData->RegAddr);
            break;
        case SENSOR_FEATURE_SET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&ov8825mipiraw_drv_lock);
                OV8825SensorCCT[i].Addr=*pFeatureData32++;
                OV8825SensorCCT[i].Para=*pFeatureData32++;
				spin_unlock(&ov8825mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_CCT_REGISTER:
            SensorRegNumber=FACTORY_END_ADDR;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=OV8825SensorCCT[i].Addr;
                *pFeatureData32++=OV8825SensorCCT[i].Para;
            }
            break;
        case SENSOR_FEATURE_SET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            for (i=0;i<SensorRegNumber;i++)
            {
            	spin_lock(&ov8825mipiraw_drv_lock);
                OV8825SensorReg[i].Addr=*pFeatureData32++;
                OV8825SensorReg[i].Para=*pFeatureData32++;
				spin_unlock(&ov8825mipiraw_drv_lock);
            }
            break;
        case SENSOR_FEATURE_GET_ENG_REGISTER:
            SensorRegNumber=ENGINEER_END;
            if (*pFeatureParaLen<(SensorRegNumber*sizeof(SENSOR_REG_STRUCT)+4))
                return FALSE;
            *pFeatureData32++=SensorRegNumber;
            for (i=0;i<SensorRegNumber;i++)
            {
                *pFeatureData32++=OV8825SensorReg[i].Addr;
                *pFeatureData32++=OV8825SensorReg[i].Para;
            }
            break;
        case SENSOR_FEATURE_GET_REGISTER_DEFAULT:
            if (*pFeatureParaLen>=sizeof(NVRAM_SENSOR_DATA_STRUCT))
            {
                pSensorDefaultData->Version=NVRAM_CAMERA_SENSOR_FILE_VERSION;
                pSensorDefaultData->SensorId=OV8825_SENSOR_ID;
                memcpy(pSensorDefaultData->SensorEngReg, OV8825SensorReg, sizeof(SENSOR_REG_STRUCT)*ENGINEER_END);
                memcpy(pSensorDefaultData->SensorCCTReg, OV8825SensorCCT, sizeof(SENSOR_REG_STRUCT)*FACTORY_END_ADDR);
            }
            else
                return FALSE;
            *pFeatureParaLen=sizeof(NVRAM_SENSOR_DATA_STRUCT);
            break;
        case SENSOR_FEATURE_GET_CONFIG_PARA:
            memcpy(pSensorConfigData, &OV8825SensorConfigData, sizeof(MSDK_SENSOR_CONFIG_STRUCT));
            *pFeatureParaLen=sizeof(MSDK_SENSOR_CONFIG_STRUCT);
            break;
        case SENSOR_FEATURE_CAMERA_PARA_TO_SENSOR:
            OV8825_camera_para_to_sensor();
            break;

        case SENSOR_FEATURE_SENSOR_TO_CAMERA_PARA:
            OV8825_sensor_to_camera_para();
            break;
        case SENSOR_FEATURE_GET_GROUP_COUNT:
            *pFeatureReturnPara32++=OV8825_get_sensor_group_count();
            *pFeatureParaLen=4;
            break;
        case SENSOR_FEATURE_GET_GROUP_INFO:
            OV8825_get_sensor_group_info(pSensorGroupInfo->GroupIdx, pSensorGroupInfo->GroupNamePtr, &pSensorGroupInfo->ItemCount);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_GROUP_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_ITEM_INFO:
            OV8825_get_sensor_item_info(pSensorItemInfo->GroupIdx,pSensorItemInfo->ItemIdx, pSensorItemInfo);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_SET_ITEM_INFO:
            OV8825_set_sensor_item_info(pSensorItemInfo->GroupIdx, pSensorItemInfo->ItemIdx, pSensorItemInfo->ItemValue);
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ITEM_INFO_STRUCT);
            break;

        case SENSOR_FEATURE_GET_ENG_INFO:
            pSensorEngInfo->SensorId = 129;
            pSensorEngInfo->SensorType = CMOS_SENSOR;
            pSensorEngInfo->SensorOutputDataFormat=SENSOR_OUTPUT_FORMAT_RAW_B;
            *pFeatureParaLen=sizeof(MSDK_SENSOR_ENG_INFO_STRUCT);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *pFeatureReturnPara32=LENS_DRIVER_ID_DO_NOT_CARE;
            *pFeatureParaLen=4;
            break;

        case SENSOR_FEATURE_INITIALIZE_AF:
            break;
        case SENSOR_FEATURE_CONSTANT_AF:
            break;
        case SENSOR_FEATURE_MOVE_FOCUS_LENS:
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            OV8825SetVideoMode(*pFeatureData16);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            OV8825GetSensorID(pFeatureReturnPara32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            OV8825SetAutoFlickerMode((BOOL)*pFeatureData16, *(pFeatureData16+1));
	        break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            OV8825SetTestPatternMode((BOOL)*pFeatureData16);
            break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			OV8825MIPISetMaxFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, *(pFeatureData32+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			OV8825MIPIGetDefaultFramerateByScenario((MSDK_SCENARIO_ID_ENUM)*pFeatureData32, (MUINT32 *)(*(pFeatureData32+1)));
			break;
        default:
            break;
    }
    return ERROR_NONE;
}	/* OV8825FeatureControl() */


SENSOR_FUNCTION_STRUCT	SensorFuncOV8825=
{
    OV8825Open,
    OV8825GetInfo,
    OV8825GetResolution,
    OV8825FeatureControl,
    OV8825Control,
    OV8825Close
};

UINT32 OV8825_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&SensorFuncOV8825;

    return ERROR_NONE;
}   /* SensorInit() */

