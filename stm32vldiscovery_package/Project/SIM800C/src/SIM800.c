#include "SIM800.h"	 	 	
#include "string.h"  
#include "stdio.h"	
#include "usart.h" 
#include "usart3.h" 
#include "timer.h"
#include "delay.h"
#include <stdlib.h>
#include "device.h"

extern u8 result_Send_Data;
extern u8 count_Send_Data;
extern u8 current_cmd;
#define TOTAL_SEND_DATA 3
#define COUNT_AT 10

u8 mode = 0;				//0,TCP连接;1,UDP连接
const char *modetbl[2] = {"TCP","UDP"};//连接模式

//const char  *ipaddr = "tuzihog.oicp.net";
//const char  *port = "28106";

//const char  *ipaddr = "42.159.117.91";
const char  *ipaddr = "116.62.187.167";
//const char  *ipaddr = "42.159.107.250";
const char  *port = "8090";



//模块链接服务器过程中,AT指令异常时置位该变量
u8 Flag_AT_Unusual = 0;

//存储PCB_ID的数组（也就是SIM卡的ICCID）
char ICCID_BUF[LENGTH_ICCID_BUF] = {0};

//存储设备重发命令的数组
char Resend_Buffer[LENGTH_RESEND] = {0};

//存储设备登陆命令的数组
char Login_Buffer[LENGTH_LOGIN] = {0};

//存储心跳包的数组
char Heart_Buffer[LENGTH_HEART] = {0};

//存储业务指令回文的数组
char Enbale_Buffer[LENGTH_ENABLE] = {0};

//存储业务执行完成指令的数组
char Device_OK_Buffer[LENGTH_DEVICE_OK] = {0};

char atcmd_ack[LENGTH_ATCMD_ACK] = {0};
bool need_ack_check = FALSE;
bool ack_ok = FALSE;

u8  Flag_SIM800C_In_Reset = 0; 

//SIM800发送命令后,检测接收到的应答
//str:期待的应答结果
//返回值:0,没有得到期待的应答结果
//其他,期待应答结果的位置(str的位置)
u8 SIM800_Check_Cmd(u8 *str)
{
	u8 ret = CMD_ACK_NONE;
	if(USART3_RX_STA&0X8000)		//接收到一次数据了
	{ 
		ret = CMD_ACK_NOK;
		USART3_RX_BUF[USART3_RX_STA&0X7FFF] = 0;//添加结束符
		BSP_Printf("recv: %s\r\n", USART3_RX_BUF);
		if(strstr((const char*)USART3_RX_BUF,(const char*)str)!=NULL)   //必须首先进行是否是正确回文判断
			ret = CMD_ACK_OK;
		else if((strstr((const char*)USART3_RX_BUF,"CLOSED")!=NULL) || (strstr((const char*)USART3_RX_BUF,"+PDP: DEACT")!=NULL))
			ret = CMD_ACK_DISCONN;
	} 
	return ret;
}

//SIM800发送命令
//cmd:发送的命令字符串(不需要添加回车了),当cmd<0XFF的时候,发送数字(比如发送0X1A),大于的时候发送字符串.
//ack:期待的应答结果,如果为空,则表示不需要等待应答
//waittime:等待时间(单位:10ms)
//返回值:0,发送成功(得到了期待的应答结果)
//       1,发送失败
u8 SIM800_Send_Cmd(u8 *cmd,u8 *ack,u16 waittime)
{
	u8 res = CMD_ACK_NONE; 
	Clear_Usart3();
	
	if(ack!=NULL)
	{
		ack_ok = FALSE;
		memset(atcmd_ack, 0, sizeof(atcmd_ack));
		strcpy(atcmd_ack, (char *)ack);
	}	
	
	if((u32)cmd <= 0XFF)
	{
		while((USART3->SR&0X40)==0);//等待上一次数据发送完成  
		USART3->DR = (u32)cmd;
	}
	else 
	{
		u3_printf("%s\r\n",cmd);//发送命令
	}

	if(ack&&waittime)		//需要等待应答
	{
		while(waittime!=0)	//等待倒计时
		{ 
			delay_ms(10);				
			if(USART3_RX_STA&0X8000)//接收到期待的应答结果
			{
				//得到了有效的回文信息，
				res = SIM800_Check_Cmd(ack);
				if((res == CMD_ACK_OK) || (res == CMD_ACK_DISCONN))   //收到正常的回文，或者断线都不需要再重试了
				{
					//后面的函数可能要处理得到的回文，所以这里不能调用函数Clear_Usart3
					break;
				} 
				//没有得到有效的回文信息，清零串口3，继续下次的接收
				else  //res == CMD_ACK_NOK
				{
					if(need_ack_check && ack_ok)
						break;
					//这里有可能清空服务器回文?	
					Clear_Usart3();
				}
			} 
			waittime--;
			//从各个角度避免等待，如果收到了 ack 就直接跳出
			//主要服务于ack = "SEND OK"
			if(need_ack_check && ack_ok)
				break;			
		}

		//如果在TIM6里发现有at回文
		if(need_ack_check && ack_ok)
			res = CMD_ACK_OK;
		
		if(waittime == 0)
		{
			//AT指令已经发出，只是在等待时间内，USART3_RX_STA没有置位或者得到的数据不包含ack
			//返回的是CMD_ACK_NONE 或者CMD_ACK_NOK
			Clear_Usart3();
		}
	}
	
	else   //不需要等待应答,这里暂时不添加相关的处理代码
	{
		;
	
	}
	return res;
} 



u8 Check_Module(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT","OK",100);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))   //只有这两种情况才有重复的意义
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))  //在正常AT 命令里基本上不可能返回"CLOSED" 吧 ，仅放在这里
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
	
}

