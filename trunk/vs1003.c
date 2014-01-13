
/* #include "mp3.h"
 */
//#include "stm32f10x.h"	   
//#include "sys.h"
//#include "delay.h"	    

#include "rtthread.h"
#include "board.h"
#include "vs1003.h"   
#include "stm32f10x_spi.h"

#define SM_DIFF         	0x01   
#define SM_JUMP         	0x02   
#define SM_RESET        	0x04   
#define SM_OUTOFWAV     	0x08   
#define SM_PDOWN        	0x10   
#define SM_TESTS        	0x20   
#define SM_STREAM       	0x40   
#define SM_PLUSV        	0x80   
#define SM_DACT         	0x100   
#define SM_SDIORD       	0x200   
#define SM_SDISHARE     	0x400   
#define SM_SDINEW       	0x800   
#define SM_ADPCM        	0x1000   
#define SM_ADPCM_HP     	0x2000 

//#define VsPlayerInterrupts(_x_)   (_x_)

#define VS_DREQ_TST()  GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_7)

/* #define VS10XX_DREQ_ENA() \
 *     cbi(VS10XX_DREQ_OE_REG, VS10XX_DREQ_BIT); \
 *     sbi(VS10XX_DREQ_PUE_REG, VS10XX_DREQ_BIT)
 * 
 */
/*
 * Bytes needed to flush internal VS buffer (size of VS interbal buffer)
 */
#define VS_FLUSH_BYTES 2048


#ifndef VS10XX_FREQ
/*! \brief Decoder crystal frequency. */
#define VS10XX_FREQ             12288000UL
#endif

/*@{*/
/*! \brief Register index. */
#define VS_CLOCKF_REG       3
/*! \brief Clock frequency mask.
 *
 * Should be set to crystal clock divided by 2000, if the clock
 * is not 24.576 MHz.
 *
 * Used for the VS1001, VS1011 and VS1002.
 */
#define VS_SC_FREQ          0x7FFF
/*! \brief VS1003/VS1033 clock frequency mask.
 *
 * Should be set to crystal clock divided by 4000, if the clock
 * is not 12.288 MHz.
 *
 * Used for the VS1003 and VS1033.
 */
#define VS_SC_X3FREQ        0x07FF

/*! \brief Clock frequency LSB. */
#define VS_SC_FREQ_LSB      0
/*! \brief Allowed multiplier addition. 
 *
 * Used for WMA decoding with the VS1003 and VS1033.
 */
// 1.5x 
#define VS_SC_ADD           0x1800
/*! \brief Clock multiplier. 
 *
 * Used for the VS1003 and VS1033.
 */
// XTALI * 3.0
#define VS_SC_MULT          0x8000
/*! \brief Clock doubler enable. 
 *
 * Used for the VS1001, VS1011 and VS1002.
 */
#define VS_CF_DOUBLER       0x8000
/*@}*/

#ifndef VS10XX_HWRST_DURATION
/*! \brief Minimum time in milliseconds to held hardware reset low. */
#define VS10XX_HWRST_DURATION   1
#endif

#ifndef VS10XX_HWRST_RECOVER
/*! \brief Milliseconds to wait after hardware reset. */
#define VS10XX_HWRST_RECOVER    10
#endif

#ifndef VS10XX_SWRST_RECOVER
/*! \brief Milliseconds to wait after software reset. */
#define VS10XX_SWRST_RECOVER    VS10XX_HWRST_RECOVER
#endif


static volatile rt_uint8_t vs_status = VS_STATUS_STOPPED;
static volatile rt_int32_t vs_flush;

static void delay_ms(rt_uint32_t ms)
{
    rt_uint32_t len;
    for (; ms > 0; ms --)
        for (len = 0; len < 100; len++ );
}


