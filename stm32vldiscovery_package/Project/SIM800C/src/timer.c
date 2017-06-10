#include "timer.h"
#include "usart.h"
#include "usart3.h"
#include "SIM800.h"
#include "string.h"  
#include "stdlib.h"  

//////////////////////////////////////////////

u8 Flag_Time_Out_Reconnec_Normal = 0;           //传输正常时重新链接服务器的超时生效时置位该变量
u8 Flag_Time_Out_AT = 0;                        //模块链接服务器过程中出现异常后的定时时间到就置位该变量
u8 Flag_Auth_Infor = 0;  												//收到身份认证命令时置位该变量

u8 Flag_Reported_Data = 0;   										//收到上报数据命令时置位该变量（现阶段是上报TDS值和温度值）

u8 Flag_Comm_OK = 0;                            //设备已经与服务器建立连接，且收到了login回文
u8 Flag_Need_Reset = 0;

extern u8 Total_Wait_Echo;
extern u8 current_cmd;
//////////////////////////////////
u32 Count_Wait_Echo = 0;         //等待服务器回文的计数值，一个单位代表50ms
u8  Flag_Wait_Echo = 0;          //需要等待服务器回文时，置位这个变量

u32 Count_Local_Time_Dev_01 = 0;        //开启设备1，本地计时的值，一个单位代表50ms
u32 Count_Local_Time_Dev_02 = 0;        //开启设备2，本地计时的值，一个单位代表50ms
u32 Count_Local_Time_Dev_03 = 0;        //开启设备3，本地计时的值，一个单位代表50ms
u32 Count_Local_Time_Dev_04 = 0;        //开启设备4，本地计时的值，一个单位代表50ms
u8  Flag_Local_Time_Dev_01 = 0;         //设备1需要本地计时，置位这个变量
u8  Flag_Local_Time_Dev_02 = 0;         //设备2需要本地计时，置位这个变量
u8  Flag_Local_Time_Dev_03 = 0;         //设备3需要本地计时，置位这个变量
u8  Flag_Local_Time_Dev_04 = 0;         //设备4需要本地计时，置位这个变量

u8  Flag_Local_Time_Dev_01_OK = 0;         //设备1本地计时完成，置位这个变量
u8  Flag_Local_Time_Dev_02_OK = 0;         //设备2本地计时完成，置位这个变量
u8  Flag_Local_Time_Dev_03_OK = 0;         //设备3本地计时完成，置位这个变量
u8  Flag_Local_Time_Dev_04_OK = 0;         //设备4本地计时完成，置位这个变量


u32 Count_Send_Heart = 0;        //再次发送心跳包的计数值，一个单位代表50ms
u8  Flag_Send_Heart = 0;         //需要等待再次发送心跳包时，置位这个变量
u8  Flag_Send_Heart_OK = 0;      //再次发送心跳包的计时到，置位这个变量

u8  Flag_Receive_Heart = 0;      //收到服务器的心跳包回文时，置位这个变量

u8 	Flag_Time_Out_Comm = 0;                      //通信超时机制生效时置位该变量
u8 	Flag_Receive_Login = 0;  										 //收到登陆信息回文时置位该变量
u8 	Flag_ACK_Echo = 0xFF;                        //设备端程序利用该变量来判定等待回文超时的是那条指令

u8 	Flag_Check_error = 0;                        //对接收到的信息进行校验，如果校验不通过就置位该变量
u8 	Flag_Receive_Resend = 0; 										 //接收到重发命令时置位该变量
u8 	Flag_Receive_Enable = 0; 										 //接收到开启设备命令时置位该变量
u8 	Flag_Receive_Device_OK = 0; 								 //接收到设备运行结束回文时置位该变量
u8 	Flag_ACK_Resend = 0xFF;                      //设备端程序利用该变量来判定要重发的是那条命令

u8 Flag_Received_SIM800C_CLOSED = 0;
u8 Flag_Received_SIM800C_DEACT = 0;
/*
	50ms的定时扫描，目的有两个：
	一是及时获取服务器的下发命令（这里的下发命令包含对设备的回文信息和主动下发的业务指令）
	二是获取SIM800C的各种输出信息（各种出错的提醒信息也包含在这其中，系统的稳定性调试要在这里多下功夫）
	50毫秒的扫描周期应该是足够快速的，不放心的话，可以尝试修改为20毫秒甚至是10毫秒......
*/

