#include "main.h"

// 定义APP的起始地址，起始地址必须为4字节对齐
#define APP_ADDRESS    (uint32_t)0x08008000

int main(void)
{
	// 设置向量表偏移地址
	SCB->VTOR = APP_ADDRESS;

	// 在bootloader跳转到APP之前，执行了关闭所有中断的操作，在这里需要重新打开
	__enable_irq();

	TIM3_Configuration();
	LED_GPIO_Config();	
	CAN_1_Init();
	FLASH_Store_Init();
	BKP_Init();

	// 初始化关闭LED1
	LED1_OFF;

	// 初始化清空BKP
	BKP_WriteRegister(BKP_FLAG_REGISTER, 0x00000000);

	while(1)
	{
		
	}
	
}



