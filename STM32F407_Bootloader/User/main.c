#include "main.h" 

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

    if (BKP_Data == 0xAaBbCcDd)
    {
        // BKP清零
        BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);

        // 存在升级请求，准备走IAP流程
        IAP_Flag = 1;
    }
    else
    {
        // 不存在升级请求，准备执行5s初始化后进App
        IAP_Flag = 0;

        // BKP也清零
        BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);
    }

    if (IAP_Flag == 0)
    {
        TIM3_Configuration();
        LED_GPIO_Config();
        CAN_1_Init();
		ISOTP_Init();
        UDS_Init();

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
            }

            if(Get_Task2_Flag() == 1)
            {
                // 5秒等待结束，无任何特殊请求，跳转到App
                Clear_Task2_Flag();

                // 跳转进App
                Jump_To_Application();
            }
        }
    }

    if(IAP_Flag == 1)
    {
        // 主循环代码
        TIM3_Configuration();
        LED_GPIO_Config();
        CAN_1_Init();
        ISOTP_Init();
        UDS_Init();

        while(1)
        {

            // LED闪烁，指示正在运行
            LED_Task();

            // 传输层处理CAN报文
            ISO15765_CAN();

            // ISO15765超时机制
            ISOTP_Timer_NCr();

            // UDS应用层
            UDS_Process();

            // ISO15765错误机制
            ISOTP_ErrorStatus();
        }        
    }
}