//定时器6中断服务程序		    
void TIM6_DAC_IRQHandler(void)
{
	u16 length = 0; 
	u8 result = 0;
	u8 result_temp = 0;
	u8 index = 0;
	char *p, *p1;
	char *p_temp = NULL;
	u8 temp_array[3] = {0};
	u8 offset = 0;
	u8 temp = 0xAA;

	if(TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)					  //是更新中断
	{	
		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);  					//清除TIM6更新中断标志

		//再次发送心跳包的定时计数
		if(Flag_Send_Heart == 0xAA)
		{
			Count_Send_Heart += 1;
			if(Count_Send_Heart >= NUMBER_TIME_HEART_50_MS)
			{
				Flag_Send_Heart = 0;
				Count_Send_Heart = 0;
				Flag_Send_Heart_OK = 0xAA;
			}
		}
		
		if(Flag_Local_Time_Dev_01 == 0xAA)
		{
			Count_Local_Time_Dev_01 += 1;
			if(Count_Local_Time_Dev_01 >= 1000)  //这里的1000要根据实际情况进行修改
			{
				Flag_Local_Time_Dev_01_OK = 0xAA;
				//关闭设备
			
			}
		}
		if(Flag_Local_Time_Dev_02 == 0xAA)
		{
			Count_Local_Time_Dev_02 += 1;
		}
		if(Flag_Local_Time_Dev_03 == 0xAA)
		{
			Count_Local_Time_Dev_03 += 1;
		}
		if(Flag_Local_Time_Dev_04 == 0xAA)
		{
			Count_Local_Time_Dev_04 += 1;
		}
		
		if(Flag_Wait_Echo == 0xAA)
		{
			Count_Wait_Echo += 1;
			//TIME_ECHO的超时时间到,等待服务器回文的最长时间到
			if(Count_Wait_Echo >= NUMBER_TIME_ECHO_50_MS)
			{
				BSP_Printf("超时机制生效\r\n");
				BSP_Printf("Count_Wait_Echo:%ld\r\n",Count_Wait_Echo);
				
				Flag_Wait_Echo = 0;
				Count_Wait_Echo = 0;
				Flag_Time_Out_Comm	= 0xAA;
			}
		}
			


		temp = Receive_Data_From_USART();
		//接收到了SIM800C的信息
		if(0x01 == temp)	
		{
			/*暂时不做处理*/
			//清零串口3的接收标志变量
			
			BSP_Printf("USART3_RX_BUF_SIM800C:%s\r\n",USART3_RX_BUF);

			if(strstr((const char*)USART3_RX_BUF,"CLOSED")!=NULL)
				Flag_Received_SIM800C_CLOSED = 0xAA;
			if(strstr((const char*)USART3_RX_BUF,"+PDP: DEACT")!=NULL)
				Flag_Received_SIM800C_DEACT = 0xAA;
			//Clear_Usart3();

		}
		//接收到了服务器的信息
		if(0x00 == temp)	
		{
			BSP_Printf("USART3_RX_BUF_服务器:%s\r\n",USART3_RX_BUF);
			BSP_Printf("Count_Wait_Echo_0X00:%ld\r\n",Count_Wait_Echo);
			
			//收到回文，就清零相关标识符
			Flag_Wait_Echo = 0;
			Count_Wait_Echo = 0;
			Total_Wait_Echo = 0;
			
			BSP_Printf("need_ack_check: %d, ack: %s\r\n",need_ack_check, atcmd_ack);
			if(need_ack_check && strstr((const char*)USART3_RX_BUF, atcmd_ack))
				ack_ok = TRUE;
				
			p=strstr((const char*)USART3_RX_BUF,"TRVBP");
			if((p1=strstr((const char*)p,"#"))!=NULL)
			{
				//调用异或和函数来校验回文	
				//length = strlen((const char *)(USART3_RX_BUF));
				length = p1 - p +1;
				//校验数据
				result = Check_Xor_Sum((char *)(p),length-5);
				BSP_Printf("result:%d\r\n",result);
				
				//取字符串中的校验值
				p_temp = (p+length-5);
				offset = 3;
				for(index = 0;index < offset;index++)
				{
					temp_array[index] = p_temp[index];
				
				}
				//校验值转化为数字，并打印
				result_temp = atoi((const char *)(temp_array));	
				BSP_Printf("result_temp:%d\r\n",result_temp);			
				
				//回文正确
				if(result == result_temp)
				{
					//收到设备登陆信息的回文
					if(strstr((const char*)p,"TRVBP00"))
					{
						BSP_Printf("收到设备登陆信息的回文\r\n");
						Flag_Receive_Login = 0xAA;					
					}
					else
					{
						//收到重发命令
						if(strstr((const char*)p,"TRVBP98"))
						{
							BSP_Printf("收到服务器重发命令\r\n");
							Flag_Receive_Resend = 0xAA;
						}
						else
						{
							//收到心跳回文
							if(strstr((const char*)p,"TRVBP01"))
							{
								BSP_Printf("收到服务器心跳回文\r\n");
								Flag_Receive_Heart = 0xAA;
							}
							else
							{
								//收到开启设备指令
								if(strstr((const char*)p,"TRVBP03"))
								{
									BSP_Printf("收到服务器开启设备指令\r\n");
									Flag_Receive_Enable = 0xAA;
									//这里需要判断要打开的设备是不是已经在运行了，
									//如果是，那就不理这条命令，让服务器通过下一条心跳判断
									//如果不是，需要保存信息到一个新的buf里面在主循环处理
								}
								else
								{
									//收到运行结束回文
									if(strstr((const char*)p,"TRVBP05"))
									{
										BSP_Printf("收到服务器运行结束回文\r\n");
										Flag_Receive_Device_OK = 0xAA;
									}
									else
									{
										BSP_Printf("不存在的服务器指令\r\n");
										Flag_Check_error = 0xAA;
									}
								}
							}
						}
					}
				}   //回文正确
				//回文异常	
				if(result != result_temp)
				{
					//置错误重发标志
					//这里不再判定是那条语句接收出错，因为对错误的内容的进行判定，意义不大，由服务器程序来判定重发的内容
					//同理，嵌入式程序在接收到服务器的重发请求时，也要判定是要重发那条语句
					BSP_Printf("服务器指令校验错\r\n");
					Flag_Check_error = 0xAA;
					//Clear_Usart3();
				}			
			}
			else
			{
				BSP_Printf("服务器指令不完整\r\n");
				Flag_Check_error = 0xAA;
			}
			//服务器消息的清空放在这里了
			Clear_Usart3();	
			BSP_Printf("current_cmd: %d\r\n", current_cmd);
		}
		TIM_SetCounter(TIM6,0); 
	}
}



