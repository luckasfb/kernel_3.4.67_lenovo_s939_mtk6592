#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 2,
	.polling_mode_ps =0,
	.polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    //.i2c_addr   = {0x0C, 0x48, 0x78, 0x00},
    //.als_level  = { 0,  1,  1,   7,  15,  15,  100, 1000, 2000,  3000,  6000, 10000, 14000, 18000, 20000},
    //.als_value  = {40, 40, 90,  90, 160, 160,  225,  320,  640,  1280,  1280,  2600,  2600, 2600,  10240, 10240},
    /* MTK: modified to support AAL */
	/*lenovo_sw liaohj modify for stellap level 2013-10-28 ---begin*/
    .als_level	= {1, 8, 30, 100, 170, 256, 1186, 1507, 1843, 2454, 3183, 3703, 5026, 6702, 11745, 18784, 22509},
    .als_value	= {5, 30, 60, 90, 150, 200, 350, 450, 550, 720, 950, 1100, 1500, 2000, 3500, 5600, 6700, 8700},
	/*lenovo_sw liaohj modify for stellap level 2013-10-28 ---end*/
    .ps_threshold_high = 90,
    .ps_threshold_low = 75,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}

//#define THRES_TOLERANCE		26	//I think this is a proper value. It should not be too big.
//#define THRES_DEFAULT_DIFF	10 //15 //35
int RPR410_THRES_TOLERANCE = 26;
int RPR410_THRES_DEFAULT_DIFF = 10;

