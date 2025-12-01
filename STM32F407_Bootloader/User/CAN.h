# ifndef CAN_H
# define CAN_H

// 环形缓冲器定义

#define RX_RING_BUFFER_SIZE  256 

typedef struct {
    CanRxMsg buffer[RX_RING_BUFFER_SIZE];  
    volatile uint16_t head;                
    volatile uint16_t tail;                
} RingBuffer_t;

// 函数声明
uint8_t CAN_RingBuffer_Read(CanRxMsg* msg);
void CAN_1_Init(void);
uint8_t CAN1_Transmit_TX(uint32_t ID, uint8_t Length, uint8_t *Data);
uint8_t CAN1_ReceiveFlag(void);
void CAN1_Receive_RX(uint32_t *ID, uint8_t *Length, uint8_t *Data);

# endif /* CAN_H */