/*******************************************************************************
* Function Name  : SPI_VS1003_Init			 
* Description    : SPI2 初始化 用于VS1003.
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
static void SPI_VS1003_Init(void)
{
    SPI_InitTypeDef  SPI_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;

    /* Enable SPI2 GPIOB clocks */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2 ,ENABLE);

    /* Configure SPI2 pins: SCK, MISO and MOSI */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	                   //选择为复用SPI口线
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Configure PB.12 as Output push-pull, used as Flash Chip select */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;						   //XCS VS1003串行控制接口选择 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);


    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0;    //PE0 VS1003 RST   
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_6;    //PC6 VS1003 XDCS  串行数据接口选择  
    GPIO_Init(GPIOC, &GPIO_InitStructure);


    // PC7 DREQ
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    // DREQ PC7
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource7);

    EXTI_InitStructure.EXTI_Line = EXTI_Line7;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_Trigger = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    EXTI_ClearITPendingBit(EXTI_Line7);

    /* SPI2 configuration */ 
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;				  //SPI 主模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;			  //8位 
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;				  //平时CLK 为低
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32;  //由于VS1003的响应速度，SPI速度还不能太快
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;				   //高位在前
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &SPI_InitStructure);

    /* Enable SPI2  */
    SPI_Cmd(SPI2, ENABLE);   									  //使能SPI2 
}

/*
void DMA_Configration(uint32_t addr, int size)
{

#define CODEC_I2S_DMA       DMA1_Channel5
#define CODEC_I2S_DMA_IRQ   DMA1_Channel5_IRQn
  
    DMA_Cmd(CODEC_I2S_DMA, DISABLE);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&(CODEC_I2S_PORT->DR));
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32) addr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = size;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(CODEC_I2S_DMA, &DMA_InitStructure);

    // Enable SPI DMA Tx request 
    SPI_I2S_DMACmd(CODEC_I2S_PORT, SPI_I2S_DMAReq_Tx, ENABLE);

    DMA_ITConfig(CODEC_I2S_DMA, DMA_IT_TC, ENABLE);
    DMA_Cmd(CODEC_I2S_DMA, ENABLE);
}

void NVIC_Configuration(void)
{
    NVIC_InitTypeDef NVIC_InitStructure; 

    NVIC_InitStructure.NVIC_IRQChannel = CODEC_I2S_DMA_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
}
*/

/*******************************************************************************
* Function Name  : SPI_FLASH_ReadByte
* Description    : Reads a byte from the SPI Flash.
*                  This function must be used only if the Start_Read_Sequence
*                  function has been previously called.
* Input          : None
* Output         : None
* Return         : Byte Read from the SPI Flash.
*******************************************************************************/
static u8 ReadWordADS(void)
{
   while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);  	   //判断SPI2 发送缓冲区是否空
  /* Send byte through the SPI2 peripheral */
  SPI_I2S_SendData(SPI2, 0);										   //发送一个空字节

  /* Wait to receive a byte */
  while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);	   //判断是否接收缓冲区非空

  /* Return the byte read from the SPI bus */
  return SPI_I2S_ReceiveData(SPI2);									   //返回接收到的数据
  //return (WriteByteADS(0));
}

/*******************************************************************************
* Function Name  : SPI_FLASH_SendByte
* Description    : Sends a byte through the SPI interface and return the byte 
*                  received from the SPI bus.
* Input          : byte : byte to send.
* Output         : None
* Return         : The value of the received byte.
*******************************************************************************/
static u8 WriteByteADS(u8 byte)
{
    /* Loop while DR register in not emplty */
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);

    /* Send byte through the SPI2 peripheral */
    SPI_I2S_SendData(SPI2, byte);	

    /* Wait to receive a byte */
    while(SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);

    /* Return the byte read from the SPI bus */
    return SPI_I2S_ReceiveData(SPI2);
}



