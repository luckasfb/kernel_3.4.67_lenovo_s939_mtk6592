#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_alsps.h>
//#include <mach/mt6577_pm_ldo.h>

static struct alsps_hw cust_alsps_hw = {
    .i2c_num    = 2,
    .polling_mode_ps =0,
    .polling_mode_als =1,
    .power_id   = MT65XX_POWER_NONE,    /*LDO is not used*/
    .power_vol  = VOL_DEFAULT,          /*LDO is not used*/
    .i2c_addr   = {0x72, 0x48, 0x78, 0x00},
    /*Lenovo-sw chenlj2 add 2011-06-03,modify parameter below two lines*/
    /*lenovo-sw jixj 2011.12.22 modified being*/
    /*
    .als_level  = { 4, 40,  80,   120,   160, 250,  400, 800, 1200,  1600, 2000, 3000, 5000, 10000, 65535},
    .als_value  = {10, 20,20,  120, 120, 280,  280,  280, 1600,  1600,  1600,  6000,  6000, 9000,  10240, 10240},
    */
    /*Lenovo chenlj2 20130725 modify for new Light sensor param begin*/
    .als_level  = {2*100, 43*100, 90*100, 110*100, 180*100,  240*100, 524*100,  674*100, 823*100, 1078*100, 1422*100, 1646*100, 2244*100, 2992*100, 5236*100, 8300*100, 10023*100, 13023*100},
    .als_value  = {5*100, 30*100, 60*100, 90*100,  150*100,  200*100, 350*100,  450*100, 550*100, 720*100,  950*100,  1100*100, 1500*100, 2000*100, 3500*100, 5600*100, 6700*100, 8700*100},
    /*Lenovo chenlj2 20130725 modify for new Light sensor param end*/
    /*lenovo-sw jixj 2011.12.22 modified end*/
    /*Lenovo-sw chenlj2 modified 2011-08-09*/
    /*lenovo-sw molg1 add for cali parameter 20140327 begin*/
    .ps_level  = {50, 80, 100, 120, 150, 200, 300, 400, 600,1023},
    .ps_close_value  = {140, 33, 29, 24, 22, 18, 16, 14, 13, 0},
    .ps_far_value = {88, 22, 20, 18, 16, 14, 13, 12, 12, 0},
    .ps_min_diff = 50,/* used for minux value between near ps level*/
     /*lenovo-sw molg1 add for cali parameter 20140327 end*/
    .ps_threshold_high = 900,
    .ps_threshold_low = 750,
    .ps_threshold = 900,
};
struct alsps_hw *get_cust_alsps_hw(void) {
    return &cust_alsps_hw;
}
/* lenovo scofield board sensors spec, plz don't change it!!! ---liaoxl.lenovo 4.19.2012 start */
int APDS9930_CMM_PPCOUNT_VALUE = 0x10;
int APDS9930_CMM_CONTROL_VALUE = 0x20;//0x68;
int ZOOM_TIME = 4;
