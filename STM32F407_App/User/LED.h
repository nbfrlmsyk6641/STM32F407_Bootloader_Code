#ifndef _LED_H_
#define _LED_H_

#define LED1_ON 		GPIO_ResetBits(GPIOE , GPIO_Pin_13)

#define LED1_OFF 		GPIO_SetBits(GPIOE , GPIO_Pin_13)

void LED_GPIO_Config(void);
void LED_Task(void);

#endif

