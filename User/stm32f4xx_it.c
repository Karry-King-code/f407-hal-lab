/* 中断服务函数：HAL 库版只需要 SysTick 心跳，其他留默认弱定义即可 */
#include "stm32f4xx_hal.h"

void NMI_Handler(void) {}
void HardFault_Handler(void) { while (1) {} }
void MemManage_Handler(void) { while (1) {} }
void BusFault_Handler(void)  { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}

/* HAL_Delay 靠 uwTick 计数，uwTick 在这里每 1ms 加一 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
