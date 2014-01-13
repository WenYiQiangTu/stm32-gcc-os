#include <rtthread.h>

/* Includes ------------------------------------------------------------------*/

#include "stm32f10x_i2c.h"
#include "stm32f10x.h"


#define TEA5756_DEBUG		1
#if (TEA5756_DEBUG == 1)
#define TEA5756_TRACE	rt_kprintf
#else
#define TEA5756_TRACE(...)
#endif

/*  98.8MHz */
#define DEFAULT_FM      98800000

/** @addtogroup StdPeriph_Examples
  * @{
  */

/** @addtogroup I2C_M24C08_EEPROM
  * @{
  */ 

#define MHz  (1000000)

#define Tea5767_WriteAddress1    0xc0
#define Tea5767_ReadAddress1     0xc1

//PT2314 ID
#define ADDRESS_AMP              0X88 

/******************************
 * Write mode register values *
 ******************************/

/* First register */
#define TEA5767_MUTE            0x80    /* Mutes output */
#define TEA5767_SEARCH          0x40    /* Activates station search */
/* Bits 0-5 for divider MSB */

/* Second register */
/* Bits 0-7 for divider LSB */

/* Third register */

/* Station search from botton to up */
#define TEA5767_SEARCH_UP       0x80

/* Searches with ADC output = 10 */
#define TEA5767_SRCH_HIGH_LVL   0x60

/* Searches with ADC output = 10 */
#define TEA5767_SRCH_MID_LVL    0x40

/* Searches with ADC output = 5 */
#define TEA5767_SRCH_LOW_LVL    0x20

/* if on, div=4*(Frf+Fif)/Fref otherwise, div=4*(Frf-Fif)/Freq) */
#define TEA5767_HIGH_LO_INJECT  0x10

/* Disable stereo */
#define TEA5767_MONO            0x08

/* Disable right channel and turns to mono */
#define TEA5767_MUTE_RIGHT      0x04

/* Disable left channel and turns to mono */
#define TEA5767_MUTE_LEFT       0x02

#define TEA5767_PORT1_HIGH      0x01

/* Fourth register */
#define TEA5767_PORT2_HIGH      0x80
/* Chips stops working. Only I2C bus remains on */
#define TEA5767_STDBY           0x40

/* Japan freq (76-108 MHz. If disabled, 87.5-108 MHz */
#define TEA5767_JAPAN_BAND      0x20

/* Unselected means 32.768 KHz freq as reference. Otherwise Xtal at 13 MHz */
#define TEA5767_XTAL_32768      0x10

/* Cuts weak signals */
#define TEA5767_SOFT_MUTE       0x08

/* Activates high cut control */
#define TEA5767_HIGH_CUT_CTRL   0x04

/* Activates stereo noise control */
#define TEA5767_ST_NOISE_CTL    0x02

/* If activate PORT 1 indicates SEARCH or else it is used as PORT1 */
#define TEA5767_SRCH_IND        0x01

/* Fifth register */

/* By activating, it will use Xtal at 13 MHz as reference for divider */
#define TEA5767_PLLREF_ENABLE   0x80

/* By activating, deemphasis=50, or else, deemphasis of 50us */
#define TEA5767_DEEMPH_75       0X40

/*****************************
 * Read mode register values *
 *****************************/

/* First register */
#define TEA5767_READY_FLAG_MASK 0x80
#define TEA5767_BAND_LIMIT_MASK 0X40
/* Bits 0-5 for divider MSB after search or preset */

/* Second register */
/* Bits 0-7 for divider LSB after search or preset */

/* Third register */
#define TEA5767_STEREO_MASK     0x80
#define TEA5767_IF_CNTR_MASK    0x7f

/* Fourth register */
#define TEA5767_ADC_LEVEL_MASK  0xf0

/* should be 0 */
#define TEA5767_CHIP_ID_MASK    0x0f

/* Fifth register */
/* Reserved for future extensions */
#define TEA5767_RESERVED_MASK   0xff


enum tea5756_mode 
{
     TEA5756_MODE_START = 0, TEA5756_MODE_SEARCH = 1, TEA5756_MODE_PRESET, TEA5756_MODE_MAX
};

#define TEA5756_FM_CHANNEL_MAX  50

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define I2C_Speed              400000
#define I2C1_SLAVE_ADDRESS7    0xA0
#define I2C_PageSize           16

