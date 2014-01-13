
#ifndef  __vs1003_h___INC
#define  __vs1003_h___INC

//触摸屏初始化
//使用外部8M晶振,PLL到72M频率		    
//正点原子@SCUT
//2008/12/14 
//V1.0  

/*-------------------------------
管脚对应
PC.1 PEN
PC.2 DIN
PC.3 CLK
PB.8 CS
PB.9 DOUT
//ADS7846有温度测量和压力测量功能
//可以参考PDF资料自己写	 
--------------------------------*/
//#define PEN  GPIOB->IDR&1<<11 //PB11  				 
//#define PEN  GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_11)



#define DOUT GPIOB->IDR&1<<14 //PA6数据输入	
#define TDIN  (1<<15)  // PA7
#define TCLK  (1<<13)  // PA5
#define TCS   (1<<12)  // PB12 
#define RST   (1<<0)  // PB14     			    
#define XDCS   (1<<6)  // PC6
#define TDIN_SET(x) GPIOB->ODR=(GPIOB->ODR&~TDIN)|(x ? TDIN:0)
#define TCLK_SET(x) GPIOB->ODR=(GPIOB->ODR&~TCLK)|(x ? TCLK:0)													    
#define TCS_SET(x)  GPIOB->ODR=(GPIOB->ODR&~TCS)|(x ? TCS:0)  
#define TRST_SET(x)  GPIOE->ODR=(GPIOE->ODR&~RST)|(x ? RST:0)  
#define TXDCS_SET(x)  GPIOC->ODR=(GPIOC->ODR&~XDCS)|(x ? XDCS:0)  

#define DREQ  GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_7)

#define VS_WRITE_COMMAND 	0x02
#define VS_READ_COMMAND 	0x03		 		 

#define VS_MODE        	0x00   
#define VS_STATUS      	0x01   
#define VS_BASS        	0x02   
#define VS_CLOCKF      	0x03   
#define VS_DECODE_TIME 	0x04   
#define VS_AUDATA      	0x05   
#define VS_WRAM        	0x06   
#define VS_WRAMADDR    	0x07   
#define VS_HDAT0       	0x08   
#define VS_HDAT1       	0x09   
#define VS_AIADDR      	0x0a   
#define VS_VOL         	0x0b   
#define VS_AICTRL0     	0x0c   
#define VS_AICTRL1     	0x0d   
#define VS_AICTRL2     	0x0e   
#define VS_AICTRL3     	0x0f   



#define MP3CMD_InitVS1003		0x11
#define MP3CMD_Play		    	0x12
#define MP3CMD_Pause			0x13
#define MP3CMD_Stop			0x14
#define MP3CMD_Next			0x15
#define MP3CMD_TestVS1003		0x16


/*
 * Status of the decoder
 */
#define VS_STATUS_STOPPED 0
#define VS_STATUS_RUNNING 1
#define VS_STATUS_EOF     2
#define VS_STATUS_EMPTY   4

/* Function prototypes */

extern void VsPlayerInit();
extern int VsPlayerReset(uint16_t mode);
extern int VsPlayerSetMode(uint16_t mode);
extern int VsPlayerKick(void);
extern int VsPlayerStop(void);
extern rt_bool_t VsPlayerFlush(void *arg);
//extern ureg_t VsPlayerInterrupts(ureg_t enable);
//extern ureg_t VsPlayerThrottle(ureg_t on);

extern uint16_t VsPlayTime(void);
extern rt_uint32_t VsGetStatus(void);

//extern int VsGetHeaderInfo(VS_HEADERINFO *vshi);

extern uint16_t VsMemoryTest(void);

extern int VsSetVolume(rt_uint8_t left, rt_uint8_t right);
//extern int VsBeep(uint8_t fsin, uint8_t ms);


#endif   /* ----- #ifndef __vs1003_h___INC  ----- */
