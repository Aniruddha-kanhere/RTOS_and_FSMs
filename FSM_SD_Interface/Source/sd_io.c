/*
 *  File: sd_io.c
 *  Author: Nelson Lombardo
 *  Year: 2015
 *  e-mail: nelson.lombardo@gmail.com
 *  License at the end of file.
 */
 
// Modified 2017 by Alex Dean (agdean@ncsu.edu) for teaching FSMs
// - Removed support for PC development (_M_IX86)
// - Split single-line loops & conditionals in source code for readability
// - Fused loops in SD_Read 
// _ Inlined __SD_Write_Block into SD_Write

#include "sd_io.h"
#include <MKL25Z4.h>
#include "debug.h"

/* Results of SD functions */
char SD_Errors[7][8] = {
    "OK",      
    "NOINIT",      /* 1: SD not initialized    */
    "ERROR",       /* 2: Disk error            */
    "PARERR",      /* 3: Invalid parameter     */
    "BUSY",        /* 4: Programming busy      */
    "REJECT",      /* 5: Reject data           */
    "NORESP"       /* 6: No response           */
};

/******************************************************************************
 Private Methods Prototypes - Direct work with SD card
******************************************************************************/

/**
    \brief Simple function to calculate power of two.
    \param e Exponent.
    \return Math function result.
*/
DWORD __SD_Power_Of_Two(BYTE e);

/**
     \brief Assert the SD card (SPI CS low).
 */
inline void __SD_Assert (void);

/**
    \brief Deassert the SD (SPI CS high).
 */
inline void __SD_Deassert (void);

/**
    \brief Change to max the speed transfer.
    \param throttle
 */
void __SD_Speed_Transfer (BYTE throttle);

/**
    \brief Send SPI commands.
    \param cmd Command to send.
    \param arg Argument to send.
    \return R1 response.
 */
DWORD __SD_Send_Cmd(BYTE cmd, DWORD arg);

/**
    \brief Get the total numbers of sectors in SD card.
    \param dev Device descriptor.
    \return Quantity of sectors. Zero if fail.
 */
DWORD __SD_Sectors (SD_DEV *dev);

/******************************************************************************
 Private Methods - Direct work with SD card
******************************************************************************/

DWORD __SD_Power_Of_Two(BYTE e)
{
    DWORD partial = 1;
    BYTE idx;
    for(idx=0; idx!=e; idx++) partial *= 2;
    return(partial);
}

inline void __SD_Assert(void){
    SPI_CS_Low();
}

inline void __SD_Deassert(void){
    SPI_CS_High();
}

void __SD_Speed_Transfer(BYTE throttle) {
    if(throttle == HIGH) SPI_Freq_High();
    else SPI_Freq_Low();
}

