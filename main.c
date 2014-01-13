/*

                         \\\|///
                       \\  - -  //
                        (  @ @  )
+---------------------oOOo-(_)-oOOo-------------------------+
|                 奋斗版STM32开发板试验程序                 |
|                      FM收音机实验                         |
|                          Sun68                            |
|                        2009.12.8                          |
|                 演示通过串口控制FM收音机                  |
|                奋斗STM32嵌入式开发工作室                  |
|                  http://OutSTM.5d6d.com 				    |             
|				   QQ: 9191274	 							|
|                              Oooo                         |
+-----------------------oooO--(   )-------------------------+
                       (   )   ) /
                        \ (   (_/
                         \_)     


      奋斗版STM32开发板TEA5767的演示DEMO（与串口助手类软件配合使用，速率115200bps） 
 
  H(h)---帮助                    S(s)---搜索节目  
  D(d)---显示有效节目            xxP(xxp)---播放选定的节目(如12P)
  xx.xM(xx.xm)---直接选定频率(如98.8M)  
*/
/* Includes ------------------------------------------------------------------*/
#include <rtthread.h>

//#include "stm32f10x_lib.h"
#include "stm32f10x_usart.h"

#include "misc.h"
#include "stm32f10x.h"
#include "math.h"

#include "stdlib.h"
#include "stdio.h"
#include <string.h>

/** @addtogroup StdPeriph_Examples
  * @{
  */

/** @addtogroup I2C_M24C08_EEPROM
  * @{
  */ 
#define TxBufferSize1   (countof(TxBuffer1) - 1)
#define RxBufferSize1   (countof(TxBuffer1) - 1)

/* Private macro -------------------------------------------------------------*/
#define countof(a)   (sizeof(a) / sizeof(*(a)))

uint8_t TxBuffer1[] = "奋斗版STM32开发板TEA5767的演示DEMO";
uint8_t RxBuffer1[RxBufferSize1],rec_f;
__IO uint8_t TxCounter1 = 0x00;
__IO uint8_t RxCounter1 = 0x00; 
uint8_t NbrOfDataToTransfer1 = TxBufferSize1;
uint8_t NbrOfDataToRead1 = RxBufferSize1;
USART_InitTypeDef USART_InitStructure;
//USART_InitTypeDef USART_InitStruct;
USART_ClockInitTypeDef USART_ClockInitStruct;

int GetKey (void) ;
int SendChar (int ch) ;
/* Private typedef -----------------------------------------------------------*/
typedef enum {FAILED = 0, PASSED = !FAILED} TestStatus;

/* Private define ------------------------------------------------------------*/
#define Tea5767_WriteAddress1    0xc0
#define Tea5767_ReadAddress1     0xc1
#define ADDRESS_AMP              0X88	     //PT2314 ID
#define BufferSize1             (countof(Tx1_Buffer)-1)
#define BufferSize2             (countof(Tx2_Buffer)-1)
#define EEPROM_WriteAddress2    (EEPROM_WriteAddress1 + BufferSize1)
#define EEPROM_ReadAddress2     (EEPROM_ReadAddress1 + BufferSize1)

/* Private macro -------------------------------------------------------------*/
#define countof(a) (sizeof(a) / sizeof(*(a)))

/* Private variables ---------------------------------------------------------*/
uint8_t Tx1_Buffer[] = {0XF0,0X2C,0XD0,0X12,0X40};
uint8_t Rx1_Buffer[] = {0XF0,0X2C,0XD0,0X12,0X40};
uint8_t Tx2_Buffer[] = {0X00,0Xc0,0xe0,0X41,0X6e,0X7e};
//uint8_t Rx1_Buffer[BufferSize1], Rx2_Buffer[BufferSize2];	  

unsigned long   FM_FREQ=98800000;		  //默认西安交通广播98.8MHz
//unsigned long   FM_FREQ=91600000;
unsigned long FM_PLL;
unsigned char PLL_HIGH=0;
unsigned char PLL_LOW=0;
unsigned char len=0; 

volatile TestStatus TransferStatus1 = FAILED, TransferStatus2 = FAILED;
    
/* Private functions ---------------------------------------------------------*/
void RCC_Configuration(void);
void SetPLL(void);

