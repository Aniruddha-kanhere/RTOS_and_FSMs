#include "common.h"

#define NUM_SECTORS_TO_READ (100)

osThreadId_t Thread_Test_SD_ID,Thread_Makework_ID,Thread_Idle_counter_ID;
osMessageQueueId_t SPI_message_Q;

SD_DEV dev[1];          // SD device descriptor
uint8_t buffer[512];    // Buffer for SD read or write data

void Thread_Makework(void * a)
{	
	Thread_Makework_ID = osThreadGetId();
	
	static int n=2;
	static int done = 0;
	static double my_pi=3.0;
	double term, prev_pi=0.0;
		
	// Nilakantha Series to approximate pi
	while(!done) 
	{	    
		term = 4.0/(n*(n+1.0)*(n+2.0));
		
		if (!(n%4)) 
		{ // is multiple of four
			term = -term;
		}
		
		prev_pi = my_pi;
		my_pi += term;
		
		if (my_pi != prev_pi) 
		{
			n += 2;
		}
		else 
		{
			// Done with approximating pi
			done = 1; 
		}
		
		osThreadYield ();
	}
  	
}

void Error_Handler(void) 
{
	Control_RGB_LEDs(1, 0, 0); // Light red LED
	while (1)
		;
}

void Thread_Test_SD(void *a) 
{
	// Write test data to given block (sector_num) in flash. 
	// Read it back, compute simple checksum to confirm it is correct.
	//Thread_Test_SD_ID = osThreadGetId();
		
	int i;
	DWORD sector_num = 0, read_sector_count=0; 
	uint32_t sum=0;
	SDRESULTS res;
	//static char err_color_code = 0; // xxxxxRGB
	
	if (SD_Init(dev) != SD_OK) 
	{
		Error_Handler(); // Initialization error
	}

	Control_RGB_LEDs(0, 1, 1); // Cyan: initialized OK
	while (1) 
	{
		for (read_sector_count=0; read_sector_count < NUM_SECTORS_TO_READ; read_sector_count++) 
		{
			// erase buffer
			for (i=0; i<SD_BLK_SIZE; i++)
				buffer[i] = 0;
			// perform SD card read
			res = SD_Read(dev, (void *)buffer, sector_num, 0, 512);	
			if (res != SD_OK) 
			{ // Was read was OK?
				Error_Handler(); // Read error
			}
			else 
			{
				Control_RGB_LEDs(0, 0, 1); // Blue: Read OK
			}
			sector_num++; // Advance to next sector
		}
		// erase buffer
		for (i=0; i<SD_BLK_SIZE; i++)
			buffer[i] = 0;
		// Load sample data into buffer
		*(uint64_t *)(&buffer[0]) = 0xFEEDDC0D;
		*(uint64_t *)(&buffer[508]) = 0xACE0FC0D;
		// SD card write to sector_num
		res = SD_Write(dev, (void *) buffer, sector_num);
		if (res != SD_OK) { // Was write completed OK?
			Error_Handler(); // Write error
		} 
		Control_RGB_LEDs(1, 0, 1); // Magenta: Write OK
		// erase buffer
		for (i=0; i<SD_BLK_SIZE; i++)
			buffer[i] = 0;
		// request SD card read to verify contents written correctly
		res = SD_Read(dev, (void *)buffer, sector_num, 0, 512);	
		if (res != SD_OK) { // Was verify read OK?
			Error_Handler(); // Verify read error
		} 
		Control_RGB_LEDs(0, 0, 1); // Blue: Verify read OK
		for (i = 0, sum = 0; i < SD_BLK_SIZE; i++)
			sum += buffer[i];		// Compute checksum
		if (sum != 0x0569) {
			Error_Handler(); // Checksum error
		} 
		Control_RGB_LEDs(1, 1, 1); // White: Checksum OK
		
		osThreadYield ();
	} 
}

void Thread_Idle_counter(void* arg)
{
	(void)arg;
	
	Thread_Idle_counter_ID = osThreadGetId();
	
	//osThreadSuspend(Thread_Makework_ID);
	//osThreadSuspend(Thread_Test_SD_ID);
	volatile uint64_t curr_count = osKernelGetTickCount();
	uint32_t osTick_Freq = osKernelGetTickFreq();
	volatile uint32_t difference_count = idle_counter;
	osDelay(osTick_Freq);
	difference_count = idle_counter - difference_count;
	curr_count = osKernelGetTickCount() - curr_count;
	//0x4890CF = 4755663            //2377022
	
	//osThreadResume(Thread_Makework_ID);
	//osThreadResume(Thread_Test_SD_ID);
	
	osThreadSuspend(Thread_Idle_counter);
}

int main(void) 
{	
	Init_Debug_Signals();
	Init_RGB_LEDs();
	Control_RGB_LEDs(1,1,0);	// Yellow - starting up
  
	SystemCoreClockUpdate();
	
	osKernelInitialize();
		
	SPI_message_Q = NULL;
	
	SPI_message_Q = osMessageQueueNew(8, sizeof(char), NULL);
		
	if(SPI_message_Q == NULL)
		Error_Handler();
		
	
	osThreadNew(Thread_Test_SD, NULL, NULL);
	//osThreadNew(Thread_Makework, NULL, NULL);		
	
	//Call this (Thread_Idle_counter) only if you wanna calculate the value of idle counter
	//osThreadNew(Thread_Idle_counter, NULL, NULL);	
	
	osKernelStart();                      // Start thread execution	
	
  for (;;) {}
	//DEBUG_TOGGLE(DBG_7);}	
}