//关闭回显
u8 Disable_Echo(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("ATE0","OK",200);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
	
}

//查看SIM是否正确检测到
u8 Check_SIM_Card(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CPIN?","OK",200);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}

u8 Check_OPS(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+COPS?","CHINA",500);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}


//查看天线质量
u8 Check_CSQ(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	u8 *p1 = NULL; 
	u8 *p2 = NULL;
	u8 p[50] = {0}; 
  	u8 signal=0;

	while(signal < 10)
	{
		delay_ms(2000);
		while(count != 0)
		{
			ret = SIM800_Send_Cmd("AT+CSQ","+CSQ:",200);
			if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
			{
				delay_ms(2000);
			}
			else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
				break;
			
			count--;
		}		
		
		if(ret == CMD_ACK_OK)
		{
			//AT指令已经指令完成，下面对返回值进行处理
			p1=(u8*)strstr((const char*)(USART3_RX_BUF),":");
			p2=(u8*)strstr((const char*)(p1),",");
			p2[0]=0;//加入结束符
			signal = atoi((const char *)(p1+2));
			//sprintf((char*)p,"信号质量:%s",p1+2);
			sprintf((char*)p,"信号质量:%d",signal);
			BSP_Printf("%s\r\n",p);
		}
		//AT指令的回文已经处理完成，清零
		Clear_Usart3();
	}	
	return ret;
}

//获取SIM卡的ICCID
//SIM卡的ICCID,全球唯一性，可以用作PCB的身份ID
//打印USART3_RX_BUF的內容 調試用途
		/*****  注意+号前面有两个空格
  +CCID: 1,"898602B8191650216485"

		OK
		****/
//这个函数还没有最终确认....
u8 Get_ICCID(void)
{
	u8 index = 0;
	u8 *p_temp = NULL;
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CCID","OK",200);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}

	if(ret == CMD_ACK_OK)	
	{
		//AT指令已经指令完成，下面对返回值进行处理
		//BSP_Printf("CCID:%s\r\n",USART3_RX_BUF);

		p_temp = USART3_RX_BUF;
		//提取ICCID信息到全局变量ICCID_BUF
		for(index = 0;index < LENGTH_ICCID_BUF;index++)
		{
			ICCID_BUF[index] = *(p_temp+OFFSET_ICCID+index);
		}
		//BSP_Printf("ICCID_BUF:%s\r\n",ICCID_BUF);
	}
	//AT指令的回文已经处理完成，清零
	Clear_Usart3();
	return ret;

}

u8 SIM800_GPRS_ON(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CIPSTART=\"TCP\",\"116.62.187.167\",\"8090\"","CONNECT OK",700);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;

}

//关闭GPRS的链接
u8 SIM800_GPRS_OFF(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CIPCLOSE=1","CLOSE OK",500);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}

//附着GPRS
u8 SIM800_GPRS_Adhere(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CGATT=1","OK",300);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}

//设置为GPRS链接模式
u8 SIM800_GPRS_Set(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CIPCSGP=1,\"CMNET\"","OK",300);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}

//设置接收数据显示IP头(方便判断数据来源)	
u8 SIM800_GPRS_Dispaly_IP(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CIPHEAD=1","OK",300);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}

//关闭移动场景 
u8 SIM800_GPRS_CIPSHUT(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CIPSHUT","SHUT OK",500);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}

//设置GPRS移动台类别为B,支持包交换和数据交换 
u8 SIM800_GPRS_CGCLASS(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CGCLASS=\"B\"","OK",300);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}


	//设置PDP上下文,互联网接协议,接入点等信息
u8 SIM800_GPRS_CGDCONT(void)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Send_Cmd("AT+CGDCONT=1,\"IP\",\"CMNET\"","OK",300);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
	
	Clear_Usart3();	
	return ret;
}

u8 Link_Server_AT(u8 mode,const char* ipaddr,const char *port)
{
	u8 count = COUNT_AT;
	u8 ret = CMD_ACK_NONE;
	char *temp1 = NULL;
	char *temp2 = NULL;
	char *temp3 = NULL;
	u8 p[256] = {0};
	u16 index = 0;
	
	if(mode)
	;
	else 
		;
		
  	sprintf((char*)p,"AT+CIPSTART=\"%s\",\"%s\",\"%s\"",modetbl[mode],ipaddr,port);	

	//发起连接
	//AT+IPSTART指令可能的回文是：CONNECT OK 和ALREADY CONNECT和CONNECT FAIL
	//这里先取三种可能回文的公共部分来作为判断该指令有正确回文的依据


	while(count != 0)
	{
		ret = SIM800_Send_Cmd(p,"CONNECT",1000);
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			delay_ms(2000);
		}
		else if((ret == CMD_ACK_OK) || (ret == CMD_ACK_DISCONN))
			break;
		
		count--;
	}
		
	//AT指令已经指令完成，下面对返回值进行处理
	if(ret == CMD_ACK_OK)
	{
		for(index = 0; index < 256; index++)
		{
			p[index]= USART3_RX_BUF[index];
		}
		temp1 = strstr((const char*)p,"CONNECT OK");
		temp2 = strstr((const char*)p,"ALREADY CONNECT");
		temp3 = strstr((const char*)p,"CONNECT FAIL");
					
		if(temp3 != NULL)
		{
			ret = CMD_ACK_NOK;
		}
	}
	Clear_Usart3();
	return ret;
}

u8 Send_Data_To_Server(char* data)
{
	u8 ret = CMD_ACK_NONE;
	BSP_Printf("准备开始发送数据\r\n");
	if((Flag_Received_SIM800C_CLOSED == 0xAA) || (Flag_Received_SIM800C_DEACT == 0xAA))
		ret = CMD_ACK_DISCONN;
	else
		ret = SIM800_Send_Cmd("AT+CIPSEND",">",500);
	
	if(ret==CMD_ACK_OK)		//发送数据
	{ 
		Clear_Usart3();   //下面这个相当于一个独立的send
		u3_printf("%s",data);
		delay_ms(100);
		need_ack_check = TRUE;
		ret=SIM800_Send_Cmd((u8*)0x1A,"SEND OK",3000);
		if(ret==CMD_ACK_OK)
		{
			//由于等待时间太长，收到Send OK的同时可能收到服务器发回的回文
			//因此不能在这里清空
			if(strstr((const char*)USART3_RX_BUF,"TRVBP")==NULL)
				Clear_Usart3();	
		}
		need_ack_check = FALSE;		
	  //delay_ms(1000); 
	}
	BSP_Printf("已完成一次发送: %d\r\n", ret);
	return ret;
}


u8 Receive_Data_From_USART(void)
{
	u8 res = 0xAA;
		
	if(USART3_RX_STA & 0X8000)		            //接收到一次数据了
	{ 
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]=0;	//添加结束符 
		if(strstr((const char*)USART3_RX_BUF,"TRVBP"))
		{
			res = 0;
		}
		else
		{
			res = 1;
		}
	}
	
	return res;
}