void Delay(__IO uint32_t nCount);
void USART_OUT(USART_TypeDef* USARTx, uint8_t *Data,uint16_t Len);
void NVIC_Configuration(void);

extern bool I2C_ReadByte(u8* pBuffer,   u8 length,   u8 DeviceAddress);

TestStatus Buffercmp(uint8_t* pBuffer1, uint8_t* pBuffer2, uint16_t BufferLength);

unsigned char ch2=0;

static void NVIC_Configuration(void)
{
#if 0
  NVIC_InitTypeDef NVIC_InitStructure;

  /* Configure the NVIC Preemption Priority Bits */  
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
  
  /* Enable the USART1 Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
#endif 
}
/**
  * @brief  Configures the different system clocks.
  * @param  None
  * @retval : None
  */
static void RCC_Configuration(void)
{
  /* Setup the microcontroller system. Initialize the Embedded Flash Interface,  
     initialize the PLL and update the SystemFrequency variable. */
  //SystemInit();

/* Enable peripheral clocks --------------------------------------------------*/
  /* GPIOB Periph clock enable */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
  /* I2C1 Periph clock enable */
  //RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
}

/**
  * @brief  Configure the used I/O ports pin
  * @param  None
  * @retval : None
  */
static void GPIO_Configuration(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure; 
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                         RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD |
                         RCC_APB2Periph_GPIOE, ENABLE);
  /* Configure I2C1 pins: SCL and SDA */
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10 | GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  //GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  //GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;                                  
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  
  	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;				     //LED1
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);					 

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_3;		 //LED2, LED3
  GPIO_Init(GPIOD, &GPIO_InitStructure);

#if 0
  //GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;		 //LCD 背光控制
  //GPIO_Init(GPIOD, &GPIO_InitStructure);
  //GPIO_SetBits(GPIOD, GPIO_Pin_13);	   	 	

  //GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;		 //LCD 背光控制
  //GPIO_Init(GPIOD, &GPIO_InitStructure);
  //GPIO_SetBits(GPIOD, GPIO_Pin_13);	   	 	


  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;	         //USART1 TX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;    //复用推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);		    //A端口 

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;	         //USART1 RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;   //复用开漏输入
  GPIO_Init(GPIOA, &GPIO_InitStructure);		         //A端口 
#endif

}


/**
  * @brief  Main program
  * @param  None
  * @retval : None
  */