#define SCL_H         GPIOB->BSRR = GPIO_Pin_10
#define SCL_L         GPIOB->BRR  = GPIO_Pin_10 
   
#define SDA_H         GPIOB->BSRR = GPIO_Pin_11
#define SDA_L         GPIOB->BRR  = GPIO_Pin_11

#define SCL_read      GPIOB->IDR  & GPIO_Pin_10
#define SDA_read      GPIOB->IDR  & GPIO_Pin_11


static rt_bool_t I2C_ReadByte(u8* pBuffer,   u8 length,   u8 DeviceAddress);
static rt_bool_t I2C_Start(void);
static void I2C_delay(void);
static void I2C_Stop(void);

static void I2C_Ack(void);

static void I2C_NoAck(void);

static rt_bool_t I2C_WaitAck(void);
static void I2C_SendByte(u8 SendByte);

/* Private function prototypes -----------------------------------------------*/
static void GPIO_Configuration(void);
static void I2C_Configuration(void);

static void delay_ms(rt_uint32_t ms)
{
    rt_uint32_t len;
    for (; ms > 0; ms --)
        for (len = 0; len < 100; len++ );
}


/* Private functions ---------------------------------------------------------*/


/**
  * @brief  Initializes peripherals used by the I2C EEPROM driver.
  * @param  None
  * @retval : None
  */
static rt_bool_t I2C_Write(rt_uint8_t* pBuffer, rt_uint8_t WriteAddr, rt_uint8_t NumByteToWrite)
{
    if(!I2C_Start())return RT_FALSE;
    I2C_SendByte(WriteAddr);
    if(!I2C_WaitAck()){I2C_Stop(); return RT_FALSE;}
    while(NumByteToWrite--)
    {
        I2C_SendByte(* pBuffer);
        I2C_WaitAck();
        pBuffer++;
    }
    I2C_Stop(); 	
    return RT_TRUE;
}

/**
  * @brief  Reads a block of data from the EEPROM.
  * @param pBuffer : pointer to the buffer that receives the data read 
  *   from the EEPROM.
  * @param ReadAddr : EEPROM's internal address to read from.
  * @param NumByteToRead : number of bytes to read from the EEPROM.
  * @retval : None
  */
static void I2C_delay(void)
{	
   u8 i=100; //这里可以优化速度	，经测试最低到5还能写入
   while(i) 
   { 
     i--; 
   } 
}

static rt_bool_t I2C_Start(void)
{
    SDA_H;
    SCL_H;
    I2C_delay();
    if(!SDA_read)return RT_FALSE;	//SDA线为低电平则总线忙,退出
    SDA_L;
    I2C_delay();
    if(SDA_read) return RT_FALSE;	//SDA线为高电平则总线出错,退出
    SDA_L;
    I2C_delay();
    return RT_TRUE;
}

static void I2C_Stop(void)
{
    SCL_L;
    I2C_delay();
    SDA_L;
    I2C_delay();
    SCL_H;
    I2C_delay();
    SDA_H;
    I2C_delay();
}

static void I2C_Ack(void)
{	
    SCL_L;
    I2C_delay();
    SDA_L;
    I2C_delay();
    SCL_H;
    I2C_delay();
    SCL_L;
    I2C_delay();
}

static void I2C_NoAck(void)
{	
    SCL_L;
    I2C_delay();
    SDA_H;
    I2C_delay();
    SCL_H;
    I2C_delay();
    SCL_L;
    I2C_delay();
}

static rt_bool_t I2C_WaitAck(void) 	 //返回为:=1有ACK,=0无ACK
{
    SCL_L;
    I2C_delay();
    SDA_H;			
    I2C_delay();
    SCL_H;
    I2C_delay();
    if(SDA_read)
    {
        SCL_L;
        return RT_FALSE;
    }
    SCL_L;
    return RT_TRUE;
}

static void I2C_SendByte(u8 SendByte) //数据从高位到低位//
{
    u8 i=8;
    while(i--)
    {
        SCL_L;
        I2C_delay();
        if(SendByte&0x80)
            SDA_H;  
        else 
            SDA_L;   
        SendByte<<=1;
        I2C_delay();
        SCL_H;
        I2C_delay();
    }
    SCL_L;
}

