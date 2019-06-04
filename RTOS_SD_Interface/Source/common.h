#ifndef COMMON_H_
#define COMMON_H_

#include "integer.h"
#include <MKL25Z4.h>
#include "spi_io.h"
#include "sd_io.h"
#include "LEDs.h"
#include "debug.h"
#include "RTE_Components.h"
#include  CMSIS_device_header
#include "cmsis_os2.h"
#include "RTX_Config.h"

extern osThreadId_t Thread_Test_SD_ID,Thread_Makework_ID,Thread_Idle_counter_ID;

extern osMessageQueueId_t SPI_message_Q;

void Error_Handler(void);

#endif
