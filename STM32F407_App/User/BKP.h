// bsp_bkp.h
#ifndef __BSP_BKP_H
#define __BSP_BKP_H


#define BKP_IAP_REQUEST_FLAG   ((uint32_t)0x11111111)

#define BKP_FLAG_REGISTER      RTC_BKP_DR0

void BKP_Init(void);
void BKP_WriteRegister(uint32_t registerIndex, uint32_t data);
uint32_t BKP_ReadRegister(uint32_t registerIndex);

#endif