static u8 I2C_ReceiveByte(void)  //数据从高位到低位//
{ 
    u8 i=8;
    u8 ReceiveByte=0;

    SDA_H;				
    while(i--)
    {
      ReceiveByte<<=1;      
      SCL_L;
      I2C_delay();
	  SCL_H;
      I2C_delay();	
      if(SDA_read)
      {
        ReceiveByte|=0x01;
      }
    }
    SCL_L;
    return ReceiveByte;
}


//读出1串数据         存放读出数据  待读出长度   器件类型(24c16或SD2403)	
static rt_bool_t I2C_ReadByte(u8* pBuffer,   u8 length,   u8 DeviceAddress)
{		
    if(!I2C_Start())return RT_FALSE;
    I2C_SendByte(DeviceAddress);//器件地址 
    if(!I2C_WaitAck()){I2C_Stop(); return RT_FALSE;}

    while(length--)
    {
        *pBuffer = I2C_ReceiveByte();
        if(length == 1)I2C_NoAck();
        else I2C_Ack(); 
        pBuffer++;

    }
    I2C_Stop(); 	
    return RT_TRUE;
}


rt_bool_t tea5756_read_status(rt_uint8_t *buffer) 
{
    if (buffer == RT_NULL) return RT_FALSE;

    rt_memset(buffer, 0, 5);
    if (5 != I2C_ReadByte(buffer, 5, Tea5767_ReadAddress1))
    {
        return RT_FALSE;
    }

    return RT_TRUE;
}

int tea5756_g_rf_signal(rt_uint16_t *strength)
{
   rt_uint8_t buffer[5];

   *strength = 0;
   if (0 == tea5756_read_status(&buffer[0]))
       *strength = (buffer[3] & TEA5767_ADC_LEVEL_MASK) << 8;

   return 0;
}

rt_bool_t tea5756_stereo(const char *buffer)
{
    int stereo = buffer[2] & TEA5767_STEREO_MASK;

    TEA5756_TRACE("Radio ST Get=%02x\n", stereo);

    return stereo ? RT_TRUE : RT_FALSE;
}

void tea5767_status_dump(rt_uint8_t *buffer)
{
    uint32_t pll_div, freq;

    if (TEA5767_READY_FLAG_MASK & buffer[0])
    {
        TEA5756_TRACE("Ready Flag on\n");
    }
    else
    {
        TEA5756_TRACE("Ready Flag off\n");
    }

    pll_div = (buffer[1] & 0x3f) << 8 | buffer[1];

    /* TEA5767_HIGH_LO32768 */
    freq = (pll_div * 32768 - 700000 - 225000) / 4;
    buffer[0] = (pll_div >> 8) & 0x3f;
    buffer[1] = pll_div & 0xff;

//    TEA5756_TRACE("Frequency %d.%03 KHz (divider = 0x%04x)\n", freq / 1000, freq % 1000, pll_div);
//
    //(TEA5767_STEREO_MASK & buffer[2] ) ? TEA5756_TRACE("Stereo\n") : TEA5756_TRACE("Mono\n");

    TEA5756_TRACE("IF Counter=%d\n", buffer[2] & TEA5767_IF_CNTR_MASK);
    TEA5756_TRACE("ADC Level=%d\n", (buffer[3] & TEA5767_ADC_LEVEL_MASK) >> 4);
    TEA5756_TRACE("Chip ID=%d\n", (buffer[3] & TEA5767_CHIP_ID_MASK));
    TEA5756_TRACE("Reserved=0x%02x\n", (buffer[4] & TEA5767_RESERVED_MASK));
}

int tea5767_standby()
{
    rt_uint8_t buffer[5];
    unsigned div;

    /* set frequency to 87.5 Mhz*/
    div = (87500 * 4 + 700 + 225 + 25) / 50;
    buffer[0] = (div >> 8) & 0x3f;
    buffer[1] = div & 0xff;
    // ? PORT1 
    buffer[2] = TEA5767_PORT1_HIGH;
    buffer[3] = TEA5767_PORT2_HIGH | TEA5767_HIGH_CUT_CTRL |
        TEA5767_ST_NOISE_CTL | TEA5767_JAPAN_BAND | TEA5767_STDBY;
    buffer[4] = 0;

    I2C_Write(buffer, I2C1_SLAVE_ADDRESS7, 5);
    return 0;
}


