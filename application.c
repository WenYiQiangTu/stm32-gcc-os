/*
 * File      : application.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2006, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2009-01-05     Bernard      the first version
 */

/**
 * @addtogroup STM32
 */
/*@{*/

#include <rtthread.h>

#include <stdio.h>

#ifdef RT_USING_DFS
/* dfs init */
#include "dfs_init.h"
#include "dfs_elm.h"
#include "dfs_fs.h"
#endif

#ifdef RT_USING_RTGUI
#include <rtgui/rtgui.h>
#include <rtgui/driver.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_system.h>

// TODO touch
/*#include <touch.h> */
/*#include <codec.h>*/

extern void radio_rtgui_init(void);

#endif


/* thread phase init */
void rt_init_thread_entry(void *parameter)
{
#ifdef RT_USING_DFS
    {
        extern void ff_convert_init();

        /* init the device filesystem */
        dfs_init();

        /* init the elmFat filesystem */
        elm_init();

        /* mount sdcard device as root directory */
        if (dfs_mount("sd0", "/SD", "elm", 0, 0) == 0)
        {
            rt_kprintf("SD filesystem %s Initialization pass\r\n", "sd0");
        }
        else 
        {
            rt_kprintf("SD filesystem Initialization failed\r\n");
        }
    }
#endif

       /* RTGUI Initialization */
#ifdef RT_USING_RTGUI
        {
            extern void rt_hw_key_init(void);
            extern void remote_init(void);
    
            rt_device_t lcd;
            rtgui_rect_t rect;
    
            //radio_rtgui_init();
            //rt_hw_lcd_init();
    
            lcd = rt_device_find("lcd");
            if (lcd != RT_NULL)
            {
                rt_device_init(lcd);
                //rtgui_graphic_set_device(lcd);
    
                /* init RT-Thread/GUI server */
                rtgui_system_server_init();
    
                /* register dock panel */
                rect.x1 = 0;
                rect.y1 = 0;
                rect.x2 = 240;
                rect.y2 = 25;
                //rtgui_panel_register("info", &rect);
                //rtgui_panel_set_nofocused("info");
    
                /* register main panel */
                rect.x1 = 0;
                rect.y1 = 25;
                rect.x2 = 240;
                rect.y2 = 320;
                //rtgui_panel_register("main", &rect);
                //rtgui_panel_set_default_focused("main");
    
                //info_init();
                //player_init();
            }
    
            //rt_hw_key_init();
            //rtgui_touch_hw_init("spi11");
            //remote_init();
        }
#endif
}


int rt_application_init()
{

/*
    float fmreq;
    fmreq = 87.5;
    fmreq = fmreq + 0.1;   

				 printf("%f\r\n", fmreq);
    do {
        if (fmreq > 108 ) break;

        fmreq++;

        printf("%f\r\n", fmreq);
    } while (1);
*/
//    tea5756_hardware_init();

//tea5657_thread_init();


//tea5657_init();
//tea5657_thread_entry();
//    stm32fighting_lcd_hard_init();

        rt_thread_t init_thread;
    
#if (RT_THREAD_PRIORITY_MAX == 32)
        init_thread = rt_thread_create("init",
                                       rt_init_thread_entry, RT_NULL,
                                       2048, 8, 20);
#else
        init_thread = rt_thread_create("init",
                                       rt_init_thread_entry, RT_NULL,
                                       2048, 80, 20);
#endif
        if (init_thread != RT_NULL) rt_thread_startup(init_thread);


	return 0;
}

/*@}*/
