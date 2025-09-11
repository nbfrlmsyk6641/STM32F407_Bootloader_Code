# include "main.h"

// 定时器计数变量
uint16_t Time_Count = 0;

// CAN1报文变量
uint32_t TxID = 0x777;
uint8_t TxLength = 4;
uint8_t TxData[8] = {0x11, 0x22, 0x33, 0x44};

void TIM3_Configuration(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef        NVIC_InitStructure;

    // 1. 开启TIM3时钟,TIM3的时钟频率为84MHz
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_InternalClockConfig(TIM3);

    // 2. 配置定时器基础参数,PSC和ARR的取值要在0-65535之间
    TIM_TimeBaseStructure.TIM_Prescaler = 8400 - 1;
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; 
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0; 

    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

    // 3. 使能TIM3的更新中断
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    // 4. 配置中断控制器NVIC
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&NVIC_InitStructure);

    // 5. 启动TIM3定时器
    TIM_Cmd(TIM3, ENABLE);
}

// TIM3中断服务函数
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        Time_Count++;

        if(Time_Count >= 10)
        {
            Time_Count = 0;
            GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
            CAN1_Transmit_TX(TxID, TxLength, TxData);
        }

        // 清除中断标志
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}
