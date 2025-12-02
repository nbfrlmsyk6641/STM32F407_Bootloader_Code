#ifndef __ISO14229_H
#define __ISO14229_H

// App中用到的UDS会话定义
#define UDS_SESSION_DEFAULT       0x01
#define UDS_SESSION_PROGRAMMING   0x02
#define UDS_SESSION_EXTENDED      0x03

// 诊断设备的物理请求ID
#define UDS_PHYSICAL_REQUEST_ID   0x7E0

void UDS_Init(void);
void ISO15765_CAN(void);
void UDS_Process(void);

# endif /* __ISO14229_H */