DWORD __SD_Send_Cmd(BYTE cmd, DWORD arg)
{
    static BYTE crc, res, cmd_copy;
    static DWORD arg_copy;
    static enum {State1, State2, State3, State1_sub, State2_sub, State3_sub, State_check} next_state = State_check;
		DWORD count=0;
	  
		switch(next_state)
		{
			case State_check:
				cmd_copy = cmd;
			  arg_copy = arg;
			  if(cmd_copy & 0x80)
				{
					next_state = State1_sub;
					cmd_copy = cmd_copy & 0x7F;
				}
				else
				{
					next_state = State1;
				}	
				
				break;
				
			case State1_sub:
				__SD_Deassert();
				SPI_RW(0xFF);
				__SD_Assert();
				SPI_RW(0xFF);

				// Send complete command set
				SPI_RW(CMD55);                        // Start and command index
				SPI_RW(0);          // Arg[31-24]
				SPI_RW(0);          // Arg[23-16]
			
			  next_state = State2_sub;
				break;
			
			case State2_sub:
				SPI_RW((BYTE)(arg >> 8 ));          // Arg[15-08]
				SPI_RW((BYTE)(arg >> 0 ));          // Arg[07-00]
				
				crc = 0x01;                         // Dummy CRC and stop
				SPI_RW(crc);                        //this is for CMD55,0. Thus no, checking
				
				SPI_Timer_On(5);			  
			  next_state = State3_sub;
				break;
			
			case State3_sub:
				do 
				{
					count++;
					res = SPI_RW(0xFF);
				} while((res & 0x80)&&(SPI_Timer_Status()==TRUE)&&(count<5));
				
				if(!(res & 0x80)||(SPI_Timer_Status()!=TRUE))
				{
					SPI_Timer_Off();
					if(res>1)
					{
						next_state = State_check;
						return(res);
					}
					else
					{
						next_state = State1;
					}
				}
				break;
				
			case State1:
				__SD_Deassert();
				SPI_RW(0xFF);
				__SD_Assert();
				SPI_RW(0xFF);

				// Send complete command set
				SPI_RW(cmd_copy);                        // Start and command index
				SPI_RW((BYTE)(arg_copy >> 24));          // Arg[31-24]
				SPI_RW((BYTE)(arg_copy >> 16));          // Arg[23-16]
			  next_state = State2;
				break;
			
			case State2:
				SPI_RW((BYTE)(arg_copy >> 8 ));          // Arg[15-08]
				SPI_RW((BYTE)(arg_copy >> 0 ));          // Arg[07-00]

				// CRC?
				crc = 0x01;                         // Dummy CRC and stop
				if(cmd_copy == CMD0) 
					crc = 0x95;         // Valid CRC for CMD0(0)
				if(cmd_copy == CMD8) 
					crc = 0x87;         // Valid CRC for CMD8(0x1AA)
				SPI_RW(crc);

				// Receive command response
				// Wait for a valid response in timeout of 5 milliseconds
				SPI_Timer_On(5);
				next_state = State3;
				break;
				
			case State3:
				do 
				{
					count++;
					res = SPI_RW(0xFF);
				} while((res & 0x80)&&(SPI_Timer_Status()==TRUE)&&(count<5));
				
				if(!(res & 0x80)||(SPI_Timer_Status()!=TRUE))
				{
					SPI_Timer_Off();
					next_state = State_check;
					return(res);
				}
				break;
		}
	  return(SENDING);
}

/*
BYTE __SD_Send_Cmd(BYTE cmd, DWORD arg)
{
    BYTE crc, res;

	// ACMD«n» is the command sequense of CMD55-CMD«n»
    if(cmd & 0x80) {
        cmd &= 0x7F;
        res = __SD_Send_Cmd(CMD55, 0);
        if (res > 1) 
					return (res);
    }

    // Select the card
    __SD_Deassert();
    SPI_RW(0xFF);
    __SD_Assert();
    SPI_RW(0xFF);

    // Send complete command set
    SPI_RW(cmd);                        // Start and command index
    SPI_RW((BYTE)(arg >> 24));          // Arg[31-24]
    SPI_RW((BYTE)(arg >> 16));          // Arg[23-16]
    SPI_RW((BYTE)(arg >> 8 ));          // Arg[15-08]
    SPI_RW((BYTE)(arg >> 0 ));          // Arg[07-00]

    // CRC?
    crc = 0x01;                         // Dummy CRC and stop
    if(cmd == CMD0) 
			crc = 0x95;         // Valid CRC for CMD0(0)
    if(cmd == CMD8) 
			crc = 0x87;         // Valid CRC for CMD8(0x1AA)
    SPI_RW(crc);

    // Receive command response
    // Wait for a valid response in timeout of 5 milliseconds
    SPI_Timer_On(5);
    do {
        res = SPI_RW(0xFF);
    } while((res & 0x80)&&(SPI_Timer_Status()==TRUE));
    SPI_Timer_Off();
		
    // Return with the response value
    return(res);
}
*/

