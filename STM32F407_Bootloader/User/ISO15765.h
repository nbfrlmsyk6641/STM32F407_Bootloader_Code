#ifndef ISO15765_H
#define ISO15765_H

# include "main.h"

// ISO 15765-2 帧类型定义
#define ISOTP_FRAME_SF      0x00       // 单帧 (Single Frame)
#define ISOTP_FRAME_FF      0x10       // 首帧 (First Frame)
#define ISOTP_FRAME_CF      0x20       // 连续帧 (Consecutive Frame)
#define ISOTP_FRAME_FC      0x30       // 流控帧 (Flow Control)

// ISO 15765-2 流控帧子类型定义
#define ISOTP_FC_CTS        0x00       // 继续发送 (Clear To Send)
#define ISOTP_FC_WT         0x01       // 等待 (Wait)
#define ISOTP_FC_OVFLW      0x02       // 溢出 (Overflow)

// UDS单次传输最大数据块定义
#define ISOTP_MAX_BUF_SIZE  4096

// ISO-TP等待超时
#define ISOTP_TIMEOUT_N_CR   2000

// 数据包传输状态机枚举
typedef enum {
    ISOTP_IDLE,                       // 空闲状态，等待新消息
    ISOTP_RX_WAIT_CF,                 // 已收到首帧，正在接收连续帧
    ISOTP_RX_COMPLETE,                // 接收完成 (一整包数据已就绪)
    ISOTP_RX_ERROR                    // 发生错误 (如序列号错误、溢出)
} IsoTpState_t;

// ISO 15765-2 接收上下文结构体
typedef struct {
    // --- 状态变量 ---
    volatile IsoTpState_t state; 
    
    // --- 接收控制 ---
    uint8_t  rx_buffer[ISOTP_MAX_BUF_SIZE]; // 接收大缓冲区 (RAM)
    uint16_t rx_total_len;                  // 总长度 (从 FF 帧解析)
    uint16_t rx_count;                      // 当前已接收字节数
    uint8_t  expected_sn;                   // 下一个期望的序列号 (0-15)
    
    // --- 发送控制 (流控参数) ---
    uint8_t  st_min;                        // 最小间隔时间 (建议 0-5ms)
    uint8_t  block_size;                    // 块大小 (0表示不分块)
    uint8_t  bs_counter;

    // --- 网络层定时参数 ---
    int32_t  timer_n_cr;

} IsoTpLink_t;

// 函数声明
void ISOTP_Init(void);
void ISOTP_Receive_Handler(CanRxMsg *msg);
void ISOTP_Transmit_SF(uint32_t id, uint8_t* data, uint8_t len);
uint8_t ISOTP_IsReceiveComplete(void);
uint8_t ISOTP_IsError(void);
uint8_t* ISOTP_GetRxBuffer(void);
uint16_t ISOTP_GetRxLength(void);
void ISOTP_Timer_NCr(void);
void ISOTP_ErrorStatus(void);


#endif /* __ISO15765_H */