#if 0
u8 Check_Link_Status(void)
{
	u8 count = 0;

	while(SIM800_Send_Cmd("AT+CMSTATE","CONNECTED",500))//检测是否应答AT指令 
	{
		if(count < COUNT_AT)
		{
			count += 1;
			delay_ms(2000);			
		}
		else
		{
//AT指令已经尝试了COUNT_AT次，仍然失败，断电SIM800，开启TIME_AT分钟的定时，定时时间到，再次链接服务器
//目前代码中没有调用本函数，也没有对Flag_TIM6_2_S的判定代码，所以暂时屏蔽掉Flag_TIM6_2_S的赋值
			//Flag_TIM6_2_S = 0xAA;
			return 1;		
		}
	}	

		//AT指令的回文不需要处理，清零
	Clear_Usart3();
	return 0;

}
#endif

//开启2G模块的电源芯片，当做急停按钮来使用
void SIM800_POWER_ON(void)
{
 u8 i= 0;
	
 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 
	
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 ;				 
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		
 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
 GPIO_Init(GPIOB, &GPIO_InitStructure);	

 GPIO_SetBits(GPIOB,GPIO_Pin_8);	

 for(i = 0; i < 5; i++)
 {
	 delay_ms(1000);	
 }
}

//关闭2G模块的电源芯片，当做急停按钮来使用
void SIM800_POWER_OFF(void)
{
 u8 i= 0;

 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 
	
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 ;				 
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		
 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
 GPIO_Init(GPIOB, &GPIO_InitStructure);	
	


//电源芯片的失能
 GPIO_ResetBits(GPIOB,GPIO_Pin_8);	

 for(i = 0; i < 5; i++)
 {
	 delay_ms(1000);	
 }

}


//通过2G模块的PWRKEY来实现开关机
void SIM800_PWRKEY_ON(void)
{
 u8 i= 0;
	
 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 
	
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 ;				 
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		
 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
 GPIO_Init(GPIOB, &GPIO_InitStructure);	

 //PWRKEY的使能
 GPIO_SetBits(GPIOB,GPIO_Pin_9);	

 for(i = 0; i < 5; i++)
 {
	 delay_ms(1000);	
 }
	//开机控制引脚释放
 GPIO_ResetBits(GPIOB,GPIO_Pin_9);
 for(i = 0; i < 10; i++)
 {
	 delay_ms(1000);	
 }

}

//通过2G模块的PWRKEY来实现开关机
void SIM800_PWRKEY_OFF(void)
{
 u8 i= 0;
	
 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 
	
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 ;				 
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		
 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
 GPIO_Init(GPIOB, &GPIO_InitStructure);	

 //PWRKEY的使能
 GPIO_SetBits(GPIOB,GPIO_Pin_9);	

 for(i = 0; i < 5; i++)
 {
	 delay_ms(1000);	
 }
	//开机控制引脚释放
 GPIO_ResetBits(GPIOB,GPIO_Pin_9);
 for(i = 0; i < 10; i++)
 {
	 delay_ms(1000);	
 }

}

void SIM800_GPRS_Restart(void)
{
	u8 temp = 0;
	SIM800_GPRS_OFF();
	for(temp = 0; temp < 30; temp++)
	{
		delay_ms(1000);
	}
	SIM800_GPRS_ON();

}

void SIM800_Powerkey_Restart(void)
{
	u8 temp = 0;
	Flag_SIM800C_In_Reset = 0xAA;
	BSP_Printf("Powerkey Restart\r\n");
	SIM800_PWRKEY_OFF();
	for(temp = 0; temp < 30; temp++)
	{
		delay_ms(1000);
	}
	SIM800_PWRKEY_ON();
	Flag_SIM800C_In_Reset = 0;
}

void SIM800_Power_Restart(void)
{
	u8 temp = 0;
	SIM800_PWRKEY_OFF();
	SIM800_POWER_OFF();
	
	for(temp = 0; temp < 30; temp++)
	{
		delay_ms(1000);
	}
	SIM800_POWER_ON();
	SIM800_PWRKEY_ON();

}

