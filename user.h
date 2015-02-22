/*
 * user.h - User preferences file
 * Modify this header to use the correct options for your installation. 
 */

#ifndef __USER_H__
#define __USER_H__

#define VERSION 0.01

#define MCU328             // Set this if using any boards other than the "Mega"

#define HWV3STD            // Use this option if using Open access v3 Standard board

//#define LCDBOARD            // Uncomment to use Adafruit RGB LCD Shield                                        
                                        
#define UBAUDRATE 9600      // Set the baud rate for the USB serial port

#define CARDFORMAT 0        // Card format
                            // 0=first 25 raw bytes from card
                            // 1=First and second parity bits stripped (default for most systems)

#endif /* __USER_H__ */


