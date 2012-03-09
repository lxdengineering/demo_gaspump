//
// nxp_lcd_driver
//
// LXD Research & Display
//
// Support for NXP I2C LCD Driver ICs, PCF85176, PCF85134, as used on
// LXD demo boards for the H4235 and H4198 glass displays. The demo
// will consist of one H4235 - a larg, 2 line x 6 digit display, and
// three H4198's - small, 1 line x 4 digit displays.
//
// The controller ICs drive "low multiplex rate" LCDs. In our case,
// the LCDs are driven statically (no multiplexing).
//
// The microcontroller interface is I2C serial
//
// Note: Datasheet references are from the PCA85176 datasheet,
//       Rev 2, 27-June-2011
//       
// H4198 (PCF85176 IC) demo board segment mapping:
//    Byte  Segs   Digit   Notes (* Has comma & period; Period is LS bit)
//    ----  -----  -----   --------------
//     0    0..7     1     Right-most digit
//     1    8..15    2     *
//     2    16..23   3     *
//     3    24..31   4     *Left-most digit   
//     4    32..39 commas  Commas at S37,38,39 (0x07 turns all on)  
//
// H4235 (PCF85134 per line) demo board segment mapping (2 sets of 6 digits):
//    Byte  Segs   Digit   Notes (* Has comma & period; Period is LS bit)
//    ----  -----  -----   --------------
//     0    0..7     1     Left-most digit
//     1    8..15    2     
//     2    16..23   3     *
//     3    24..31   4     *
//     4    32..39   5     *     
//     5    40..47   6     Right-most digit   
//     6    48..55 commas  Commas at S48,49,50 (0xe0 turns all on)         
//     7    56..59         n/c


// TEMP ONLY... Bus pirate testing, cut'n'paste snippets, for the H4198:
//
//  HiZ>m           ; Mode select (4 for i2c; 2 for 50kHz)
//  I2C>P           ; Enable pull-ups
//  I2C>W           ; Power supply on
//  I2C>(1)         ; Address search macro (should see 0x70)
//  i2c>[0x70 0xc9 0x80 0xe0 0xf8 0xf0]            ; Init sequence
//  i2c>[0x70 0x80 0x60 0xff 0xff 0xff 0xff 0xff]  ; All on
//  i2c>[0x70 0x80 0x60 0x00 0x00 0x00 0x00 0x00]  ; All off
//
//  Note that, on H4198, most IC o/p are 3.3v at init. Only [0x70 0xc9]
//    is needed to get switching waveforms on the outputs (72.5KHz).
//
//  More 4198 examples...
//  Device select 0 (0xe0):
//    [0x70 0xe0 0x00 0xff 0xff 0xff 0xff 0xff]   ; All on
//    [0x70 0xe0 0x00 0x00 0x00 0x00 0x00 0x00]   ; All off
//    [0x70 0xe0 0x00 0xff 0x00 0x00 0x00 0x00]   ; Digit 1 (right); S0..7
//    [0x70 0xe0 0x00 0x00 0x00 0x00 0xff 0x00]   ; Digit 4 (left) & dp3; S24..31
//    [0x70 0xe0 0x00 0x00 0x00 0x00 0xfe 0x00]   ; As above, period off
//    [0x70 0xe0 0x00 0x00 0x00 0x00 0x00 0x07]   ; Commas on (S37,S38,S40)
//
//                     '1'  '2'  '3'  '4' 
//    [0x70 0xe0 0x00 0x0c 0xb6 0x9e 0xcc 0x07]   ; "4321" & commas
//

#include <p32xxxx.h>
#include <plib.h>
#include <ctype.h>

#include "product_config.h"
#include "nxp_lcd_driver.h"
#include "p32_utils.h"


// Define which I2C port the driver is attached to
#if defined GILBARCO_DUINOMITE
  #define LCD_I2C_BUS I2C1
#else
  #error need a product defined
#endif


// PIC32 I2C notes
//   - If you google "pic32 i2c", you get a variety of coding styles:
//        - direct register programming
//        - "old" library from Microchip (OpenI2C1(), etc)
//        - "new" library from Microchip (I2CConfigure(<which i2c>...) etc)
// We'll try to stick with the latest, from docs & example code from
// the Microchip mplabc32\v2.02\pic32mx  dirs.
//
// Note this has been problematic... on the 'mx460 board, there was no
// hint that the microchip libs were doing anything to the hardware
// (looking at the I2C SFRs within MPLAB, you could not see anything
// being done to the I2Cxxxx registers).  On the 'mx795, we finally
// got things going after adding a dummy I2C status read prior to the
// I2C start.
// 
// PIC32 Family Reference Manual, Ch. 24 ("Inter-Integrated Circuit")
// has a good i2c overview.


