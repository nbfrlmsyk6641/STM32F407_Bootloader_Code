#ifndef _MAIN_H_
#define _MAIN_H_

#include "stm32f4xx.h"
#include "LED.H"
#include "CAN.h"
#include "TIM3.h"
#include "Flash.h"
#include "BKP.h"
#include "ISO15765.h"
#include "ISO14229.h"

// 定义APP的起始地址，起始地址必须为4字节对齐
#define APP_ADDRESS    (uint32_t)0x08008000

extern volatile uint8_t g_timer_1s_flag;


#endif

