# include "main.h"

// 定时器计数变量与任务标志位，加上static前缀表示仅在本文件内可见
static volatile uint16_t Time_Task1 = 0;
static volatile uint16_t Task1_Flag = 0;

static volatile uint16_t Time_Task2 = 0;
static volatile uint16_t Task2_Flag = 0;

static volatile uint16_t Time_Task3 = 0;
static volatile uint16_t Task3_Flag = 0;

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
    // NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

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
    // 一次中断0.1s（100ms）
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        Time_Task1 ++;
        Time_Task2 ++;
        Time_Task3 ++;

        // 2 * 100ms = 200ms
        if(Time_Task1 >= 4)
        {
            Time_Task1 = 0;
            Task1_Flag = 1;
        }

        // 10 * 100ms = 1000ms = 1s
        if(Time_Task3 >= 10)
        {
            Time_Task3 = 0;
            Task3_Flag = 1;
        }

        // 50 * 100ms = 5000ms = 5s
        if(Time_Task2 >= 50)
        {
            Time_Task2 = 0;
            Task2_Flag = 1;
        }

        // 清除中断标志
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

// 任务标志位接口函数
uint16_t Get_Task1_Flag(void)
{
    return Task1_Flag;
}

uint16_t Get_Task2_Flag(void)
{
    return Task2_Flag;
}

uint16_t Get_Task3_Flag(void)
{
    return Task3_Flag;
}

void Clear_Task1_Flag(void)
{
    Task1_Flag = 0;
}

void Clear_Task2_Flag(void)
{
    Task2_Flag = 0;
}

void Clear_Task3_Flag(void)
{
    Task3_Flag = 0;
}
