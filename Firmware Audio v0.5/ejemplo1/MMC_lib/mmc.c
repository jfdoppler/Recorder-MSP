
#include "mmc.h"
#include "hal_SPI.h"
#include "intrinsics.h"

#define  SELECT()	   CS_LOW()    /* CS = L */
#define	DESELECT()	CS_HIGH()   /* CS = H */
#define	MMC_SEL		CS_READ()   /* CS status (true:CS == L) */

/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* Definitions for MMC/SDC command */
#define CMD0	(0x40+0)	   /* GO_IDLE_STATE */
#define CMD1	(0x40+1)	   /* SEND_OP_COND (MMC) */
#define ACMD41	(0xC0+41)   /* SEND_OP_COND (SDC) */
#define CMD8	(0x40+8)	   /* SEND_IF_COND */
#define CMD16	(0x40+16)	/* SET_BLOCKLEN */
#define CMD17	(0x40+17)	/* READ_SINGLE_BLOCK */
#define CMD24	(0x40+24)	/* WRITE_BLOCK */
#define CMD55	(0x40+55)	/* APP_CMD */
#define CMD58	(0x40+58)	/* READ_OCR */

///* Card type flags (CardType) */
//#define CT_MMC			0x01	/* MMC ver 3 */
//#define CT_SD1			0x02	/* SD ver 1 */
//#define CT_SD2			0x04	/* SD ver 2 */
//#define CT_BLOCK		0x08	/* Block addressing */

/* Varialbes */
static BYTE CardType;

//================================================================================================================
//================================================================================================================
//
// FUNCIONES PARA EL MMC CARD
//
//================================================================================================================
//================================================================================================================

static void init_spi (void)		/* Initialize SPI port */
{
  // Init Port for MMC (default high)
  MMC_PxOUT |= MMC_SIMO + MMC_UCLK; //Activar bits del simo y clk
  MMC_PxDIR |= MMC_SIMO + MMC_UCLK;
  // Chip Select
  MMC_CS_PxOUT |= MMC_CS;
  MMC_CS_PxDIR |= MMC_CS;
  // Card Detect
  MMC_CD_PxDIR &=  ~MMC_CD;
  // Enable secondary function
  MMC_PxSEL |= MMC_SIMO + MMC_SOMI + MMC_UCLK;
  MMC_PxSEL2 |= MMC_SIMO + MMC_SOMI + MMC_UCLK;
  // Init SPI Module
  halSPISetup();
}

void dly_100us (void)		/* Delay 100 microseconds */
{
   __delay_cycles(100);
}

static void xmit_spi (BYTE d)		/* Send a byte to the MMC */
{
   spiSendByte(d);
}

static BYTE rcv_spi (void)			/* Send a 0xFF to the MMC and get the received byte */
{
   return (BYTE)spiSendByte(0xFF);
}

