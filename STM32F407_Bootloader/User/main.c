#include "main.h" 

// 无升级时周期发送报文变量
uint32_t Normal_TxID = 0x000;
uint8_t Normal_TxLength = 8;
uint8_t Normal_TxData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 有升级时周期发送报文变量
uint32_t Boot_TxID = 0x004;
uint8_t Boot_TxLength = 8;
uint8_t Boot_TxData[8] = {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44};

// IAP标志位
uint8_t IAP_Flag = 0;

// BKP变量
uint32_t BKP_Data = 0;

int main(void)
{
    // 初始化配置中断分组
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // 检查BKP寄存器中的IAP请求标志
    BKP_Init();

    BKP_Data = BKP_ReadRegister(BKP_FLAG_REGISTER);

    if (BKP_Data == 0x11111111)
    {
        // BKP清零
        BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);

        // 存在升级请求
        IAP_Flag = 1;
    }
    else
    {
        // 不存在升级请求
        IAP_Flag = 0;
    }

    if (IAP_Flag == 0)
    {
        TIM3_Configuration();
        LED_GPIO_Config();
        CAN_1_Init();

        while(1)
        {
            // 主循环代码
            if(Get_Task1_Flag() == 1)
            {
                // 5s等待过程中
                Clear_Task1_Flag();

                // 持续闪烁LED且频率高于App的LED闪烁频率
                GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
            }

            if(Get_Task3_Flag() == 1)
            {
                // 5s等待过程中
                Clear_Task3_Flag();

                // 每1s发送一帧CAN报文
                CAN1_Transmit_TX(Normal_TxID, Normal_TxLength, Normal_TxData);
            }

            if(Get_Task2_Flag() == 1)
            {
                // 5秒等待结束，无任何特殊请求，跳转到App
                Clear_Task2_Flag();

                Jump_To_Application();
            }
        }
    }

    if(IAP_Flag == 1)
    {
        // 升级的逻辑还没写，所以跟无升级请求的代码差不多，只是发送的报文上做一个区分
        TIM3_Configuration();
        LED_GPIO_Config();
        CAN_1_Init();

        while(1)
        {
            // 主循环代码
            if(Get_Task1_Flag() == 1)
            {
                // 5s等待过程中
                Clear_Task1_Flag();

                // 持续闪烁LED且频率高于App的LED闪烁频率
                GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
            }

            if(Get_Task3_Flag() == 1)
            {
                // 5s等待过程中
                Clear_Task3_Flag();

                // 每1s发送一帧CAN报文
                CAN1_Transmit_TX(Boot_TxID, Boot_TxLength, Boot_TxData);
            }

            if(Get_Task2_Flag() == 1)
            {
                // 5秒等待结束，跳转到App
                Clear_Task2_Flag();

                Jump_To_Application();
            }
        }
    }

}