DWORD __SD_Sectors (SD_DEV *dev)
{
    BYTE csd[16];
    BYTE idx;
    DWORD ss = 0,temp;
    WORD C_SIZE = 0;
    BYTE C_SIZE_MULT = 0;
    BYTE READ_BL_LEN = 0;
	  
	
	  while((temp = __SD_Send_Cmd(CMD9,0)) == SENDING){
		}
	
    if(temp==0) 
    {
        // Wait for response
        while (SPI_RW(0xFF) == 0xFF);
        for (idx=0; idx!=16; idx++) 
					csd[idx] = SPI_RW(0xFF);
        // Dummy CRC
        SPI_RW(0xFF);
        SPI_RW(0xFF);
        SPI_Release();
        if(dev->cardtype & SDCT_SD1)
        {
            ss = csd[0];
            // READ_BL_LEN[83:80]: max. read data block length
            READ_BL_LEN = (csd[5] & 0x0F);
            // C_SIZE [73:62]
            C_SIZE = (csd[6] & 0x03);
            C_SIZE <<= 8;
            C_SIZE |= (csd[7]);
            C_SIZE <<= 2;
            C_SIZE |= ((csd[8] >> 6) & 0x03);
            // C_SIZE_MULT [49:47]
            C_SIZE_MULT = (csd[9] & 0x03);
            C_SIZE_MULT <<= 1;
            C_SIZE_MULT |= ((csd[10] >> 7) & 0x01);
        }
        else if(dev->cardtype & SDCT_SD2)
        {
						// READ_BL_LEN = 9;
            // C_SIZE [69:48]
            C_SIZE = (csd[7] & 0x3F);
            C_SIZE <<= 8;
            C_SIZE |= (csd[8] & 0xFF);
            C_SIZE <<= 8;
            C_SIZE |= (csd[9] & 0xFF);
            C_SIZE_MULT = 8; // AD changed
        }
        ss = (C_SIZE + 1);
        ss *= __SD_Power_Of_Two(C_SIZE_MULT + 2);
        ss *= __SD_Power_Of_Two(READ_BL_LEN);
        // ss /= SD_BLK_SIZE; ?? Bug in original code?

        return (ss);
    } else return (0); // Error
}

/******************************************************************************
 Public Methods - Direct work with SD card
******************************************************************************/
/*
SDRESULTS SD_Init(SD_DEV *dev)
{
    BYTE n, cmd, ct, ocr[4];
    BYTE idx;
    BYTE init_trys;
	
    ct = 0;
    for(init_trys=0; ((init_trys!=SD_INIT_TRYS)&&(!ct)); init_trys++)
    {
        // Initialize SPI for use with the memory card
			  //=================State 1======================
        SPI_Init();

        SPI_CS_High();
        SPI_Freq_Low();
			  //=================State 1 end======================
			
			  //=================State 2======================
        // 80 dummy clocks
        for(idx = 0; idx != 10; idx++) 
					SPI_RW(0xFF);

        SPI_Timer_On(500);
        while(SPI_Timer_Status()==TRUE) {
				}
        SPI_Timer_Off();

        dev->mount = FALSE;
        SPI_Timer_On(500);
        while ((__SD_Send_Cmd(CMD0, 0) != 1)&&(SPI_Timer_Status()==TRUE)) {
				}
	      SPI_Timer_Off();
        // Idle state
        if (__SD_Send_Cmd(CMD0, 0) == 1) {                      
            // SD version 2?
            if (__SD_Send_Cmd(CMD8, 0x1AA) == 1) {
                // Get trailing return value of R7 resp
                for (n = 0; n < 4; n++) 
									ocr[n] = SPI_RW(0xFF);
                // VDD range of 2.7-3.6V is OK?  
                if ((ocr[2] == 0x01)&&(ocr[3] == 0xAA))
                {
                    // Wait for leaving idle state (ACMD41 with HCS bit)...
                    SPI_Timer_On(1000);
                    while ((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(ACMD41, 1UL << 30))) {
										}
                    SPI_Timer_Off(); 
                    // CCS in the OCR? 
										// AGD: Delete SPI_Timer_Status call?
                    if ((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(CMD58, 0) == 0))
                    {
                        for (n = 0; n < 4; n++) 
													ocr[n] = SPI_RW(0xFF);
                        // SD version 2?
                        ct = (ocr[0] & 0x40) ? SDCT_SD2 | SDCT_BLOCK : SDCT_SD2;
                    }
                }
            } else {
                // SD version 1 or MMC?
                if (__SD_Send_Cmd(ACMD41, 0) <= 1)
                {
                    // SD version 1
                    ct = SDCT_SD1; 
                    cmd = ACMD41;
                } else {
                    // MMC version 3
                    ct = SDCT_MMC; 
                    cmd = CMD1;
                }
                // Wait for leaving idle state
                SPI_Timer_On(250);
                while((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(cmd, 0))) {
								}
                SPI_Timer_Off();
                if(SPI_Timer_Status()==FALSE) 
									ct = 0;
                if(__SD_Send_Cmd(CMD59, 0))   
									ct = 0;   // Deactivate CRC check (default)
                if(__SD_Send_Cmd(CMD16, 512)) 
									ct = 0;   // Set R/W block length to 512 bytes
            }
        }
    }
    if(ct) {
        dev->cardtype = ct;
        dev->mount = TRUE;
        dev->last_sector = __SD_Sectors(dev) - 1;
        dev->debug.read = 0;
        dev->debug.write = 0;
        __SD_Speed_Transfer(HIGH); // High speed transfer
    }
    SPI_Release();
    return (ct ? SD_OK : SD_NOINIT);
}*/