// nxpInit - Initialize the driver for static operation
//
// Follwing power-on, the IC resets as follows:
//   - All LCD outputs (backplane & segment) = Vlcd
//   - Mode: 1:4 mulplex, 1/3 bias
//   - Blinking off
//   - Input & output bank selectors are reset
//   - I2C bus is init'd
//   - Data ptr & sub-addr counter set to 0
//   - Display is disabled
//
// This routine sets the LCD drivers to static mode, blinking off, enabled
//
void nxpInit(int pbClk)
{
    uint8_t initBytes[32];
    uint32_t actualFreq;
    int i;

    delay_ms(2);    // Delay at least 1ms after POR before i2c comms

    // Set up the i2c interface
    //
#if defined GILBARCO_DUINOMITE
    // 3.3v on UEXT is switched by RB13
    TRISBCLR = BIT_13;  // Set RB12 as output
    LATBCLR = BIT_13;   // Low to enable 3.3v
#else
  #error define a product...
#endif

    I2CConfigure(LCD_I2C_BUS, 0 /*I2C_ENABLE_SLAVE_CLOCK_STRETCHING | I2C_ENABLE_HIGH_SPEED*/);
    actualFreq = I2CSetFrequency(LCD_I2C_BUS, pbClk, 100000);  // Seemed OK at 400KHz
    //actualFreq = I2CSetFrequency(LCD_I2C_BUS, pbClk, 40000);  // Seemed OK at 400KHz
    //I2CSetSlaveAddress(...   not needed if we're master only)
    I2CEnable(LCD_I2C_BUS, TRUE);
    delay_ms(10);   // Note that some delay IS REQUIRED before i2c comms start.


    // Set up data bytes to be sent for init, including:
    //
    //   Mode-set command (0xC0) or'd with:
    //      bit3    =  1 (enable display), and
    //      bits1,0 = 01 (static mode)
    //

    // TODO: This init byte-sequence is a holdover from when we were trying
    //       to accomodate both IC types. It should be seperated out and
    //       optimized/cleaned-up for each of the PCF85134 & PCF85176 ICs
    //
    //                        60seg                    40seg
    //                        ----------------         -----------------
    initBytes[0] = 0x80;   // Command follows          DataPtr=0
    initBytes[1] = 0xc9;   // Mode set                 same
    initBytes[2] = 0x80;   // Command follows          DataPtr=0
    initBytes[3] = 0xF8;   // In & out bank select 0   same
    initBytes[4] = 0x00;   // (last) command follows   ?
    // TODO: This is interpreted as data by the 4198's; Not a big deal, but...
    initBytes[5] = 0xf0;   // Blink mode normal, off   same? ignored?

    nxpRawWrite(LCD_A1, initBytes, 6);  // Small displays
    delay_ms(2);
    nxpRawWrite(LCD_A2, initBytes, 6);  // Large display
    delay_ms(2);

    // Set all segments on
    for(i=0; i<16; i++) initBytes[i] = 0xff;
    h4235_Write(1,initBytes);
    h4235_Write(2,initBytes);
    h4198_Write(1,initBytes);
    h4198_Write(2,initBytes);
    h4198_Write(3,initBytes);
    delay_ms(750);

    // Turn all segments off
    for(i=0; i<16; i++) initBytes[i] = 0;
    h4235_Write(1,initBytes);
    h4235_Write(2,initBytes);
    h4198_Write(1,initBytes);
    h4198_Write(2,initBytes);
    h4198_Write(3,initBytes);
    delay_ms(200);
}


