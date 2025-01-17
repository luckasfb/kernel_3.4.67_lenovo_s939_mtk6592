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

/*****************************************************************************
*  Copyright Statement:
*  --------------------
*  This software is protected by Copyright and the information contained
*  herein is confidential. The software may not be copied and the information
*  contained herein may not be used or disclosed except with the written
*  permission of MediaTek Inc. (C) 2008
*
   BY OPENING THIS FILE, BUYER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
*  THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
*  RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO BUYER ON
*  AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
*  NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
*  SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
*  SUPPLIED WITH THE MEDIATEK SOFTWARE, AND BUYER AGREES TO LOOK ONLY TO SUCH
*  THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. MEDIATEK SHALL ALSO
*  NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE RELEASES MADE TO BUYER'S
*  SPECIFICATION OR TO CONFORM TO A PARTICULAR STANDARD OR OPEN FORUM.
*
*  BUYER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND CUMULATIVE
*  LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
*  AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
*  OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY BUYER TO
*  MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*
*  THE TRANSACTION CONTEMPLATED HEREUNDER SHALL BE CONSTRUED IN ACCORDANCE
*  WITH THE LAWS OF THE STATE OF CALIFORNIA, USA, EXCLUDING ITS CONFLICT OF
*  LAWS PRINCIPLES.  ANY DISPUTES, CONTROVERSIES OR CLAIMS ARISING THEREOF AND
*  RELATED THERETO SHALL BE SETTLED BY ARBITRATION IN SAN FRANCISCO, CA, UNDER
*  THE RULES OF THE INTERNATIONAL CHAMBER OF COMMERCE (ICC).
*
*****************************************************************************/

#ifndef BUILD_LK
#include <linux/string.h>
#endif
#include "lcm_drv.h"

#ifdef BUILD_LK
	#include <platform/mt_gpio.h>
#elif defined(BUILD_UBOOT)
	#include <asm/arch/mt_gpio.h>
#else
	#include <mach/mt_gpio.h>
#endif


// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  				(720)
#define FRAME_HEIGHT 				(1280)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

//#define LCM_DSI_CMD_MODE			0

#define LCM_ID    0x1283
#if 1//defined(LENOVO_PROJECT_SCOFIELD) || defined(LENOVO_PROJECT_SCOFIELDTD)
#define LCM_BACKLIGHT_EN GPIO18
#define LCM_ID_PIN GPIO114
#define LCM_COMPARE_BY_SW 0
#else
#define LCM_BACKLIGHT_EN GPIO116
#define LCM_ID_PIN GPIO92
#define LCM_COMPARE_BY_SW 1
#endif

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------
static unsigned int lcm_esd_test = FALSE; ///only for ESD test

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    	(lcm_util.set_reset_pin((v)))

#define UDELAY(n) 		(lcm_util.udelay(n))
#define MDELAY(n) 		(lcm_util.mdelay(n))


// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define dsi_set_cmdq_V4(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq_V4(pdata, queue_size, force_update)

#define wrtie_cmd(cmd)						lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)			lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) 						lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)    
static unsigned int lcm_cabcmode_index = 0;
       
static unsigned int lcm_compare_id(void);
static unsigned int lcm_check_status(void);
// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}