//返回1   某条AT指令执行错误
//返回0   成功连接上服务器
u8 SIM800_Link_Server_AT(void)
{
	u8 ret = CMD_ACK_NONE;
	//操作AT指令进行联网操作
	if((ret = Check_Module()) == CMD_ACK_OK)
		if((ret = Disable_Echo()) == CMD_ACK_OK)
			if((ret = Check_SIM_Card()) == CMD_ACK_OK)
				//if((ret = Check_CSQ()) == CMD_ACK_OK)
					if((ret = Get_ICCID()) == CMD_ACK_OK)
						//if((ret = Check_OPS()) == CMD_ACK_OK)
							//if((ret = SIM800_GPRS_OFF()) == CMD_ACK_OK)
								if((ret = SIM800_GPRS_CIPSHUT()) == CMD_ACK_OK)
									if((ret = SIM800_GPRS_CGCLASS()) == CMD_ACK_OK)
										if((ret = SIM800_GPRS_CGDCONT()) == CMD_ACK_OK)
											//if((ret = SIM800_GPRS_Adhere()) == CMD_ACK_OK)
												if((ret = SIM800_GPRS_Set()) == CMD_ACK_OK)
													//if((ret = SIM800_GPRS_Dispaly_IP()) == CMD_ACK_OK)
														if((ret = Link_Server_AT(0, ipaddr, port)) == CMD_ACK_OK)											
																;

	return ret;
}

u8 SIM800_Link_Server_Powerkey(void)
{
	u8 count = 5;
	u8 ret = CMD_ACK_NONE;	
	while(count != 0)
	{
		ret = SIM800_Link_Server_AT();
		if(ret != CMD_ACK_OK)
		{
			SIM800_Powerkey_Restart();
		}
		else
			break;
		count--;
	}

	return ret;

}
u8 SIM800_Link_Server(void)
{
	u8 count = 5;
	u8 ret = CMD_ACK_NONE;
	while(count != 0)
	{
		ret = SIM800_Link_Server_Powerkey();
		if(ret != CMD_ACK_OK)
		{
			SIM800_Power_Restart();
		}
		else
			break;
		count--;
	}
	
	return ret;

}


void Get_Login_Data(void)
{
	char temp_Login_Buffer[LENGTH_LOGIN] = {0}; 
	char temp_Result[3] = {0}; 
	u8 Result_Validation = 0;
	u8 i = 0;
	char temp00[] = "TRVAP00,059,000,SIM800_";
	char temp01[] = ",";
	char temp02[] = "#";
	char temp03[] = "0";
	char temp04[] = "00";
	char temp05[] = "0000,";       //设备运行状态
	//char temp06[] = "0000000000000000,";   //设备运行时间

	//清零Resend_Buffer
	for(i = 0; i < LENGTH_LOGIN; i++)
	{
		Login_Buffer[i] = 0;
	}
	
	//添加TRVAP00,FF,000,SIM800_
	
	for(i = 0; i < strlen(temp00); i++)
	{
		temp_Login_Buffer[i] = temp00[i];
	}
	strcat(Login_Buffer,temp_Login_Buffer);
	Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	
	//添加SIM卡的ICCID
	for(i = 0; i < LENGTH_ICCID_BUF; i++)
	{
		temp_Login_Buffer[i] = ICCID_BUF[i];
	}
	
	strcat(Login_Buffer,temp_Login_Buffer);
	Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	
		
	//添加,分隔符
	for(i = 0; i < strlen(temp01); i++)
	{
		temp_Login_Buffer[i] = temp01[i];
	}
	strcat(Login_Buffer,temp_Login_Buffer);
	Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	
	//由于读取的是GPIO 高低，因此是设备实时状态
	for(i = 0; i < strlen(temp05); i++)
	{
		//BSP_Printf("Device[%d]: %d\n", i, Device_Power_Status(i));	
		temp05[i] = (ON==Device_Power_Status(i))?'1':temp05[i];		
	}
	
	//strcat(Login_Buffer,temp_Login_Buffer);
	//Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	strncat(Login_Buffer,temp05,strlen(temp05));
	
	//添加设备运行时间
	Device_Timer_Status(temp_Login_Buffer);

	strcat(Login_Buffer,temp_Login_Buffer);	
	Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	
	//添加校验和
	Result_Validation = Check_Xor_Sum(Login_Buffer, strlen(Login_Buffer));
	
	//校验值转化为字符串
  	sprintf(temp_Result,"%d",Result_Validation);
	
	//添加校验值
	//判断校验值是否是三个字符
	//校验值是三个字符，直接拼接

	if(strlen(temp_Result) == 1)
	{
		//添加校验值
		for(i = 0; i < 2; i++)
		{
			temp_Login_Buffer[i] = temp04[i];
		}
		temp_Login_Buffer[2] = temp_Result[0];
		
		strcat(Login_Buffer,temp_Login_Buffer);
		Clear_buffer(temp_Result,3);
		Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	}
	if(strlen(temp_Result) == 2)
	{
		//添加校验值
		temp_Login_Buffer[0] = temp03[0];
		temp_Login_Buffer[1] = temp_Result[0];
		temp_Login_Buffer[2] = temp_Result[1];
		
		strcat(Login_Buffer,temp_Login_Buffer);
		Clear_buffer(temp_Result,3);
		Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	}
	if(strlen(temp_Result) == 3)
	{
			//添加校验值
		for(i = 0; i < strlen(temp_Result); i++)
		{
			temp_Login_Buffer[i] = temp_Result[i];
		}
		strcat(Login_Buffer,temp_Login_Buffer);
		Clear_buffer(temp_Result,3);
		Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	}	
	
	//添加,分隔符
	for(i = 0; i < strlen(temp01); i++)
	{
		temp_Login_Buffer[i] = temp01[i];
	}
	strcat(Login_Buffer,temp_Login_Buffer);
	Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	
	
	//添加结束符  #
	for(i = 0; i < strlen(temp02); i++)
	{
		temp_Login_Buffer[i] = temp02[i];
	}
	strcat(Login_Buffer,temp_Login_Buffer);
	Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);


}


