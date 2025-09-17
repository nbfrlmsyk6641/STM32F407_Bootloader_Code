#include "main.h" 

// 函数指针类型定义
typedef void (*pFunction)(void);

// 跳转到应用程序
void Jump_To_Application(void)
{
    uint32_t JumpAddress;
    pFunction JumpToApplication;

    if (((*(volatile uint32_t*)APP_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
    {
        JumpAddress = *(volatile uint32_t*) (APP_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;

        __disable_irq();
        SCB->VTOR = APP_ADDRESS;
        __set_MSP(*(volatile uint32_t*) APP_ADDRESS);
        
        JumpToApplication();
    }
}