/*************************************************************/
/*  函数名称 :  Mp3WriteRegister                             */
/*  函数功能 ： 写vs1003寄存器                               */
/*  参数     :  寄存器地址，待写数据的高8位，待写数据的低8位 */
/*  返回值   :  无                                           */
/*-----------------------------------------------------------*/
void Mp3WriteRegister(unsigned char addressbyte, unsigned char highbyte, unsigned char lowbyte)
{
    TXDCS_SET(1);       //xDCS = 1
    TCS_SET(0);       //xCS = 0
    WriteByteADS(VS_WRITE_COMMAND); //发送写寄存器命令
    WriteByteADS(addressbyte);      //发送寄存器的地址
    WriteByteADS(highbyte);         //发送待写数据的高8位
    WriteByteADS(lowbyte);          //发送待写数据的低8位
    TCS_SET(1);       //xCS = 1
}


/*************************************************************/
/*  函数名称 :  Mp3ReadRegister                              */
/*  函数功能 ： 写vs1003寄存器                               */
/*  参数     :  寄存器地址				     				 */
/*  返回值   :  vs1003的16位寄存器的值                       */
/*-----------------------------------------------------------*/
unsigned int Mp3ReadRegister(unsigned char addressbyte)
{
    unsigned int resultvalue = 0;
    TXDCS_SET(1);                //xDCS =1
    TCS_SET(0);                   //xCS = 0
    WriteByteADS(VS_READ_COMMAND); //发送读寄存器命令
    WriteByteADS((addressbyte));//发送寄存器的地址
    resultvalue = (unsigned int )(ReadWordADS() << 8);//读取高8位数据
    resultvalue |= ReadWordADS();  //读取低8位数据
    TCS_SET(1);                    //xCS = 1        
    return resultvalue;                 //返回16位寄存器的值
}

/***********************************************************/

/*---------------------------------------------------------*/
void mp3start(void)
{
    TRST_SET(0);          //xRST = 0   复位vs1003(); 
    delay_ms(10);       
    WriteByteADS(0xff);   //发送一个字节的无效数据，启动SPI传输
    TXDCS_SET(1);         //xDCS =1
    TCS_SET(1);           //xCS = 1
    TRST_SET(1);          //xRST =1   
    delay_ms(10);             

    Mp3WriteRegister(VS_MODE,0x08,0x00);  //进入vs1003的播放模式

    Mp3WriteRegister(3, 0x98, 0x00);   //设置vs1003的时钟,3倍频
    Mp3WriteRegister (5, 0xBB, 0x81);   //采样率48k，立体声
    Mp3WriteRegister(VS_BASS, 0x00, 0x00);//设置重音
    Mp3WriteRegister(0x0b,0x00,0x00);      //VS1003 音量

    while(DREQ==0);
    //等待DREQ为高  表示能够接受音乐数据输入

}

/*!
 * \brief Enable or disable player interrupts.
 *
 * This routine is typically used by applications when dealing with 
 * unprotected buffers.
 *
 * \param enable Disables interrupts when zero. Otherwise interrupts
 *               are enabled.
 *
 * \return Zero if interrupts were disabled before this call.
 */
rt_uint16_t VsPlayerInterrupts(rt_uint16_t enable)
{
    //static rt_uint16_t is_enabled = 0;
    rt_uint16_t rc;

    //rc = is_enabled;
    if(enable) 
    {
        // TODO add DREQ interrupt line 
        
        //VS10XX_DREQ_ENA();
       
        //NutIrqEnable(&VS10XX_SIGNAL);
        //rt_hw_interrupt_enable(enable);
    }
    else 
    {
        //NutIrqDisable(&VS10XX_SIGNAL);
        
        //rc = rt_hw_interrupt_disable();
    }
    //is_enabled = enable;

    return rc;
}


/**
 * @brief Wait for Decoder ready
 *
 * This function will check the DREQ line, Decoder interrupts must be disabled
 */
static rt_bool_t VsWaitReady(void) 
{
    int tmo;

    for (tmo = 0; tmo < 5000; tmo++) 
    {
        if (VS_DREQ_TST())
            return RT_TRUE;
    }
    return RT_FALSE;
}

