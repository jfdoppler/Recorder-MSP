/* Modificado usando el tuto de Ian Lesnet
En mmc.c/h todo lo que dice "Agregado" es sacado de los archivos del firmware 
record.
Esta todo para que funcione (por ej. hay dos funciones para mandar comandos:
sendCmd y send_cmd (la primera es la del tuto, la segunda de Ian)

Este proyecto esta basado en el de USCI SPI ejemplo1 del tutorial de microembebidos.com:
http://microembebidos.com/2013/05/07/tutorial-msp430-usci_b0-spi-sdmmc-parte-iii/
C:\Users\Juan\Desktop\MicroSD\Tutorial\tutorial msp430 USCI_B0 SPI

Modifico para adaptarlo al procesador MSP430G2553.

VERSION 0.1
- Reemplaza el WDT por TimerA (mayor versatilidad en frecuencia) -> OK
- Implementa un buffer circular para evitar perdida de datos entre cambio de sector -> OK
- Detecta sectores problematicos cuando el tiempo de unmount-mount se prolonga demasiado y el buffer no alcanzo
- Graba a 15kHz: en sectores normales llega a mandar todo el buffer antes del final del sector

VERSION 0.2
- Mide en dos canales A1 y A2 alternativamente (no en simultaneo)
- Mantiene la frecuencia de 16kHz -> cada canal se mide a 8kHz
  Alternativa: reducir la frec y medir los dos canales en cada interrupt
- Pruebas: en 16kHz funca perfecto, a 20kHz se va medio al carajo

VERSION 0.3
- Triggerea cuando la señal en A2 supera un valor umbral

VERSION 0.4
- Multiples mediciones

VERSION 0.5 (falta testear)
- Para evitar perdida de tiempo en instancias de buffer overflow, escribo 0's

*/
#include "io430.h"
#include "intrinsics.h"
#include <bsp.h>
#include <mmc.h>

// Variables para montar la sd
unsigned char status = 1;
unsigned int timeout = 0;

// Buffer
int buffer_in = 0; //Index entrada
int buffer_out = 0; //Index salida
#define buffer_size 175 // Tamaño buffer

#define umbral 0 // valor umbral para el trigger
#define MedirSectores 400 // Cantidad de sectores por medicion

int inicio;

struct audioStr {
  volatile unsigned char msb[buffer_size]; //stores one 8 bit audio output sample
  volatile unsigned char lsb[buffer_size]; //guarda los 2 bits perdidos (10bit ADC -> 8bit audio)
  unsigned char Rec;                       //audio record or play?
  unsigned char Trigger; 
  unsigned char bufferBytes;               //number of audio bytes available
  unsigned char canal;                     //selecciona que canal se va a medir
} audio;

struct flashstr {
  unsigned int currentByte;    //byte counter for current 512byte sector
  unsigned long currentSector; //current sector
  unsigned long sectorInicial; //Sector inicial de la medicion
} flashDisk;
  
unsigned char sdWrite(unsigned char data);

//this function writes one byte to the SD card
//it takes care of switching to the next 512 byte sector
//the first sector to write to must already be mounted
//the flaskdisk variables must be setup for the location to start reading
unsigned char sdWrite(unsigned char data)
{
  unsigned char status;
  mmcWriteByte(data);             //write audio sample to the sector
  flashDisk.currentByte++;        //increment the byte counter
  if(flashDisk.currentByte==512+1)
  {                               //we have reached the end of the current sector
    if(inicio){                   // Si salto el trigger
      flashDisk.sectorInicial = flashDisk.currentSector; //Guarda en que sector empieza la medicion
      inicio = 0;
    }
    mmcWriteUnmount(flashDisk.currentByte);  //close the sector
    flashDisk.currentByte=1;      //reset byte counter to 1

    if(flashDisk.currentSector-flashDisk.sectorInicial == MedirSectores) // TESTEAR
    {
      audio.Rec = 0; //Dejo de medir
      flashDisk.sectorInicial = flashDisk.currentSector; 
      audio.Trigger = 1; //Vuelvo a medir el trigger
      audio.canal = 1;
      audio.bufferBytes = 0;
      buffer_in = 0; //Vacio el buffer
      P1OUT_bit.P4 ^= 1;
    }
    flashDisk.currentSector++;                     //go to the next sector
    status=mmcWriteMount(flashDisk.currentSector); //mount by byte, not the sector number.... -> MAL
                                                   // Se monta por sector!
  }
  return status;                  //no errors....
}

void Rec(){
    //NEED TO UNMOUNT THE LAST SECTOR!!!!!
    mmcReadUnmount(flashDisk.currentByte);  //close the sector HACE FALTA?
    
    //------write audio to SD card....
    //setup the write
    audio.bufferBytes = 0;        //flush any old values from the buffer
    audio.Rec = 0;                //enable recording code in WDT routine.....
    audio.Trigger = 1;            // Activa medicion del trigger
    audio.canal = 1;
    flashDisk.currentByte = 1;      //reading byte 0 of the current 512 byte sector
    
    status = mmcWriteMount(flashDisk.currentSector); //mount the first sector so it is ready to write, 
                                                     //now all writes go through sdWrite()
                                                     // Agregue el +1 porque no estaba cambiando de sector// Lo saque y cambia(?)
}

