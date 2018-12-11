
#include "io430.h"

// SPI port definitions              // Adjust the values for the chosen
#define SPI_PxSEL         P1SEL      // interfaces, according to the pin
#define SPI_PxSEL2         P1SEL2  // AGREGO, selecciona funcion alternativa del pin
                                    // en G2553 la funcion principal no es usci
#define SPI_PxDIR         P1DIR      // assignments indicated in the
#define SPI_PxIN          P1IN       // chosen MSP430 device datasheet.
#define SPI_PxOUT         P1OUT
//#define SPI_SIMO          0x02 Estos valores eran para la f2272 simo: 3.1
//#define SPI_SOMI          0x04 somi 3.2
//#define SPI_UCLK          0x08 clk 3.3
// En G2553
#define SPI_SIMO          0x80 // 1.7 7 = 8° bit = 1000 0000 = 128dec = 80hex
#define SPI_SOMI          0x40 // 1.6 6 = 7° bit = 0100 0000 = 64dec = 40hex
#define SPI_UCLK          0x20 // 1.5 7 = 6° bit = 0010 0000 = 32dec = 20hex

//----------------------------------------------------------------------------
// SPI port selections.  Select which port will be used for the interface 
//----------------------------------------------------------------------------
// SPI port definitions              // Adjust the values for the chosen
#define MMC_PxSEL         SPI_PxSEL      // interfaces, according to the pin
#define MMC_PxSEL2         SPI_PxSEL2
#define MMC_PxDIR         SPI_PxDIR      // assignments indicated in the
#define MMC_PxIN          SPI_PxIN       // chosen MSP430 device datasheet.
#define MMC_PxOUT         SPI_PxOUT      
#define MMC_SIMO          SPI_SIMO
#define MMC_SOMI          SPI_SOMI
#define MMC_UCLK          SPI_UCLK

// Chip Select (CS)
//#define MMC_CS_PxOUT      P3OUT
//#define MMC_CS_PxDIR      P3DIR
//#define MMC_CS            0x40
// Elijo el 2.4
#define MMC_CS_PxOUT      P2OUT
#define MMC_CS_PxDIR      P2DIR
#define MMC_CS            0x10 // 10000

// Card Detect ESTE NO ESTABA CONECTADO EN SIMULACION
//#define MMC_CD_PxIN       P3IN
//#define MMC_CD_PxDIR      P3DIR
//#define MMC_CD            0x80
// Elijo el 2.5
#define MMC_CD_PxIN       P2IN
#define MMC_CD_PxDIR      P2DIR
#define MMC_CD            0x20


#define CS_LOW()    MMC_CS_PxOUT &= ~MMC_CS               // Card Select
//#define CS_HIGH()   while(!halSPITXDONE); MMC_CS_PxOUT |= MMC_CS  // Card Deselect
#define CS_HIGH()   MMC_CS_PxOUT |= MMC_CS  // Card Deselect
//#define CS_READ()    !P3IN_bit.P3IN_6 --> El CS estaba en 3.6
//#define CS_READ()    !P2IN_bit.P2IN_4 // El CS esta en 2.4
#define CS_READ()    !P2IN_bit.P4 //--> Para mi tendria que escribirse asi

#define DUMMY_CHAR 0xFF

//----------------------------------------------------------------------------
//  These constants are used to identify the chosen SPI interfaces.
//----------------------------------------------------------------------------
 #define halSPIRXBUF  UCB0RXBUF
 #define halSPI_SEND(x) UCB0TXBUF=x
 #define halSPITXREADY  (IFG2&UCB0TXIFG)     /* Wait for TX to be ready */ 
 #define halSPITXDONE  (UCB0STAT&UCBUSY)     /* Wait for TX to finish */
 #define halSPIRXREADY (IFG2&UCB0RXIFG)      /* Wait for TX to be ready */ 
 #define halSPIRXFG_CLR IFG2 &= ~UCB0RXIFG   
 #define halSPI_PxIN  SPI_USART0_PxIN
 #define halSPI_SOMI  SPI_USART0_SOMI

// Varialbes

// Function Prototypes
void halSPISetup (void);
unsigned char spiSendByte(const unsigned char data);
unsigned char spiReadFrame(unsigned char* pBuffer, unsigned int size);
unsigned char spiSendFrame(unsigned char* pBuffer, unsigned int size);
