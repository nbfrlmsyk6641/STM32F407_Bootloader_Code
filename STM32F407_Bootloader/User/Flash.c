# include "main.h"

/********************************************************************************
 *
 * STM32F407VET6 Flash Memory Map
 * (Total Size: 512 KB)
 *
 * +-----------------------------------------------------------------------------+
 * | Partition         | Start Address | End Address   | Size                 |
 * |-------------------|---------------|---------------|----------------------|
 * | Bootloader        | 0x08000000    | 0x08007FFF    |  32 KB (0x00008000)  |
 * |-------------------|---------------|---------------|----------------------|
 * | Application (App) | 0x08008000    | 0x08052FFF    | 300 KB (0x0004B000)  |
 * |-------------------|---------------|---------------|----------------------|
 * | Download Area     | 0x08053000    | 0x0807FFFF    | 180 KB (0x0002D000)  |
 * +-----------------------------------------------------------------------------+
 *
 ********************************************************************************/

static const uint32_t APP_SECTOR_LIST[5] = {
    FLASH_SECTOR_2, 
    FLASH_SECTOR_3, 
    FLASH_SECTOR_4, 
    FLASH_SECTOR_5, 
    FLASH_SECTOR_6
};

static const uint32_t APP_SECTOR_START_ADDR[5] = {
    APPLICATION_SECTOR_2, 
    APPLICATION_SECTOR_3, 
    APPLICATION_SECTOR_4, 
    APPLICATION_SECTOR_5, 
    APPLICATION_SECTOR_6  
};



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

    // 擦除所有页，慎用，MCU全擦完就变砖
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

uint8_t IAP_Erase_App_Sectors(uint32_t firmware_size)
{
    uint32_t i = 0;
    uint32_t firmware_end_address = 0;
    
    // 1. 计算固件的结束地址，解锁Flash，清除标志位
    firmware_end_address = APPLICATION_START_ADDRESS + firmware_size - 1;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);

    // 2. 循环遍历所有App可能占用的扇区
    for (i = 0; i < APP_SECTOR_COUNT; i++)
    {
        // 3. 检查固件的结束地址是否落在了这个扇区或更远的扇区
        if (firmware_end_address >= APP_SECTOR_START_ADDR[i])
        {
            // 如果落在了这个扇区就需要擦掉这个扇区
            if (FLASH_EraseSector(APP_SECTOR_LIST[i], VoltageRange_3) != FLASH_COMPLETE)
            {
                // 擦除失败！
                FLASH_Lock();
                return 1; 
            }
        }
    }

    FLASH_Lock();
    return 0; // 所有相关扇区均擦除成功
}

uint32_t IAP_Calculate_CRC_On_Flash(uint32_t start_addr, uint32_t size)
{
    uint32_t i = 0;
    uint32_t word_count = 0;
    uint32_t temp_word = 0;
    uint8_t remaining_bytes = 0;

    // 1. 开启CRC外设的时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);
    
    // 2. 复位CRC单元 (清除上一次的计算结果)
    CRC_ResetDR();

    // 3. 计算有多少个完整的32位字 (Word)
    word_count = size / 4;
    remaining_bytes = size % 4; // 计算剩余不足4字节的字节数

    // 4. 循环计算所有完整的字
    for (i = 0; i < word_count; i++)
    {
        // 从Flash中读取一个32位字
        temp_word = FLASH_ReadWord(start_addr + i * 4); 
        // 将这个字送入硬件CRC单元进行计算
        CRC_CalcCRC(temp_word);
    }

    // 5. 处理剩余的字节 (如果固件大小不是4的整数倍)
    if (remaining_bytes > 0)
    {
        temp_word = 0; // 清零
        // 读取剩余的 1, 2, 或 3 个字节
        for (i = 0; i < remaining_bytes; i++)
        {
            // 按字节读取，并移位到正确的位置 (小端模式)
            temp_word |= (uint32_t)(FLASH_ReadByte(start_addr + word_count * 4 + i)) << (i * 8);
        }
        
        CRC_CalcCRC(temp_word);
    }

    // 6. 返回最终的CRC计算结果
    return CRC_GetCRC();
}