static void lcm_get_params(LCM_PARAMS *params)
{
	unsigned int div1 = 0;
	unsigned int div2 = 0;
	unsigned int pre_div = 0;
	unsigned int post_div = 0;
	unsigned int fbk_sel = 0;
	unsigned int fbk_div = 0;
	unsigned int lane_no = 0;//lcm_params->dsi.LANE_NUM;

	unsigned int cycle_time;
	unsigned int ui;
	unsigned int hs_trail_m, hs_trail_n;
	

		memset(params, 0, sizeof(LCM_PARAMS));
	
		params->type   = LCM_TYPE_DSI;

		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		// enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;

#if (LCM_DSI_CMD_MODE)
		params->dsi.mode   = CMD_MODE;
#else
		params->dsi.mode   = BURST_VDO_MODE;
#endif
	
		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_FOUR_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
		params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
		params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Highly depends on LCD driver capability.
		// Not support in MT6573
		params->dsi.packet_size=256;

		// Video mode setting		
		params->dsi.intermediat_buffer_num = 0;

		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 2;
		params->dsi.vertical_backporch					= 14;
		params->dsi.vertical_frontporch					= 16;
		params->dsi.vertical_active_line				= FRAME_HEIGHT; 

		params->dsi.horizontal_sync_active				= 2;
		params->dsi.horizontal_backporch				= 34;
		params->dsi.horizontal_frontporch				= 34;
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;

		// Bit rate calculation
		params->dsi.pll_div1=0;		// fref=26MHz, fvco=fref*(div1+1)	(div1=0~63, fvco=500MHZ~1GHz)
		params->dsi.pll_div2=1; 		// div2=0~15: fout=fvo/(2*div2)
		params->dsi.fbk_div =16;    //fref=26MHz, fvco=fref*(fbk_div+1)*2/(div1_real*div2_real)
//		params->dsi.PLL_CLOCK=220;

//		params->dsi.HS_PRPR = 4;
//		params->dsi.pll_select=1;
		params->dsi.hs_read = 1;//need hs read
#if 1
		params->bl_app.min =1;
		params->bl_app.def =102;
		params->bl_app.max =255;

		params->bl_bsp.min =6;//min > 500ns
		params->bl_bsp.def =102;
		params->bl_bsp.max =255;
#endif

#if 1
        div1 = params->dsi.pll_div1;
        div2 = params->dsi.pll_div2;
        fbk_div = params->dsi.fbk_div;
		switch(div1)
   		{
	  		case 0:
		 	div1 = 1;
		 	break;

			case 1:
		 	div1 = 2;
		 	break;

			case 2:
			case 3:
			 div1 = 4;
			 break;

			default:
				div1 = 4;
				break;
		}

		switch(div2)
		{
			case 0:
				div2 = 1;
				break;
			case 1:
				div2 = 2;
				break;
			case 2:
			case 3:
				div2 = 4;
				break;
			default:
				div2 = 4;
				break;
		}

		switch(pre_div)
		{
		  case 0:
			 pre_div = 1;
			 break;

		  case 1:
			 pre_div = 2;
			 break;

		  case 2:
		  case 3:
			 pre_div = 4;
			 break;

		  default:
			 pre_div = 4;
			 break;
		}

		switch(post_div)
		{
		  case 0:
			 post_div = 1;
			 break;

		  case 1:
			 post_div = 2;
			 break;

		  case 2:
		  case 3:
			 post_div = 4;
			 break;

		  default:
			 post_div = 4;
			 break;
		}

		switch(fbk_sel)
		{
		  case 0:
			 fbk_sel = 1;
			 break;

		  case 1:
			 fbk_sel = 2;
			 break;

		  case 2:
		  case 3:
			 fbk_sel = 4;
			 break;

		  default:
			 fbk_sel = 4;
			 break;
		}
  		cycle_time=(1000*4*div2*div1)/(fbk_div*26)+0x01;

		ui=(1000*div2*div1)/(fbk_div*26*0x2)+0x01;
#define NS_TO_CYCLE(n, c)	((n) / (c))

   hs_trail_m=1;
   hs_trail_n= NS_TO_CYCLE(((hs_trail_m * 0x4) + 0x60 - 75), cycle_time);
   // +3 is recommended from designer becauase of HW latency
   params->dsi.HS_TRAIL = ((hs_trail_m > hs_trail_n) ? hs_trail_m : hs_trail_n);

   params->dsi.HS_PRPR	=  NS_TO_CYCLE((0x40 + 0x5 * ui + 25), cycle_time);
   // HS_PRPR can't be 1.
   if (params->dsi.HS_PRPR == 0)
	  params->dsi.HS_PRPR = 1;

#endif

}