/**
  * @brief  Configure the used I/O ports pin
  * @param  None
  * @retval : None
  */
static void GPIO_Configuration(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure; 
  RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE);

  /* Configure I2C1 pins: SCL and SDA */
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_10 | GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
  * @brief  I2C Configuration
  * @param  None
  * @retval : None
  */
static void I2C_Configuration(void)
{
  I2C_InitTypeDef  I2C_InitStructure; 

  /* I2C configuration */
  I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
  I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
  I2C_InitStructure.I2C_OwnAddress1 = I2C1_SLAVE_ADDRESS7;
  I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
  I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
  I2C_InitStructure.I2C_ClockSpeed = I2C_Speed;
  
  /* I2C Peripheral Enable */
  I2C_Cmd(I2C1, ENABLE);
  /* Apply I2C configuration after enabling it */
  I2C_Init(I2C1, &I2C_InitStructure);
}

static void RCC_Configuration(void)
{
  /* Setup the microcontroller system. Initialize the Embedded Flash Interface,  
     initialize the PLL and update the SystemFrequency variable. */
  
  /* GPIOB Periph clock enable */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
}
                                                            
/* -----+---------------------------------------------------+--- *\
 * MSB  |    |    |    |    |     |    |    | Function      |
 * -----+---------------------------------------------------+-----+
 * 0    |  0 | B2 | B1 | B0 | A2  | A1 | A0 | Volume Control|
 * ---------------------------------------------------------|
 * 1    |  1 | 0  | B1 | B0 | A2  | A1 | A0 | Speaker ATT L |
 *----------------------------------------------------------+
 * 1    |  1 | 1  | B1 | B0 | A2  | A1 | A0 | Speaker ATT R |
 * ---------------------------------------------------------+
 * 0    |  1 | 0  | G1 | G0 | S2  | S1 | S0 | Audio Switch  |
 * ---------------------------------------------------------+
 * 0    |  1 | 1  | 0  | C3 | C2  | C1 | C0 | Bass  Control |
 * ---------------------------------------------------------+
 * 0    |  1 | 1  | 1  | C3 | C2  | C1 | C0 | Treble Control|
 * -----+---------------------------------------------------+----*/

// volume 10dB steps
void amplifier_set(void)
{
    /*  */
    rt_uint8_t buff[] = {0X00,0Xc0,0xe0,0X41,0X6e,0X7e};
    I2C_Write(&buff[0], ADDRESS_AMP, 6);
    return;
}

// vol 0 ~ 7, 7 is low volume. 
void amplifier_tunner(rt_uint8_t vol)
{
    rt_uint8_t buff[] = {0X00,0X00,0xe0,0X41,0X6e,0X7e};
   
    buff[0] |= (vol << 2);
    I2C_Write(&buff[0], ADDRESS_AMP, 6);
}

/**
 * @brief Calculation of the 14-bit word  
 *                                    4 x (fRF + fIF )
 *  -- high side injection : N = ------------------------- 
 *                                      fRef
 *  
 *                                    4 x (fRF- fIF)
 *  -- low side injection : N = -------------------------- 
 *                                     fRef
 *
 * @param fRf Wanted tuning frequency [Hz];
 * @param opermode : Search up or Present.
 */
void tea5756_set_radiofreq(unsigned long fRf , enum tea5756_mode opermode)
{
    /*
     * fRef = 32768 == 32.768KHz for 32.768kHz crystal
     */
    rt_uint32_t  fRef = 32768, fIf =  225;     
    rt_uint8_t buffer[5] = {0x00};
//    rt_uint8_t   buffer[5] = {0XF0,0X2C,0XD0, 0x12,0X40};
    rt_uint32_t  pll;

    pll =(rt_uint32_t)((4000 *(fRf/1000 + fIf))/fRef);

    buffer[0] = (rt_uint8_t) ((pll >> 8) & 0x3f);
    if(opermode ==  TEA5756_MODE_SEARCH)
    {
        buffer[0] |= (TEA5767_MUTE | TEA5767_SEARCH);
        
        // Search up, search stop level ADC output = 7
        buffer[2] |= TEA5767_SEARCH_UP | TEA5767_SRCH_MID_LVL;
    } 
    buffer[1] = pll & 0xff;

    buffer[2] |= TEA5767_HIGH_LO_INJECT;

    // Activates stereo noise control, clock Frequency = 32.768 kHz, europe band  87.5 ~ 108 Mhz.
    buffer[3] = TEA5767_XTAL_32768 | TEA5767_ST_NOISE_CTL;

    // de-emphasis time const is 75 us
    buffer[4] = TEA5767_DEEMPH_75;

    I2C_Write(&buffer[0], Tea5767_WriteAddress1, 5); 
}

