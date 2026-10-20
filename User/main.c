/*
 * 节点 3 · 串口收发（HAL 库版 · 练习模板）
 *
 * 目标行为与寄存器版相同：中断接收 + 环形缓冲 + 回显，一个字节不丢。
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ 去哪找函数：全部 UART 函数声明在这个文件（Keil 打开后 F12 跳转）：  │
 * │   Libraries/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_uart.h        │
 * │ 本节点要用的五个（行号实测于 2.17.0 版头文件）：                  │
 * │   718  HAL_UART_Init(huart)                                      │
 * │        把波特率/字长/收发使能一次配好（= BRR+CR1 的代办员）        │
 * │   745  HAL_UART_Transmit(huart, buf, 长度, 超时ms)                │
 * │        阻塞发送（= "等TXE再写DR"的循环打包）                       │
 * │   748  HAL_UART_Receive_IT(huart, buf, 长度)                      │
 * │        挂一个中断接收：字节到了自动存进 buf 并触发回调             │
 * │   773  HAL_UART_RxCpltCallback(huart)                            │
 * │        上面收完后 HAL 反过来叫你（弱定义，你覆写它）               │
 * │   —— 还有 HAL_UART_MspInit(huart)：HAL_UART_Init 会自动调它，      │
 * │      时钟/GPIO/NVIC 这些"芯片相关"的活全写在这（= 寄存器版①②搬家）│
 * └─────────────────────────────────────────────────────────────────┘
 *
 * 你的任务四处：
 *   A. MX_UART1_Init：给 huart1 填七个字段 + 调 HAL_UART_Init
 *   B. 覆写 HAL_UART_MspInit：时钟 + PA9/PA10 复用 AF7 + NVIC（寄存器版①②搬家）
 *   C. 覆写 HAL_UART_RxCpltCallback：字节进环形缓冲 + 重新挂 Receive_IT
 *   D. main 开头挂第一个 Receive_IT（已给）
 * 主循环回显已给。
 */

#include "stm32f4xx_hal.h"

#define BUF_LEN 64
volatile uint8_t  rbuf[BUF_LEN];
volatile uint16_t rhead = 0, rtail = 0;
uint8_t rx_byte;                          /* 中断接收的落脚点（Receive_IT 往这存） */

UART_HandleTypeDef huart1;                /* 串口句柄：HAL 靠它认得这颗串口 */

static void MX_UART1_Init(void);

int main(void)
{
  HAL_Init();
  MX_UART1_Init();                        /* A */

  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   /* D：挂第一个中断接收 */

  while (1)
  {
    if (rtail != rhead)                   /* 缓冲有货（已给） */
    {
      uint8_t c = rbuf[rtail];
      rtail = (rtail + 1) % BUF_LEN;
      HAL_UART_Transmit(&huart1, &c, 1, 100);   /* 回显，最多等 100ms */
    }
  }
}

/* A：七个字段全照寄存器版概念填，然后一句 HAL_UART_Init 全部生效 */
static void MX_UART1_Init(void)
{
   huart1.Instance          = USART1;                 /*哪颗串口 */
   huart1.Init.BaudRate     = 115200;                 /* 波特率（HAL 替你算 BRR=0x8B） */
   huart1.Init.WordLength   = UART_WORDLENGTH_8B;      /* 8 数据位 */
   huart1.Init.StopBits     = UART_STOPBITS_1;        /* 1 停止位 */
   huart1.Init.Parity       = UART_PARITY_NONE;       /*无校验 */
   huart1.Init.Mode         = UART_MODE_TX_RX;        /*收发都开（= TE|RE） */
   huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;    /*无流控 */
   huart1.Init.OverSampling = UART_OVERSAMPLING_16;   /*16 倍过采样（BRR 公式里的那个 16） */
   HAL_UART_Init(&huart1); 
}

/* B：HAL_UART_Init 每次都会自动调这个回调——
      时钟/GPIO/NVIC 这些芯片相关的活全在这（= 寄存器版的①②两步整体搬家）*/
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
      GPIO_InitTypeDef gpio = {0};
			if(huart -> Instance == USART1)
			{
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_USART1_CLK_ENABLE();
				
				gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
				gpio.Mode      = GPIO_MODE_AF_PP; ///* 复用推挽，不是 OUTPUT_PP */
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF7_USART1;		
				HAL_GPIO_Init(GPIOA, &gpio);
			}
			//配置 USART1 串口中断优先级，并开启 USART1 的 NVIC 中断通道
     HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
     HAL_NVIC_EnableIRQ(USART1_IRQn);
}


/* C：取消注释并补全。每收完指定字节数（这里=1），HAL 自动叫这个*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  rbuf[rhead] = rx_byte;
  rhead = (rhead + 1) % BUF_LEN;
  HAL_UART_Receive_IT(&huart, &rx_byte, 1);   //← 重新挂上，继续收下一个
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
