# include "main.h"

#define STORE_START_ADDRESS ((uint32_t)0x080E0000)
#define STORE_COUNT		     512
#define FLASH_SECTOR_11      FLASH_Sector_11

// 定义1KB大小的数组，数组大小1KB，一般就对应每次写入就是1KB，512个16位数据
uint16_t Store_Data[STORE_COUNT];

uint32_t FLASH_ReadWord(uint32_t Address)
{
    // 从指定地址的寄存器中读取一个字（32位）数据
    return*((__IO uint32_t*)Address);
}

uint16_t FLASH_ReadHalfWord(uint32_t Address)
{
    // 从指定地址的寄存器中读取半个字（16位）数据
    return*((__IO uint16_t*)Address);
}

uint8_t FLASH_ReadByte(uint32_t Address)
{
    // 从指定地址的寄存器中读取一个字节（8位）数据
    return*((__IO uint8_t*)Address);
}

void MyFLASH_EraseAllSectors(void)
{
    FLASH_Unlock();

    // 擦除所有页,慎用，MCU全擦完就变砖
    FLASH_EraseAllSectors(VoltageRange_3);

    FLASH_Lock();
}

void MyFLASH_EraseSectors(uint32_t Sector)
{
    FLASH_Unlock();

    // 擦除指定页，Flash的擦除规则是必须整个扇区擦除，但写入可以指定地址指定大小写入
    FLASH_EraseSector(Sector, VoltageRange_3);

    FLASH_Lock();
}

// 考虑到STM32必须整页擦除的特性，在后续需要具体修改某个地址的某个数值时，通常先整页读出到SRAM，修改后再重新写入

void FLASH_WriteWord(uint32_t Address, uint32_t Data)
{
    FLASH_Unlock();

    // 写入一个字（32位，4字节）数据
    FLASH_ProgramWord(Address, Data);

    FLASH_Lock();
}

void FLASH_WriteHalfWord(uint32_t Address, uint16_t Data)
{
    FLASH_Unlock();

    // 写入半个字（16位，2字节）数据
    FLASH_ProgramHalfWord(Address, Data);

    FLASH_Lock();
}

FLASH_Status FLASH_Write_Buffer(uint32_t Address, uint16_t* pData, uint16_t Count)
{
    FLASH_Status flash_status = FLASH_COMPLETE;
    uint16_t i = 0;

    FLASH_Unlock();

    for(i = 0; (i < Count) && (flash_status == FLASH_COMPLETE); i++)
    {
        // 在循环中，只执行写入操作
        flash_status = FLASH_ProgramHalfWord(Address + i * 2, pData[i]);
    }

    FLASH_Lock();

    return flash_status;
}

void FLASH_Store_Init(void)
{
    uint16_t i;

    // 1. 先将Flash中的数据读入RAM
    for (i = 0; i < STORE_COUNT; i ++)
    {
        Store_Data[i] = FLASH_ReadHalfWord(STORE_START_ADDRESS + i * 2);
    }

    // 2. 在RAM中检查标志位
    if (Store_Data[0] != 0xA5A5)
    {
        // 3. 如果标志位不对，在RAM中准备好初始化数据
        Store_Data[0] = 0xA5A5;
        for (i = 1; i < STORE_COUNT; i ++)
        {
            Store_Data[i] = 0x0000;
        }
        
        // 4. 一次性将准备好的数据写入Flash
        FLASH_Store_Save();
    }
}

void FLASH_Store_Save(void)
{
	MyFLASH_EraseSectors(FLASH_SECTOR_11);

    FLASH_Write_Buffer(STORE_START_ADDRESS, Store_Data, STORE_COUNT);
}

void FLASH_Store_Clear(void)
{
    uint16_t i;

	for (i = 1; i < STORE_COUNT; i ++)
	{
		Store_Data[i] = 0x0000;
	}

	FLASH_Store_Save();
}

