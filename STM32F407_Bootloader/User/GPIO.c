#include "main.h"

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

void LED_Task(void)
{
	if (Get_Task1_Flag() == 1)
	{
		Clear_Task1_Flag();
		GPIO_ToggleBits(GPIOE, GPIO_Pin_13);
	}
}

