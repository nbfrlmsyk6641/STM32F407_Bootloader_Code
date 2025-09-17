#include "main.h"   

int main(void)
{
    // 外设初始化
    TIM3_Configuration();
    LED_GPIO_Config();

    while(1)
    {
        // 主循环代码
        if(Task1_Flag)
        {
            // 5s等待，闪烁频率高于App
            Task1_Flag = 0;
            GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
        }

        if(Task2_Flag)
        {
            // 5秒后跳转到App
            Task2_Flag = 0;
            Jump_To_Application();
        }
    }
}

