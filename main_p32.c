
// main_p32.c
//
// 2012 LXD Research & Display
// M McClafferty
//
// Gilbarco LCD Demo - i2c on UEXT connector.
//
// Platforms:
//   Olimex Duinomite (pic32mx795F512h)
//   Olimex PIC32-MX460 (TODO?? Never did get i2c working here)
//
/*
 * Configuration Bit Settings:
 *		- SYSCLK = 80 MHz (8MHz Crystal/ FPLLIDIV * FPLLMUL / FPLLODIV)
 *		- PBCLK = 40 MHz
 *		- Primary Osc w/PLL (XT+,HS+,EC+PLL)
 *		-  WDT OFF
 *		- Other options are don't care
 *
 ********************************************************************/

#include <plib.h>       // PIC32 Peripheral library
#include <stdint.h>
#include <string.h>

#include "product_config.h"
#include "p32_utils.h"       // Our misc utils for pic32 (delays, etc)
#include "nxp_lcd_driver.h"  // 


#include "ConfigurationBits.h"


// TEST ONLY

// 40seg
//uint8_t allOnData[]  = {0xe0, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff};
//uint8_t allOffData[] = {0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// 60seg  (ldp = load data pointer)
//                        cmd,  ldp, data,   1     2     3     4     5    6   commas n/a
//uint8_t allOnData[]   = {0x80, 0x00, 0x40, 0xfe, 0xfe, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff};
//uint8_t allOffData[]  = {0x80, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
//uint8_t commaData[]   = {0x80, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00};

uint8_t segmentBuf[16];

const char* testStr = "0123456789abcdefghijlnopstu0123456789";