/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/
static BYTE send_cmd (
	BYTE cmd,		/* 1st byte (Start + Index) */
	DWORD arg		/* Argument (32 bits) */
)
{
	BYTE n, res;

	if (cmd & 0x80) {	/* ACMD<n> is the command sequense of CMD55-CMD<n> */
		cmd &= 0x7F;
		res = send_cmd(CMD55, 0);
		if (res > 1) return res;
	}

	/* Select the card */
	DESELECT();
	rcv_spi();
	SELECT();
	rcv_spi();

	/* Send a command packet */
	xmit_spi(cmd);						/* Start + Command index */
	xmit_spi((BYTE)(arg >> 24));		/* Argument[31..24] */
	xmit_spi((BYTE)(arg >> 16));		/* Argument[23..16] */
	xmit_spi((BYTE)(arg >> 8));			/* Argument[15..8] */
	xmit_spi((BYTE)arg);				/* Argument[7..0] */
	n = 0x01;							/* Dummy CRC + Stop */
	if (cmd == CMD0) n = 0x95;			/* Valid CRC for CMD0(0) */
	if (cmd == CMD8) n = 0x87;			/* Valid CRC for CMD8(0x1AA) */
	xmit_spi(n);

	/* Receive a command response */
	n = 10;								/* Wait for a valid response in timeout of 10 attempts */
	do {
          int kk = 0;
          do{                         // Cuando esta ocupado devuelve 0xFF -> espero a que vuelva una respuesta valida
		res = rcv_spi();
                kk++;
          }while (res == 0xFF && kk<100);
	} while ((res & 0x80) && --n);

	return res;			/* Return with the response value */
}
/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (void)
{
	BYTE n, cmd, ty, ocr[4];
	UINT tmr;

#if _USE_WRITE
	if (CardType && MMC_SEL) disk_writep(0, 0);	/* Finalize write process if it is in progress */
#endif
	init_spi();		/* Initialize ports to control MMC */
	DESELECT();
	for (n = 10; n; n--) rcv_spi();	/* 80 Dummy clocks with CS=H */

	ty = 0;
	if (send_cmd(CMD0, 0) == 1) {			/* Enter Idle state */
		if (send_cmd(CMD8, 0x1AA) == 1) {	/* SDv2 */
			for (n = 0; n < 4; n++) ocr[n] = rcv_spi();		/* Get trailing return value of R7 resp */
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) {			/* The card can work at vdd range of 2.7-3.6V */
				for (tmr = 10000; tmr && send_cmd(ACMD41, 1UL << 30); tmr--) dly_100us();	/* Wait for leaving idle state (ACMD41 with HCS bit) */
				if (tmr && send_cmd(CMD58, 0) == 0) {		/* Check CCS bit in the OCR */
					for (n = 0; n < 4; n++) ocr[n] = rcv_spi();
					ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;	/* SDv2 (HC or SC) */
				}
			}
		} else {							/* SDv1 or MMCv3 */
			if (send_cmd(ACMD41, 0) <= 1) 	{
				ty = CT_SD1; cmd = ACMD41;	/* SDv1 */
			} else {
				ty = CT_MMC; cmd = CMD1;	/* MMCv3 */
			}
			for (tmr = 10000; tmr && send_cmd(cmd, 0); tmr--) dly_100us();	/* Wait for leaving idle state */
			if (!tmr || send_cmd(CMD16, 512) != 0)			/* Set R/W block length to 512 */
				ty = 0;
		}
	}
	CardType = ty;
	DESELECT();
	rcv_spi();

	return ty ? 0 : STA_NOINIT;
        //return 0;
}

/*-----------------------------------------------------------------------*/
/* Read partial sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_read_low (
	BYTE *buff,		/* Pointer to the read buffer (NULL:Read bytes are forwarded to the stream) */
	DWORD lba			/* Sector number (LBA) */
)
{
	DRESULT res;
	BYTE rc;
	WORD bc;
  WORD cnt = 512;

	if (!(CardType & CT_BLOCK)){
		lba *= 512;		/* Convert to byte address if needed */
	}
	res = RES_ERROR;
	if (send_cmd(CMD17, lba) == 0) {		/* READ_SINGLE_BLOCK */
		bc = 40000;
		do {							/* Wait for data packet */
			rc = rcv_spi();
		} while (rc == 0xFF && --bc);		
		if (rc == 0xFE) {				/* A data packet arrived */
			bc = 2;
			/* Store data to the memory */
			do {
				*buff++ = rcv_spi();
			} while (--cnt);
			/* Skip trailing bytes and CRC */
			do {
        rcv_spi();
      } while (--bc);
			res = RES_OK;
		}
	}
	DESELECT();
	rcv_spi();
	return res;
}

/*-----------------------------------------------------------------------*/
/* Write partial sector                                                  */
/*-----------------------------------------------------------------------*/

DRESULT disk_write_low (
	BYTE *buff,	/* Pointer to the bytes to be written (NULL:Initiate/Finalize sector write) */
	DWORD lba		/* Sector number (LBA) */
)
{
	DRESULT res;
	WORD bc;
	WORD wc;

	res = RES_ERROR;
	if (!(CardType & CT_BLOCK)){
		lba *= 512;	/* Convert to byte address if needed */
	}
	if (send_cmd(CMD24, lba) == 0) {		/* WRITE_SINGLE_BLOCK */
		xmit_spi(0xFF);
		xmit_spi(0xFE);		/* Data block header */
		wc = 512;							/* Set byte counter */
		while (wc) {		/* Send data bytes to the card */
			xmit_spi(*buff++);
			wc--;
		}
		bc = 2;
		while (bc--) {
			xmit_spi(0);	/* Fill left bytes and CRC with zeros */
		}
		/*if ((rcv_spi() & 0x1F) == 0x05) {	// Receive data resp and wait for end of write process in timeout of 500ms //
			for (bc = 5000; rcv_spi() != 0xFF && bc; bc--) {
				dly_100us();	// Wait ready //
			}
			if (bc){
				res = RES_OK;
			}
		}*/
		DESELECT();
		rcv_spi();		
	}	
	return res;
}

/*-----------------------------------------------------------------------*/
/* Funciones agregadas IAN                                               */
/*-----------------------------------------------------------------------*/