static void lcm_init_register(void)
{
	unsigned int data_array[16];

data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0x00043902;
data_array[1] = 0x018312FF;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0x80001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x008312FF;
dsi_set_cmdq(&data_array, 2, 1);
//enable inverse mode 
data_array[0] = 0x00023902;
data_array[1] = 0x00008300;
dsi_set_cmdq(&data_array, 2, 1);
data_array[0] = 0x00023902;
data_array[1] = 0x000041B3;
dsi_set_cmdq(&data_array, 2, 1);
//enable end
data_array[0] = 0x80001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000A3902;
data_array[1] = 0x006400C0;
data_array[2] = 0x6400110F;
data_array[3] = 0x0000110F;
dsi_set_cmdq(&data_array, 4, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00073902;
data_array[1] = 0x005C00C0;
data_array[2] = 0x00040001;
dsi_set_cmdq(&data_array, 3, 1);

data_array[0] = 0x87001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x18C41500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xB4001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00023902;
data_array[1] = 0x000050C0;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0x81001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x66C11500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0x81001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x000282C4;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x49C41500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xC6001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x03B01500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00053902;
data_array[1] = 0x021102F5;
data_array[2] = 0x00000011;
dsi_set_cmdq(&data_array, 3, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x50C51500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0x94001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x66C51500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xB2001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x000000F5;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0xB4001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x000000F5;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0xB6001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x000000F5;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0xB8001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x000000F5;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0x94001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x02F51500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xBA001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x03F51500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xB2001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x40C51500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xB4001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0xC0C51500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xA0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x061005C4;
data_array[2] = 0x10150502;
data_array[3] = 0x02071005;
data_array[4] = 0x00101505;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xB0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x000000C4;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0x91001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x005019C5;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x00BCBCD8;
dsi_set_cmdq(&data_array, 2, 1);


//VCOM OTP
//data_array[0] = 0x00001500;
//dsi_set_cmdq(&data_array, 1, 1);
//data_array[0] = 0x64D91500;
//dsi_set_cmdq(&data_array, 1, 1);


//GAMMA setting
/*
//Scofield GAMMA2.4
data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x0B0701E1;
data_array[2] = 0x0B0D060D;
data_array[3] = 0x1007040A;
data_array[4] = 0x0A110F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);

data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x0B0701E2;
data_array[2] = 0x0B0D060D;
data_array[3] = 0x1007040A;
data_array[4] = 0x0A110F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);
*/
//Orise GAMMA2.0
/*
data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x070301E1;
data_array[2] = 0x0B07060D;
data_array[3] = 0x1507040A;
data_array[4] = 0x0E150F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);

data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x070301E2;
data_array[2] = 0x0B07060D;
data_array[3] = 0x1507040A;
data_array[4] = 0x0E150F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);
*/
//GAMMA2.2

data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x080401E1;
data_array[2] = 0x0B08060D;
data_array[3] = 0x1007040A;
data_array[4] = 0x0A110F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);

data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x080401E2;
data_array[2] = 0x0B08060D;
data_array[3] = 0x1007040A;
data_array[4] = 0x0A110F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);

//GAMMA2.4
/*
data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x090501E1;
data_array[2] = 0x0B08060D;
data_array[3] = 0x0E07040A;
data_array[4] = 0x080E0F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);

data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00113902;
data_array[1] = 0x090501E2;
data_array[2] = 0x0B08060D;
data_array[3] = 0x0E07040A;
data_array[4] = 0x080E0F08;
data_array[5] = 0x00000001;
dsi_set_cmdq(&data_array, 6, 1);
*/


data_array[0] = 0xB0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00033902;
data_array[1] = 0x00B804C5;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0xBB001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x80C51500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0x80001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000C3902;
data_array[1] = 0x000000CB;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
dsi_set_cmdq(&data_array, 4, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x000000CB;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xA0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x000000CB;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xB0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x000000CB;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xC0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x050505CB;
data_array[2] = 0x00050505;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xD0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x000000CB;
data_array[2] = 0x05050000;
data_array[3] = 0x05050505;
data_array[4] = 0x00000505;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xE0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CB;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00050500;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xF0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000C3902;
data_array[1] = 0xFFFFFFCB;
data_array[2] = 0xFFFFFFFF;
data_array[3] = 0xFFFFFFFF;
dsi_set_cmdq(&data_array, 4, 1);

data_array[0] = 0x80001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x0E0C0ACC;
data_array[2] = 0x00040210;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x000000CC;
data_array[2] = 0x2D2E0000;
data_array[3] = 0x0F0D0B09;
data_array[4] = 0x00000301;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xA0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CC;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x002D2E00;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xB0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x0B0D0FCC;
data_array[2] = 0x00010309;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xC0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00103902;
data_array[1] = 0x000000CC;
data_array[2] = 0x2E2D0000;
data_array[3] = 0x0A0C0E10;
data_array[4] = 0x00000204;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xD0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CC;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x002E2D00;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0x80001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000D3902;
data_array[1] = 0x00038DCE;
data_array[2] = 0x8B00038C;
data_array[3] = 0x038A0003;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CE;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xA0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x040B38CE;
data_array[2] = 0x000000FC;
data_array[3] = 0xFD040A38;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xB0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x040938CE;
data_array[2] = 0x000000FE;
data_array[3] = 0xFF040838;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xC0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x050738CE;
data_array[2] = 0x00000000;
data_array[3] = 0x01050638;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xD0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x050538CE;
data_array[2] = 0x00000002;
data_array[3] = 0x03050438;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0x80001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CF;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0x90001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CF;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xA0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CF;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xB0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000F3902;
data_array[1] = 0x000000CF;
data_array[2] = 0x00000000;
data_array[3] = 0x00000000;
data_array[4] = 0x00000000;
dsi_set_cmdq(&data_array, 5, 1);

data_array[0] = 0xC0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x000C3902;
data_array[1] = 0x200101CF;
data_array[2] = 0x01000020;
data_array[3] = 0x08000002;
dsi_set_cmdq(&data_array, 4, 1);

data_array[0] = 0xB5001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00073902;
data_array[1] = 0xFFF133C5;
data_array[2] = 0x00FFF133;
dsi_set_cmdq(&data_array, 3, 1);

data_array[0] = 0xA0001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x02C11500;
dsi_set_cmdq(&data_array, 1, 1);

//pwm frequence 68k
data_array[0] = 0xB1001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00C61500;
dsi_set_cmdq(&data_array, 1, 1);

data_array[0] = 0xB4001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x10C61500;
dsi_set_cmdq(&data_array, 1, 1);


data_array[0] = 0x00001500;
dsi_set_cmdq(&data_array, 1, 1);
data_array[0] = 0x00043902;
data_array[1] = 0xFFFFFFFF;
dsi_set_cmdq(&data_array, 2, 1);

data_array[0] = 0x00351500;//te on
dsi_set_cmdq(&data_array, 1, 1);    

data_array[0] = 0x24531500;//bl mode
dsi_set_cmdq(&data_array, 1, 1); 

data_array[0] = 0x00551500;//cabc 03:move 00:off
dsi_set_cmdq(&data_array, 1, 1); 

data_array[0] = 0x00110500;                
dsi_set_cmdq(&data_array, 1, 1); 
MDELAY(150); 

data_array[0] = 0x00290500;                
dsi_set_cmdq(&data_array, 1, 1);    
MDELAY(5); 

} 

static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(5);
	SET_RESET_PIN(0);
	MDELAY(5);
	SET_RESET_PIN(1);
	MDELAY(10);

#ifdef BUILD_LK
    printf("[wj]otm1283a init code.\n");
#endif
    lcm_init_register();

mt_set_gpio_out(LCM_BACKLIGHT_EN, 1);
}