/**
 * @brief Read from a register.
 * Decoder interrupts must have been disabled before calling this function
 * @param address
 *
 * @return : Register contents
 */
static uint16_t VsRegRead(uint8_t address) 
{
    uint16_t data;

    // Select command channel 
    // data chip select XDCS 
    TXDCS_SET(1);                
    // chip select 
    TCS_SET(0);                 

    VsWaitReady();

    WriteByteADS(VS_READ_COMMAND);
    WriteByteADS((uint8_t) address);
    data = (uint8_t) ReadWordADS() << 8;
    data |= ReadWordADS();
    
    TCS_SET(1);

    return data;
}

/**
 * @brief Write vs1003 register
 *
 * @param address vs1003 internal spi address register index
 * @param data 16bit data value.
 */
static void VsRegWrite(uint8_t address, uint16_t data)
{
    TXDCS_SET(1);
    TCS_SET(0);

    WriteByteADS(VS_WRITE_COMMAND);
    WriteByteADS((uint8_t) address);

    WriteByteADS((uint8_t) (data >> 8 & 0xff));
    WriteByteADS((uint8_t) (data & 0xff));
    
    TCS_SET(1);
}

/**
 * @brief Serves two purposes
 * -- it is called by VsPlayerKick() to initially fill the decoder buffer.
 * -- it is used as an interrupt handler for the decoder.
 * @param arg
 */
void VsPlayerFeed(void *arg)
{
    rt_uint8_t  j = 32;
    rt_int32_t total = 0;
    rt_uint8_t *bp = RT_NULL;
    unsigned int consumed = 0, available = 0;
    
    if (!VS_DREQ_TST())
    {
        return ;
    }

    /*  feed the decoder util its buffer is full or we ran out of data */
    if (vs_status == VS_STATUS_RUNNING)
    {
        do 
        {
            if (consumed >= available)
            {
                if (consumed) 
                {
                    NutSegBufReadCommit(consumed);
                    consumed = 0;
                }

                // all bytes consumed, request new.
                bp = (rt_uint8_t *)NutSegBufReadRequest(&available);
                if (!available) 
                {
                    vs_status = VS_STATUS_EOF;
                    break;
                }
            }

            // we have some data int the buffer, feed it
            WriteByteADS(*bp);
            bp++;
            consumed++;
            total++;
            if (total > 4096)
            {
                vs_status = VS_STATUS_EOF;
                break;
            }


            // allow 32 bytes to be send as long as DREQ is Set
            if (VS_DREQ_TST()) 
            {
                j = 32;
            }
        } while (j--);

        NutSegBufReadLast(consumed);
    }

    /* 
     * Flush the internal VS buffer. 
     */
    if (vs_status != VS_STATUS_RUNNING && vs_flush) 
    {
        do 
        {
            //VsSdiPutByte(0);
            WriteByteADS(0);
            if (--vs_flush == 0) 
            {
                // Decoder internal buffer is flushed. 
                vs_status = VS_STATUS_EMPTY;
                break;
            }
            // Allow 32 bytes to be sent as long as DREQ is set, 
            // This includes the one in progress. 
            if (VS_DREQ_TST())
            {
                j = 32;
            }
        } while(j--);
    }

}

/**
 * @brief Set up decoder internal buffer flushing.
 *   This routine will set up internal VS buffer flushing. unless the buffer is already empty 
 *   and starts the playback, if necessary. The internal VS buffer is flushed in VsPlayerFeed()
 *   at the end of the stream.
 * @return TRUE: ok, FALSE: failed. 
 */
