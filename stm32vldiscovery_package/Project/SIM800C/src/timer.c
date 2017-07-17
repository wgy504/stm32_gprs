#include "timer.h"
#include "usart.h"
#include "usart3.h"
#include "SIM800.h"
#include "string.h"  
#include "stdlib.h"  
#include "device.h"

extern void Reset_Device_Status(u8 status);

//��ʱ��6�жϷ������		    
void TIM6_DAC_IRQHandler(void)
{
	u8 index;
	if(TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)					  //�Ǹ����ж�
	{	
		TIM_ClearITPendingBit(TIM6, TIM_IT_Update);  					//���TIM6�����жϱ�־

		//BSP_Printf("TIM6_S Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		//BSP_Printf("TIM6_S HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);

	
		//�ٴη����������Ķ�ʱ����
		if(dev.hb_timer >= HB_1_MIN)
		{
			//for(index=DEVICE_01; index<DEVICEn; index++)
			{
				//BSP_Printf("TIM6 Dev[%d].total: %d, passed: %d\n", index, g_device_status[index].total, g_device_status[index].passed);
			}
			
			if(dev.status == CMD_IDLE)
			{
				BSP_Printf("TIM6: HB Ready\r\n");
				dev.msg_recv = 0;	
				Reset_Device_Status(CMD_HB);
			}
			dev.hb_timer = 0;
		}
		else
			dev.hb_timer++;

		for(index=DEVICE_01; index<DEVICEn; index++)
		{
			switch(g_device_status[index].power)
			{
				case ON:
				{
					if(g_device_status[index].total==0)
					{
						BSP_Printf("Error: Dev[%d] %d %d %d\n", index, g_device_status[index].power, g_device_status[index].total, g_device_status[index].passed);
						g_device_status[index].power = UNKNOWN;
					}
					else
					{
						if(g_device_status[index].passed >= g_device_status[index].total)
						{
							BSP_Printf("Dev[%d]: %d %d %d\n", index, g_device_status[index].power, g_device_status[index].total, g_device_status[index].passed);					
							g_device_status[index].passed=g_device_status[index].total=0;
							g_device_status[index].power=OFF;
							Device_OFF(index);
							if(dev.status == CMD_IDLE)
							{
								BSP_Printf("TIM6: �����豸״̬ΪCLOSE_DEVICE\r\n");
								dev.msg_recv = 0;
								Reset_Device_Status(CMD_CLOSE_DEVICE);
							}
						}
						else
							g_device_status[index].passed++;
					}
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
						BSP_Printf("[%d]: Ready to send Msg MSG_DEV_LOGIN again\n", dev.msg_timeout);
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
						BSP_Printf("[%d]: Ready to send Msg MSG_DEV_HB again\n", dev.msg_timeout);
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
						BSP_Printf("[%d]: Ready to send Msg MSG_DEV_CLOSE again\n", dev.msg_timeout);
					}
					dev.reply_timeout++;
				}				
			break;
			default:
			break;
		}

		if(dev.msg_timeout >= NUMBER_MSG_MAX_RETRY)
		{	
			BSP_Printf("Retry sending too many times, need reset\n");	
			dev.msg_timeout = 0;
			dev.need_reset = ERR_RETRY_TOO_MANY_TIMES;
		}	
		//BSP_Printf("TIM6_E Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		//BSP_Printf("TIM6_E HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);
		
		TIM_SetCounter(TIM6,0); 
	}
}

