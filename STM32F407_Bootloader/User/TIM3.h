# ifndef __TIM3_H
# define __TIM3_H

void TIM3_Configuration(void);
uint16_t Get_Task1_Flag(void);
uint16_t Get_Task2_Flag(void);
uint16_t Get_Task3_Flag(void);
void Clear_Task1_Flag(void);
void Clear_Task2_Flag(void);
void Clear_Task3_Flag(void);
uint8_t TIM3_GetNCrFlag(void);
void TIM3_ClearNCrFlag(void);

# endif