SDRESULTS SD_Init(SD_DEV *dev)
{
	  static enum {State1, State2, State3, State4, State5, State6, State7, State8, State9, State10, State11, State12, State13, State14, State_CT_check,return_state} next_state = State1;
    static BYTE n, cmd, ct=0, ocr[4];
    static BYTE idx;
    static BYTE init_trys=0;	    
		DWORD temp;		
	  static BYTE count=0;
		DEBUG_START(DBG_4);
	  
		switch(next_state)
		{
		  case State1:
				init_trys++;
			
			  if(init_trys == 1)
					ct=0;
				
			  if(init_trys > SD_INIT_TRYS || (ct))
					next_state = State_CT_check;
				else
				{
					SPI_Init();
					SPI_CS_High();
					SPI_Freq_Low();
					/*for(idx = 0; idx != 5; idx++) 
						SPI_RW(0xFF);*/
					next_state = State2;
				}
				break;
				
			case State2:
				for(idx = count; idx != 10 && (idx<count+3); idx++) 
						SPI_RW(0xFF);
			  
			  if(idx!=10)
				{
					count=idx;
				}
				else
				{
					count=0;
					SPI_Timer_On(500);
					next_state = State3;
				}
				break;
			
			case State3:
				if(SPI_Timer_Status()!=TRUE)
				{
					SPI_Timer_Off();
					dev->mount = FALSE;
					SPI_Timer_On(500);
					next_state = State4;
				}
				break;
				
			case State4:
				if((temp=__SD_Send_Cmd(CMD0, 0)) != SENDING)				
				if((temp == 1)||(SPI_Timer_Status()==FALSE))
				{
					SPI_Timer_Off();
					next_state = State5;
				}
				break;
				
			case State5:
				if((temp=__SD_Send_Cmd(CMD0, 0)) != SENDING)
				{
					if(temp == 1)			
						next_state = State6;
					else
						next_state = State1;        //ret
			  }
				break;
				
			case State6:
				if((temp=__SD_Send_Cmd(CMD8, 0x1AA)) != SENDING)
				{
					if(temp == 1)					
						next_state = State7;
					else
						next_state = State11;
				}
				break;
				
			case State7:
				for(n=0;n<4;n++)
			     ocr[n] = SPI_RW(0xFF);
			  if((ocr[2] == 0x01)&&(ocr[3] == 0xAA))
        {
					SPI_Timer_On(1000);
					next_state = State8;
				}
				else
					next_state = State1;       //ret
				break;
				
			case State8:
				if((temp=__SD_Send_Cmd(ACMD41, 1UL<<30)) != SENDING)
				{
					if((SPI_Timer_Status()==FALSE)||(!temp))
					{
						SPI_Timer_Off();
						next_state = State9;
					}
				}
				break;
				
		  case State9:
				if((temp=__SD_Send_Cmd(CMD58, 0)) != SENDING)
				{
					if ((SPI_Timer_Status()==TRUE)&&(temp == 0))
						next_state = State10;
					else
						next_state = State1;   //ret
				}
				break;
				
			case State10:
				for(n=0;n<4;n++)
					ocr[n] = SPI_RW(0xFF);
				
			  ct = (ocr[0] & 0x40) ? SDCT_SD2 | SDCT_BLOCK : SDCT_SD2;
			  if(ct)
					next_state = State_CT_check;
				else
					next_state = State1;   //ret
				break;
			
			case State11:
				if((temp=__SD_Send_Cmd(ACMD41, 0)) != SENDING)
				{
					if (temp <= 1)
					{
							// SD version 1
							ct = SDCT_SD1; 
							cmd = ACMD41;
					} else {
							// MMC version 3
							ct = SDCT_MMC; 
							cmd = CMD1;
					}
					// Wait for leaving idle state
					SPI_Timer_On(250);
					next_state = State12;
				}
				break;
				
			case State12:
				if((temp=__SD_Send_Cmd(cmd, 0)) != SENDING)
				{
					if((SPI_Timer_Status()==FALSE) ||  (!temp))
					{
						SPI_Timer_Off();
						if(SPI_Timer_Status()==FALSE)
							ct=0;
						next_state = State13;
					}
				}
				break;
				
			case State13:
				if((temp=__SD_Send_Cmd(CMD59, 0)) != SENDING)
				{
					if(temp)				
						ct=0;
					
					next_state=State14;
			  }
				break;
				
			case State14:
				if((temp=__SD_Send_Cmd(CMD16, 512)) != SENDING)
				{
					if(temp)				
						ct=0;
					
					next_state = State_CT_check;
			  }
				break;				
				
			case State_CT_check:
				if(ct) 
				{
					dev->cardtype = ct;
					dev->mount = TRUE;
					dev->last_sector = __SD_Sectors(dev) - 1;
					dev->debug.read = 0;
					dev->debug.write = 0;
					__SD_Speed_Transfer(HIGH); // High speed transfer
				}
				next_state = return_state;
				break;
				
			case return_state:
				SPI_Release();
			  next_state = State1;
			  init_trys = 0;
			  DEBUG_STOP(DBG_4);
				return(ct?SD_OK:SD_NOINIT);
				break;
		}
		
		DEBUG_STOP(DBG_4);
		return SD_MY_BUSY;		
}