//发送登陆信息给服务器
u8 Send_Login_Data(void)
{
	u8 ret = CMD_ACK_NONE;
	Get_Login_Data();
	BSP_Printf("Login_Buffer:%s\r\n",Login_Buffer);
	ret = Send_Data_To_Server(Login_Buffer);
	//发送成功
	if(ret == CMD_ACK_OK)
	{
		Flag_ACK_Resend = 0x01;
		Flag_ACK_Echo = 0x01;
		//开启等待服务器回文的超时机制
		Flag_Wait_Echo = 0xAA;
		Count_Wait_Echo = 0;
	}

	return ret;

}

u8 Send_Login_Data_Normal(void)
{
	u8 temp = 0;
	u8 ret = CMD_ACK_NONE;
	u8 count = 5;	//执行count次，还不成功的话，就重启GPRS
	while(count != 0)
	{
		//Clear_Usart3();
		ret = Send_Login_Data();
		//Clear_Usart3();
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			//发送数据失败
			for(temp = 0; temp < 30; temp++)
			{
				delay_ms(1000);
			}
		}
		else if((ret == CMD_ACK_OK) ||(ret == CMD_ACK_DISCONN))
			break;
		count -= 1;
	}
	
	return ret;
	
}

u8 Send_Login_Data_To_Server(void)
{
	u8 count = 5;
	u8 ret = CMD_ACK_NONE;
	current_cmd = CMD_LOGIN;
	Flag_Send_Heart = 0x00;    //把当前心跳定时器停掉，主循环里收到登录回文后会打开正常心跳
	while(count != 0)
	{
		ret = Send_Login_Data_Normal();
		if(ret != CMD_ACK_OK)
		{
			SIM800_GPRS_Restart();
		}
		else
			break;
		count--;	
	}

	return ret;

}


void Get_Heart_Data(void)
{
	char temp_Heart_Buffer[LENGTH_HEART] = {0}; 
	char temp_Result[3] = {0}; 
	u8 Result_Validation = 0;
	u8 i = 0;
	char temp00[] = "TRVAP01,031,000,";
	char temp01[] = ",";
	char temp02[] = "#";
	char temp03[] = "0";
	char temp04[] = "00";
	char temp05[] = "0000,";
	//char temp06[] = "1122334455667788,";
	

	//清零Resend_Buffer
	for(i = 0; i < LENGTH_HEART; i++)
	{
		Heart_Buffer[i] = 0;
	}
	
	//添加TRVAP01,FF,000,
	
	for(i = 0; i < strlen(temp00); i++)
	{
		temp_Heart_Buffer[i] = temp00[i];
	}
	strcat(Heart_Buffer,temp_Heart_Buffer);
	Clear_buffer(temp_Heart_Buffer,LENGTH_HEART);

	//由于读取的是GPIO 高低，因此是设备实时状态
	for(i = 0; i < strlen(temp05); i++)
	{
		//BSP_Printf("Device[%d]: %d\n", i, Device_Power_Status(i));	
		temp05[i] = (ON==Device_Power_Status(i))?'1':temp05[i];		
	}
	
	//strcat(Login_Buffer,temp_Login_Buffer);
	//Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	strncat(Heart_Buffer,temp05,strlen(temp05));
	
	//添加设备运行时间
	Device_Timer_Status(temp_Heart_Buffer);
	
	strcat(Heart_Buffer,temp_Heart_Buffer);
	Clear_buffer(temp_Heart_Buffer,LENGTH_HEART);
	
	//添加校验和
	Result_Validation = Check_Xor_Sum(Heart_Buffer, strlen(Heart_Buffer));
	
	//校验值转化为字符串
  sprintf(temp_Result,"%d",Result_Validation);
	
	//添加校验值
	//判断校验值是否是三个字符
	//校验值是三个字符，直接拼接

	if(strlen(temp_Result) == 1)
	{
		//添加校验值
		for(i = 0; i < 2; i++)
		{
			temp_Heart_Buffer[i] = temp04[i];
		}
		temp_Heart_Buffer[2] = temp_Result[0];
		
		
		strcat(Heart_Buffer,temp_Heart_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Heart_Buffer,LENGTH_HEART);
	}
	if(strlen(temp_Result) == 2)
	{
		//添加校验值
		temp_Heart_Buffer[0] = temp03[0];
		temp_Heart_Buffer[1] = temp_Result[0];
		temp_Heart_Buffer[2] = temp_Result[1];
		
		strcat(Heart_Buffer,temp_Heart_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Heart_Buffer,LENGTH_HEART);
	}
	if(strlen(temp_Result) == 3)
	{
			//添加校验值
		for(i = 0; i < strlen(temp_Result); i++)
		{
			temp_Heart_Buffer[i] = temp_Result[i];
		}
		strcat(Heart_Buffer,temp_Heart_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Heart_Buffer,LENGTH_HEART);
	}	
	
	//添加,分隔符
	for(i = 0; i < strlen(temp01); i++)
	{
		temp_Heart_Buffer[i] = temp01[i];
	}
	strcat(Heart_Buffer,temp_Heart_Buffer);
	Clear_buffer(temp_Heart_Buffer,LENGTH_HEART);
	
	
	//添加结束符  #
	for(i = 0; i < strlen(temp02); i++)
	{
		temp_Heart_Buffer[i] = temp02[i];
	}
	strcat(Heart_Buffer,temp_Heart_Buffer);
	Clear_buffer(temp_Heart_Buffer,LENGTH_HEART);


}

//发送心跳包给服务器
u8 Send_Heart_Data(void)
{
	u8 ret = CMD_ACK_NONE;
	Get_Heart_Data();
	ret = Send_Data_To_Server(Heart_Buffer);
	//发送成功
	if(ret == CMD_ACK_OK)
	{
		Flag_ACK_Resend = 0x02;
		Flag_ACK_Echo = 0x02;
		//开启等待服务器回文的超时机制
		Flag_Wait_Echo = 0xAA;
		Count_Wait_Echo = 0;
	}

	return ret;

}

