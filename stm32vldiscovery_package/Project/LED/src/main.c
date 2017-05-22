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
#include "delay.h"
#include "timer.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define  LSE_FAIL_FLAG  0x80
#define  LSE_PASS_FLAG  0x100
/* Private macro -------------------------------------------------------------*/
/* Private consts ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

u32 LSE_Delay = 0;
u32 count = 0;

//static void LED_Init(void)
//{
// 
// GPIO_InitTypeDef  GPIO_InitStructure;
// 	
// RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 
//	
// GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;				 
// GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		
// GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
// GPIO_Init(GPIOB, &GPIO_InitStructure);	

// GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
// GPIO_Init(GPIOB, &GPIO_InitStructure);	
//	
// GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
// GPIO_Init(GPIOB, &GPIO_InitStructure);	
//	
// GPIO_ResetBits(GPIOB,GPIO_Pin_3);	
// GPIO_ResetBits(GPIOB,GPIO_Pin_4);
// GPIO_ResetBits(GPIOB,GPIO_Pin_5);	

//}

//static void LED_ON(void)
//{
// 			
// GPIO_SetBits(GPIOB,GPIO_Pin_3);						
// GPIO_SetBits(GPIOB,GPIO_Pin_4);		
// GPIO_SetBits(GPIOB,GPIO_Pin_5);		
//}

//static void LED_OFF(void)
//{
// 			
// GPIO_ResetBits(GPIOB,GPIO_Pin_3);		
// GPIO_ResetBits(GPIOB,GPIO_Pin_4);	
// GPIO_ResetBits(GPIOB,GPIO_Pin_5);		

//}


static void LED_Init(void)
{
 
 GPIO_InitTypeDef  GPIO_InitStructure;
 	
 RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 
	
 GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;				 
 GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		
 GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		 
 GPIO_Init(GPIOB, &GPIO_InitStructure);	

 GPIO_ResetBits(GPIOB,GPIO_Pin_5);	

}

static void LED_ON(void)
{
 			
	
 GPIO_SetBits(GPIOB,GPIO_Pin_5);		
}

static void LED_OFF(void)
{
 			

 GPIO_ResetBits(GPIOB,GPIO_Pin_5);		

}


//////////////////////////////////////////////

/* Private function prototypes -----------------------------------------------*/

void RCC_LSECheck(void);	
/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
  /* Enable GPIOx Clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	
	delay_init();
	
	LED_Init();
	
	while(1)
	{
		LED_ON();
		for(count = 0; count < 5; count++)
		{
			delay_ms(200);
		}
		
		LED_OFF();
		for(count = 0; count < 5; count++)
		{
			delay_ms(200);
		}


	}
	

}



	





void RCC_LSECheck(void)
{
	while(1)
  {
    if(LSE_Delay < LSE_FAIL_FLAG)
    {
      /* check whether LSE is ready, with 4 seconds timeout */
      delay_ms(500);
      LSE_Delay += 0x10;
      if(RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET)
      {
        /* Set flag: LSE PASS */
        LSE_Delay |= LSE_PASS_FLAG;
        /* Turn Off Led4 */
        //STM32vldiscovery_LEDOff(LED4);
        /* Disable LSE */
        RCC_LSEConfig(RCC_LSE_OFF);
        break;
      }        
    }
    
    /* LSE_FAIL_FLAG = 0x80 */  
    else if(LSE_Delay >= LSE_FAIL_FLAG)
    {          
      if(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
      {
        /* Set flag: LSE FAIL */
        LSE_Delay |= LSE_FAIL_FLAG;
        /* Turn On Led4 */
        //STM32vldiscovery_LEDOn(LED4);
      }        
      /* Disable LSE */
      RCC_LSEConfig(RCC_LSE_OFF);
      break;
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
