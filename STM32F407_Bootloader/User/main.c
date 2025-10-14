#include "main.h" 

// 周期发送报文变量
uint32_t TxID = 0x000;
uint8_t TxLength = 8;
uint8_t TxData[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

int main(void)
{
    // 外设初始化
    TIM3_Configuration();
    LED_GPIO_Config();
    CAN_1_Init();

    while(1)
    {
        // 主循环代码
        if(Get_Task1_Flag() == 1)
        {
            // 5s等待过程中，持续闪烁LED且频率高于App的LED闪烁频率
            Clear_Task1_Flag();
            GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
        }

        if(Get_Task3_Flag() == 1)
        {
            // 5s等待过程中，每1s发送一帧报文
            Clear_Task3_Flag();
            CAN1_Transmit_TX(TxID, TxLength, TxData);
        }

        if(Get_Task2_Flag() == 1)
        {
            // 5秒等待结束，确认无软件更新请求后跳转到App
            Clear_Task2_Flag();
            Jump_To_Application();
        }
    }
}

