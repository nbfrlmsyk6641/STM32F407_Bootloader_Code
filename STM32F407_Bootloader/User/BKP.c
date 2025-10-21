# include "main.h"
#include "stm32f4xx_pwr.h"
#include "stm32f4xx_rcc.h"

void BKP_Init(void)
{
    // 使能电源和备份接口时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

    // 使能对备份寄存器的写访问
    PWR_BackupAccessCmd(ENABLE);
}

void BKP_WriteRegister(uint32_t registerIndex, uint32_t data)
{
    // 使能对备份寄存器的写访问
    PWR_BackupAccessCmd(ENABLE);

    // 写入数据到指定的备份寄存器
    RTC_WriteBackupRegister(registerIndex, data);
}

uint32_t BKP_ReadRegister(uint32_t registerIndex)
{
    // 使能对备份寄存器的写访问
    PWR_BackupAccessCmd(ENABLE);

    // 从指定的备份寄存器读取数据
    return RTC_ReadBackupRegister(registerIndex);
}


