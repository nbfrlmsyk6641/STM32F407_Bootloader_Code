#include "stm32f4xx.h"   

// 定义APP的起始地址,512KB的flash，32KB的bootloader，480KB的APP
#define APP_ADDRESS    (uint32_t)0x08008000

// 定义一个函数指针，用于指向APP的复位中断函数
typedef void (*pFunction)(void);

// 全局变量
pFunction JumpToApplication;
uint32_t JumpAddress;


int main(void)
{
	if (((*(volatile uint32_t*)APP_ADDRESS) & 0x2FFE0000 ) == 0x20000000)
    {
		// a. 获取APP的复位中断地址
        JumpAddress = *(volatile uint32_t*) (APP_ADDRESS + 4);
        
        // b. 将地址转换为函数指针
        JumpToApplication = (pFunction) JumpAddress;

        // c. 关闭所有中断，在bootloader关闭记得要在APP中重新开启
        __disable_irq();

        // d. 设置APP的中断向量表偏移地址，将中断向量表与App进行匹配
        SCB->VTOR = APP_ADDRESS;
        
        // e. 设置APP的栈顶指针
        __set_MSP(*(volatile uint32_t*) APP_ADDRESS);
        
        // f. 执行跳转
        JumpToApplication();
    }
}