int tea5657_init(void)
{
/*
  float a=0;
  unsigned long fm_ch[50];
  unsigned char *b,ch1=0;
*/  
  /* System clocks configuration ---------------------------------------------*/
  RCC_Configuration();
  NVIC_Configuration();
  /* Initialize the I2C EEPROM driver ----------------------------------------*/		
  
  GPIO_Configuration();  
  #if 0
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

  /* Configure USART1 */
  USART_Init(USART1, &USART_InitStructure);
 
  
  /* Enable USART1 Receive and Transmit interrupts */
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  //USART_ITConfig(USART1, USART_IT_TXE, ENABLE);

  /* Enable the USART1 */
  USART_Cmd(USART1, ENABLE);  
  #endif
  Delay(0xafffff);

#if 0  
  //USART_OUT(USART1,&TxBuffer1[0],TxBufferSize1+2);
  printf("\r\n 奋斗版STM32开发板TEA5767的演示DEMO \n");
  printf("\r\n");
  printf("\r\n");
  printf("\r\n H(h)---帮助                    S(s)---搜索节目  \n");
  printf("\r\n D(d)---显示有效节目            xxP(xxp)---播放选定的节目(如12P) \n");
  printf("\r\n xx.xM(xx.xm)---直接选定频率(如98.8M)      \n");
  printf("\r\n");
  printf("\r\n");
  I2C_Write(Tx2_Buffer,ADDRESS_AMP,6); 	  
  
  I2C_Write(Tx1_Buffer, Tea5767_WriteAddress1, 5); 
  
  SetPLL();
  
			//USART_OUT(USART1,"Search......",15+2);
			printf("\n 搜索FM节目! \n");
			Tx1_Buffer[0] = 0XF0; 
			//I2C_Write(Tx1_Buffer, Tea5767_WriteAddress1, 5); 
			//Delay(0xffffff);
			FM_FREQ=87500000;
			FM_FREQ=FM_FREQ+100000;				
		  	//SetPLL();
		  	tea5756_setpll(FM_FREQ, 2);
			//Delay(0x6fffff);
			I2C_ReadByte(Rx1_Buffer,5,Tea5767_ReadAddress1);
			ch2=0;
		 	while(1){
		    //IF<51  IF>55  LEVEL<9
				fm_pub:;
				FM_FREQ=FM_FREQ+100000;			
		  		if(FM_FREQ>108000000){FM_FREQ=98800000; break;}  
				SetPLL(); 
				Delay(0x0fffff);
				I2C_ReadByte(Rx1_Buffer,5,Tea5767_ReadAddress1);				
				a=FM_FREQ;
				a=a/1000000;		
				//if((Rx1_Buffer[0]&0x3f)!=(Tx1_Buffer[0]&0x3f)||(Rx1_Buffer[1]!=Tx1_Buffer[1])||Rx1_Buffer[2]<51||Rx1_Buffer[2]>=55||(Rx1_Buffer[3]>>4)<9){	
				if((Rx1_Buffer[0]&0x3f)!=(Tx1_Buffer[0]&0x3f)||(Rx1_Buffer[1]!=Tx1_Buffer[1])||(Rx1_Buffer[1]&0x80!=0x80)||Rx1_Buffer[2]<50||Rx1_Buffer[2]>=56||(Rx1_Buffer[3]>>4)<7||(Rx1_Buffer[3]>>4)>14){
				    //printf("\n 当前FM频率是:   %g MHz     有信号!!!  %u  %u\n",a,Rx1_Buffer[2],Rx1_Buffer[3]>>4);
					printf("\n 当前FM频率是:   %g MHz \n",a); 
					//printf("\n 当前FM频率是:   %g MHz     无信号!!!    %u  %u\n",a, Rx1_Buffer[2],Rx1_Buffer[3]>>4);				
				}
				else {					
					//printf("\n 当前FM频率是:   %g MHz     有信号!!!  %u    %u  %u\n",a,Rx1_Buffer[1], Rx1_Buffer[2],Rx1_Buffer[3]>>4);
					printf("\n 当前FM频率是:   %g MHz     有信号!!!    %u  %u\n",a, Rx1_Buffer[2],Rx1_Buffer[3]>>4);
					fm_ch[ch2]= FM_FREQ; 					
					ch2++;
					goto fm_pub;
				}
				
				
			}
			if(FM_FREQ!=98800000) goto fm_pub;
			printf("\r\n 有效的FM频率总共有:   %u 个 \n",ch2); 
			ch1=ch2;
			while(ch1--){
				a=fm_ch[ch1];
				a=a/1000000;	
				printf("\r\n %u  FM频率: %g  MHz \n",ch1,a); 
			}				
#endif			
}

