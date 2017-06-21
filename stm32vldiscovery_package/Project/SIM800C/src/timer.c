#include "timer.h"
#include "usart.h"
#include "usart3.h"
#include "SIM800.h"
#include "string.h"  
#include "stdlib.h"  
#include "device.h"

extern void Reset_Device_Status(u8 status);

//定时器6中断服务程序		    
void TIM6_DAC_IRQHandler(void)
{
	u8 index;
	if(TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)					  //是更新中断
	{	
		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);  					//清除TIM6更新中断标志

		//BSP_Printf("TIM6_S Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		//BSP_Printf("TIM6_S HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);

	
		//再次发送心跳包的定时计数
		if(dev.hb_timer >= HB_1_MIN)
		{
			if(dev.status == CMD_IDLE)
			{
				BSP_Printf("TIM6: HB Ready\r\n");
				Reset_Device_Status(CMD_HB);
			}
		}
		else
			dev.hb_timer++;

		for(index=DEVICE_01; index<DEVICEn; index++)
		{
			switch(g_device_status[index].power)
			{
				case ON:
					if(g_device_status[index].total==0)
						g_device_status[index].power = UNKNOWN;
					else
					{
						if(g_device_status[index].passed >= g_device_status[index].total)
						{
							g_device_status[index].passed=g_device_status[index].total=0;
							g_device_status[index].power=OFF;
							Device_OFF(index);
							if(dev.status == CMD_IDLE)
							{
								BSP_Printf("TIM6: 设置设备状态为CLOSE_DEVICE\r\n");
								Reset_Device_Status(CMD_CLOSE_DEVICE);
							}
						}
						else
							g_device_status[index].passed++;
					}
					break;
					
				case OFF:
					if(g_device_status[index].total!=0)
						g_device_status[index].power = UNKNOWN;
					break;
					
				case UNKNOWN:
				default:
				break;
			}
		}
	
		switch(dev.status)
		{
			case CMD_LOGIN:
				if(dev.msg_expect & MSG_DEV_LOGIN)
				{
					if(dev.reply_timeout >= HB_1_MIN)
					{
						dev.msg_expect &= ~MSG_DEV_LOGIN;
						dev.reply_timeout = 0;
						dev.msg_timeout++;
					}
					dev.reply_timeout++;
				}
			break;
			case CMD_HB:
				if(dev.msg_expect & MSG_DEV_HB)
				{
					if(dev.reply_timeout >= HB_1_MIN)
					{
						dev.msg_expect &= ~MSG_DEV_HB;
						dev.reply_timeout = 0;
						dev.msg_timeout++;
					}
					dev.reply_timeout++;
				}				
			break;
			case CMD_CLOSE_DEVICE:
				if(dev.msg_expect & MSG_DEV_CLOSE)
				{
					if(dev.reply_timeout >= HB_1_MIN)
					{
						dev.msg_expect &= ~MSG_DEV_CLOSE;
						dev.reply_timeout = 0;
						dev.msg_timeout++;
					}
					dev.reply_timeout++;
				}				
			break;
			default:
			break;
		}

		if(dev.msg_timeout >= NUMBER_MSG_MAX_RETRY)
			dev.need_reset = TRUE;
		//BSP_Printf("TIM6_E Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		//BSP_Printf("TIM6_E HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);
		
		TIM_SetCounter(TIM6,0); 
	}
}

