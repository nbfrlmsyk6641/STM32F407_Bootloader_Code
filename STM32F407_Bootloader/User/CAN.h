# ifndef CAN_H
# define CAN_H

void CAN_1_Init(void);
uint8_t CAN1_Transmit_TX(uint32_t ID, uint8_t Length, uint8_t *Data);
uint8_t CAN1_ReceiveFlag(void);
void CAN1_Receive_RX(uint32_t *ID, uint8_t *Length, uint8_t *Data);

# endif /* CAN_H */

