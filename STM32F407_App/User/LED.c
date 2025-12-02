#include "main.h"

// LED硬件初始化
void LED_GPIO_Config(void)
{

	GPIO_InitTypeDef  GPIO_InitStructure;
	/* Enable the GPIO_LED Clock */
	RCC_AHB1PeriphClockCmd(  RCC_AHB1Periph_GPIOE, ENABLE); 		
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	/*初始化完后，关闭LED1*/ 
	LED1_OFF;
}

// LED闪烁任务
void LED_Task(void)
{
	if (TIM3_GetTimeFlag() == 1)
	{
		TIM3_ClearTimeFlag();
		
		// 翻转LED1状态
		GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
	}
}