#pragma push
#pragma diag_suppress 1441
/*SDRESULTS SD_Read(SD_DEV *dev, void *dat, DWORD sector, WORD ofs, WORD cnt)
{
    SDRESULTS res;
    BYTE tkn, data;
    WORD byte_num;
		
    res = SD_ERROR;
    if ((sector > dev->last_sector)||(cnt == 0)) 
			return(SD_PARERR);
    // Convert sector number to byte address (sector * SD_BLK_SIZE)
//    if (__SD_Send_Cmd(CMD17, sector * SD_BLK_SIZE) == 0) { // Only for SDSC
      if (__SD_Send_Cmd(CMD17, sector ) == 0) { // Only for SDHC or SDXC   
			SPI_Timer_On(100);  // Wait for data packet (timeout of 100ms)
        do {
            tkn = SPI_RW(0xFF);
        } while((tkn==0xFF)&&(SPI_Timer_Status()==TRUE));
        SPI_Timer_Off();
        // Token of single block?
        if(tkn==0xFE) { 
					// AGD: Loop fusion to simplify FSM formation
					byte_num = 0;
          do {
						data = SPI_RW(0xff);
						if ((byte_num >= ofs) && (byte_num < ofs+cnt)) {
               *(BYTE*)dat = data;
               ((BYTE *) dat)++;
						} // else discard bytes before and after data
          } while(++byte_num < SD_BLK_SIZE + 2 ); // 512 byte block + 2 byte CRC
          res = SD_OK;
        }
    }
    SPI_Release();
    dev->debug.read++;
    return(res);
}*/

