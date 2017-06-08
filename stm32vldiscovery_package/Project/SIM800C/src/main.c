/**
  ******************************************************************************
  * @file    Demo/src/main.c 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    09/13/2010
  * @brief   Main program body
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2010 STMicroelectronics</center></h2>
  */ 

/* Includes ------------------------------------------------------------------*/
#include "stm32F10x.h"
#include "STM32vldiscovery.h"
#include "usart.h"
#include "usart3.h"
#include "delay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timer.h"
#include "SIM800.h"


////////////////////////ST官方程序框架需要的变量和函数///////////////////////////////////////////

/**********************程序中断优先级设置******************************************  
                PreemptionPriority        SubPriority
USART3                 0											0

DMA1                   1											0

TIM7									 2											0	

TIM6									 3											0

TIM4                   3                      1

************************************************************************************/

////////////////////////用户程序自定义的变量和函数///////////////////////////////////////////

//发送函数使用的一组变量
u8 result_Send_Data = 0xAA;
u8 count_Send_Data = 0x00;
u8 current_cmd=0;
#define TOTAL_SEND_DATA 3
#define TOTAL_WAIT_ECO  3        
u8 Total_Wait_Echo  =  0;

int main(void)
{
	//u8 temp = 0x00;
	u32 Count_Heart = 0;
	//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); 
	/* Enable GPIOx Clock */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

	delay_init();

#ifdef LOG_ENABLE	
	//注意串口1仅仅用来发送，接收的宏没有使能
	//#define EN_USART1_RX 			   0		//使能（1）/禁止（0）串口1接收
	usart1_init(115200);                            //串口1,Log
#endif

	usart3_init(115200);                            //串口3,对接SIM800
	
	//清零USART3_RX_BUF和USART3_RX_STA
	//在使用串口3之前先清零，排除一些意外情况
	Clear_Usart3();
	Flag_Comm_OK = 0;	
	Flag_Need_Reset = 0;
	
	//开机
	//包含2G模块电源芯片的软件使能和2G模块的PWRKEY的使能
	//SIM800_POWER_ON函数是作为急停使用
	SIM800_POWER_ON();  
	SIM800_PWRKEY_ON();
	BSP_Printf("SIM800C开机完成\r\n");
	
	//连接服务器失败，闪烁一颗LED作为提醒，后期用短信来替换
	if(SIM800_Link_Server() != CMD_ACK_OK)
	{
		BSP_Printf("SIM800C连接服务器失败，请检查\r\n");
		//while(1){闪烁LED}
	}
	BSP_Printf("SIM800C连接服务器完成\r\n");
	
	//程序运行至此，已经成功的链接上服务器，下面发送登录信息给服务器
		//发送登录信息给服务器失败，闪烁一颗LED作为提醒，后期用短信来替换
	if(Send_Login_Data_To_Server() != CMD_ACK_OK)
	{
		BSP_Printf("SIM800C发送登录信息给服务器失败，请检查\r\n");
		//while(1){闪烁LED}
	}
	BSP_Printf("SIM800C发送登录信息给服务器完成\r\n");


	
	//程序运行至此，已经成功发送登录信息给服务器，下面开启定时器，等待服务器的回文信息
	//清零串口3的数据，等待接收服务器的数据
	Clear_Usart3();
	
	//开启定时器，每50ms查询一次服务器是否有下发命令
	//主循环TIMER
	TIM6_Int_Init(499,2399);						     //50ms中断
	TIM_SetCounter(TIM6,0); 
	TIM_Cmd(TIM6,ENABLE);
	
	while(1)
	{
		if((Flag_Received_SIM800C_CLOSED == 0xAA) || (Flag_Received_SIM800C_DEACT == 0xAA))
		{
			Clear_Usart3();
			Flag_Received_SIM800C_CLOSED=0;
			Flag_Received_SIM800C_DEACT=0;
			Flag_Need_Reset =0xAA;
			BSP_Printf("连接断开，准备重启\r\n");
			goto reset;
		}
		
		//如果收到登陆信息回文，就进入发送心跳包流程
		if(Flag_Comm_OK == 0xAA)//这个条件是一切消息可以发送的基础，但是不影响定时器
		{			

			//如果收到开启设备的指令,发送正确接收的回文给服务器，并按照业务指令开启设备
			if(Flag_Receive_Enable == 0xAA)
			{
				//根据当前设备状态进行开启(GPIO)，已经开了的就不处理了
				//待实现
				//开启设备并本地计时
				//if(Flag_Device_Run == 0x01)
				{
					//Enable_Device(1);
					Flag_Local_Time_Dev_01 = 0xAA;    //设备可以开始计时
				}	
				
				//仅当当前没有处理其他消息时，才会发送Enable 回文
				if(current_cmd == CMD_NONE)
				{
					//发送回文给服务器（这里要有四路设备的状态，并且有设备的已运行时间：Count_Local_Time个50毫秒）
					if(Send_Enable_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("发送设备开关状态信息失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;						
						//while(1){闪烁LED}
					}
					else
					{				
						BSP_Printf("SIM800C发送Enable 回文给服务器完成\r\n");
						Clear_Usart3();
					}					
				}
			}
			
			if(Flag_Local_Time_Dev_01_OK == 0xAA)
			{
				Flag_Local_Time_Dev_01_OK = 0;
				//关闭设备(GPIO)
				//待实现
				
				if(current_cmd == CMD_NONE)
				{
					//发送设备运行结束命令给服务器
					if(Send_Device_OK_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("发送设备运行结束信息失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;							
						//while(1){闪烁LED}
					}	
					else
					{				
						BSP_Printf("SIM800C发送设备运行结束给服务器完成\r\n");
						Clear_Usart3();
					}					
				}
			}
			
			//如果收到设备运行结束命令的回文
			if(Flag_Receive_Device_OK == 0xAA)
			{
				current_cmd = CMD_NONE;	
				Flag_Receive_Device_OK = 0;
				//暂时没有要处理的逻辑
			}
		
			//如果收到服务器的心跳包回文，开启再次发送心跳包的定时
			if(Flag_Receive_Heart == 0xAA)
			{
				Flag_Receive_Heart = 0;
				//收到回文就开启再次发送心跳包的定时
				Flag_Send_Heart = 0xAA;
				
				if(current_cmd == CMD_HB)       //当前就是在等待心跳回文(是否检测太严格了)
				{
					current_cmd = CMD_NONE;
				}
				Clear_Usart3();
			}

			//发送心跳包的定时时间到，必须处理完心跳定时器才能清除这个标志
			if(Flag_Send_Heart_OK == 0xAA)
			{
				//但只有在当前没有执行任何命令时，才发送心跳包消息，否则继续循环	
				if(current_cmd == CMD_NONE)
				{
					//心跳包的定时时间这个操作一定要做，因此不着急清除这个标志
					//如果当前有其他操作进行中，等待此操作完成后会进入这里运行
					Flag_Send_Heart_OK = 0;				
					Count_Heart += 1;
					BSP_Printf("第%ld次发送心跳包\r\n",Count_Heart);
					BSP_Printf("一分钟的心跳包间隔到,再次发送心跳包\r\n");				
					if(Send_Heart_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("SIM800C发送心跳包给服务器失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{				
						BSP_Printf("SIM800C发送心跳包给服务器完成\r\n");
						Clear_Usart3();
					}
				}
			}		

			//接收到来至于服务器的重发命令，需要判定要重发的是那条指令
			//在本框架程序中，以登录信息的重发为例来给出示例代码
			if(Flag_Receive_Resend == 0xAA)
			{
				Flag_Receive_Resend = 0;
				//根据Flag_ACK_Resend的值来确定要重发的是那条命令
				if(Flag_ACK_Resend == 0x02)
				{
					Flag_ACK_Resend = 0xFF;
					if(Send_Heart_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("SIM800C发送心跳包给服务器失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("SIM800C发送心跳包给服务器完成\r\n");
						Clear_Usart3();
					}
				}
				
				if(Flag_ACK_Resend == 0x04)
				{
					Flag_ACK_Resend = 0xFF;
					if(Send_Enable_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("SIM800C发送设备运行开启命令给服务器失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{					
						BSP_Printf("SIM800C发送设备运行开启命令给服务器完成\r\n");
						Clear_Usart3();
					}
				}
				
				if(Flag_ACK_Resend == 0x05)
				{
					Flag_ACK_Resend = 0xFF;
					if(Send_Device_OK_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("SIM800C发送设备运行结束命令给服务器失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;						
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("SIM800C发送设备运行结束命令给服务器完成\r\n");
						Clear_Usart3();
					}
				}	
			}
			
			if(Flag_Check_error == 0xAA)
			{
				//校验出错，要求服务器重发
				Flag_Check_error = 0;
				if(Send_Resend_Data_To_Server() != CMD_ACK_OK)
				{
					BSP_Printf("SIM800C发送重发命令给服务器失败，请检查\r\n");
					current_cmd = CMD_NONE;
					Flag_Need_Reset = 0xAA;
					goto reset;						
					//while(1){闪烁LED}
				}
				else
				{
					BSP_Printf("SIM800C发送重发命令给服务器完成\r\n");
					Clear_Usart3();
				}
			}

			//设备发送信息给服务器后等待回文，有个最长等待时间，如果超时，还没有收到服务器的回文,就要进行若干次的重新发送
			if(Flag_Time_Out_Comm == 0xAA)
			{
				BSP_Printf("1分钟的等待服务器回文超时\r\n");
				Flag_Time_Out_Comm = 0;
				Total_Wait_Echo	+= 1;	
				//根据变量Flag_ACK_Echo的值来判定要重新发送的命令
				if(Flag_ACK_Echo == 0x02)
				{
					Flag_ACK_Echo = 0xFF;
					BSP_Printf("1分钟的等待服务器回文超时，重新发送心跳包\r\n");
					if(Send_Heart_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送心跳包失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送心跳包成功\r\n");				
						Clear_Usart3();
					}

				}
				if(Flag_ACK_Echo == 0x03)
				{
					Flag_ACK_Echo = 0xFF;
					BSP_Printf("1分钟的等待服务器回文超时，重新发送设备运行结束命令\r\n");			
					if(Send_Resend_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送设备运行结束命令失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送设备运行结束命令成功\r\n");
						Clear_Usart3();
					}
				}
				if(Flag_ACK_Echo == 0x05)
				{
					Flag_ACK_Echo = 0xFF;
					BSP_Printf("1分钟的等待服务器回文超时，重新发送设备运行结束命令\r\n");			
					if(Send_Device_OK_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送设备运行结束命令失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送设备运行结束命令成功\r\n");
						Clear_Usart3();
					}
				}			
			}
			
			//若干次重发后，仍然收不到服务器回文，重启GPRS		
			if(Total_Wait_Echo == TOTAL_WAIT_ECO)
			{
				Total_Wait_Echo = 0;
				Flag_ACK_Echo = 0xFF;
				Flag_Need_Reset = 0xAA;
				Clear_Usart3();
				goto reset;
			}			
			
		}
		//建立连接，需要发送login给服务器，等待login回文
		//还没有收到服务器发回login回文时，完全不处理其他消息
		else  
		{
			if(Flag_Receive_Login == 0xAA)  //收到login 回文
			{
				Flag_Receive_Login = 0;			
				if(current_cmd == CMD_LOGIN)   //当前设备就是在等待login回文状态
				{
					//收到了要等待的回文，需要清除resend/echo 标志了吧?
					Flag_ACK_Resend = 0xFF;
					Flag_ACK_Echo = 0xFF;		
					
					BSP_Printf("SIM800C正确接收登录信息的回文\r\n");
					//发送心跳包数据给服务器
					if(Send_Heart_Data_To_Server() != CMD_ACK_OK)  //重连接后的第一个心跳
					{
						BSP_Printf("SIM800C发送心跳包给服务器失败，重新登录服务器\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("SIM800C发送心跳包给服务器完成，等待服务器回文\r\n");
						Flag_Comm_OK = 0xAA;     //正常发出心跳才算此连接有效
						Clear_Usart3();
					}	
				}
			}		
			
			if(Flag_Receive_Resend == 0xAA)
			{
				Flag_Receive_Resend = 0;
				//未连接时仅有login消息的重发处理
				//这里没有考虑服务器要求重发Resend 消息。。。感觉没啥必要
				if(Flag_ACK_Resend == 0x01)
				{
					Flag_ACK_Resend = 0xFF;
					//发送登录信息给服务器失败，闪烁一颗LED作为提醒，后期用短信来替换
					if(Send_Login_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("SIM800C发送登录信息给服务器失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("SIM800C发送登录信息给服务器完成\r\n");
						Clear_Usart3();
					}
				}
			}
			
			if(Flag_Check_error == 0xAA)
			{
				//校验出错，要求服务器重发
				Flag_Check_error = 0;
				if(Send_Resend_Data_To_Server() != CMD_ACK_OK)
				{
					BSP_Printf("SIM800C发送重发命令给服务器失败，请检查\r\n");
					current_cmd = CMD_NONE;
					Flag_Need_Reset = 0xAA;
					goto reset;
					//while(1){闪烁LED}
				}
				else
				{
					BSP_Printf("SIM800C发送重发命令给服务器完成\r\n");
					Clear_Usart3();
				}
			}			

			if(Flag_Time_Out_Comm == 0xAA)
			{
				BSP_Printf("1分钟的等待服务器回文超时\r\n");
				Flag_Time_Out_Comm = 0;
				Total_Wait_Echo	+= 1;	
				//根据变量Flag_ACK_Echo的值来判定要重新发送的命令
				if(Flag_ACK_Echo == 0x01)
				{
					Flag_ACK_Echo = 0xFF;
					BSP_Printf("1分钟的等待服务器回文超时，重新发送登录信息\r\n");

					//发送登录信息给服务器失败，闪烁一颗LED作为提醒，后期用短信来替换
					if(Send_Login_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送登录信息失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;	
						goto reset;
						//while(1){闪烁LED}
					}
					else
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送登录信息成功\r\n");
						Clear_Usart3();
					}
					
				}
				//发送Resend 消息给服务器超时。。。，感觉没必要进行到底了。。。
				if(Flag_ACK_Echo == 0x03)
				{
					Flag_ACK_Echo = 0xFF;
					BSP_Printf("1分钟的等待服务器回文超时，重新发送设备运行结束命令\r\n");			
					if(Send_Resend_Data_To_Server() != CMD_ACK_OK)
					{
						BSP_Printf("1分钟的等待服务器回文超时，发送设备运行结束命令失败，请检查\r\n");
						current_cmd = CMD_NONE;
						Flag_Need_Reset = 0xAA;	
						goto reset;						
						//while(1){闪烁LED}
					}
					else
					{					
						BSP_Printf("1分钟的等待服务器回文超时，发送设备运行结束命令成功\r\n");
						Clear_Usart3();
					}
				}
			}
			
			//若干次重发后，仍然收不到服务器回文，重启GPRS		
			if(Total_Wait_Echo == TOTAL_WAIT_ECO)
			{
				BSP_Printf("TOTAL_WAIT_ECO次等待服务器回文超时，重新开启GPRS\r\n");
				Total_Wait_Echo = 0;
				Flag_ACK_Echo = 0xFF;
				Flag_Need_Reset = 0xAA;
				Clear_Usart3();
				goto reset;
			}			
		}

	reset:
		if(Flag_Need_Reset == 0xAA)
		{
			BSP_Printf("开始重启\r\n");		
			//首先认为连接没建立
			Flag_Comm_OK = 0;
			//清楚重发等待
			Total_Wait_Echo = 0;
			Flag_ACK_Echo = 0xFF;
			//清除心跳包标志
			Flag_Send_Heart = 0;
			Flag_Send_Heart_OK = 0;
			//不清除设备运行计时标志，因为那是设备自己的状态，跟SIM800C 无关
			
			SIM800_Powerkey_Restart(); //或者这里直接pwr/pwrkey?
			if(SIM800_Link_Server() == CMD_ACK_OK)
				if(Send_Login_Data_To_Server() == CMD_ACK_OK)
				{
					//如果执行不到这里，就会重复reset
					Flag_Need_Reset = 0;
				}	
		}

		//接收到了SIM800C的信息
		if(0x01 == Receive_Data_From_USART())	
		{
			/*暂时不做处理*/
			//清零串口3的接收标志变量
			
			BSP_Printf("Main USART3_RX_BUF_SIM800C:%s\r\n",USART3_RX_BUF);

			if(strstr((const char*)USART3_RX_BUF,"CLOSED")!=NULL)
				Flag_Received_SIM800C_CLOSED = 0xAA;
			if(strstr((const char*)USART3_RX_BUF,"+PDP: DEACT")!=NULL)
				Flag_Received_SIM800C_DEACT = 0xAA;
			Clear_Usart3();

		}
	}
}


#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/******************* (C) COPYRIGHT 2010 STMicroelectronics *****END OF FILE****/
