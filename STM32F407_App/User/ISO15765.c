# include "ISO15765.h"
#include <string.h>

// 定义上下文对象
IsoTpLink_t g_isotp;

// MCU端发送至诊断设备的CAN ID
#define ISOTP_TX_ID   0x7E8

// 流控帧发送函数，流控帧格式：30 + BS + STmin
static void ISOTP_Send_FC(void)
{
    uint8_t tx_data[8] = {0};
    
    // Byte 0: PCI=0x30 (FC类型), 低4位 FS=0 (CTS, 继续发送)
    tx_data[0] = 0x30; 
    
    // Byte 1: BS (Block Size) = 0 (不分块，一次发完)
    tx_data[1] = 0x00; 
    
    // Byte 2: STmin (最小间隔) = 2ms 
    tx_data[2] = 0x02; 
    
    // 发送报文
    CAN1_Transmit_TX(ISOTP_TX_ID, 8, tx_data);
}

// 协议初始化函数
void ISOTP_Init(void)
{
    g_isotp.state = ISOTP_IDLE;
    g_isotp.rx_count = 0;
    g_isotp.rx_total_len = 0;
    g_isotp.expected_sn = 0;

    // 清空缓冲区
    memset(g_isotp.rx_buffer, 0, ISOTP_MAX_BUF_SIZE);
}

// 单帧发送函数
void ISOTP_Transmit_SF(uint32_t id, uint8_t* data, uint8_t len)
{
    uint8_t tx_data[8] = {0x00}; // 默认填充 0x00
    
    if (len > 7) return; // 单帧最多发7字节
    
    // Byte 0: PCI=0x00 (SF) | Len
    tx_data[0] = len;
    
    // 复制数据到 Byte 1~7，所以调用这个函数要注意，data的长度不能超过7
    memcpy(&tx_data[1], data, len);
    
    // 发送
    CAN1_Transmit_TX(id, 8, tx_data);
}

// 接收处理函数
void ISOTP_Receive_Handler(CanRxMsg *msg)
{
    uint8_t pci_type;
    uint8_t payload_len;
    uint8_t sn;
    uint16_t i;
    uint16_t remaining_bytes;
    uint8_t copy_len;
    
    // 1. 获取 PCI 类型 (高4位)
    pci_type = msg->Data[0] & 0xF0;
    
    switch (pci_type)
    {
        // ---------------------------------------------------------
        // Case A: 单帧 (SF - Single Frame) -> 0x00
        // ---------------------------------------------------------
        case ISOTP_FRAME_SF:
            // 解析长度 (低4位)
            payload_len = msg->Data[0] & 0x0F;
            
            if (payload_len > 7) return; // 错误保护
            
            // 提取数据,总长度、已接收长度、数据内容
            g_isotp.rx_total_len = payload_len;
            g_isotp.rx_count = payload_len;
            
            for (i = 0; i < payload_len; i++)
            {
                g_isotp.rx_buffer[i] = msg->Data[1 + i];
            }
            
            // 状态直接变更为 完成
            g_isotp.state = ISOTP_RX_COMPLETE;
            break;

        // ---------------------------------------------------------
        // Case B: 首帧 (FF - First Frame) -> 0x10
        // ---------------------------------------------------------
        case ISOTP_FRAME_FF:
            // 解析总长度 (Byte0 低4位 + Byte1)
            // 例如: 10 80 -> 0x080 = 128 字节
            g_isotp.rx_total_len = ((msg->Data[0] & 0x0F) << 8) | msg->Data[1];
            
            // 保护：如果数据太大，超过了我们要开辟的 4KB RAM，必须报错
            if (g_isotp.rx_total_len > ISOTP_MAX_BUF_SIZE)
            {
                g_isotp.state = ISOTP_RX_ERROR;
                return;
            }
            
            // 提取 FF 帧自带的前 6 个字节数据
            for (i = 0; i < 6; i++)
            {
                g_isotp.rx_buffer[i] = msg->Data[2 + i];
            }
            g_isotp.rx_count = 6;
            
            // 初始化连续帧计数器 (首帧FF 之后期望收到 SN=1)
            g_isotp.expected_sn = 1;
            
            // 切换状态：等待连续帧
            g_isotp.state = ISOTP_RX_WAIT_CF;
            
            // 发送流控帧 (FC)，告诉上位机可以开始发包
            ISOTP_Send_FC();
            break;

        // ---------------------------------------------------------
        // Case C: 连续帧 (CF - Consecutive Frame) -> 0x20
        // ---------------------------------------------------------
        case ISOTP_FRAME_CF:
            // 如果当前没在等 CF，说明这是乱入的帧，忽略
            if (g_isotp.state != ISOTP_RX_WAIT_CF) return;
            
            // 检查序列号 (SN)，是否为期望的值
            sn = msg->Data[0] & 0x0F;
            
            if (sn != g_isotp.expected_sn)
            {
                // 序列号错误，丢包，报错误码
                g_isotp.state = ISOTP_RX_ERROR;
                return;
            }
            
            // 计算本次要复制多少字节
            // 通常是 7 字节，但最后一帧可能少于 7 字节
            remaining_bytes = g_isotp.rx_total_len - g_isotp.rx_count;
            copy_len = (remaining_bytes > 7) ? 7 : remaining_bytes;
            
            // 复制数据到大缓冲区
            for (i = 0; i < copy_len; i++)
            {
                g_isotp.rx_buffer[g_isotp.rx_count + i] = msg->Data[1 + i];
            }
            g_isotp.rx_count += copy_len;
            
            // 更新期望的下一个 SN (0-15 循环)
            g_isotp.expected_sn = (g_isotp.expected_sn + 1) % 16;
            
            // 检查是否接收完毕
            if (g_isotp.rx_count >= g_isotp.rx_total_len)
            {
                g_isotp.state = ISOTP_RX_COMPLETE;
            }
            break;
            
        default:
            break;
    }
}