SDRESULTS SD_Read(SD_DEV *dev, void *dat, DWORD sector, WORD ofs, WORD cnt)
{
    static SDRESULTS res = SD_ERROR;
    static BYTE tkn, data;
    static WORD byte_num,N=17;
	  static enum{State1, State2, State3, State4, State5} next_state = State1;
		WORD count=0;
		DWORD temp;
		
		DEBUG_START(DBG_2);
		
    switch(next_state)
		{
			case State1:
				res = SD_ERROR;
				if ((sector > dev->last_sector)||(cnt == 0)) 
				{
					DEBUG_STOP(DBG_2);
					return(SD_PARERR);
				}
				else
					next_state = State2;
				break;
			
			case State2:
				if((temp=__SD_Send_Cmd(CMD17, sector)) != SENDING)
				{
					if (temp == 0)
					{
						SPI_Timer_On(100);
						next_state = State3;
					}
					else
						next_state = State5;
			  }
				break;
			
			case State3:
			  do 
				{
            tkn = SPI_RW(0xFF);
					  count++;
        }while((tkn==0xFF)&&(SPI_Timer_Status()==TRUE) &&(count<N));
      	
				if((tkn!=0xFF)||(SPI_Timer_Status()!=TRUE))
				{
					SPI_Timer_Off();
					if(tkn==0xFE)
					{
						count=0;
						next_state = State4;
						byte_num = 0;
					}
					else
					{
						next_state = State5;
					}
				}
				break;
				
			case State4:
				do 
				{
					count++;
					data = SPI_RW(0xFF);
					if ((byte_num >= ofs) && (byte_num < ofs+cnt)) 
					{
						*(BYTE*)dat = data;
						((BYTE *) dat)++;
					} 
				} while((++byte_num < SD_BLK_SIZE + 2)&&(count<N)); // 512 byte block + 2 byte CRC
				
				if(byte_num>=SD_BLK_SIZE)
				{
					res = SD_OK;
					next_state = State5;
				}
				break;
				
			case State5:
				SPI_Release();
        dev->debug.read++;
			  next_state = State1;
			  DEBUG_STOP(DBG_2);
        return(res);			
				break;
		}
		
		DEBUG_STOP(DBG_2);
		
		return(SD_MY_BUSY);
}
#pragma pop
/*SDRESULTS SD_Write(SD_DEV *dev, void *dat, DWORD sector)
{
    WORD idx;
    BYTE line;


		// Query invalid?
    if(sector > dev->last_sector) {
			return(SD_PARERR);
		}

    // Convert sector number to bytes address (sector * SD_BLK_SIZE)
		//    if(__SD_Send_Cmd(CMD24, sector * SD_BLK_SIZE)==0) { // Only for SDSC
		if(__SD_Send_Cmd(CMD24, sector)==0) { // Only for SDHC or SDXC   
			// Send token (single block write)
			SPI_RW(0xFE);
			// Send block data
			for(idx=0; idx!=SD_BLK_SIZE; idx++) 
				SPI_RW(*((BYTE*)dat + idx));
			// Dummy CRC
			SPI_RW(0xFF);
			SPI_RW(0xFF);
			// If not accepted, returns the reject error
			if((SPI_RW(0xFF) & 0x1F) != 0x05) {
				return(SD_REJECT);
			}
			
			// Waits until finish of data programming with a timeout
			SPI_Timer_On(SD_IO_WRITE_TIMEOUT_WAIT);
			do {
					line = SPI_RW(0xFF);
			} while((line==0)&&(SPI_Timer_Status()==TRUE));
			SPI_Timer_Off();
			dev->debug.write++;

			if(line==0) {
				return(SD_BUSY);
			}	else {
				return(SD_OK);	
			}
		}
    else {
			return(SD_ERROR);
		}
}*/