rt_bool_t VsPlayerFlush(void *arg)
{
    VsPlayerInterrupts(0);
    /* Set up fluhing unless both buffers are empty. */
    //if (vs_status != VS_STATUS_EMPTY || NutSegBufUsed()) {
    if (vs_status != VS_STATUS_EMPTY) 
    {
        if (vs_flush == 0)
        {
            vs_flush = VS_FLUSH_BYTES;
        }
        /* start the playback if necessary */
        if (vs_status != VS_STATUS_RUNNING)
        {
            VsPlayerKick();
        }
    }
    VsPlayerInterrupts(1);

    return RT_TRUE;
}

/*!
 * \brief Stops the playback.
 *
 * This routine will stops the MP3 playback, VsPlayerKick() may be used 
 * to resume the playback.
 *
 * \return 0 on success, -1 otherwise.
 */
rt_bool_t VsPlayerStop(void)
{
    rt_uint32_t ief;

    ief = VsPlayerInterrupts(0);
    /* Check whether we need to stop at all to not overwrite other than running status */
    if (vs_status == VS_STATUS_RUNNING)
    {
        vs_status = VS_STATUS_STOPPED;
    }
    VsPlayerInterrupts(ief);

    return RT_TRUE;
}


/**
 * @brief Start playerback.
 *  This routine will send the first MP3 data bytes to the decoder, 
 *  until it is completely filled. The data buffer should have been 
 *  filled before calling this function.
 *
 *  -- Decoder interrupt will be enabled.
 *
 * @return TRUE : success, FALSE: failed. 
 */
rt_bool_t VsPlayerKick(void)
{
    VsPlayerInterrupts(0);
    vs_status = VS_STATUS_RUNNING;
    VsPlayerFlush(RT_NULL);
    VsPlayerInterrupts(1);

    return RT_TRUE;
}

/*!
 * \brief Software reset the decoder.
 *
 * This function is typically called after VsPlayerInit() and at the end
 * of each track.
 *
 * \param mode Any of the following flags may be or'ed (check the data sheet)
 * - \ref VS_SM_DIFF Left channel inverted.
 * - \ref VS_SM_LAYER12 Allow MPEG layers I and II.
 * - \ref VS_SM_MP12 Allow MPEG layers I and II on VS1001K.
 * - \ref VS_SM_FFWD VS1001K fast forward.
 * - \ref VS_SM_RESET Software reset.
 * - \ref VS_SM_OUTOFWAV Jump out of WAV decoding.
 * - \ref VS_SM_TESTS Allow SDI tests.
 * - \ref VS_SM_PDOWN Switch to power down mode.
 * - \ref VS_SM_BASS VS1001K bass/treble enhancer.
 * - \ref VS_SM_STREAM Stream mode.
 * - \ref VS_SM_SDISHARE Share SPI chip select.
 * - \ref VS_SM_SDINEW VS1002 native mode (automatically set).
 * - \ref VS_SM_ADPCM VS1033 ADPCM recording.
 * - \ref VS_SM_ADPCM_HP VS1033 ADPCM high pass filter.
 * - \ref VS_SM_LINE_IN VS1033 ADPCM recording selector.
 * - \ref VS_SM_CLK_RANGE VS1033 input clock range.
 *
 * \return 0 on success, -1 otherwise.
 */
