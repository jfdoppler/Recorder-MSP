
#include "integer.h"

/* Status of Disk Functions */
typedef BYTE	DSTATUS;


/* Results of Disk Functions */
typedef enum {
	RES_OK = 0,		/* 0: Function succeeded */
	RES_ERROR,		/* 1: Disk error */
	RES_NOTRDY,		/* 2: Not ready */
	RES_PARERR		/* 3: Invalid parameter */
} DRESULT;

#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */

/* Card type flags (CardType) */
#define CT_MMC			0x01	/* MMC ver 3 */
#define CT_SD1			0x02	/* SD ver 1 */
#define CT_SD2			0x04	/* SD ver 2 */
#define CT_SDC			(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK		0x08	/* Block addressing */

/*---------------------------------------*/
/* Prototypes for disk control functions */

static void init_spi (void);		/* Initialize SPI port */
void dly_100us (void);				/* Delay 100 microseconds */
static void xmit_spi (BYTE d);	/* Send a byte to the MMC */
static BYTE rcv_spi (void);		/* Send a 0xFF to the MMC and get the received byte */
static BYTE send_cmd (BYTE cmd, DWORD arg);

DSTATUS disk_initialize (void);
DRESULT disk_read_low (BYTE*, DWORD);//DRESULT disk_readp (BYTE*, DWORD, WORD, WORD);
DRESULT disk_write_low (BYTE*, DWORD);


// Agregado
//I divided the bigger functions into multiple parts so that 
//a micro with less than 512K ram can work with the SD card
//mounts a SD sector at address..
//returns error codes
//mount an SD sector (address) for write
//returns error codes
char mmcWriteMount(unsigned long address);
//write a single byte to the SD card
void mmcWriteByte(unsigned char data);
//currentByte helps unmount sectors if we havn't finished writing to the end
char mmcWriteUnmount(int currentByte);  
void mmcReadUnmount(int currentByte);


// this variable will be used to track the current block length
// this allows the block length to be set only when needed
// unsigned long _BlockLength = 0;

// error/success codes
#define MMC_SUCCESS           0x00
#define MMC_BLOCK_SET_ERROR   0x01
#define MMC_RESPONSE_ERROR    0x02
#define MMC_DATA_TOKEN_ERROR  0x03
#define MMC_INIT_ERROR        0x04
#define MMC_CRC_ERROR         0x10
#define MMC_WRITE_ERROR       0x11
#define MMC_OTHER_ERROR       0x12
#define MMC_TIMEOUT_ERROR     0xFF

#define MMC_SET_BLOCKLEN           0x50     //CMD16 Set block length for next read/write
#define MMC_WRITE_BLOCK            0x58     //CMD24
#define MMC_R1_RESPONSE       0x00

#define MMC_GO_IDLE_STATE          0x40     //CMD0
#define MMC_SEND_OP_COND           0x41     //CMD1
#define MMC_READ_CSD               0x49     //CMD9

// mmc init
char initMMC (void);
// set MMC in Idle mode
char mmc_GoIdle();

char mmcGetXXResponse(const char resp);

// Read the Card Size from the CSD Register
unsigned long MMC_ReadCardSize(void);