int main( void )
{
  WDTCTL = WDTPW + WDTHOLD;
  
  // Arrancan los relojes, ver bsp.c
  MainClockInit();
  SMCLKInit();
    
  // Seteo el puerto 1.4 como salida logica para controlar un led
  P1DIR_bit.P4 = 1;//LED en 1.4 para ver cuando deja de andar
  __delay_cycles(1000000);
  P1OUT_bit.P4 ^= 1;// Apaga
  __delay_cycles(1000000);
  P1OUT_bit.P4 ^= 1;// Prende
  __delay_cycles(1500000);
  
  // Initialization of the MMC/SD-card
  while (status != 0){
    status = disk_initialize(); // Inicializa el disco
    timeout++;
    P1OUT_bit.P4 ^= 1;//Titila
    __delay_cycles(500000);
    // Try 150 times till error
    if (timeout == 150){
      P1OUT_bit.P4 ^= 1;
      __delay_cycles(250000);
      P1OUT_bit.P4 ^= 1;
      __delay_cycles(250000);
      P1OUT_bit.P4 ^= 1;
      __delay_cycles(250000);
      P1OUT_bit.P4 ^= 1;
      __low_power_mode_0();
    }
  }
  P1OUT_bit.P4 ^= 1; //Prende el led
  
  //setup the ADC10
  ADC10CTL0 &= ~ENC;				// Disable Conversion (apagar antes de configurar)
  ADC10CTL0 = SREF_1 + REFON + REF2_5V + ADC10SHT_2 + ADC10ON + ADC10IE; // Rango, sample and hold time, ADC10ON, interrupt enabled
  ADC10AE0 |= BIT1 + BIT2;                           // PA.1, A.2 ADC option select
  Rec(); //Prepara la tarjeta sd (monta el primer sector). En esta version no hace audio.Rec = 1, sino audio.Trigger = 1 !!
  flashDisk.currentSector = 0;
  
  // Seteo el timer:
  CCTL0 = CCIE;
  TA0CTL = TASSEL_2 + ID_0 + MC_1; // Clear + Source + Divisor + Modo + Interrupt
  TACCR0 = 61; // Cuenta hasta TACCR0 + 1 = 62us -> 16kHz
  
  __enable_interrupt(); //Habilita las interrupciones

  while(1){
    if(audio.Rec){ // Si estamos en modo grabar
      if(audio.bufferBytes > buffer_size){       // Buffer overflow (perdi datos) --> escribo 0's
        status = sdWrite(0x00);
        status = sdWrite(0x00);
        audio.bufferBytes--;
      }else if(audio.bufferBytes > 0){           //There is a byte to be written in the record buffer
        buffer_out = buffer_in - audio.bufferBytes + 1;
        if(buffer_out < 0) buffer_out += buffer_size;
        status = sdWrite(audio.msb[buffer_out]); //write the byte, get the result code in status
        status = sdWrite(audio.lsb[buffer_out]);
        audio.bufferBytes--;                     //subtract one from the buffer counter
      }
    }
  }
}

// Cada vez que el timer llega a TACCR0 corre el siguiente codigo
// Lo que hace es activar la conversion del ADC10
#pragma vector=TIMER0_A0_VECTOR
__interrupt void interrupt_timer(void)
{
  if(audio.Rec){                 //if in record mode...trigger ADC to get audio sample for recording
    if(audio.canal == 1){            //Selecciono 1 de los dos canales
      ADC10CTL0 &= ~ENC;
      ADC10CTL1 &= ~0xFFFF;
      ADC10CTL1 = CONSEQ_0 + INCH_1;
      audio.canal = 2;          // Cambio de canal para la proxima
    }else if(audio.canal == 2){
      ADC10CTL0 &= ~ENC;
      ADC10CTL1 &= ~0xFFFF;
      ADC10CTL1 = CONSEQ_0 + INCH_2;
      audio.canal = 1;
    }
    ADC10CTL0 |= ENC + ADC10SC; //Set bit to start conversion
                                //Aca interrumpe el ADC10 y va al otro interrupt (abajo)
  }else if(audio.Trigger){
    ADC10CTL0 &= ~ENC;
    ADC10CTL1 &= ~0xFFFF;
    ADC10CTL1 = CONSEQ_0 + INCH_1; //Selecciono canal para trigger
    ADC10CTL0 |= ENC + ADC10SC;
    while (ADC10CTL1 & BUSY);// espero a que se desocupe

    if(ADC10MEM > umbral){
      inicio = 1;
      audio.Rec = 1;
      audio.Trigger = 0;
      audio.bufferBytes = 0;
    }
  }
}

//ADC10 interrupt service routine
//when the ADC10 completes an audio sample measurement, this code runs
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
  audio.bufferBytes++;          //new byte in the buffer (hay un byte mas para transmitir)
  buffer_in = buffer_in + 1;
  if(buffer_in == buffer_size) buffer_in =  0;
  while (ADC10CTL1 & BUSY);// espero a que se desocupe
  // El ADC10 guarda la medicion de 10 bits en ADC10MEM
  audio.msb[buffer_in] = (ADC10MEM>>2);       //convert audio to 8 bit sample....  >> binary shift -> mueve los bits 2 lugares
  audio.lsb[buffer_in] = ADC10MEM & 0x03;
}