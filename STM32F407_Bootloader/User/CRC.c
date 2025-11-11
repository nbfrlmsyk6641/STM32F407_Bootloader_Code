#include "main.h"

uint32_t IAP_Software_Calculate_CRC(const uint8_t * pData, uint32_t uiDataSize)
{
    uint32_t i_index;
    uint32_t j_index;
    uint32_t uiCrc = 0xFFFFFFFF; // 1. 初始值
    const uint32_t uiPolynomial = 0x04C11DB7; // (这是一个反转的多项式，用于匹配Reflect In/Out)

    for (i_index = 0; i_index < uiDataSize; i_index++)
    {
        uiCrc ^= pData[i_index]; // 2. 输入数据

        for (j_index = 0; j_index < 8; j_index++)
        {
            if (uiCrc & 0x00000001)
            {
                uiCrc = (uiCrc >> 1) ^ 0xEDB88320; // 0xEDB88320 是 0x04C11DB7 的反转
            }
            else
            {
                uiCrc = (uiCrc >> 1);
            }
        }
    }

    // 3. 最终异或
    return (uiCrc ^ 0xFFFFFFFF);
}

/**
 * @brief  IAP_Calculate_CRC_On_Flash (软件版)
 * @note   这是 IAP_STATE_VERIFY 要调用的接口
 *
 * @param  start_address: Flash 起始地址
 * @param  size_in_bytes: 要计算的总字节数 (g_firmware_total_size)
 * @return 32-bit CRC (PKZIP 标准)
 */
uint32_t IAP_Calculate_CRC_On_Flash(uint32_t start_address, uint32_t size_in_bytes)
{
    // 1. (重要!) 确保CRC时钟是关闭的，我们不再需要它
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, DISABLE);
    
    // 2. 将 Flash 地址 (一个指针) 和 大小 传递给软件CRC函数
    return IAP_Software_Calculate_CRC((const uint8_t*)start_address, size_in_bytes);
}