SDRESULTS SD_Write(SD_DEV *dev, void *dat, DWORD sector)
{
	  static enum {State1, State2, State3, State4, State5/*, State6*/}next_state=State1;
    static WORD idx,prvidx=0,N=17;
    static BYTE line;
    WORD count=0;
    DWORD temp;
		
		DEBUG_START(DBG_3);
		
		switch(next_state)
		{
			case State1:
				if(sector > dev->last_sector)
				{
					DEBUG_STOP(DBG_3);
					return(SD_PARERR);
				}
				next_state = State2;
				break;
				
			case State2:
				if((temp=__SD_Send_Cmd(CMD24, sector)) != SENDING)
				{
					if(temp==0)
					{
						SPI_RW(0xFE);
						next_state = State3;
					}
					else
					{
						next_state = State1;
						DEBUG_STOP(DBG_3);
						return(SD_ERROR);						
					}
			  }
				break;
				
			case State3:
				for(idx = prvidx;(idx!=SD_BLK_SIZE) && (idx<prvidx+N);idx++)
					SPI_RW(*((BYTE*)dat + idx));
				
			  prvidx = idx;
			  if(idx == SD_BLK_SIZE)
				{
					next_state = State4;
					prvidx=0;
				}
				break;
				
			case State4:
				SPI_RW(0xFF);
			  SPI_RW(0xFF);
			  if((SPI_RW(0xFF) & 0x1F) != 0x05)
				{
					next_state=State1;
					DEBUG_STOP(DBG_3);
					return(SD_REJECT);
				}
				else
				{
				  next_state = State5;
					SPI_Timer_On(SD_IO_WRITE_TIMEOUT_WAIT);
				}
				break;
				
			case State5:
				do
				{
					count++;
					line = SPI_RW(0xFF);
				}while((line==0)&&(SPI_Timer_Status()==TRUE)&&(count<N));
				
				if((line!=0)||(SPI_Timer_Status()!=TRUE))
				{
					next_state = State1;
					SPI_Timer_Off();
					dev->debug.write++;
					if(line==0)
					{
						DEBUG_STOP(DBG_3);
						return(SD_BUSY);
					}
					else
					{
						DEBUG_STOP(DBG_3);
						return(SD_OK);
					}
				}
				break;
				
			/*case State6:
				next_state = State1;
			  DEBUG_STOP(DBG_3);
			  return(SD_ERROR);
				break;*/
		}

	 DEBUG_STOP(DBG_3);
   return(SD_MY_BUSY);    
}


SDRESULTS SD_Status(SD_DEV *dev)
{
    return(__SD_Send_Cmd(CMD0, 0) ? SD_OK : SD_NORESPONSE);
}

// «sd_io.c» is part of:
/*----------------------------------------------------------------------------/
/  ulibSD - Library for SD cards semantics            (C)Nelson Lombardo, 2015
/-----------------------------------------------------------------------------/
/ ulibSD library is a free software that opened under license policy of
/ following conditions.
/
/ Copyright (C) 2015, ChaN, all right reserved.
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/

// Derived from Mister Chan works on FatFs code (http://elm-chan.org/fsw/ff/00index_e.html):
/*----------------------------------------------------------------------------/
/  FatFs - FAT file system module  R0.11                 (C)ChaN, 2015
/-----------------------------------------------------------------------------/
/ FatFs module is a free software that opened under license policy of
/ following conditions.
/
/ Copyright (C) 2015, ChaN, all right reserved.
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/