// main() ---------------------------------------------------------------------
//
int main(void)
{
    int i;
    int pbClk;         // Peripheral bus clock
    char tempStr[16];
    uint8_t tempBytes[16];

    // Pins that share ANx functions (analog inputs) will default to
    // analog mode (AD1PCFG = 0x0000) on reset.  To enable digital I/O
    // Set all ones in the ADC Port Config register. (I think it's always
    // PORTB that is shared with the ADC inputs). PORT B is NOT 5v tolerant!
    // Also, RB6 & 7 are the ICD/ICSP lines.
    AD1PCFG = 0xffff;  // Port B as digital i/o.

    // The pic32 has 2 programming i/f's: ICD/ICSP and JTAG. The
    // starter kit uses JTAG, whose pins are muxed w/ RA0,1,4 & 5.
    // If we wanted to disable JTAG to use those pins, we'd need:
    // DDPCONbits.JTAGEN = 0;  // Disable the JTAG port.
	//DDPCON &= ~(1<<3);	// we don't use JTAG (for now)
    mJTAGPortEnable(0);     // Turn off JTAG

 	// Configure the device for maximum performance, but do not change
	// the PBDIV clock divisor.  Given the options, this function will
	// change the program Flash wait states, RAM wait state and enable
	// prefetch cache, but will not change the PBDIV.  The PBDIV value
	// is already set via the pragma FPBDIV option above.
   	pbClk = SYSTEMConfig(CPU_HZ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);

	//uart_init(pbClk);

    //putsUART2("\r\n\r\n\r\n*** Olimex PIC32-MX460 DEMO PROGRAM ***\r\n");



    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // STEP 2. configure the port registers
    //PORTSetPinsDigitalOut(IOPORT_D, BIT_1);				//LED1
    //PORTSetPinsDigitalOut(IOPORT_D, BIT_2);				//LED2

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // STEP 3. initialize the port pin states = outputs low
	//mPORTDSetBits(BIT_1);								//LED1
    //mPORTDClearBits(BIT_2);								//LED2
	
    // Note: It is recommended to disable vector interrupts prior to
    // configuring the change notice module, if they are enabled.
    // The user must read one or more IOPORTs to clear the IO pin
    // change notice mismatch condition, then clear the change notice
    // interrupt flag before re-enabling the vector interrupts.

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // STEP 7. enable multi-vector interrupts
    INTEnableSystemMultiVectoredInt();
    

    // Gilbarco, initialize
    nxpInit(pbClk);

// TEST ONLY...
    lcdWrite(LCD_L1,"111111,");
    lcdWrite(LCD_L1,"11111,1");
    lcdWrite(LCD_L1,"1111,11");
    lcdWrite(LCD_L1,"111,111");
    lcdWrite(LCD_L1,"11,1111");

    lcdWrite(LCD_S1,"7777,");
    lcdWrite(LCD_S1,"777,7");
    lcdWrite(LCD_S1,"77,77");
    lcdWrite(LCD_S1,"7,777");
    lcdWrite(LCD_S1,",7777");

    lcdWrite(LCD_S1,"3.652");
    lcdWrite(LCD_S2,"3.821");
    lcdWrite(LCD_S3,"4.027");

	// Simple "fill-up" - Continuously inc gallons & price, and display
    float gallons = 180.00;    // Starting gallon count
	int   grade = 3;           // Fuel grade, 1,2, or 3
    float price = 4.027;       // Price of grade 1
    float total = 0;           // Total price
    lcdWrite(LCD_S1,"----");
    lcdWrite(LCD_S2,"----");
    lcdWrite(LCD_S3,"4.027");
    while(1)
    {
        gallons += .009;                    // Inc total fuel pumped
        total = gallons * price;            // Update total price
        sprintf(tempStr, "%6.2f", total);   // Display price
        lcdWrite(LCD_L1, tempStr);
        sprintf(tempStr, "%6.3f", gallons); // Display gallons
        lcdWrite(LCD_L2, tempStr);
        if(gallons > 200.0)
		{
			gallons = 190.0;             // Reset gallons
            if(++grade > 3) grade = 1;   // Cycle fuel grade

            lcdWrite(LCD_L1,"");
            lcdWrite(LCD_L2,"");
            lcdWrite(LCD_S1,"3.652");
            lcdWrite(LCD_S2,"3.821");
            lcdWrite(LCD_S3,"4.027");
            delay_ms(500);

            switch(grade)
            {
                case 1:
                    lcdWrite(LCD_L1,"  87  ");
                    lcdWrite(LCD_L2," CHEAP");
                    lcdWrite(LCD_S1,"3.652"); price = 3.652;
                    lcdWrite(LCD_S2,"----");
                    lcdWrite(LCD_S3,"----");
                    break;
                case 2:
                    lcdWrite(LCD_L1," 100LL");
                    lcdWrite(LCD_L2," SO SO");
                    lcdWrite(LCD_S1,"----");
                    lcdWrite(LCD_S2,"3.821"); price = 3.821;
                    lcdWrite(LCD_S3,"----");
                    break;
                case 3:
                    lcdWrite(LCD_L1," JET A");
                    lcdWrite(LCD_L2,"YABABE");
                    lcdWrite(LCD_S1,"----");
                    lcdWrite(LCD_S2,"----"); price = 4.027;
                    lcdWrite(LCD_S3,"4.027");
                    break;
            }
            delay_ms(1000);
		}
        delay_us(500);
    }

    while(1)
    {
        for(i=0;i<16;i++) tempBytes[i] = 0;
        h4235_Write(1,tempBytes);   delay_ms(200);
        h4235_Write(2,tempBytes);   delay_ms(200);
        h4198_Write(1,tempBytes);   delay_ms(200);
        h4198_Write(2,tempBytes);   delay_ms(200);
        h4198_Write(3,tempBytes);   delay_ms(200);

        for(i=0;i<16;i++) tempBytes[i] = 0xff;
        h4235_Write(1,tempBytes);   delay_ms(200);
        h4235_Write(2,tempBytes);   delay_ms(200);
        h4198_Write(1,tempBytes);   delay_ms(200);
        h4198_Write(2,tempBytes);   delay_ms(200);
        h4198_Write(3,tempBytes);   delay_ms(200);

        lcdWrite(LCD_L1, "111111"); delay_ms(200);
        lcdWrite(LCD_L2, "222222"); delay_ms(200);
        lcdWrite(LCD_S1, "3333");   delay_ms(200);
        lcdWrite(LCD_S2, "4444");   delay_ms(200);
        lcdWrite(LCD_S3, "5555");   delay_ms(200);
    }

/*
    i = 0;
    while(1)
    {
        // Copy 6 chars from the test string to a temp string
        strncpy(tempStr, &testStr[i], 6);
        tempStr[6] = 0;

        // Display the 6 char substring from the test string
        //h4235_SetSegments(tempStr, segmentBuf);
        //h4235_Write(1, segmentBuf);
        //h4235_SetSegments(tempStr, segmentBuf);
        //h4235_Write(2, segmentBuf);

        lcdWrite(LCD_L1, tempStr);
        lcdWrite(LCD_L2, tempStr);
        //lcdWrite(LCD_S1, tempStr);
        delay_ms(350);

        // Move to the next location in the test string
        i++;
        if(i > 9) i = 0;
    }
*/   
    return 0;
}