static void lcm_suspend(void)
{
	unsigned int data_array[16];

#if defined(BUILD_LK) || defined(BUILD_UBOOT)
	printf("%s, lcm_init \n", __func__);
#else
	printk("%s, lcm_init \n", __func__);
#endif	
//begin lenovo jixu add for power consume 20130131
mt_set_gpio_out(LCM_BACKLIGHT_EN, 0);//diable backlight ic
//end lenovo jixu add for power consume 20130131
    	data_array[0] = 0x00280500;                
    	dsi_set_cmdq(&data_array, 1, 1);    
		MDELAY(20);
    	data_array[0] = 0x00100500;                
    	dsi_set_cmdq(&data_array, 1, 1); 
		MDELAY(150); 
//begin lenovo jixu add for power consume 20130206
//		SET_RESET_PIN(0);
//end lenovo jixu add for power consume 20130206
}


static void lcm_resume(void)
{
int i;
unsigned int data_array[16];

#if defined(BUILD_LK) || defined(BUILD_UBOOT)
	printf("%s, lcm_resume otm1283a\n", __func__);
#else
	printk("%s, lcm_resume otm1283a\n", __func__);
#endif	
#if 0
	SET_RESET_PIN(1);
	MDELAY(5);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(50);

	lcm_init_register();

		mt_set_gpio_out(LCM_BACKLIGHT_EN, 1);

#else
	data_array[0] = 0x00110500; 			   
	dsi_set_cmdq(&data_array, 1, 1); 
	MDELAY(150); 
	
	data_array[0] = 0x00290500; 			   
	dsi_set_cmdq(&data_array, 1, 1);	
	MDELAY(5); 
#endif
mt_set_gpio_out(LCM_BACKLIGHT_EN, 1);

}