char mmcGetResponse(void);
char mmcGetXXResponse(const char resp);
char mmcCheckBusy(void);
unsigned char spiSendByte(const unsigned char data);
char mmc_GoIdle();

char initMMC (void){
  int i;

  CS_HIGH(); //sey CS high
  for(i=0;i<11;i++)
    spiSendByte(0xff);//set DI high with ff (10 times)

  return (mmc_GoIdle());
}

void mmcWriteByte(unsigned char data)
{
  spiSendByte(data);            
}

void mmcSendCmd (const char cmd, unsigned long data, const char crc)
{
  char frame[6];
  char temp;
  int i;
  frame[0]=(cmd|0x40);
  for(i=3;i>=0;i--){
    temp=(char)(data>>(8*i));
    frame[4-i]=(temp);
  }
  frame[5]=(crc);
  for(i=0;i<6;i++)
    spiSendByte(frame[i]);
}

#include "math.h"
unsigned long MMC_ReadCardSize(void)
{
  // Read contents of Card Specific Data (CSD)

  unsigned long MMC_CardSize;
  unsigned short i,      // index
                 j,      // index
                 b,      // temporary variable
                 response,   // MMC response to command
                 mmc_C_SIZE;

  unsigned char mmc_READ_BL_LEN,  // Read block length
                mmc_C_SIZE_MULT;

  CS_LOW ();

  spiSendByte(MMC_READ_CSD);   // CMD 9
  for(i=4; i>0; i--)      // Send four dummy bytes
    spiSendByte(0);
  spiSendByte(0xFF);   // Send CRC byte

  response = mmcGetResponse();

  // data transmission always starts with 0xFE
  b = spiSendByte(0xFF);

  if( !response )
  {
    while (b != 0xFE) b = spiSendByte(0xFF);
    // bits 127:87
    for(j=5; j>0; j--)          // Host must keep the clock running for at
      b = spiSendByte(0xff);


    // 4 bits of READ_BL_LEN
    // bits 84:80
    b =spiSendByte(0xff);  // lower 4 bits of CCC and
    mmc_READ_BL_LEN = b & 0x0F;

    b = spiSendByte(0xff);

    // bits 73:62  C_Size
    // xxCC CCCC CCCC CC
    mmc_C_SIZE = (b & 0x03) << 10;
    b = spiSendByte(0xff);
    mmc_C_SIZE += b << 2;
    b = spiSendByte(0xff);
    mmc_C_SIZE += b >> 6;

    // bits 55:53
    b = spiSendByte(0xff);

    // bits 49:47
    mmc_C_SIZE_MULT = (b & 0x03) << 1;
    b = spiSendByte(0xff);
    mmc_C_SIZE_MULT += b >> 7;

    // bits 41:37
    b = spiSendByte(0xff);

    b = spiSendByte(0xff);

    b = spiSendByte(0xff);

    b = spiSendByte(0xff);

    b = spiSendByte(0xff);

  }

  for(j=4; j>0; j--)          // Host must keep the clock running for at
    b = spiSendByte(0xff);  // least Ncr (max = 4 bytes) cycles after
                               // the card response is received
  b = spiSendByte(0xff);
  CS_LOW ();

  MMC_CardSize = (mmc_C_SIZE + 1);
  // power function with base 2 is better with a loop
  // i = (pow(2,mmc_C_SIZE_MULT+2)+0.5);
  for(i = 2,j=mmc_C_SIZE_MULT+2; j>1; j--)
    i <<= 1;
  MMC_CardSize *= i;
  // power function with base 2 is better with a loop
  //i = (pow(2,mmc_READ_BL_LEN)+0.5);
  for(i = 2,j=mmc_READ_BL_LEN; j>1; j--)
    i <<= 1;
  MMC_CardSize *= i;

  return (MMC_CardSize);

}

// mmc Get Responce
char mmcGetResponse(void)
{
  //Response comes 1-8bytes after command
  //the first bit will be a 0
  //followed by an error code
  //data will be 0xff until response
  int i=0;

  char response;

  while(i<=64)
  {
    response=spiSendByte(0xff);
    if(response==0x00)break;
    if(response==0x01)break;
    i++;
  }
  return response;
}


char mmcCheckBusy(void)
{
  //Response comes 1-8bytes after command
  //the first bit will be a 0
  //followed by an error code
  //data will be 0xff until response
  int i=0;

  char response;
  char rvalue;
  while(i<=64)
  {
    response=spiSendByte(0xff);
    response &= 0x1f;
    switch(response)
    {
      case 0x05: rvalue=MMC_SUCCESS;break;
      case 0x0b: return(MMC_CRC_ERROR);
      case 0x0d: return(MMC_WRITE_ERROR);
      default:
        rvalue = MMC_OTHER_ERROR;
        break;
    }
    if(rvalue==MMC_SUCCESS)break;
    i++;
  }
  i=0;
  do
  {
    response=spiSendByte(0xff);
    i++;
  }while(response==0);
  return response;
}

