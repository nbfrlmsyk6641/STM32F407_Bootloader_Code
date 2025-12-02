#include "main.h"

// 定义UDS协议上下文变量
typedef struct {
    uint8_t  current_session;      // 当前会话
    uint8_t  pre_prog_condition;   // 预编程条件标志
    uint32_t s3_server_timer;      // S3 定时器 （暂未实现，预留）
} UDS_Context_t;

static UDS_Context_t g_uds_ctx;

// UDS否定响应发送函数
static void UDS_Send_NRC(uint8_t sid, uint8_t nrc)
{
    uint8_t resp[3];
    resp[0] = 0x7F; // Negative Response
    resp[1] = sid;  // Service ID
    resp[2] = nrc;  // Error Code
    ISOTP_Transmit_SF(0x7E8, resp, 3);
}

// UDS协议初始化函数
void UDS_Init(void)
{
    g_uds_ctx.current_session = UDS_SESSION_DEFAULT;
    g_uds_ctx.pre_prog_condition = 0;
    g_uds_ctx.s3_server_timer = 0;
}

// 传输层接收CAN报文处理函数
void ISO15765_CAN(void)
{
    CanRxMsg msg;
    uint32_t id;

    // 检查环形缓冲器是否有新报文
    if(CAN_RingBuffer_Read(&msg) == 1)
    {
        if (msg.IDE == CAN_Id_Standard)
        {
            id = msg.StdId;
        }
        else
        {
            id = msg.ExtId;
        }
        
        if (id == UDS_PHYSICAL_REQUEST_ID)
        {
            // UDS服务，调用ISO-TP处理
            ISOTP_Receive_Handler(&msg);
        }
    }

}

// UDS应用层处理函数
void UDS_Process(void)
{
    // 检查 ISO-TP 是否收到了完整数据包
    if (ISOTP_IsReceiveComplete() == 1)
    {
        uint8_t* rx_buf = ISOTP_GetRxBuffer();

        uint8_t sid = rx_buf[0];
        uint8_t sub_func = 0;
        uint8_t* p_data = &rx_buf[1];
        
        if (ISOTP_GetRxLength() > 1)
        {
            sub_func = rx_buf[1] & 0x7F;
        }

        // --- 核心业务分发 ---
        switch (sid)
        {
            // --------------------------------------------------
            // Service 0x10: Session Control
            // --------------------------------------------------
            case 0x10:
            {
                if (sub_func == UDS_SESSION_DEFAULT)
                {
                    g_uds_ctx.current_session = UDS_SESSION_DEFAULT;
                    g_uds_ctx.pre_prog_condition = 0;
                    
                    uint8_t resp[] = {0x50, 0x01, 0x00, 0x32, 0x01, 0xF4};
                    ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                }
                else if (sub_func == UDS_SESSION_EXTENDED)
                {
                    g_uds_ctx.current_session = UDS_SESSION_EXTENDED;
                    
                    uint8_t resp[] = {0x50, 0x03, 0x00, 0x32, 0x01, 0xF4};
                    ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                }
                else if (sub_func == UDS_SESSION_PROGRAMMING)
                {
                    // 检查跳转条件
                    if (g_uds_ctx.current_session == UDS_SESSION_EXTENDED && 
                        g_uds_ctx.pre_prog_condition == 1)
                    {
                        uint8_t resp[] = {0x50, 0x02, 0x00, 0x32, 0x01, 0xF4};

                        ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                        
                        BKP_WriteRegister(BKP_FLAG_REGISTER, 0xAaBbCcDd);

                        NVIC_SystemReset();
                    }
                    else
                    {
                        UDS_Send_NRC(0x10, 0x22); // Conditions Not Correct
                    }
                }
                else
                {
                    UDS_Send_NRC(0x10, 0x12); // SubFunc Not Supported
                }
            }
            break;

            // --------------------------------------------------
            // Service 0x31: Routine Control (Check)
            // --------------------------------------------------
            case 0x31:
            {
                if (g_uds_ctx.current_session != UDS_SESSION_EXTENDED)
                {
                    UDS_Send_NRC(0x31, 0x7F); // Service Not Supported in Active Session
                }
                else if (p_data[0]==0x01 && p_data[1]==0xFF && p_data[2]==0x00)
                {
                    g_uds_ctx.pre_prog_condition = 1; 
                    
                    uint8_t resp[] = {0x71, 0x01, 0xFF, 0x00};
                    ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
                }
                else
                {
                    UDS_Send_NRC(0x31, 0x31); // Request Out Of Range
                }
            }
            break;

            // --------------------------------------------------
            // Service 0x3E: Tester Present
            // --------------------------------------------------
            case 0x3E:
            {
                uint8_t resp[] = {0x7E, 0x00};
                ISOTP_Transmit_SF(0x7E8, resp, sizeof(resp));
            }
            break;

            default:
                UDS_Send_NRC(sid, 0x11); // Service Not Supported
                break;
        }

        // 处理完毕，重置 ISO-TP
        ISOTP_Init();
    }
    else if (ISOTP_IsError() == 1)
    {
        // 如果发生错误，重置 ISO-TP
        ISOTP_Init();
    }
}