static void lcm_update(unsigned int x, unsigned int y,
                       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0]= 0x00053902;
	data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2]= (x1_LSB);
	data_array[3]= 0x00053902;
	data_array[4]= (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[5]= (y1_LSB);
	data_array[6]= 0x002c3909;

	dsi_set_cmdq(&data_array, 7, 0);

}

static void lcm_setbacklight(unsigned int level)
{
	unsigned int data_array[16];


	if(level > 255) 
	level = 255;

	data_array[0] = 0x00023902;      
	data_array[1] = (0x51|(level<<8)); 
	#ifdef BUILD_LK
	dsi_set_cmdq_V4(&data_array, 2, 1); 
	#else
	dsi_set_cmdq(&data_array, 2, 1); 
	#endif

}

static unsigned int lcm_esd_check(void)
{
unsigned char buffer[2],ret;

#ifndef BUILD_UBOOT
	if(lcm_esd_test)
	{
	lcm_esd_test = FALSE;
	return TRUE;
	}

	/// please notice: the max return packet size is 1
	/// if you want to change it, you can refer to the following marked code
	/// but read_reg currently only support read no more than 4 bytes....
	/// if you need to read more, please let BinHan knows.
	/*
	unsigned int data_array[16];
	unsigned int max_return_size = 1;

	data_array[0]= 0x00003700 | (max_return_size << 16); 

	dsi_set_cmdq(&data_array, 1, 1);
	*/
	//return FALSE;//jixu temp
	read_reg_v2(0x0A, buffer,2);
	#ifndef BUILD_LK
	//printk("[JX] %s 0x0A 0=0x%x 1=0x%x \n",__func__,buffer[0],buffer[1]);
	#endif
	ret = buffer[0]==0x9C?0:1;
	#ifndef BUILD_LK
	//printk("[JX] %s ret=%d \n",__func__,ret);
	#endif
	if(ret) return TRUE;

	read_reg_v2(0x0E, buffer,2);
	#ifndef BUILD_LK
	//printk("[JX] %s 0x0E 0=0x%x 1=0x%x \n",__func__,buffer[0],buffer[1]);
	#endif
	ret = ((buffer[0])&(0xf0))==0x80?0:1;
	#ifndef BUILD_LK
	//printk("[JX] %s ret=%d \n",__func__,ret);
	#endif
	if(ret) return TRUE;
	else return FALSE;
#endif
}

static unsigned int lcm_esd_recover(void)
{
#ifndef BUILD_LK
	printk("[JX]+ %s \n",__func__);
#endif
	lcm_init();
#ifndef BUILD_LK
	printk("[JX]- %s \n",__func__);
#endif

}

