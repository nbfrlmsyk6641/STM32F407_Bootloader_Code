#include "main.h"

// 定义APP的起始地址，起始地址必须为4字节对齐
#define APP_ADDRESS    (uint32_t)0x08008000

int main(void)
{
	
	SCB->VTOR = APP_ADDRESS;

	__enable_irq();

	TIM3_Configuration();
	LED_GPIO_Config();	
	CAN_1_Init();
	FLASH_Store_Init();

	LED1_OFF;

	while(1)
	{
		
	}
	
}