char mmcSetBlockLength (const unsigned long blocklength)
{
  //  char rValue = MMC_TIMEOUT_ERROR;
  //  char i = 0;
  // SS = LOW (on)
  CS_LOW ();
  // Set the block length to read
  //MMC_SET_BLOCKLEN =CMD16
  mmcSendCmd(MMC_SET_BLOCKLEN, blocklength, 0xFF);

  // get response from MMC - make sure that its 0x00 (R1 ok response format)
  if(mmcGetResponse()!=0x00)
  { initMMC();
    mmcSendCmd(MMC_SET_BLOCKLEN, blocklength, 0xFF);
    mmcGetResponse();
  }

  CS_HIGH ();

  // Send 8 Clock pulses of delay.
  spiSendByte(0xff);

  return MMC_SUCCESS;
} // Set block_length

char mmc_GoIdle(){
  char response=0x01;
  CS_LOW();

  //Send Command 0 to put MMC in SPI mode
  mmcSendCmd(MMC_GO_IDLE_STATE,0,0x95);
  //Now wait for READY RESPONSE
  if(mmcGetResponse()!=0x01)
    return MMC_INIT_ERROR;

  while(response==0x01)
  {
    CS_HIGH();
    spiSendByte(0xff);
    CS_LOW();
    mmcSendCmd(MMC_SEND_OP_COND,0x00,0xff);
    response=mmcGetResponse();
  }
  CS_HIGH();
  spiSendByte(0xff);
  return (MMC_SUCCESS);
}

char mmcGetXXResponse(const char resp)
{
  //Response comes 1-8bytes after command
  //the first bit will be a 0
  //followed by an error code
  //data will be 0xff until response
  int i=0;

  char response;

  while(i<=1000)
  {
    response=spiSendByte(0xff);
    if(response==resp)break;
    i++;
  }
  return response;
}

char mmcWriteUnmount(int currentByte)      
{
  char rvalue = MMC_RESPONSE_ERROR;         // MMC_SUCCESS;
  int i;
  
  for(i=512;currentByte<i; i--) // i = tamaño sector?
  {
    spiSendByte(0x1f);//send dummy data till we hit the end of the sector
  }

  // put CRC bytes (not really needed by us, but required by MMC)
  spiSendByte(0xff);
  spiSendByte(0xff);

  // read the data response xxx0<status>1 : status 010: Data accected, status 101: Data
  //   rejected due to a crc error, status 110: Data rejected due to a Write error.
  rvalue=mmcCheckBusy();
//  rvalue = MMC_SUCCESS;

  CS_HIGH ();
  // Send 8 Clock pulses of delay.
  spiSendByte(0xff);
  
  return rvalue;  
}

char mmcWriteMount(unsigned long address)
{
  char rvalue = MMC_RESPONSE_ERROR;         // MMC_SUCCESS;

  // Set the block length to read
  if (mmcSetBlockLength (512) == MMC_SUCCESS)   // block length could be set
  {
    // SS = LOW (on)
    CS_LOW ();
    // send write command
    mmcSendCmd (MMC_WRITE_BLOCK,address, 0xFF);

    // check if the MMC acknowledged the write block command
    // it will do this by sending an affirmative response
    // in the R1 format (0x00 is no errors)
    if (mmcGetXXResponse(MMC_R1_RESPONSE) == MMC_R1_RESPONSE)
    {
      spiSendByte(0xff);
      // send the data token to signify the start of the data
      spiSendByte(0xfe);

      rvalue = MMC_SUCCESS;
    }
    else
    {
      // the MMC never acknowledge the write command
      rvalue = MMC_RESPONSE_ERROR;   // 2
    }
  }
  else
  {
    rvalue = MMC_BLOCK_SET_ERROR;   // 1
  }

  return rvalue;
} // mmc_write_block

void mmcReadUnmount(int currentByte)
{
  int i;
  for(i=513;currentByte<i; i--)
  {
    spiSendByte(0x1f);//send dummy data till we hit the end of the sector
  }
  // get CRC bytes (not really needed by us, but required by MMC)
  spiSendByte(0xff);
  spiSendByte(0xff);
  CS_HIGH ();
  spiSendByte(0xff);
}