u8 Send_Heart_Data_Normal(void)
{
	u8 temp = 0;
	u8 ret = CMD_ACK_NONE;
	u8 count = 5;	//执行count次，还不成功的话，就重启GPRS
	while(count != 0)
	{
		//Clear_Usart3();
		ret = Send_Heart_Data();
		//Clear_Usart3();
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			//发送数据失败
			for(temp = 0; temp < 30; temp++)
			{
				delay_ms(1000);
			}
		}
		else if((ret == CMD_ACK_OK) ||(ret == CMD_ACK_DISCONN))
			break;
		count -= 1;
	}
	
	return ret;

}

u8 Send_Heart_Data_To_Server(void)
{
	u8 ret = CMD_ACK_NONE;
	current_cmd = CMD_HB;	
	ret = Send_Heart_Data_Normal();
	return ret;
}

void Get_Resend_Data(void)
{
	char temp_Resend_Buffer[LENGTH_RESEND] = {0}; 
	char temp_Result[3] = {0}; 
	u8 Result_Validation = 0;
	u8 i = 0;
	char temp00[] = "TRVAP98,009,000,";
	char temp01[] = ",";
	char temp02[] = "#";
	char temp03[] = "0";
	char temp04[] = "00";
	

	//清零Resend_Buffer
	for(i = 0; i < LENGTH_RESEND; i++)
	{
		Resend_Buffer[i] = 0;
	}
	
	//添加TRVAP98,FF,000,
	
	for(i = 0; i < strlen(temp00); i++)
	{
		temp_Resend_Buffer[i] = temp00[i];
	}
	strcat(Resend_Buffer,temp_Resend_Buffer);
	Clear_buffer(temp_Resend_Buffer,LENGTH_RESEND);
	
	//添加校验和
	Result_Validation = Check_Xor_Sum(Resend_Buffer, strlen(Resend_Buffer));
	
	//校验值转化为字符串
  sprintf(temp_Result,"%d",Result_Validation);
	
	//添加校验值
	//判断校验值是否是三个字符
	//校验值是三个字符，直接拼接

	if(strlen(temp_Result) == 1)
	{
		//添加校验值
		for(i = 0; i < 2; i++)
		{
			temp_Resend_Buffer[i] = temp04[i];
		}
		temp_Resend_Buffer[2] = temp_Result[0];
		
		strcat(Resend_Buffer,temp_Resend_Buffer);
		Clear_buffer(temp_Result,3);
		Clear_buffer(temp_Resend_Buffer,LENGTH_RESEND);
	}
	if(strlen(temp_Result) == 2)
	{
		//添加校验值
		temp_Resend_Buffer[0] = temp03[0];
		temp_Resend_Buffer[1] = temp_Result[0];
		temp_Resend_Buffer[2] = temp_Result[1];
		
		strcat(Resend_Buffer,temp_Resend_Buffer);
		Clear_buffer(temp_Result,3);
		Clear_buffer(temp_Resend_Buffer,LENGTH_RESEND);
	}
	if(strlen(temp_Result) == 3)
	{
			//添加校验值
		for(i = 0; i < strlen(temp_Result); i++)
		{
			temp_Resend_Buffer[i] = temp_Result[i];
		}
		strcat(Resend_Buffer,temp_Resend_Buffer);
		Clear_buffer(temp_Result,3);
		Clear_buffer(temp_Resend_Buffer,LENGTH_RESEND);
	}	
	
	//添加,分隔符
	for(i = 0; i < strlen(temp01); i++)
	{
		temp_Resend_Buffer[i] = temp01[i];
	}
	strcat(Resend_Buffer,temp_Resend_Buffer);
	Clear_buffer(temp_Resend_Buffer,LENGTH_RESEND);
	
	
	//添加结束符  #
	for(i = 0; i < strlen(temp02); i++)
	{
		temp_Resend_Buffer[i] = temp02[i];
	}
	strcat(Resend_Buffer,temp_Resend_Buffer);
	Clear_buffer(temp_Resend_Buffer,LENGTH_RESEND);


}



//收到的消息校验错误，要求服务器重新发送
u8 Send_Resend_Data(void)
{
	u8 ret = CMD_ACK_NONE;
	Get_Resend_Data();
	ret = Send_Data_To_Server(Resend_Buffer);
	//发送成功
	if(ret == CMD_ACK_OK)
	{
		Flag_ACK_Resend = 0x03;
		Flag_ACK_Echo = 0x03;
		//开启等待服务器回文的超时机制
		Flag_Wait_Echo = 0xAA;
		Count_Wait_Echo = 0;
	}

	return ret;

}

u8 Send_Resend_Data_Normal(void)
{
	u8 temp = 0;
	u8 ret = CMD_ACK_NONE;
	u8 count = 5;	//执行count次，还不成功的话，就重启GPRS
	while(count != 0)
	{
		//Clear_Usart3();
		ret = Send_Resend_Data();
		//Clear_Usart3();
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			//发送数据失败
			for(temp = 0; temp < 30; temp++)
			{
				delay_ms(1000);
			}
		}
		else if((ret == CMD_ACK_OK) ||(ret == CMD_ACK_DISCONN))
			break;
		count --;
	}

	return ret;

}

u8 Send_Resend_Data_To_Server(void)
{
	u8 ret = CMD_ACK_NONE;
	//current_cmd =  之前的流程，所以不需要改变;		
	ret = Send_Resend_Data_Normal();
	return ret;
}



