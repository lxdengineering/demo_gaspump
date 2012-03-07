#ifndef _NXP_LCD_DRIVER_H_
#define _NXP_LCD_DRIVER_H_

#include <stdint.h>

// From a software viewpoint, we have 5 LCDs:
#define LCD_L1 1  /* Large display, line 1 (H4235, top line, 6 digits) */
#define LCD_L2 2  /* Large display, line 2 (H4235, bottom line, 6 digits) */
#define LCD_S1 3  /* Small display, left (H4198, 4 digits) */
#define LCD_S2 4  /* Small display, middle (H4198, 4 digits) */
#define LCD_S3 5  /* Small display, right (H4198, 4 digits) */


// NXP Controller IC's I2C addresses are 0x70 or 0x72
#define LCD_A1 0x70  /* H4198 displays (up to 3, with NXP PCF85176 ICs */
#define LCD_A2 0x72  /* H4235's two sub-displays (2 NXP PCF85134 ICs */

// Initialize the pic's I2C interface, and the NXP LCD control ICs.
void nxpInit(int peripheralBusClock);


// Write a string to one of the LCDs
int lcdWrite(int lcd,  // LCD to write to (LCD_L1, LCD_L2, LCD_S1,... )
             char *s); // The string to write; usually digits, with optional periods or commas
             

// ---------------------------------------------------------------------
// Private functions - not intended for external use

// Given a string to display, convert to a raw segmentData[] array
// that, if sent to the LCD controller, would display the input string.
int h4198_SetSegments(const char *displayStr,   // Display string to process
                      uint8_t segmentData[5]);  // Return 5 data bytes (40segments)
int h4235_SetSegments(const char *displayStr,   // String to display
                      uint8_t segmentData[8]);  // 60 bits (7.5 bytes) segment data

// Send a raw segmentData[] array to the LCD controller IC
int h4198_Write(int dispNumber, uint8_t segmentData[5]);
int h4235_Write(int dispNumber, uint8_t segmentData[8]);


int nxpRawWrite(uint8_t i2c_address, uint8_t data[], int n);
uint8_t sevenSegCode(char c);

#endif
