# include "main.h"

// 初始化环形缓冲器
static RingBuffer_t g_rx_buffer = { {0}, 0, 0 };

// 环形计数器溢出变量
volatile uint32_t g_buffer_overflow_cnt = 0;  // 溢出计数器
volatile uint8_t  g_buffer_error_flag = 0;    // 溢出标志位

// 环形缓冲器写入函数
static void RingBuffer_Write(CanRxMsg msg)
{
    // 计算下一个写入位置，通过取余实现环绕
    uint16_t next_head = (g_rx_buffer.head + 1) % RX_RING_BUFFER_SIZE;

    // 检查是否已满，next_head = tail代表缓冲器满
    if (next_head != g_rx_buffer.tail)
    {
        // 1. 存入数据
        g_rx_buffer.buffer[g_rx_buffer.head] = msg;
        
        // 2. 移动头指针
        g_rx_buffer.head = next_head;
    }
    else
    {
        // 记录溢出次数
        g_buffer_overflow_cnt++;
        
        g_buffer_error_flag = 1;
    }
}

// 环形缓冲器读取函数
uint8_t CAN_RingBuffer_Read(CanRxMsg* msg)
{
    // 检查是否为空, head = tail 代表缓冲器空
    if (g_rx_buffer.head == g_rx_buffer.tail)
    {
        return 0; // 没有数据
    }

    // 1. 读取数据
    *msg = g_rx_buffer.buffer[g_rx_buffer.tail];

    // 2. 移动尾指针
    g_rx_buffer.tail = (g_rx_buffer.tail + 1) % RX_RING_BUFFER_SIZE;

    return 1; // 成功
}



void CAN_1_Init(void)
{
    // 结构体定义
    GPIO_InitTypeDef       GPIO_InitStructure;
    CAN_InitTypeDef        CAN_InitStructure;
    CAN_FilterInitTypeDef  CAN_FilterInitStructure;
    NVIC_InitTypeDef       NVIC_InitStructure;

    // 1、初始化CAN1引脚时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    // 1、初始化CAN外设时钟，CAN时钟为42MHz
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    // 2、将引脚复用为CAN功能
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource0, GPIO_AF_CAN1);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF_CAN1);

    // 2、CAN1引脚配置，PD1，TX引脚，推挽输出，上拉引脚
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;          
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;       
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;    
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;        
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // 2、CAN1引脚配置，PD0，RX引脚，上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF; 
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // 2、初始化CAN1前复位CAN1
    CAN_DeInit(CAN1);
	CAN_StructInit(&CAN_InitStructure);

    // 3、CAN1初始化配置
    CAN_InitStructure.CAN_TTCM = DISABLE;
	CAN_InitStructure.CAN_ABOM = DISABLE;
	CAN_InitStructure.CAN_AWUM = DISABLE;
	CAN_InitStructure.CAN_NART = DISABLE;
	CAN_InitStructure.CAN_RFLM = DISABLE;
	CAN_InitStructure.CAN_TXFP = DISABLE;

    // 3、调试使用回环模式，调试结束使用正常模式
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;

    // 3、初始化CAN波特率，波特率 = 42MHz / CAN_Prescaler / (CAN_BS1 + CAN_BS2 + 1)，以下数据配置波特率为1000Kbps
    CAN_InitStructure.CAN_Prescaler = 3;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_10tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_3tq;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;

    CAN_Init(CAN1, &CAN_InitStructure);

    // 4、CAN1 过滤器配置
    CAN_FilterInitStructure.CAN_FilterNumber = 0;

    // 不设置过滤器
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
	CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
	CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;

    // 过滤器为32位位宽
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;

    CAN_FilterInit(&CAN_FilterInitStructure);

    // 5、使能CAN1接收中断
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

    // 6、配置CAN1中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_IRQn; 
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

uint8_t CAN1_Transmit_TX(uint32_t ID, uint8_t Length, uint8_t *Data)
{
    uint8_t i;
    uint8_t transmit_mailbox = 0;
    uint32_t timeout = 0;

    CanTxMsg TxMessage;

    // 配置为发送标准数据帧
    TxMessage.StdId = ID;
    TxMessage.ExtId = 0;
    TxMessage.IDE = CAN_Id_Standard;
    TxMessage.RTR = CAN_RTR_Data;
    TxMessage.DLC = Length;

    // 复制数据
    for (i = 0; i < Length; i++)
    {
        TxMessage.Data[i] = Data[i];
    }

    // 发送数据
    transmit_mailbox = CAN_Transmit(CAN1, &TxMessage);

    // 检查发送邮箱状态
    if (transmit_mailbox == CAN_TxStatus_NoMailBox)
    {
        // 3个邮箱都满，直接返回失败
        return 1; 
    }

    // 等待发送完成，超时则退出
    while (CAN_TransmitStatus(CAN1, transmit_mailbox) == CAN_TxStatus_Pending)
    {
        timeout++;
        if (timeout > 0x1FFFF) 
        {
            // 超时，取消发送并返回失败
            CAN_CancelTransmit(CAN1, transmit_mailbox);
            return 2;
        }
    }

    // 成功发送，返回0
    if (CAN_TransmitStatus(CAN1, transmit_mailbox) == CAN_TxStatus_Ok)
    {
        return 0;
    }

    // 返回2也代表发送失败
    return 2;
}

uint8_t CAN1_ReceiveFlag(void)
{
    if (CAN_MessagePending(CAN1, CAN_FIFO0) > 0)
    {
        // FIFO里有报文
        return 1; 
    }

    // FIFO里没有报文
    return 0;
}

void CAN1_Receive_RX(uint32_t *ID, uint8_t *Length, uint8_t *Data)
{
    uint8_t i;
    CanRxMsg RxMessage;

    CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);

    // 开始解析报文数据
    if (RxMessage.IDE == CAN_Id_Standard)
    {
        *ID = RxMessage.StdId;
    }
    else
    {
        *ID = RxMessage.ExtId;
    }

    if(RxMessage.RTR == CAN_RTR_Data)
    {
        // 数据帧
        *Length = RxMessage.DLC;
        for (i = 0; i < *Length; i++)
        {
            Data[i] = RxMessage.Data[i];
        }
    }
    else
    {
        // 遥控帧，暂时不做处理
    }

}

// CAN1接收中断服务函数
void CAN1_RX0_IRQHandler(void)
{
    CanRxMsg RxMessage;

    // 1. 检查 FIFO0 消息挂起中断
    if (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET)
    {
        // 2. 从硬件 FIFO 读取报文
        CAN_Receive(CAN1, CAN_FIFO0, &RxMessage);

        // 3. 将报文推入软件环形缓冲器
        RingBuffer_Write(RxMessage);
    }
}


