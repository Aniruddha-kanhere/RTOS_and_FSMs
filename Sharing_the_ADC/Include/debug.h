#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <cmsis_os2.h>

#define MASK(x) (1UL << (x))

// Debug Signals on port B
#define DBG_1 1 	
#define DBG_2 2	  
#define DBG_3 3		
#define DBG_4 8		
#define DBG_5 9		
#define DBG_6 10  
#define DBG_7 11

// Debug Strobe on port E
#define DBG_STRB 4

// Debug Signal mapping 
#define DBG_TREADACC_POS		DBG_1 	

#define DBG_TREADTS_POS 		DBG_2

#define DBG_TREFILLSB_POS 	DBG_3
#define DBG_IRQTPM_POS			DBG_3

#define DBG_TUPDATESCR_POS	DBG_4

#define DBG_TSNDMGR_POS 		DBG_5
#define DBG_IRQ_ADC_POS			DBG_5

#define DBG_IRQDMA_POS 			DBG_6
#define DBG_CONTROLLER_POS	DBG_6

#define DBG_TIDLE						DBG_7

#define DEBUG_START(channel) { PTB->PSOR = MASK(channel); } 
#define DEBUG_STOP(channel) { PTB->PCOR = MASK(channel); }
#define DEBUG_TOGGLE(channel) { PTB->PTOR = MASK(channel); }

typedef struct Queue{
	int ChannelNum;	
	uint32_t *ResultPtr;
	osThreadId_t TID;
	int FlagNum;
	char valid;
}Queue;

extern osMessageQueueId_t my_os_q;
extern Queue ADC_q[5];

void Init_Debug_Signals(void);

#endif // DEBUG_H