// h4235_Write - Write display data to an H4235 LCD, via a PCF85134
//               controller IC.
//
// The H4235 consists of two, 6-digit displays, each of which is
// controlled by its own NXP PCF85134 60-segment LCD controller.
//
// Returns zero on success
//
int h4235_Write(int disp,         // Display number: 1 (top) or 2 (bottom)
               uint8_t segData[]) // 60bits (7.5bytes) of LCD segment data
{
    int i;
    uint8_t bytesToSend[16];

    if(disp < 1 || disp > 2) 
        return 1;                 // Error
    
    // Send command bytes
    bytesToSend[0] = 0x80;        // Control byte: Command follows
    if(disp == 1)
        bytesToSend[1] = 0xe1;    // Device address for top line
    else
        bytesToSend[1] = 0xe0;    // Device address for bottom line
    bytesToSend[2] = 0x80;        // Control byte: Command follows
    bytesToSend[3] = 0x00;        // Data pointer = 0
    bytesToSend[4] = 0x40;        // Control byte: Data follows
    for(i=0; i<7; i++)            // 7 of 8 data bytes
    {
        bytesToSend[i+5] = segData[i];
    }
    bytesToSend[12] = 0;          // 8th data byte (10th byte overall) is not used

    // Write to the controller IC
    return nxpRawWrite(LCD_A2, bytesToSend, 13);
}


// h4198_Write - Write display data to an H4198 LCD, via PCF85176
//               controller IC.
//
// There can be up to 3 H4198s, each with their own NXP 40-segment
// LCD controller IC.
//
// Inputs:
//   dispNum - Which of the 3 displays to write to, 1..3 (3 on right)
//   segData - Raw segment data, 5 bytes (40 segments)
//
// Note: The dispNum 1..3 maps to the controller IC's "device addresses", 0..2.
//       These can be set via jumpers on the H4198 demo board:
//         Left   (dispNum 1): No jumper   (device addr 0)
//         Middle (dispNum 2): Jumper "A0" (device addr 1)
//         Right  (dispNum 3): Jumper "A1" (device addr 2)
//
// Returns zero on success; Error code otherwise.
//
int h4198_Write(int dispNum, uint8_t segData[])
{
    int i;
    uint8_t bytesToSend[16];

    // Check dispNum in range
    if(dispNum < 1 || dispNum > 3)
        return 1;  // Error
    
    // Send control bytes
    bytesToSend[0] = 0x80;               // Data pointer = 0; More commands follow
    bytesToSend[1] = 0x60 | (dispNum-1); // Device address; No more commands (data follows)

    // Copy the 5 bytes of raw segment data
    for(i=0; i<5; i++)
    {
        bytesToSend[i+2] = segData[i];
    }

    // Send to the controller IC
    return nxpRawWrite(LCD_A1, bytesToSend, 7);
}


// nxpRawWrite
//
// Write n data bytes to the LCD driver IC, via i2c bus. This includes
// preceding the bytes with an i2c start condition, and following the
// bytes with an i2c stop condition.
//
// Inputs:
//   sa   - I2C Slave address (we use 2: 0x70 and 0x72)
//   data - Byte array data to send
//   n    - Length of byte array
//
// Returns 0 on success; Error code otherwise TODO: Meaningfull error codes
//
// TODO: Add timeouts all over
//
int nxpRawWrite(uint8_t sa, uint8_t data[], int n)
{
    int i;
    I2C_STATUS status;

    // Wait for bus idle, then issue an i2c start
    while(!I2CBusIsIdle(LCD_I2C_BUS))
    {
        // TODO: If the nxp's get stuck, this stop seems to shake
        // them loose.  Verify this a valid thing to do?
        // TODO: Proper timout, here and everywhere.
        I2CStop(LCD_I2C_BUS);
        delay_ms(2);
    }

    // MAGIC ALERT! The addition of this statement seems to get this code working.
    // Without this, the following do..while() loop hangs forever.
    status = I2CGetStatus(LCD_I2C_BUS);

    // I2C Start (Returns either success or I2C_MASTER_BUS_COLLISION)
    if(I2CStart(LCD_I2C_BUS) != I2C_SUCCESS)
    {
        return 1;
    }

    // Wait for the start to complete. NOTE: Hangs forever, without
    // the dummy I2CGetStatus(), prior to the I2CStart() call.
    do 
    {
        status = I2CGetStatus(LCD_I2C_BUS);
        if(I2C_ARBITRATION_LOSS & status)
            I2CClearStatus(LCD_I2C_BUS, I2C_ARBITRATION_LOSS);
    } while ( !(status & I2C_START) );
    
    // Send the device slave address (this device is write-only,
    // so the R/W bit (bit 0) of the slave address is always zero.
    //
    while(!I2CTransmitterIsReady(LCD_I2C_BUS));
    status = I2CSendByte(LCD_I2C_BUS, sa);
    if(status != (I2C_STATUS)I2C_SUCCESS)
    {
        return 2;
    }
    while(!I2CTransmissionHasCompleted(LCD_I2C_BUS));
    if(!I2CByteWasAcknowledged(LCD_I2C_BUS))
    {
        return 3;
    }

    // Send the remaining data bytes.
    //
    for(i=0; i<n; i++)
    {
        while(!I2CTransmitterIsReady(LCD_I2C_BUS));
        status = I2CSendByte(LCD_I2C_BUS, data[i]);
        if(status != (I2C_STATUS)I2C_SUCCESS)
        {
            return 4;
        }
        while(!I2CTransmissionHasCompleted(LCD_I2C_BUS));
        if(!I2CByteWasAcknowledged(LCD_I2C_BUS))
        {
            return 5;
        }
    }

    // I2C Stop
    I2CStop(LCD_I2C_BUS);
    do {
        status = I2CGetStatus(LCD_I2C_BUS);
    } while(!(status & I2C_STOP));

    return 0;   // Successful return value
}