rt_bool_t VsPlayerReset(rt_uint16_t mode)
{
    /* Disable decoder interrupts and feeding. */
    VsPlayerInterrupts(0);
    vs_status = VS_STATUS_STOPPED;

    /* Software reset. */
    VsRegWrite(VS_MODE, SM_RESET);
    /* The decoder needs 9600 XTAL cycles. This is at least twice. */
    //    NutDelay(VS10XX_SWRST_RECOVER);
    delay_ms(50);

    /*
     * Check for correct reset.
     */
    if ((mode & SM_RESET) != 0 || !VS_DREQ_TST())
    {
        /* If not succeeded, give it one more chance and try hw reset. */
        //SciReset(1);
        TRST_SET(1);
        /* No idea how long we must held reset low. */
        //NutDelay(VS10XX_HWRST_DURATION);
        delay_ms(VS10XX_HWRST_RECOVER);
        //SciReset(0);
        TRST_SET(0);

        /* No idea how long we need to wait here. */
        //NutDelay(VS10XX_HWRST_RECOVER);

        /* Set codec mode. */
        VsRegWrite(VS_MODE, SM_SDINEW | mode);

        // XTALI x 3.0 ~ (1.5 + 3.0)
        VsRegWrite(VS_CLOCKF, (uint16_t)(VS_SC_MULT| VS_SC_ADD | VS_SC_FREQ_LSB));

        // set bass
        VsRegWrite(VS_BASS, 0x0000);

        //+----------------------------------------------+------+-------+
        //| Bit 15: 1                                    | Bit 0        |
        //+----------------------------------------------+--------------+
        //|  sample rate divided by two                  | (0)mono data |
        //                                               | (1)sereo     |
        //+----------------------------------------------+--------------+
        VsRegWrite(VS_AUDATA , ((48000 >> 1) << 1) & 0x1);

        delay_ms(VS10XX_HWRST_RECOVER);
        if (!VS_DREQ_TST())
        {
            return RT_FALSE;
        }
    }

    return RT_TRUE;
}

void VsPlayerInit(void)
{
    SPI_VS1003_Init();

    TRST_SET(0);          //xRST = 0   复位vs1003(); 
    delay_ms(10);       
    WriteByteADS(0xff);   //发送一个字节的无效数据，启动SPI传输
    TXDCS_SET(1);         //xDCS =1
    TCS_SET(1);           //xCS = 1
    TRST_SET(1);          //xRST =1   
    delay_ms(10);             

    //    Mp3WriteRegister(VS_MODE,0x08,0x00);  
    //    //进入vs1003的播放模式
    VsPlayerSetMode(0x0000);

    //设置vs1003的时钟,3倍频
    VsRegWrite(VS_CLOCKF, (uint16_t)(VS_SC_MULT| VS_SC_ADD | VS_SC_FREQ_LSB));

    // set bass
    VsRegWrite(VS_BASS, 0x0000);

    //+----------------------------------------------+------+-------+
    //| Bit 15: 1                                    | Bit 0        |
    //+----------------------------------------------+--------------+
    //|  sample rate divided by two                  | (0)mono data |
    //                                               | (1)sereo     |
    //+----------------------------------------------+--------------+
    VsRegWrite(VS_AUDATA , ((48000 >> 1) << 1) & 0x1);

    VsSetVolume(20, 20);

    while(VS_DREQ_TST()==0) ;
}

/**
 * @brief  Set mode register of the decoder
 *
 * @param mode Any of the following flags may be or'ed (check the data sheet)
 *
 * @return  Always 0
 */
int VsPlayerSetMode(uint16_t mode) 
{
    rt_int32_t ief;

    // disable interrupt
    ief = VsPlayerInterrupts(0);

    Mp3WriteRegister(VS_MODE, (SM_SDINEW | mode) >> 8, (rt_uint8_t)mode);

    // enable interrupt
    VsPlayerInterrupts(ief);
    
    return 0;
}

/**
 * @brief Returns play time since last reset
 *
 * @return  Play time since reset in seconds.
 */
rt_uint16_t VsPlayerTime(void)
{
    rt_uint16_t rc;
    rt_int32_t ief;

    ief = VsPlayerInterrupts(0);
    rc = VsRegRead(VS_DECODE_TIME);
    VsPlayerInterrupts(ief);
    
    return rc;
}

/**
 * @brief Returns the status  of the player.
 *
 * @return  Any of the folloing value.
 * -- VS_STATUS_STOPPED : player is ready to be started by VsPlayerKick()
 * -- VS_STATUS_RUNNING : player is running
 * -- VS_STATUS_EOF     : player has reach the end of stream after VsPlayerFlush() has benn called
 * -- VS_STATUS_EMPTY   : player runs out of data, VsPlayerKick() will restart it.
 */