//定时器7中断服务程序		    
void TIM7_IRQHandler(void)
{ 	
	u16 length = 0; 
	u8 result = 0;
	u8 result_temp = 0;
	u8 index = 0;
	char *p, *p1;
	char *p_temp = NULL;
	u8 temp_array[3] = {0};
	u8 offset = 0;

	if (TIM_GetITStatus(TIM7, TIM_IT_Update) != RESET)//是更新中断
	{	 		
		TIM_ClearITPendingBit(TIM7, TIM_IT_Update);  //清除TIM7更新中断标志    
		USART3_RX_STA|=1<<15;	//标记接收完成
		TIM_Cmd(TIM7, DISABLE);  //关闭TIM7
		
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]=0;	//添加结束符 

		memset(dev.usart_data, 0, sizeof(dev.usart_data));
		strcpy(dev.usart_data, (const char *)USART3_RX_BUF);

		BSP_Printf("USART BUF:%s\r\n",USART3_RX_BUF);

		BSP_Printf("TIM7_S Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		BSP_Printf("TIM7_S HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);
		
		if((strstr((const char*)USART3_RX_BUF,"CLOSED")!=NULL) || (strstr((const char*)USART3_RX_BUF,"+PDP: DEACT")!=NULL))
		{
			dev.msg_recv |= MSG_DEV_RESET;
			dev.need_reset = TRUE;
		}
		else
		{	
			//CMD_NONE: 连接之前的状态
			if( (dev.status == CMD_NONE) || (dev.status == CMD_LOGIN) || (dev.status == CMD_HB) || (dev.status == CMD_CLOSE_DEVICE)
				 || (dev.status == CMD_OPEN_DEVICE))
			{
				if(dev.msg_expect & MSG_DEV_ACK)
				{
					if(strstr((const char*)USART3_RX_BUF, dev.atcmd_ack) != NULL)
					{
						dev.msg_recv |= MSG_DEV_ACK;
						dev.msg_expect &= ~MSG_DEV_ACK;
						//必须在收到Send Ok 回文后才允许接收服务器回文
						if(strstr(dev.atcmd_ack, "SEND OK")!=NULL) 
						{
							switch(dev.status)
							{
								case CMD_LOGIN:
									dev.msg_expect |= MSG_DEV_LOGIN;
								break;
								case CMD_HB:
									dev.msg_expect |= MSG_DEV_HB;
								break;
								case CMD_CLOSE_DEVICE:
									dev.msg_expect |= MSG_DEV_CLOSE;
								break;
								default:
									BSP_Printf("Wrong Status: %d\r\n", dev.status);
								break;
							}
						}	
					}
				}
			}	

			if((p=strstr((const char*)USART3_RX_BUF,"TRVBP"))!=NULL)
			{
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
						if(strstr((const char*)p,"TRVBP00")!=NULL)
						{
							BSP_Printf("收到设备登陆信息的回文\r\n");
							if((dev.status == CMD_LOGIN) && (dev.msg_expect & MSG_DEV_LOGIN))
							{
								Reset_Device_Status(CMD_IDLE);
							}
						}
						else
						{
							//收到心跳回文
							if(strstr((const char*)p,"TRVBP01")!=NULL)
							{
								BSP_Printf("收到服务器心跳回文\r\n");
								if((dev.status == CMD_HB) && (dev.msg_expect & MSG_DEV_HB))
								{
									Reset_Device_Status(CMD_IDLE);
								}
							}
							else
							{
								//收到开启设备指令
								if(strstr((const char*)p,"TRVBP03")!=NULL)
								{
									BSP_Printf("收到服务器开启设备指令\r\n");

									if(dev.status == CMD_IDLE)
									{
										Reset_Device_Status(CMD_OPEN_DEVICE);
										strncpy(dev.device_on_cmd_string, p, p1 - p +1);
									}
								}
								else
								{
									//收到运行结束回文
									if(strstr((const char*)p,"TRVBP05")!=NULL)
									{
										BSP_Printf("收到服务器运行结束回文\r\n");
										if((dev.status == CMD_CLOSE_DEVICE) && (dev.msg_expect & MSG_DEV_CLOSE))
										{
											Reset_Device_Status(CMD_IDLE);
										}
									}
									else
									{
										BSP_Printf("不存在的服务器指令\r\n");
									}
								}
							}
						}
					}   //回文正确			
				}
			}		
			//服务器消息的清空放在这里了
			Clear_Usart3();	
		}
		
		BSP_Printf("TIM7_E Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		BSP_Printf("TIM7_E HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);
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
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2 ;//抢占优先级0
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