void tea5756_auto_search()
{
    int         ch1, ch2;
    const rt_uint32_t delta = 100000; 
    rt_uint8_t  rxBuf[] = {0XF0,0X2C,0XD0,0X12,0X40};
    rt_uint32_t freq  = 87500000, fmCh[TEA5756_FM_CHANNEL_MAX]; 

    rt_kprintf("\r\n start radio search \r\n");

    freq = 87500000 + delta;
    tea5756_set_radiofreq(freq, TEA5756_MODE_START);
    delay_ms(50);
    I2C_ReadByte(rxBuf, 5, Tea5767_ReadAddress1);

    ch2 = 0;
    while(1)
    {
        // add 100kHz to the tuning memory
        freq = freq + delta;
        if (freq > 108000000) {freq = DEFAULT_FM; break;}
        tea5756_set_radiofreq(freq, TEA5756_MODE_SEARCH);
        
        // wait 10ms, have the singal available?
        delay_ms(20);   
        if (!I2C_ReadByte(rxBuf, 5, Tea5767_ReadAddress1))
            break;
        
        // the band limit has been reached 
        if (rxBuf[0] & TEA5767_BAND_LIMIT_MASK)
            break;
        
        // signal freq found
        if ((rxBuf[0] & TEA5767_READY_FLAG_MASK) 
            // if counter result
            && (rxBuf[2] > 50 && rxBuf[2] <= 56)
            // level ADC output
            && ((rxBuf[3] >> 4 ) > 7) && ((rxBuf[3] >> 4 < 14)))
        {
            // tea5767_status_dump(&rxBuf[0]);
            rt_kprintf("current radio freq:%u(Hz), ADC level=%d\r\n", freq, (rt_uint8_t) (rxBuf[3] >> 4)); 
            fmCh[ch2++]= freq;                    
            if (ch2 > TEA5756_FM_CHANNEL_MAX) break;
        }
        else 
        {
            rt_kprintf("no singal \r\n");
        }
        rt_memset(rxBuf, 0x00, 5); 
    }

    rt_kprintf("\r\n total effective radio channel #%u\r\n", ch2); 
/*     ch1 = ch2;
 *     while(ch1--)
 *     {
 *         rt_kprintf("\r\n %u  FM频率: %u  MHz \n",ch1, fmCh[ch1]); 
 *     }
 */
}

rt_bool_t tea5756_autodetection(void)
{
    rt_uint8_t rxBuf[7] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; 

    I2C_ReadByte(rxBuf, 7, Tea5767_ReadAddress1);

    if (rxBuf[0] == rxBuf[1] && rxBuf[0] == rxBuf[2]
            && rxBuf[0] == rxBuf[3] && rxBuf[0] == rxBuf[4])
    {
        rt_kprintf("all bytes are equal, not tea5767 chip found.\r\n");
        return RT_FALSE;
    }

    // Read Byte 4 : CI[3:1] = 0, Byte 5 : 0
    if ((rxBuf[3] & 0x0f) != 0x00 && (rxBuf[4] != 0x00)) 
    {
        rt_kprintf("chip id CI[3:1] not all zeros, not tea5767 chip\r\n");
        return RT_FALSE;
    }

    rt_kprintf("found tea5767 chip\r\n");
    return RT_TRUE;
}

/**
 * @brief Initializes tea5756 hardware configuration
 */
void tea5756_hardware_init()
{
    RCC_Configuration();
    GPIO_Configuration();
 
/*     amplifier_set();
 */
    amplifier_tunner(7);

    tea5756_set_radiofreq(DEFAULT_FM, TEA5756_MODE_START);

/*     tea5756_auto_search();
 */
}

#ifdef RT_USING_FINSH
#include "finsh.h"

void radio_search()
{
    tea5756_auto_search();
}
FINSH_FUNCTION_EXPORT(radio_search, radio auto search);
#endif