void Get_Enable_Data(void)
{
	char temp_Enable_Buffer[LENGTH_ENABLE] = {0}; 
	char temp_Result[3] = {0}; 
	u8 Result_Validation = 0;
	u8 i = 0;
	char temp00[] = "TRVAP03,027,";
	char temp01[] = ",";
	char temp02[] = "#";
	char temp03[] = "0";
	char temp04[] = "00";
	char temp05[] = "0000,";
	//char temp06[] = "1122334455667788,";
	

	//清零Resend_Buffer
	for(i = 0; i < LENGTH_ENABLE; i++)
	{
		Enbale_Buffer[i] = 0;
	}
	
	//添加TRVAP01,FF,000,
	
	for(i = 0; i < strlen(temp00); i++)
	{
		temp_Enable_Buffer[i] = temp00[i];
	}
	strcat(Enbale_Buffer,temp_Enable_Buffer);
	Clear_buffer(temp_Enable_Buffer,LENGTH_ENABLE);
	
	//由于读取的是GPIO 高低，因此是设备实时状态
	for(i = 0; i < strlen(temp05); i++)
	{
		//BSP_Printf("Device[%d]: %d\n", i, Device_Power_Status(i));	
		temp05[i] = (ON==Device_Power_Status(i))?'1':temp05[i];		
	}
	
	//strcat(Login_Buffer,temp_Login_Buffer);
	//Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	strncat(Enbale_Buffer,temp05,strlen(temp05));
	
	//添加设备运行时间
	Device_Timer_Status(temp_Enable_Buffer);
	
	strcat(Enbale_Buffer,temp_Enable_Buffer);
	Clear_buffer(temp_Enable_Buffer,LENGTH_ENABLE);
	
	//添加校验和
	Result_Validation = Check_Xor_Sum(Enbale_Buffer, strlen(Enbale_Buffer));
	
	//校验值转化为字符串
  sprintf(temp_Result,"%d",Result_Validation);
	
	//添加校验值
	//判断校验值是否是三个字符
	//校验值是三个字符，直接拼接

	if(strlen(temp_Result) == 1)
	{
		//添加校验值
		for(i = 0; i < 2; i++)
		{
			temp_Enable_Buffer[i] = temp04[i];
		}
		temp_Enable_Buffer[2] = temp_Result[0];
		
		
		strcat(Enbale_Buffer,temp_Enable_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Enable_Buffer,LENGTH_ENABLE);
	}
	if(strlen(temp_Result) == 2)
	{
		//添加校验值
		temp_Enable_Buffer[0] = temp03[0];
		temp_Enable_Buffer[1] = temp_Result[0];
		temp_Enable_Buffer[2] = temp_Result[1];
		
		strcat(Enbale_Buffer,temp_Enable_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Enable_Buffer,LENGTH_ENABLE);
	}
	if(strlen(temp_Result) == 3)
	{
			//添加校验值
		for(i = 0; i < strlen(temp_Result); i++)
		{
			temp_Enable_Buffer[i] = temp_Result[i];
		}
		strcat(Enbale_Buffer,temp_Enable_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Enable_Buffer,LENGTH_ENABLE);
	}	
	
	//添加,分隔符
		for(i = 0; i < strlen(temp01); i++)
		{
			temp_Enable_Buffer[i] = temp01[i];
		}
		strcat(Enbale_Buffer,temp_Enable_Buffer);
		Clear_buffer(temp_Enable_Buffer,LENGTH_ENABLE);
	
	
	//添加结束符  #
	for(i = 0; i < strlen(temp02); i++)
	{
		temp_Enable_Buffer[i] = temp02[i];
	}
		strcat(Enbale_Buffer,temp_Enable_Buffer);
		Clear_buffer(temp_Enable_Buffer,LENGTH_ENABLE);


}



//发送接收业务指令完成回文给服务器
u8 Send_Enable_Data(void)
{
	u8 ret = CMD_ACK_NONE;
	Get_Enable_Data();
	ret = Send_Data_To_Server(Enbale_Buffer);
	//发送成功
	if(ret == CMD_ACK_OK)
	{
		Flag_ACK_Resend = 0x04;
		//Flag_ACK_Echo = 0x04;    //事实上这条消息不需要超时重发
		//开启等待服务器回文的超时机制
		//Flag_Wait_Echo = 0xAA;
		//Count_Wait_Echo = 0;
	}

	return ret;

}

u8 Send_Enable_Data_Normal(void)
{
	u8 temp = 0;
	u8 ret = CMD_ACK_NONE;
	u8 count = 5;	//执行count次，还不成功的话，就重启GPRS
	while(count != 0)
	{
		//Clear_Usart3();
		ret= Send_Enable_Data();
		//Clear_Usart3();
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			//发送数据失败
			for(temp = 0; temp < 30; temp++)
			{
				delay_ms(1000);
			}
		}
		else if((ret == CMD_ACK_OK) ||(ret == CMD_ACK_DISCONN))
			break;
		count -= 1;
	}
	
	return ret;

}

//这个流程其实是一条设备回文，设备在CMD_NONE 状态的时候(当前没有处理任何消息)才会进入此流程
//当然硬件的开关在收到服务器指令时就需要完成，不依赖于这条消息什么时候发送    
u8 Send_Enable_Data_To_Server(void)
{
	u8 ret = CMD_ACK_NONE;
	current_cmd = CMD_ENABLE_DEVICE; 
	ret = Send_Enable_Data_Normal();
	current_cmd = CMD_NONE;    //这条命令发完就完了，不期待任何返回		
	return ret;
}