//定时器7中断服务程序		    
void TIM7_IRQHandler(void)
{ 	
	if (TIM_GetITStatus(TIM7, TIM_IT_Update) != RESET)//是更新中断
	{	 		
		TIM_ClearITPendingBit(TIM7, TIM_IT_Update  );  //清除TIM7更新中断标志    
	  USART3_RX_STA|=1<<15;	//标记接收完成
		TIM_Cmd(TIM7, DISABLE);  //关闭TIM7
	}	    
}


	 



//通用定时器6中断初始化
//这里选择为APB1的1倍，而APB1为24M
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz 
//arr：自动重装值。
//psc：时钟预分频数	
void TIM6_Int_Init(u16 arr,u16 psc)
{	
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);				//TIM6时钟使能    
	
	//定时器TIM6初始化
	TIM_TimeBaseStructure.TIM_Period = arr;                     //设置在下一个更新事件装入活动的自动重装载寄存器周期的值	
	TIM_TimeBaseStructure.TIM_Prescaler =psc;                   //设置用来作为TIMx时钟频率除数的预分频值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数模式
	TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);             //根据指定的参数初始化TIMx的时间基数单位
 
	TIM_ITConfig(TIM6,TIM_IT_Update,ENABLE );                   //使能指定的TIM6中断,允许更新中断
	
	//TIM_Cmd(TIM6,ENABLE);//开启定时器6
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3 ;//抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
	
}

//TIMER7的初始化 用在USART3（对接SIM800）的中断接收程序/////////
//通用定时器7中断初始化
//这里选择为APB1的1倍，而APB1为24M
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz 
//arr：自动重装值。
//psc：时钟预分频数		 
void TIM7_Int_Init(u16 arr,u16 psc)
{	
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);//TIM7时钟使能    
	
	//定时器TIM7初始化
	TIM_TimeBaseStructure.TIM_Period = arr;                     //设置在下一个更新事件装入活动的自动重装载寄存器周期的值	
	TIM_TimeBaseStructure.TIM_Prescaler =psc;                   //设置用来作为TIMx时钟频率除数的预分频值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数模式
	TIM_TimeBaseInit(TIM7, &TIM_TimeBaseStructure);             //根据指定的参数初始化TIMx的时间基数单位
 
	TIM_ITConfig(TIM7,TIM_IT_Update,ENABLE );                   //使能指定的TIM7中断,允许更新中断
	
	TIM_Cmd(TIM7,ENABLE);//开启定时器7
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2 ;	//抢占优先级0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		    	//子优先级2
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			      	//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
	
}