//��ʱ��7�жϷ������		    
void TIM7_IRQHandler(void)
{ 	
	u16 length = 0; 
	u8 result = 0;
	u8 result_temp = 0;
	char *uart_data_left;
	char *p, *p1;
	char *p_temp = NULL;
	u8 msg_wait_check=MSG_STR_ID_MAX;

	if (TIM_GetITStatus(TIM7, TIM_IT_Update) != RESET)//�Ǹ����ж�
	{	 		
		TIM_ClearITPendingBit(TIM7, TIM_IT_Update);  //���TIM7�����жϱ�־    
		USART3_RX_STA|=1<<15;	//��ǽ������
		TIM_Cmd(TIM7, DISABLE);  //�ر�TIM7
		
		USART3_RX_BUF[USART3_RX_STA&0X7FFF]=0;	//��ӽ����� 

		BSP_Printf("USART BUF:%s\r\n",USART3_RX_BUF);

		//BSP_Printf("TIM7_S Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		//BSP_Printf("TIM7_S HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);
		
		if((strstr((const char*)USART3_RX_BUF,"CLOSED")!=NULL) || (strstr((const char*)USART3_RX_BUF,"PDP: DEACT")!=NULL) || (strstr((const char*)USART3_RX_BUF,"PDP DEACT")!=NULL))
		{
			//dev.msg_recv |= MSG_DEV_RESET;
			dev.need_reset = ERR_DISCONNECT;
		}
		else
		{	
			if( (dev.status == CMD_NONE) || (dev.status == CMD_LOGIN) || (dev.status == CMD_HB) || (dev.status == CMD_CLOSE_DEVICE)
				 || (dev.status == CMD_OPEN_DEVICE))
			{
				if(dev.msg_expect & MSG_DEV_ACK)
				{
					if((strstr(dev.atcmd_ack, ">")!=NULL) && (strstr((const char*)USART3_RX_BUF,"ERROR")!=NULL))
					{
						//dev.msg_recv |= MSG_DEV_RESET;
						dev.need_reset = ERR_SEND_CMD;
					}
					
					if(strstr((const char*)USART3_RX_BUF, dev.atcmd_ack) != NULL)
					{
						memset(dev.usart_data, 0, sizeof(dev.usart_data));
						strcpy(dev.usart_data, (const char *)USART3_RX_BUF);	
						
						dev.msg_recv |= MSG_DEV_ACK;
						dev.msg_expect &= ~MSG_DEV_ACK;
						//�������յ�Send Ok ���ĺ��������շ���������
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
								case CMD_OPEN_DEVICE:
									BSP_Printf("Open Device No Need Resp\r\n");
									Reset_Device_Status(CMD_IDLE);
								break;
								default:
									BSP_Printf("Wrong Status: %d\r\n", dev.status);
								break;
							}
						}	
					}
				}
			}	

			if((dev.status == CMD_LOGIN) && (dev.msg_expect & MSG_DEV_LOGIN))
			{
				msg_wait_check = MSG_STR_ID_LOGIN;
			}
			else if((dev.status == CMD_HB) && (dev.msg_expect & MSG_DEV_HB))
			{
				msg_wait_check = MSG_STR_ID_HB;
			}
			else if((dev.status == CMD_CLOSE_DEVICE) && (dev.msg_expect & MSG_DEV_CLOSE))
			{
				msg_wait_check = MSG_STR_ID_CLOSE;
			}
			else if(dev.status == CMD_IDLE)
			{
				msg_wait_check = MSG_STR_ID_OPEN;
			}

			if(msg_wait_check != MSG_STR_ID_MAX)
			{
				uart_data_left = (char *)USART3_RX_BUF;
				while((p=strstr(uart_data_left, msg_id_s[msg_wait_check]))!=NULL)
				{
					BSP_Printf("Check MSG:%s\n",uart_data_left);
					if((p1=strstr((const char*)p,"#"))!=NULL)
					{
						//�������ͺ�����У�����	
						length = p1 - p +1;
						//У������
						result = Check_Xor_Sum((char *)(p),length-5);
						BSP_Printf("result:%d\r\n",result);
						
						//ȡ�ַ����е�У��ֵ,У��ֵת��Ϊ���֣�����ӡ
						result_temp = atoi((const char *)(p+length-5));	
						BSP_Printf("result_temp:%d\r\n",result_temp);
						
						//������ȷ
						if(result == result_temp)
						{
							p_temp = p+(MSG_STR_LEN_OF_ID+1)+(MSG_STR_LEN_OF_LENGTH+1);
							if((p_temp+(MSG_STR_LEN_OF_SEQ+1)) < p1)
							{
								//BSP_Printf("seq:%d %d\r\n",dev.msg_seq, atoi(p_temp));
								if(msg_wait_check == MSG_STR_ID_OPEN)
								{
									BSP_Printf("Recv Seq:%d Msg:%d from Server\n", atoi(p_temp), msg_wait_check);
									dev.msg_seq_s = atoi(p_temp);
									Reset_Device_Status(CMD_OPEN_DEVICE);
									strncpy(dev.device_on_cmd_string, p, p1 - p +1);
									break;
								}
								else if((msg_wait_check == MSG_STR_ID_LOGIN) || (msg_wait_check == MSG_STR_ID_HB) || 
										(msg_wait_check == MSG_STR_ID_CLOSE))
								{
									if(atoi(p_temp) == dev.msg_seq)
									{
										BSP_Printf("Recv Seq:%d Msg:%d from Server\n", dev.msg_seq, msg_wait_check);
										Reset_Device_Status(CMD_TO_IDLE);
										break;							
									}
								}
							}
						}
						uart_data_left = p1;
					}
					else
						break;
				}
			}
			
			/*
			     1. �ڽ��յĵط��ж�����˳�����ʹ�ܴ��ڽ���
			     2. �������½�����ɾ�ʹ�ܽ���, ��״̬�ж���Щ��Ϣ����Ч��
			*/
			Clear_Usart3();	
		}
		
		//BSP_Printf("TIM7_E Dev Status: %d, Msg expect: %d, Msg recv: %d\r\n", dev.status, dev.msg_expect, dev.msg_recv);
		//BSP_Printf("TIM7_E HB: %d, HB TIMER: %d, Msg TIMEOUT: %d\r\n", dev.hb_count, dev.hb_timer, dev.msg_timeout);
	}

}