static unsigned int lcm_compare_id(void)
{
#if LCM_COMPARE_BY_SW
	unsigned int id=0;
	unsigned char buffer[4];
	unsigned int data_array[16];  

    SET_RESET_PIN(1);
   MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(50);//Must over 6 ms

   	data_array[0] = 0x00023902;
    	data_array[1] = 0x00000000;    
    	dsi_set_cmdq_V4(&data_array, 2, 1);     
    	data_array[0] = 0x00043902; //EXTC=1
    	data_array[1] = 0x018312FF;                 
    	dsi_set_cmdq_V4(&data_array, 2, 1);    

    	data_array[0] = 0x00023902; //ORISE mode enable
    	data_array[1] = 0x00008000;    
    	dsi_set_cmdq_V4(&data_array, 2, 1);     
    	data_array[0] = 0x00033902;
    	data_array[1] = 0x008312FF;    
    	dsi_set_cmdq_V4(&data_array, 2, 1);  
 
    	data_array[0] = 0x00023902; //ORISE mode enable
    	data_array[1] = 0x0000C600;    
    	dsi_set_cmdq_V4(&data_array, 2, 1);     
    	data_array[0] = 0x00023902;
    	data_array[1] = 0x000003B0;    
    	dsi_set_cmdq_V4(&data_array, 2, 1);    

	data_array[0] = 0x00043700;
	dsi_set_cmdq_V4(&data_array, 1, 1);

	read_reg_v2(0xA1, buffer, 4);
	id = buffer[3]|(buffer[2]<<8); //we only need ID
//        id = read_reg(0xDA);
#if defined(BUILD_LK) || defined(BUILD_UBOOT)
	printf("%s, otm1283a id = 0x%08x\n", __func__, id);
#else
       printk("%s, otm1283a id = 0x%08x\n", __func__, id);
#endif

	return (LCM_ID == id)?1:0;
//	return 1;
#else
	unsigned int ret = 0;
	ret = mt_get_gpio_in(LCM_ID_PIN);
#if defined(BUILD_LK)
	printf("%s, [jx]otm1283a LCM_ID_PIN = %d \n", __func__, ret);
#endif	

	return (ret == 1)?1:0;
#endif
}
static void lcm_setcabcmode(unsigned int mode)
{
	unsigned int data_array[16];

	#ifdef BUILD_LK
	printf("%s mode=%d\n",__func__,mode);
	#else
	printk("%s mode=%d\n",__func__,mode);
	#endif

	switch(mode){
		case 0:
			lcm_cabcmode_index=0;
			break;
		case 1:
			lcm_cabcmode_index=1;
			break;
		case 2:
			lcm_cabcmode_index=2;
			break;
		case 3:
			lcm_cabcmode_index=3;
			break;
		default:
			break;
	}
	data_array[0] = 0x00023902;      
	data_array[1] = (0x55|(lcm_cabcmode_index<<8)); 
	dsi_set_cmdq(&data_array, 2, 1); 

	 MDELAY(10);
}



static void lcm_getcabcstate(unsigned int * state)
{

	*state = lcm_cabcmode_index;
	return ;
}
#ifdef LENOVO_LCD_BACKLIGHT_COMPATIBLE_SUPPORT
static int lcm_detect_backlight(void)
{
//return 1; that is lcm control backlight
//for scofield GPIO127=1 and GPIO128=1 is BB PWM control;
		unsigned int ret_gpio127 = 0;
		unsigned int ret_gpio128 = 0;
		ret_gpio127 = mt_get_gpio_in(GPIO127);
		ret_gpio128 = mt_get_gpio_in(GPIO128);
#if defined(BUILD_LK)
		printf("[JX] %s,  gpio127=%d gpio128=%d \n", __func__, ret_gpio127,ret_gpio128);
#endif	
	return !(ret_gpio127 & ret_gpio128);

}
#endif
static void lcm_set_inverse(unsigned int mode)
{
	unsigned int data_array[16];

#ifdef BUILD_LK
	printf("%s mode=%d\n",__func__,mode);
#else
	printk("%s mode=%d\n",__func__,mode);
#endif

	if(mode){
		data_array[0] = 0x00210500;
		dsi_set_cmdq(data_array, 1, 1);
	}else
	{
		data_array[0] = 0x00200500;
		dsi_set_cmdq(data_array, 1, 1);
		
	}
	
	 MDELAY(10);
}

LCM_DRIVER otm1283a_hd720_dsi_vdo_boe_5_0inch_lcm_drv = 
{
	.name = "otm1283a_hd720_dsi_vdo_boe_5_0inch",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.esd_check   = lcm_esd_check,
  .esd_recover   = lcm_esd_recover,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
	.set_backlight	= lcm_setbacklight,
#if defined(LENOVO_LCD_BACKLIGHT_CONTROL_BY_LCM) || defined(LENOVO_LCD_BACKLIGHT_COMPATIBLE_SUPPORT)
	.set_cabcmode = lcm_setcabcmode,
	.get_cabcmode = lcm_getcabcstate,
#endif
#ifdef LENOVO_LCD_BACKLIGHT_COMPATIBLE_SUPPORT
	.detect_backlight = lcm_detect_backlight,
#endif
	.set_inversemode = lcm_set_inverse,

};
