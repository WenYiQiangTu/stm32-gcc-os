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

#ifdef RT_USING_RTGUI
#include <rtgui/rtgui.h>
#include <rtgui/driver.h>
#include <rtgui/rtgui_server.h>
#include <rtgui/rtgui_system.h>

// TODO touch
/*#include <touch.h> */
/*#include <codec.h>*/
#include "ff.h"

extern void radio_rtgui_init(void);

#endif


/* thread phase init */
void rt_init_thread_entry(void *parameter)
{
#ifdef RT_USING_DFS
//FATFS fs;            // Work area (file system object) for logical drive
    {
        
        extern void ff_convert_init();

        /* init the device filesystem */
        dfs_init();

        /* init the elmFat filesystem */
        elm_init();


        //f_mount(0, &fs);
#if 1
        /* mount spi flash fat as root directory */
        //if (dfs_mount("spi0", "/", "elm", 0, 0) == 0)
        {
            rt_kprintf("SPI File System initialized!\n");

            /* mount sd card fat partition 1 as SD directory */
            if (dfs_mount("sd0", "/", "elm", 0, 0) == 0)
                rt_kprintf("SD File System initialized!\n");
            else
                rt_kprintf("SD File System init failed!\n");
        }
        //else
        //    rt_kprintf("SPI File System init failed!\n");
#endif
    }
#endif
       /* RTGUI Initialization */
#ifdef RT_USING_RTGUI
    radio_rtgui_init();
#endif

    /* LwIP Initialization */
#ifdef RT_USING_LWIP
    {
        extern void lwip_sys_init(void);
        extern void rt_hw_enc28j60_init(void);

        eth_system_device_init();

        /* register ethernetif device */
        rt_hw_enc28j60_init();
        /* init all device */
        rt_device_init_all();

        /* init lwip system */
        lwip_sys_init();
        rt_kprintf("TCP/IP initialized!\n");
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

 //   tea5756_hardware_init();   
 
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

#ifdef RT_USING_RTGUI
    today_init();
#endif
    return 0;
}

/*@}*/