void Get_Device_OK_Data(void)
{
	char temp_Device_OK_Buffer[LENGTH_DEVICE_OK] = {0}; 
	char temp_Result[3] = {0}; 
	u8 Result_Validation = 0;
	u8 i = 0;
	char temp00[] = "TRVAP05,027,";
	char temp01[] = ",";
	char temp02[] = "#";
	char temp03[] = "0";
	char temp04[] = "00";
	char temp05[] = "0000,";
	//char temp06[] = "1122334455667788,";
	

	//清零Resend_Buffer
	for(i = 0; i < LENGTH_DEVICE_OK; i++)
	{
		Device_OK_Buffer[i] = 0;
	}
	
	//添加TRVAP05,FF,
	
	for(i = 0; i < strlen(temp00); i++)
	{
		temp_Device_OK_Buffer[i] = temp00[i];
	}
	strcat(Device_OK_Buffer,temp_Device_OK_Buffer);
	Clear_buffer(temp_Device_OK_Buffer,LENGTH_DEVICE_OK);
	
	//由于读取的是GPIO 高低，因此是设备实时状态
	for(i = 0; i < strlen(temp05); i++)
	{
		//BSP_Printf("Device[%d]: %d\n", i, Device_Power_Status(i));	
		temp05[i] = (ON==Device_Power_Status(i))?'1':temp05[i];		
	}
	
	//strcat(Login_Buffer,temp_Login_Buffer);
	//Clear_buffer(temp_Login_Buffer,LENGTH_LOGIN);
	strncat(Device_OK_Buffer,temp05,strlen(temp05));
	
	//添加设备运行时间
	Device_Timer_Status(temp_Device_OK_Buffer);
	
	strcat(Device_OK_Buffer,temp_Device_OK_Buffer);
	Clear_buffer(temp_Device_OK_Buffer,LENGTH_DEVICE_OK);
	
	//添加校验和
	Result_Validation = Check_Xor_Sum(Device_OK_Buffer, strlen(Device_OK_Buffer));
	
	//校验值转化为字符串
  sprintf(temp_Result,"%d",Result_Validation);
	
	//添加校验值
	//判断校验值是否是三个字符
	//校验值是三个字符，直接拼接

	if(strlen(temp_Result) == 1)
	{
		//添加校验值
		for(i = 0; i < 2; i++)
		{
			temp_Device_OK_Buffer[i] = temp04[i];
		}
		temp_Device_OK_Buffer[2] = temp_Result[0];
		
		
		strcat(Device_OK_Buffer,temp_Device_OK_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Device_OK_Buffer,LENGTH_DEVICE_OK);
	}
	if(strlen(temp_Result) == 2)
	{
		//添加校验值
		temp_Device_OK_Buffer[0] = temp03[0];
		temp_Device_OK_Buffer[1] = temp_Result[0];
		temp_Device_OK_Buffer[2] = temp_Result[1];
		
		strcat(Device_OK_Buffer,temp_Device_OK_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Device_OK_Buffer,LENGTH_DEVICE_OK);
	}
	if(strlen(temp_Result) == 3)
	{
			//添加校验值
		for(i = 0; i < strlen(temp_Result); i++)
		{
			temp_Device_OK_Buffer[i] = temp_Result[i];
		}
		strcat(Device_OK_Buffer,temp_Device_OK_Buffer);
		Clear_buffer(temp_Result,3);
	  Clear_buffer(temp_Device_OK_Buffer,LENGTH_DEVICE_OK);
	}	
	
	//添加,分隔符
	for(i = 0; i < strlen(temp01); i++)
	{
		temp_Device_OK_Buffer[i] = temp01[i];
	}
		strcat(Device_OK_Buffer,temp_Device_OK_Buffer);
	  Clear_buffer(temp_Device_OK_Buffer,LENGTH_DEVICE_OK);
	
	
	//添加结束符  #
	for(i = 0; i < strlen(temp02); i++)
	{
		temp_Device_OK_Buffer[i] = temp02[i];
	}
		strcat(Device_OK_Buffer,temp_Device_OK_Buffer);
	  Clear_buffer(temp_Device_OK_Buffer,LENGTH_DEVICE_OK);


}



//发送业务执行完成指令给服务器
u8 Send_Device_OK_Data(void)
{
	u8 ret = CMD_ACK_NONE;
	Get_Device_OK_Data();
	ret = Send_Data_To_Server(Device_OK_Buffer);
	//发送成功
	if(ret == CMD_ACK_OK)
	{
		Flag_ACK_Resend = 0x05;
		Flag_ACK_Echo = 0x05;
		//开启等待服务器回文的超时机制
		Flag_Wait_Echo = 0xAA;
		Count_Wait_Echo = 0;
	}

	return ret;

}

u8 Send_Device_OK_Data_Normal(void)
{
	u8 temp = 0;
	u8 ret = CMD_ACK_NONE;
	u8 count = 5;	//执行count次，还不成功的话，就重启GPRS
	while(count != 0)
	{
		//Clear_Usart3();
		ret = Send_Device_OK_Data();
		//Clear_Usart3();
		if((ret == CMD_ACK_NONE) || (ret == CMD_ACK_NOK))
		{
			//发送数据失败
			for(temp = 0; temp < 30; temp++)
			{
				delay_ms(1000);
			}
		}
		else if((ret == CMD_ACK_OK) ||(ret == CMD_ACK_DISCONN))
			break;
		count --;		
	}

	return ret;

}

u8 Send_Device_OK_Data_To_Server(void)
{
	u8 ret = CMD_ACK_NONE;
	current_cmd = CMD_CLOSE_DEVICE;
	ret = Send_Device_OK_Data_Normal();
	return ret;
}

void Clear_buffer(char* buffer,u16 length)
{
	u16 i = 0;
	for(i = 0; i < length;i++)
	{
		buffer[i] = 0;
	}
}

//////////////异或校验和函数///////
u8 Check_Xor_Sum(char* pBuf, u16 len)
{
  u8 Sum = 0;
  u8 i = 0;
	Sum = pBuf[0];
	
  for (i = 1; i < len; i++ )
  {
    Sum = (Sum ^ pBuf[i]);
  }
	
  return Sum;
}