void tea5657_thread_entry(void* parameter)
{
    float a=0;
    unsigned long fm_ch[50];
    unsigned char *b,ch1=0;
/*
    printf("\r\n 奋斗版STM32开发板TEA5767的演示DEMO \n");
      printf("\r\n");
      printf("\r\n");
      printf("\r\n H(h)---帮助                    S(s)---搜索节目  \n");
      printf("\r\n D(d)---显示有效节目            xxP(xxp)---播放选定的节目(如12P) \n");
      printf("\r\n xx.xM(xx.xm)---直接选定频率(如98.8M)      \n");
      printf("\r\n");
      printf("\r\n");
*/      
      I2C_Write(Tx2_Buffer,ADDRESS_AMP, 3);    
      
      I2C_Write(Tx1_Buffer, Tea5767_WriteAddress1, 5); 
      
      SetPLL();
      
                //USART_OUT(USART1,"Search......",15+2);
                //printf("\n 搜索FM节目! \n");
                Tx1_Buffer[0] = 0XF0; 
                //I2C_Write(Tx1_Buffer, Tea5767_WriteAddress1, 5); 
                //Delay(0xffffff);
                FM_FREQ=87500000;
                FM_FREQ=FM_FREQ+100000;             
                SetPLL();
                //tea5756_setpll(FM_FREQ, 2);
                //Delay(0x6fffff);
                I2C_ReadByte(Rx1_Buffer,5,Tea5767_ReadAddress1);
                ch2=0;
                while(1){
                //IF<51  IF>55  LEVEL<9
                    fm_pub:;
                    FM_FREQ=FM_FREQ+100000;         
                    if(FM_FREQ>108000000){FM_FREQ=98800000; break;}  
                    SetPLL(); 
                    Delay(0x0fffff);
                    I2C_ReadByte(Rx1_Buffer,5,Tea5767_ReadAddress1);                
                    a=FM_FREQ;
                    a=a/1000000;        
                    //if((Rx1_Buffer[0]&0x3f)!=(Tx1_Buffer[0]&0x3f)||(Rx1_Buffer[1]!=Tx1_Buffer[1])||Rx1_Buffer[2]<51||Rx1_Buffer[2]>=55||(Rx1_Buffer[3]>>4)<9){    
                    if((Rx1_Buffer[0]&0x3f)!=(Tx1_Buffer[0]&0x3f)||(Rx1_Buffer[1]!=Tx1_Buffer[1])||(Rx1_Buffer[1]&0x80!=0x80)||Rx1_Buffer[2]<50||Rx1_Buffer[2]>=56||(Rx1_Buffer[3]>>4)<7||(Rx1_Buffer[3]>>4)>14){
                        //printf("\n 当前FM频率是:   %g MHz     有信号!!!  %u  %u\n",a,Rx1_Buffer[2],Rx1_Buffer[3]>>4);
                        //rt_kprintf("\r\n 当前FM频率是:   %u MHz \r\n", FM_FREQ); 
                        //printf("\n 当前FM频率是:   %g MHz     无信号!!!    %u  %u\n",a, Rx1_Buffer[2],Rx1_Buffer[3]>>4);              
                    }
                    else {                  
                        //printf("\n 当前FM频率是:   %g MHz     有信号!!!  %u    %u  %u\n",a,Rx1_Buffer[1], Rx1_Buffer[2],Rx1_Buffer[3]>>4);
                        rt_kprintf("\r\n 当前FM频率是:   %u MHz     有信号!!!    %u  %u\r\n", FM_FREQ, Rx1_Buffer[2],Rx1_Buffer[3]>>4);
                        fm_ch[ch2]= FM_FREQ;                    
                        ch2++;
                        goto fm_pub;
                    }
                    
                    
                }
                if(FM_FREQ!=98800000) goto fm_pub;
                rt_kprintf("\r\n 有效的FM频率总共有:   %u 个 \n",ch2); 
                ch1=ch2;
                while(ch1--){
                    a=fm_ch[ch1];
                    a=a/1000000;    
                    rt_kprintf("\r\n %u  FM频率: %u  MHz \r\n",ch1, FM_FREQ); 
                }              


FM_FREQ=98800000;
SetPLL();
//USART_OUT(USART1,&TxBuffer1[0],len+2);
rt_kprintf("\r 当前FM频率是:   %u\n MHz \n", FM_FREQ); 
                
}

void tea5657_thread_init()
{
    rt_thread_t tid;

    tid = rt_thread_create("tea5657", tea5657_thread_entry, RT_NULL,
        2048, 25, 10);

    if (tid != RT_NULL) rt_thread_startup(tid);
}



///////
void USART_OUT(USART_TypeDef* USARTx, uint8_t *Data,uint16_t Len){ 
	uint16_t i;
	for(i=0; i<Len-2; i++){
		USART_SendData(USARTx, Data[i]);
		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC)==RESET);
	}
	USART_SendData(USARTx, 0x0d);
	while(USART_GetFlagStatus(USARTx, USART_FLAG_TC)==RESET);
	USART_SendData(USARTx, 0x0a);
	while(USART_GetFlagStatus(USARTx, USART_FLAG_TC)==RESET);

}

static void Delay(__IO uint32_t nCount)
{
  for(; nCount != 0; nCount--);
}