rt_uint32_t VsGetStatus(void)
{
    return vs_status;
}

/**
 * @brief Set volume 
 * ex: VsSetVolume(255 - rc_rvolume, 255 - rc_rvolume);
 * mute: 255, 255
 *
 * @param left  Left channel attenuation, provided in 0.5dB steps
 *      Set to 255 for ananlog power down.
 * @param right Right channel attenuation.
 * 
 * @return  Always 0.
 */
int VsSetVolume(rt_uint8_t left, rt_uint8_t right)
{
    // disable interrupt
    VsPlayerInterrupts(0);
    // left * 256 + right
    Mp3WriteRegister(VS_VOL, left << 8, right);  
    VsPlayerInterrupts(1);

    return 0;
}

/**
 * @brief for audio stream type,
 * -----------------+-------------
 *  HDA0     HDA1   |
 * -----------------+------------
 *  0x7761  0x7665  | WAV files
 * -----------------+------------
 *  data    0x574D  | WMA files
 *  speed           |
 * -----------------------------+
 *  Page37  0x4D54  | MIDI files|
 * -----------------------------+
 *  SCI_HDAT[0...1] : MP3 files
 * ------------------------------
 * @return Always 0
 */
rt_uint16_t VsGetHeaderInfo()
{
    rt_uint16_t usp[2] = {0x00, 0x00};
    rt_uint16_t ief;

    ief = VsPlayerInterrupts(0);
    usp[1] = Mp3ReadRegister(VS_HDAT1);
    usp[0] = Mp3ReadRegister(VS_HDAT0); 
    VsPlayerInterrupts(ief);

    return usp[1] << 8 | usp[0];
}

/**
 * @brief Write a specified @len number of bytes from @data_addr to vs1003 
 *    data interface.
 *
 * @param data_addr
 * @param len
 *
 * @return 
 */
static rt_bool_t VsSdiWrite_P(rt_uint8_t *data_addr, rt_int32_t len)
{
    // data interface
    TXDCS_SET(0);

    while (len--) 
    {
        if (!VS_DREQ_TST() && VsWaitReady())
            return RT_FALSE;
        WriteByteADS(*data_addr);

        data_addr++;
    }

    TXDCS_SET(1);

    return RT_TRUE;
}

/**
 * @brief initialized decoder memeory test 
 *
 * @return test result
 * - Bit 0: Good X ROM 
 * - Bit 1: Good Y ROM
 * - Bit 2: Good I ROM
 * - Bit 3: Good X RAM 
 * - Bit 4: Good Y RAM 
 * - Bit 5: Good I RAM
 * - Bit 6: Mux test succeeded.
 */
rt_uint16_t VsMemoryTest(void)
{
    rt_uint16_t rc;
    rt_uint16_t ief;
    static rt_uint8_t mtcmd[] = {0x4D, 0xEA, 0x6D, 0x54, 0x00, 0x00, 0x00, 0x00};

    ief = VsPlayerInterrupts(0);
    VsRegWrite(VS_MODE, SM_TESTS | SM_SDINEW);

    VsSdiWrite_P(&mtcmd[0], sizeof(mtcmd));

    while (((rc=VsRegRead(VS_HDAT0)) & 0x8000) == 0) ;

    VsRegWrite(VS_MODE, SM_SDINEW);
    VsPlayerInterrupts(ief);

    return rc;
}


#ifdef RT_USING_FINSH
#include <finsh.h>

/* 0 ~ 255*/
void vol(rt_uint16_t v)
{
    rt_kprintf("set volume %d (maximun 255)", v);
    VsSetVolume(v, v);
}

FINSH_FUNCTION_EXPORT(vol, set vsplayer volume);
FINSH_FUNCTION_EXPORT(VsMemoryTest,  vsplayer memory test);
#endif
