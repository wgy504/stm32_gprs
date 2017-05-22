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

/* 这个宏仅用于调试阶段排错 */
#define BSP_Printf		printf
//#define BSP_Printf(...)




//发送函数使用的一组变量
u8 result_Send_Data = 0xAA;
u8 count_Send_Data = 0x00;
#define TOTAL_SEND_DATA 3
#define TOTAL_WAIT_ECO  3        
u8 Total_Wait_Echo  =  0;

int main(void)
{
	u8 temp = 0x00;
	u32 Count_Heart = 0;
	//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); 
  /* Enable GPIOx Clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

	delay_init();
	
	//注意串口1仅仅用来发送，接收的宏没有使能
	//#define EN_USART1_RX 			   0		//使能（1）/禁止（0）串口1接收
  usart1_init(115200);                            //串口1,Log
	
	usart3_init(115200);                            //串口3,对接SIM800
	
	//清零USART3_RX_BUF和USART3_RX_STA
	//在使用串口3之前先清零，排除一些意外情况
  Clear_Usart3();
	
	//开机
	//包含2G模块电源芯片的软件使能和2G模块的PWRKEY的使能
	//SIM800_POWER_ON函数是作为急停使用
	SIM800_POWER_ON();  
	SIM800_PWRKEY_ON();
	BSP_Printf("SIM800C开机完成\r\n");
	
	while(1)
	{
		if(SIM800_Link_Server() == 0x01)
		{
			//如果AT指令执行若干次仍然失败，只能延时后重启模块的GPRS链接
			//如果模块连接不上服务器，系统没有任何事可以做，所有此时用delay函数和定时器没有区别
			SIM800_GPRS_OFF();
			for(temp = 0; temp < 30; temp++)
			{
				delay_ms(1000);
			}
			//SIM800_Link_Server函数中已经包含了函数SIM800_GPRS_ON的功能，所以这里屏蔽SIM800_GPRS_ON
			//SIM800_GPRS_ON();
		}
		else
		{
			break;
		}
	}
	 BSP_Printf("SIM800C连接服务器完成\r\n");
	//程序运行至此，已经成功的链接上服务器，下面发送登录信息给服务器
	//发送函数最多执行COUNT_SEND_DATA次，仍然失败的话，就断开GPRS,延时，重新连接GPRS
	//发送函数的处理思路是必须保证数据的发送成功，不然整个系统都没有意义
   Send_Login_Data_To_Server();
	 BSP_Printf("SIM800C发送登录信息完成\r\n");
	
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
		//如果收到登陆信息回文，就进入发送心跳包流程
		if(Flag_Receive_Login == 0xAA)
		{
			BSP_Printf("SIM800C正确接收登录信息的回文\r\n");
			Flag_Receive_Login = 0;
			//发送心跳包数据给服务器
			Send_Heart_Data_To_Server();
			BSP_Printf("SIM800C发送心跳包完成\r\n");

		}
		//如果收到服务器的心跳包回文，开启再次发送心跳包的定时
    if(Flag_Receive_Heart == 0xAA)
		{
			Flag_Receive_Heart = 0;
			//开启再次发送心跳包的定时
			Flag_Send_Heart = 0xAA;
		}
		//如果发送心跳包的定时时间到，发送心跳包
		if(Flag_Send_Heart_OK == 0xAA)
		{
			Count_Heart += 1;
			BSP_Printf("第%ld次发送心跳包\r\n",Count_Heart);
			BSP_Printf("一分钟的心跳包间隔到,再次发送心跳包\r\n");
			Flag_Send_Heart_OK = 0;
			Send_Heart_Data_To_Server();
			BSP_Printf("SIM800C发送心跳包完成\r\n");
		}


  	//接收到来至于服务器的重发命令，需要判定要重发的是那条指令
		//在本框架程序中，以登录信息的重发为例来给出示例代码
		if(Flag_Receive_Resend == 0xAA)
		{
			Flag_Receive_Resend = 0;
			//根据Flag_ACK_Resend的值来确定要重发的是那条命令
			if(Flag_ACK_Resend == 0x01)
			{
				Flag_ACK_Resend = 0xFF;
			//发送登陆信息给服务器
				Send_Login_Data_To_Server();
			}
			if(Flag_ACK_Resend == 0x02)
			{
				Flag_ACK_Resend = 0xFF;
				Send_Heart_Data_To_Server();
			}
			
			if(Flag_ACK_Resend == 0x03)
			{
				Flag_ACK_Resend = 0xFF;
				Send_Device_OK_To_Server();  //设备运行结束命令
			}
			if(Flag_ACK_Resend == 0x04)
			{
				Flag_ACK_Resend = 0xFF;
				;
				;
			}
			if(Flag_ACK_Resend == 0x05)
			{

			}	
		}
		
		
		if(Flag_Check_error == 0xAA)
		{
			//校验出错，要求服务器重发
			Flag_Check_error = 0;
      Send_Resend_Data_To_Server();
		}

		

		//设备发送信息给服务器后等待回文，有个最长等待时间，如果超时，还没有收到服务器的回文,就要进行若干次的重新发送
		
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
				Send_Login_Data_To_Server();
			}
			if(Flag_ACK_Echo == 0x02)
			{
				Flag_ACK_Echo = 0xFF;
				BSP_Printf("1分钟的等待服务器回文超时，重新发送心跳包\r\n");
				Send_Heart_Data_To_Server();
				
				BSP_Printf("1分钟的等待服务器回文超时，发送心跳包成功\r\n");
			}
			if(Flag_ACK_Echo == 0x03)
			{
				Flag_ACK_Echo = 0xFF;
				Send_Device_OK_To_Server();  //设备运行结束命令
			}


		}
		//若干次重发后，仍然收不到服务器回文，重启GPRS		
		if(Total_Wait_Echo == TOTAL_WAIT_ECO)
		{
			BSP_Printf("TOTAL_WAIT_ECO次等待服务器回文超时，重新开启GPRS\r\n");
			Total_Wait_Echo = 0;
			
			SIM800_GPRS_OFF();
			BSP_Printf("SIM800C关闭GPRS_Send_Total_Wait_Echo\r\n");

			for(temp = 0; temp < 30; temp++)
			{
				delay_ms(1000);
			}
			
			SIM800_GPRS_ON();
			BSP_Printf("SIM800C打开GPRS_Send_Total_Wait_Echo\r\n");
			//根据变量Flag_ACK_Echo的值来判定要重新发送的命令
			if(Flag_ACK_Echo == 0x01)
			{
				Flag_ACK_Echo = 0xFF;
				Send_Login_Data_To_Server();
			}
			if(Flag_ACK_Echo == 0x02)
			{
				BSP_Printf("Total_Wait_Echo次等待服务器回文超时，重新发送心跳包\r\n");
				Flag_ACK_Echo = 0xFF;
				Send_Heart_Data_To_Server();
				BSP_Printf("Total_Wait_Echo次等待服务器回文超时，发送心跳包成功\r\n");
			}
			if(Flag_ACK_Echo == 0x03)
			{
				Flag_ACK_Echo = 0xFF;
				Send_Device_OK_To_Server();  //设备运行结束命令
			}
			

		}
		

	

		//如果收到开启设备的指令,发送正确接收的回文给服务器，并按照业务指令开启设备
		if(Flag_Receive_Enable == 0xAA)  
		{
			static u8 Flag_Device_Run = 0;    //保证设备在执行业务指令器件能继续响应业务指令（本路的和其他三路的）
			Flag_Device_Run += 1;
			//发送回文给服务器（这里要有四路设备的状态，并且有设备的已运行时间：Count_Local_Time个50毫秒）
			Send_Enable_Data_To_Server();
			//开启设备并本地计时
			if(Flag_Device_Run == 0x01)
			{
				Enable_Device(1);
				Flag_Local_Time_Dev_01 = 0xAA;
			}
		}
		
		//如果设备运行时间到
		if(Flag_Local_Time_Dev_01_OK == 0xAA)
		{
			Flag_Local_Time_Dev_01_OK = 0;
			//发送设备运行结束命令给服务器
			Send_Device_OK_To_Server();
		
		}
		//如果收到设备运行结束命令的回文
		if(Flag_Receive_Device_OK == 0xAA)
		{
			Flag_Receive_Device_OK = 0;
			//暂时没有要处理的逻辑
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