// h4235SetSegments / h4198SetSegments
//
// Given a string to display (displayStr), prepare the bytes
// that will be sent to the LCD driver to show that string.
//
// Since bytes ordering is different for the H4198 & H4235,
// each has their variant of this routine.
//
// Inputs:
//   displayStr - The string to display, with optional decimal
//                points or commas.
//                Example input strings:
//                    "1.23"
//                    "123"
//                    " 5,95"
// Outputs:
//   segmentByte  - Array of bytes that, when sent to the LCD
//                driver, will energize the correct segments to
//                display the input string.
//
// Returns 0 on success; Error code otherwise
//
int h4198_SetSegments(const char *displayStr,   // Display string to process
                      uint8_t segmentByte[5])   // Return 5 data bytes (40segments)
{
    int i;
    int len;         // Input string length
    int digit;       // Current digit
    uint8_t code;    // Seven segment code

    const int displaySize = 4;

    for(i=0; i<5; i++) segmentByte[i] = 0;

    len = strlen(displayStr); // Length of input string
    digit = 0;                // No digits have been found yet

    // Walk through the input string from right-to-left
    for(i=len-1; i>=0; i--)
    {
        // Is this a period? And does a period exist at this digit location?
        if(displayStr[i] == '.' && digit > 0)
        {
                // Set the period for the current digit
                segmentByte[digit] |= 1;  // LS bit turns on the period
        }
        // Is this a comma? And does a comma exist at this digit?
        else if(displayStr[i] == ',' && digit > 0)
        {
                // Set the comma segment for the current digit
                segmentByte[4] |= (0x04 >> (digit-1));
        }
        // Not a comma or period - try to get a 7-segment code for this char
        else
        {
            code = sevenSegCode(displayStr[i]);
            if(code != 0xff)             // A supported digit or character?
            {
                digit++;                 // Count the valid digits
                if(digit > displaySize)  // Any room left to continue?
                    break;               // If not, quit.

                // Set the segments to display this digit
                segmentByte[digit-1] |= code;
            }
        }
    }
    return 0;
}


int h4235_SetSegments(const char *displayStr,   // String to display
                      uint8_t segmentByte[8])   // 60 bits (7.5 bytes) of segment data
{
    int i;
    int len;         // Input string length
    int digit;       // Current digit
    uint8_t code;    // Seven segment code

    for(i=0; i<8; i++) segmentByte[i] = 0;

    len = strlen(displayStr); // Length of input string

    // Start "digit" at 7 - one past the right-most digit for the H4235
    digit = 7;

    // Walk through the input string from right-to-left.
    for(i=len-1; i>=0; i--)
    {
        // Is this a period, and does this position have a period?
        if(displayStr[i] == '.' && digit < 7)
        {
                // Set the period for the current digit
                segmentByte[digit-2] |= 1;  // LS bit turns on the period
        }
        // Is this a comma?
        else if(displayStr[i] == ',' && digit < 7)
        {
            segmentByte[6] |= (0x20 << (6-digit));  // Set the comma segment
        }
        // else, we assume this is a digit/char
        else
        {
            code = sevenSegCode(displayStr[i]);
            if(code != 0xff)             // A supported digit or char?
            {
                digit--;                 // Update our current working digit
                if(digit < 1)            // Any room left to continue?
                    break;               // If not, quit.

                // Set the segments to display this digit
                segmentByte[digit-1] |= code;
            }
        }
    }

    return 0;
}