//ͨ�ö�ʱ��6�жϳ�ʼ��
//����ѡ��ΪAPB1��1������APB1Ϊ24M
//arr���Զ���װֵ��
//psc��ʱ��Ԥ��Ƶ��
//��ʱ�����ʱ����㷽��:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=��ʱ������Ƶ��,��λ:Mhz 
//arr���Զ���װֵ��
//psc��ʱ��Ԥ��Ƶ��	
void TIM6_Int_Init(u16 arr,u16 psc)
{	
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);				//TIM6ʱ��ʹ��    
	
	//��ʱ��TIM6��ʼ��
	TIM_TimeBaseStructure.TIM_Period = arr;                     //��������һ�������¼�װ�����Զ���װ�ؼĴ������ڵ�ֵ	
	TIM_TimeBaseStructure.TIM_Prescaler =psc;                   //����������ΪTIMxʱ��Ƶ�ʳ�����Ԥ��Ƶֵ
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //����ʱ�ӷָ�:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM���ϼ���ģʽ
	TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);             //����ָ���Ĳ�����ʼ��TIMx��ʱ�������λ
 
	TIM_ITConfig(TIM6,TIM_IT_Update,ENABLE );                   //ʹ��ָ����TIM6�ж�,��������ж�
	
	//TIM_Cmd(TIM6,ENABLE);//������ʱ��6
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2 ;//��ռ���ȼ�0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		//�����ȼ�3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQͨ��ʹ��
	NVIC_Init(&NVIC_InitStructure);	//����ָ���Ĳ�����ʼ��VIC�Ĵ���
	
}

//TIMER7�ĳ�ʼ�� ����USART3���Խ�SIM800�����жϽ��ճ���/////////
//ͨ�ö�ʱ��7�жϳ�ʼ��
//����ѡ��ΪAPB1��1������APB1Ϊ24M
//arr���Զ���װֵ��
//psc��ʱ��Ԥ��Ƶ��
//��ʱ�����ʱ����㷽��:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=��ʱ������Ƶ��,��λ:Mhz 
//arr���Զ���װֵ��
//psc��ʱ��Ԥ��Ƶ��		 
void TIM7_Int_Init(u16 arr,u16 psc)
{	
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);//TIM7ʱ��ʹ��    
	
	//��ʱ��TIM7��ʼ��
	TIM_TimeBaseStructure.TIM_Period = arr;                     //��������һ�������¼�װ�����Զ���װ�ؼĴ������ڵ�ֵ	
	TIM_TimeBaseStructure.TIM_Prescaler =psc;                   //����������ΪTIMxʱ��Ƶ�ʳ�����Ԥ��Ƶֵ
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //����ʱ�ӷָ�:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM���ϼ���ģʽ
	TIM_TimeBaseInit(TIM7, &TIM_TimeBaseStructure);             //����ָ���Ĳ�����ʼ��TIMx��ʱ�������λ
 
	TIM_ITConfig(TIM7,TIM_IT_Update,ENABLE );                   //ʹ��ָ����TIM7�ж�,��������ж�
	
	TIM_Cmd(TIM7,ENABLE);//������ʱ��7
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2 ;	//��ռ���ȼ�0
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		    	//�����ȼ�2
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			      	//IRQͨ��ʹ��
	NVIC_Init(&NVIC_InitStructure);	//����ָ���Ĳ�����ʼ��VIC�Ĵ���
	
}

