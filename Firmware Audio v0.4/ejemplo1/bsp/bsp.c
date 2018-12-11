
#include "bsp.h"

void MainClockInit(void){
   BCSCTL1 = CALBC1_8MHZ;// Use 8Mhz cal data for DCO
   DCOCTL = CALDCO_8MHZ;// Use 8Mhz cal data for DCO
}

void SMCLKInit(void){
   //Configuramos el SMCLK para que el DCO sea su source clock y divisor /1.
   BCSCTL2_bit.SELS = 0;//clock source = DCO
   BCSCTL2_bit.DIVS0 = 1;//divisor /1
   BCSCTL2_bit.DIVS1 = 1;//divisor /1
}