// lcdWrite - Top level routine to write a string to the
//            H4235 or one of the H4198 LCDs.
//
int lcdWrite(int lcd,  // The LCD to write to: LCD_L1 ... LCD_S3
             char *s)  // The string to write
{
    uint8_t segmentData[32];    // Temp area for raw segment data
    int retval;

    if(lcd < LCD_L1 || lcd > LCD_S3) return 1;  // lcd number out of range?

    if(lcd == LCD_L1 || lcd == LCD_L2)          // Is this the H4235?
    {
        // Prepare the raw segment data for an H4235
        retval = h4235_SetSegments(s, segmentData);
        if(retval) return retval;

        // Write to the appropriate line of the H4235
        if(lcd == LCD_L1) retval = h4235_Write(1, segmentData);
        else              retval = h4235_Write(2, segmentData);

        if(retval) return retval;
    }
    else  // One of the H4198s
    {
        retval = h4198_SetSegments(s, segmentData);
        if(retval) return retval;

        if(lcd == LCD_S1)      retval = h4198_Write(1, segmentData);
        else if(lcd == LCD_S2) retval = h4198_Write(2, segmentData);
        else                    retval = h4198_Write(3, segmentData);

        if(retval) return retval;
    }

    return 0;  // Success
}


/* 
7-segment codes.
Dig   gfedcba abcdefg
---   ------- -------
 0     0x3F    0x7E
 1     0x06    0x30
 2     0x5B    0x6D
 3     0x4F    0x79
 4     0x66    0x33
 5     0x6D    0x5B
 6     0x7D    0x5F
 7     0x07    0x70
 8     0x7F    0x7F
 9     0x6F    0x7B
 A     0x77    0x77
 b     0x7C    0x1F
 C     0x39    0x4E
 d     0x5E    0x3D
 E     0x79    0x4F
 F     0x71    0x47

 G     0x6f    0x7B;
 H     0x76    0x37;
 I     0x06    0x30;
 J     0x0e    0x38;
 K     ----    ----
 L     0x38    0x0e;
 M     ----    ----
 N     0x54    ----;  (poor char)
 O     0x3f    0x7e;
 P     0x73    0x67;
 Q     ----    ----
 R     ----    ----
 S     0x6d    0x5B;
 T     0x78    ----
 U     0x3e    0x3E;
 V     ----    ----
 W     ----    ----
 X     ----    ----
 Y     0x6e    ----
 Z     ----    ----
' '    ----    0x00;
'-'    0x40    0x01;

*/

// sevenSegCode
//
// Given a hexadecimal digit, return the segment code that will
// display that digit on our LCD.  Returns 0xff if digit is not valid.
//
uint8_t sevenSegCode(char c)
{
    uint8_t code = 0;

    switch(toupper(c))
    {
        case ' ': code = 0x00; break;
        case '-': code = 0x40; break;
        case '0': code = 0x3F; break;
        case '1': code = 0x06; break;
        case '2': code = 0x5B; break;
        case '3': code = 0x4F; break;
        case '4': code = 0x66; break;
        case '5': code = 0x6D; break;
        case '6': code = 0x7D; break;
        case '7': code = 0x07; break;
        case '8': code = 0x7F; break;
        case '9': code = 0x6F; break;
        case 'A': code = 0x77; break;
        case 'B': code = 0x7C; break;
        case 'C': code = 0x39; break;
        case 'D': code = 0x5E; break;
        case 'E': code = 0x79; break;
        case 'F': code = 0x71; break;
        // "extended" chars
        case 'G': code = 0x6f; break;
        case 'H': code = 0x76; break;
        case 'I': code = 0x06; break;
        case 'J': code = 0x0e; break;
            //case 'K':
        case 'L': code = 0x38; break;
            //case 'M':
        case 'N': code = 0x54; break; // this one's a stretch
        case 'O': code = 0x3f; break;
        case 'P': code = 0x73; break;
            //case 'Q':
            //case 'R':
        case 'S': code = 0x6d; break;
        case 'T': code = 0x78; break; // eh? 
        case 'U': code = 0x3E; break;
            //case 'V':
            //case 'W':
            //case 'X':
        case 'Y': code = 0x6e; break;
            //case 'Z':

        default:
            code = 0xff; break;      // Invalid 
    }

    if(code != 0xff) {  // Was it a valid digit?
        code <<= 1;     // Our segments are in upper 7 bits.
    }

    return code;
}
