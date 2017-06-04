#ifndef _TIMER_H
#define _TIMER_H
#include "sys.h"
	

//用来设置设备等待服务器的回文的最长时间（单位是分钟）
#define TIME_ECHO 1
#define NUMBER_TIME_ECHO_50_MS (TIME_ECHO*60*1000/50)  	    //1MIN

//用来设置心跳包的发送间隔（单位是分钟）
//常规的心跳包间隔是1分钟到10分钟
#define TIME_HEART 1
#define NUMBER_TIME_HEART_50_MS (TIME_HEART*60*1000/50)  	  //1MIN



#define OFFSET_Connec 30                      	//TRVBP00,000,UBICOM_AUTH_INFOR,0001,070,X,# 
#define OFFSET_TDS    35                      	//TRVBP00,000,UBICOM_AUTH_INFOR,0001,070,X,# 


extern u8 Flag_Time_Out_Comm;                       //通信的超时机制生效时置位该变量
extern u8 Flag_Time_Out_Reconnec_Normal;            //传输正常时重新链接服务器的超时生效时置位该变量
extern u8 Flag_Time_Out_AT;                         //模块链接服务器过程中出现异常后的定时时间到就置位该变量
extern u8 Flag_Auth_Infor;  												//收到身份认证命令时置位该变量
extern u8 Flag_Login;  												      //收到登陆信息回文时置位该变量
extern u8 Flag_Reported_Data;   										//收到上报数据命令时置位该变量（现阶段是上报TDS值和温度值）
extern u8 Flag_Resend; 														  //接收到重发命令时置位该变量
extern u8 Flag_Check_error;                         //对接收到的信息进行校验，如果校验不通过就置位该变量
extern u8 Flag_ACK_Resend;              					  //设备端程序利用该变量来判定要重发的是那条命令
extern u8 Flag_Comm_OK;                             //整个通信过程顺利结束时置位该变量
extern u8 Flag_Need_Reset;

////////////////////////////////
extern u32 Count_Wait_Echo;         //等待服务器回文的计数值，一个单位代表50ms
extern u8  Flag_Wait_Echo;          //需要等待服务器回文时，置位这个变量

extern u32 Count_Local_Time_Dev_01;        //开启设备1，本地计时的值，一个单位代表50ms
extern u32 Count_Local_Time_Dev_02;        //开启设备2，本地计时的值，一个单位代表50ms
extern u32 Count_Local_Time_Dev_03;        //开启设备3，本地计时的值，一个单位代表50ms
extern u32 Count_Local_Time_Dev_04;        //开启设备4，本地计时的值，一个单位代表50ms
extern u8  Flag_Local_Time_Dev_01;         //设备1需要本地计时，置位这个变量
extern u8  Flag_Local_Time_Dev_02;         //设备2需要本地计时，置位这个变量
extern u8  Flag_Local_Time_Dev_03;         //设备3需要本地计时，置位这个变量
extern u8  Flag_Local_Time_Dev_04;         //设备4需要本地计时，置位这个变量
extern u8  Flag_Local_Time_Dev_01_OK;      //设备1本地计时完成，置位这个变量
extern u8  Flag_Local_Time_Dev_02_OK;      //设备2本地计时完成，置位这个变量
extern u8  Flag_Local_Time_Dev_03_OK;      //设备3本地计时完成，置位这个变量
extern u8  Flag_Local_Time_Dev_04_OK;      //设备4本地计时完成，置位这个变量

extern u8  Flag_Send_Heart_OK;      //再次发送心跳包的计时到，置位这个变量

extern u8  Flag_Receive_Heart;      //收到服务器的心跳包回文时，置位这个变量
extern u8  Flag_Receive_Login;  										 //收到登陆信息回文时置位该变量

extern u32 Count_Send_Heart;        //再次发送心跳包的计数值，一个单位代表50ms
extern u8  Flag_Send_Heart;         //需要等待再次发送心跳包时，置位这个变量

extern u8  Flag_ACK_Echo;           //设备端程序利用该变量来判定等待回文超时的是那条指令

extern u8  Flag_Receive_Resend; 		//接收到重发命令时置位该变量

extern u8  Flag_Receive_Enable; 		//接收到开启设备命令时置位该变量

extern u8  Flag_Receive_Device_OK;  //接收到设备运行结束回文时置位该变量

extern u8 Flag_Received_SIM800C_CLOSED;
extern u8 Flag_Received_SIM800C_DEACT;

void TIM6_Int_Init(u16 arr,u16 psc);
void TIM7_Int_Init(u16 arr,u16 psc);




#endif
