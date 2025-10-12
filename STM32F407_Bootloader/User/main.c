#include "main.h"   

int main(void)
{
    // 外设初始化
    TIM3_Configuration();
    LED_GPIO_Config();

    while(1)
    {
        // 主循环代码
        if(Get_Task1_Flag() == 1)
        {
            // 5s等待，闪烁频率高于App
            Clear_Task1_Flag();
            GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
        }

        if(Get_Task2_Flag() == 1)
        {
            // 5秒后跳转到App
            Clear_Task2_Flag();
            Jump_To_Application();
        }
    }
}

