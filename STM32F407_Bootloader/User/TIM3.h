# ifndef __TIM3_H
# define __TIM3_H

extern volatile uint16_t Time_Task1;
extern volatile uint16_t Task1_Flag;
extern volatile uint16_t Time_Task2;
extern volatile uint16_t Task2_Flag;

void TIM3_Configuration(void);

# endif