void SetPLL(void)
{
   //FM_PLL=(unsigned long)((4000*(FM_FREQ/1000+225))/32768); 	//计算ＰＬＬ值
   FM_PLL=(unsigned long)((4000*(FM_FREQ/1000+225))/32768); 	//计算ＰＬＬ值
   if(rec_f==2) PLL_HIGH=(unsigned char)(((FM_PLL >> 8)&0X3f)|0xc0);	 //PLL高字节值
   else PLL_HIGH=(unsigned char)((FM_PLL >> 8)&0X3f);	 //PLL高字节值
   //Tx1_Buffer[0]=(Tx1_Buffer[0]&0XC0)|PLL_HIGH;		 //I2C第一字节值
   Tx1_Buffer[0]=PLL_HIGH;		 //I2C第一字节值
   PLL_LOW=(unsigned char)FM_PLL;			      		 //PLL低字节值
   Tx1_Buffer[1]= PLL_LOW;						 //I2C第二字节值
   I2C_Write(Tx1_Buffer, Tea5767_WriteAddress1, 5); 
}

#if 0
int SendChar (int ch)  {                /* Write character to Serial Port     */

  USART_SendData(USART1, (unsigned char) ch);
  while (!(USART1->SR & USART_FLAG_TXE));
  return (ch);
}




/*******************************************************************************
* Function Name  : fputc
* Description    : Retargets the C library printf function to the USART.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
int fputc(int ch, FILE *f)
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART */
  USART_SendData(USART1, (u8) ch);

  /* Loop until the end of transmission */
  while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
  {
  }

  return ch;
}
#endif

/**
  * @brief  Compares two buffers.
  * @param pBuffer1, pBuffer2: buffers to be compared.
  * @param BufferLength: buffer's length
  * @retval : PASSED: pBuffer1 identical to pBuffer2
  *   FAILED: pBuffer1 differs from pBuffer2
  */
TestStatus Buffercmp(uint8_t* pBuffer1, uint8_t* pBuffer2, uint16_t BufferLength)
{
  while(BufferLength--)
  {
    if(*pBuffer1 != *pBuffer2)
    {
      return FAILED;
    }
    
    pBuffer1++;
    pBuffer2++;
  }

  return PASSED;  
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif


//#include "Tea5756.c"

/**
  * @}
  */ 

/**
  * @}
  */ 

static unsigned char pt2314_vol_db[]={0x38|0x07,0x38|0x06,0x38|0x05,0x38|0x04,0x38|0x03,0x38|0x02,0x38|0x01,0x38|0x00,
                              0x30|0x07,0x30|0x06,0x30|0x05,0x30|0x04,0x30|0x03,0x30|0x02,0x30|0x01,0x30|0x00,
                             0x28|0x07,0x28|0x06,0x28|0x05,0x28|0x04,0x28|0x03,0x28|0x02,0x28|0x01,0x28|0x00,
                              0x20|0x07,0x20|0x06,0x20|0x05,0x20|0x04,0x20|0x03,0x20|0x02,0x20|0x01,0x20|0x00,
                             0x18|0x07,0x18|0x06,0x18|0x05,0x18|0x04,0x18|0x03,0x18|0x02,0x18|0x01,0x18|0x00,
                              0x10|0x07,0x10|0x06,0x10|0x05,0x10|0x04,0x10|0x03,0x10|0x02,0x10|0x01,0x10|0x00,
                             0x08|0x07,0x08|0x06,0x08|0x05,0x08|0x04,0x08|0x03,0x08|0x02,0x08|0x01,0x08|0x00,
                              0x00|0x07,0x00|0x06,0x00|0x05,0x00|0x04,0x00|0x03,0x00|0x02,0x00|0x01,0x00|0x00,
                             };
/*
void pt2314_volume_ctrl()
{
    if(VolCutm==0)
              {
               tab_display[4]=MR_6625[2]|MR_6625[5]|MR_6625[Choose];
               vol_iic[4]=0xdf;//静音
               vol_iic[5]=0xff;
              } 
          else
               {
               tab_display[4]=MR_6625[1]|MR_6625[5]|MR_6625[Choose];
               vol_iic[4]=0xc0;//开静音
               vol_iic[5]=0xe0;
               }
        
         vol_iic[0]=vol_db[Choose_vbt[0]]|vol;
         vol_iic[1]=Tiaoyin[Choose_vbt[1]]|bass;
         vol_iic[2]=Tiaoyin[Choose_vbt[2]]|treble;
    
         if(Choose==0)
             {vol_iic[3]=0x5c;}
         if(Choose==3)
             {vol_iic[3]=0x5d;}
         if(Choose==4)
             {vol_iic[3]=0x5e;}
         if(Choose==6)
             {vol_iic[3]=0x5f;}

}
*/
/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
