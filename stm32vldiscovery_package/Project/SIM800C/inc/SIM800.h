#ifndef __SIM800C_H__
#define __SIM800C_H__	 
#include "sys.h"


//存储PCB_ID的数组（也就是SIM卡的ICCID）
#define LENGTH_ICCID_BUF 20     								//ICCID的长度是20个字符
extern char ICCID_BUF[LENGTH_ICCID_BUF];
#define OFFSET_ICCID 2                          //

//存储设备重发命令的数组
#define LENGTH_RESEND 35
extern char Resend_Buffer[LENGTH_RESEND];

//存储设备登陆命令的数组
#define LENGTH_LOGIN 80
extern char Login_Buffer[LENGTH_LOGIN];

//存储心跳包的数组
#define LENGTH_HEART 50
extern char Heart_Buffer[LENGTH_HEART];

//存储业务指令回文的数组
#define LENGTH_ENABLE 50
extern char Enbale_Buffer[LENGTH_ENABLE];

//存储业务执行完成指令的数组
#define LENGTH_DEVICE_OK 50
extern char Device_OK_Buffer[LENGTH_DEVICE_OK];

/*********WJ*********/
bool SIM800_Check_Cmd(u8 *str);
u8 	SIM800_Send_Cmd(u8 *cmd,u8 *ack,u16 waittime);
void Clear_buffer(char* buffer,u16 length);
u8 Check_Xor_Sum(char* pBuf, u16 len);

u8 	Check_Module(void);
u8 	Disable_Echo(void);
u8 	Check_SIM_Card(void);
u8 	Check_CSQ(void);
u8 	Get_ICCID(void);


u8 	SIM800_GPRS_Adhere(void);
u8	SIM800_GPRS_Set(void);
u8 	SIM800_GPRS_Dispaly_IP(void);
u8 	SIM800_GPRS_CIPSHUT(void);
u8 	SIM800_GPRS_CGCLASS(void);
u8 	SIM800_GPRS_CGDCONT(void);

u8 	Link_Server_Echo(void);
u8 	Link_Server_AT(u8 mode,const char* ipaddr,const char *port);

u8 	Send_Data_To_Server(char* data);
u8 	Receive_Data_From_USART(void);

u8 	SIM800_GPRS_ON(void);
u8	SIM800_GPRS_OFF(void);
void SIM800_POWER_ON(void);
void SIM800_POWER_OFF(void);
void SIM800_PWRKEY_ON(void);
void SIM800_PWRKEY_OFF(void);

void SIM800_GPRS_Restart(void);
void SIM800_Powerkey_Restart(void);
void SIM800_Power_Restart(void);


u8 SIM800_Link_Server(void);
u8 SIM800_Link_Server_AT(void);
u8 SIM800_Link_Server_Powerkey(void);

void Get_Login_Data(void);
u8 Send_Login_Data(void);
u8 Send_Login_Data_Normal(void);
u8 Send_Login_Data_GPRS(void);
u8 Send_Login_Data_Powerkey(void);
u8 Send_Login_Data_To_Server(void);

void Get_Heart_Data(void);
u8 Send_Heart_Data(void);
u8 Send_Heart_Data_Normal(void);
u8 Send_Heart_Data_GPRS(void);
u8 Send_Heart_Data_Powerkey(void);
u8 Send_Heart_Data_To_Server(void);

void Get_Resend_Data(void);
u8 Send_Resend_Data(void);
u8 Send_Resend_Data_Normal(void);
u8 Send_Resend_Data_GPRS(void);
u8 Send_Resend_Data_Powerkey(void);
u8 Send_Resend_Data_To_Server(void);

void Get_Enable_Data(void);
u8 Send_Enable_Data(void);
u8 Send_Enable_Data_Normal(void);
u8 Send_Enable_Data_GPRS(void);
u8 Send_Enable_Data_Powerkey(void);
u8 Send_Enable_Data_To_Server(void);

void Get_Device_OK_Data(void);
u8 Send_Device_OK_Data(void);
u8 Send_Device_OK_Data_Normal(void);
u8 Send_Device_OK_Data_GPRS(void);
u8 Send_Device_OK_Data_Powerkey(void);
u8 Send_Device_OK_Data_To_Server(void);

void Enable_Device(u8 mode);





/*********WJ*********/
enum
{ 
	CMD_LOGIN = 0x01,
	CMD_HB = 0x02,
	CMD_ENABLE_DEVICE = 0x03,
	CMD_CLOSE_DEVICE = 0x04,
	CMD_NONE,
};

enum
{ 
	CMD_ACK_OK = 0,                 //USART3_RX_STA置位，返回的数据正确
	CMD_ACK_NOK = 1,               //USART3_RX_STA置位，返回的数据不正确
	CMD_ACK_DISCONN = 2,       //USART3_RX_STA置位，返回的数据表明掉线
	CMD_ACK_NONE,                  //USART3_RX_STA没有置位
};

#endif





