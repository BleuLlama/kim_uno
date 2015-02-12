/*
 * display
 *
 *   This drives the LEDs for common anode, cathode, and size configuration
 *   It also handles text rendering
 */

#include "Arduino.h"
#include "config.h"

extern "C" {

char shiftKey = 0;  // is the keypad shift key in effect?
char threeHex[3][2];        // seLED display
char textHex[7];

const byte ledSegments[8] = { A5, 2,3,4,5,6,7,8 }; // note col A5 is the extra one linked to DP
// ledSegments corrolates with the bits in "dig" below ( dp, a, b, c, d, e, f, g)

const byte ledDigits[8] =  { 12, 13, A0, A1,  A4, A2, A3, A7 };
// notice that the displays are ordred on the KIM UNO board as:
// led1 led2 led3 led4   led7 led5 led6 (led8)    (led8 is not connected)
//  D12  D13  A0   A1     A4   A2   A3    (A7)


byte segmentLookup[31] = { /* NOTE: this mirrors the values in the end of the ROM "TABLE" */ 
// bits     _6543210
// digits   abcdefg
          B01111110,//0  
          B00110000,//1
          B01101101,//2
          B01111001,//3
          B00110011,//4
          B01011011,//5
          B01011111,//6
          B01110000,//7
          B01111111,//8
          B01111011,//9
          B01110111,//a
          B00011111,//b
          B01001110,//c
          B00111101,//d
          B01001111,//e
          B01000111,//f
          /* all of the above are confirmed identical to the ROM  */
          
          /* additional characters */
          B00000001, //g printed as -
          B00001000, //h printed as _
          B00000000, //i printed as <space>
          
          /* some letters we need for text display */
          B00010101, // 19 'n'  "s on"
          B00011101, // 20 'o'  "s on"/"s off"
          B01100111, // 21 'p'  "EEP"
          B00000101, // 22 'r'  "E ro"/"E rw"
          B00001111, // 23 't'  "sst"
          B00111110, // 24 'U'  "Uno"  
          B00101010, // 25 'w'  "rw" (common version for 7 seg)
          B00001101, // 26 'c'
          B00010111, // 27 'h'
          B00100000, // 28 'i'
          B01111101, // 29 'a'
          B10000000  // 30 '.'
};


////////////////////////////////////////
// for text display
unsigned long textTimeout; 

// displayText
//  pass it in one of these values, and the number of ms to display (eg 500)
//  for text display

#define kDt_blank   (0)
#define kDt_SST_ON  (1)
#define kDt_SST_OFF (2)
#define kDt_EE_RW   (3)
#define kDt_EE_RO   (4)
#define kDt_Uno     (5)
#define kDt_Scott   (6)
#define kDt_Oscar   (7)

void displayText( int which, long timeMillis )
{
  for( int x = 0 ; x<8 ; x++ ) 
    textHex[x] = 18; // ' ' space

  if( which == kDt_SST_ON ) {
    textHex[0] = 5;  // 5
    textHex[1] = 5;  // 5
    textHex[2] = 23; // t
    textHex[4] = 20; // o
    textHex[5] = 19; // n
  } else if( which == kDt_SST_OFF ) {
    textHex[0] = 5;  // 5
    textHex[1] = 5;  // 5
    textHex[2] = 23; // t
    textHex[4] = 20; // o
    textHex[5] = 15; // f
    textHex[6] = 15; // f
  } else if( which ==  kDt_EE_RW ) {
    textHex[0] = 14; // E
    textHex[1] = 14; // E
    textHex[2] = 21; // P
    textHex[4] = 22; // R
    textHex[5] = 25; // W
  } else if( which ==  kDt_EE_RO ) {
    textHex[0] = 14; // E
    textHex[1] = 14; // E
    textHex[2] = 21; // P
    textHex[4] = 22; // R
    textHex[5] = 20; // o
  } else if( which == kDt_Uno ) {
    textHex[0] = 24; // U
    textHex[1] = 19; // n
    textHex[2] = 20; // o
    textHex[3] = 30; // .
    textHex[4] = 0;  // 0
    textHex[5] = 7;  //   7
  } else if( which == kDt_Scott ) {
    textHex[0] = 5;  // s
    textHex[1] = 26; // c
    textHex[2] = 20; // o
    textHex[3] = 23; // t
    textHex[4] = 23; // t
  } else if( which == kDt_Oscar ) {
    textHex[0] = 20; // o
    textHex[1] = 5;  // s
    textHex[2] = 26; // c
    textHex[3] = 29; // a
    textHex[4] = 22; // r
  }

  textTimeout = millis() + timeMillis; // when to switch back
}

void disableLEDs()
{
  for (int led=0;led<8;led++)
  { 
    pinMode(ledDigits[led], INPUT);  // set led pins to input
                                  // not really necessary, just to stop them
                                 // from driving either high or low.
    digitalWrite(ledDigits[led], HIGH); // Use builtin pullup resistors
  }
}


// display a bitpattern, map it to segments
void displayPattern( int digit, int pattern )
{
#ifdef kDisplayIsCommonAnode
  #define kClearSeg    HIGH
  #define kClearDigit  LOW
  #define kEnableSeg   LOW
  #define kEnableDigit HIGH
#else
  #define kClearSeg    LOW
  #define kClearDigit  HIGH
  #define kEnableSeg   HIGH
  #define kEnableDigit LOW
#endif
  byte commonAnode = 1;
  
  // clear all segments
  for( int i=0 ; i<=8 ; i++ ) /* segment a .. segment f, dp */
    digitalWrite( ledSegments[i], kClearSeg );

  // clear all digits
  for( int j=0 ; j <=7 ; j++ ) /* digit 0 .. digit 6 */
    digitalWrite( ledDigits[j], kClearDigit );

  // enable the right segments
  if( pattern & 0x80 ) digitalWrite( ledSegments[0], kEnableSeg ); // dp
  if( pattern & 0x40 ) digitalWrite( ledSegments[1], kEnableSeg ); // a
  if( pattern & 0x20 ) digitalWrite( ledSegments[2], kEnableSeg ); // b
  if( pattern & 0x10 ) digitalWrite( ledSegments[3], kEnableSeg ); // c
  if( pattern & 0x08 ) digitalWrite( ledSegments[4], kEnableSeg ); // d
  if( pattern & 0x04 ) digitalWrite( ledSegments[5], kEnableSeg ); // e
  if( pattern & 0x02 ) digitalWrite( ledSegments[6], kEnableSeg ); // f
  if( pattern & 0x01 ) digitalWrite( ledSegments[7], kEnableSeg ); // g

  // enable this digit
  digitalWrite( ledDigits[digit], kEnableDigit );
  delay( 2 );
  digitalWrite( ledDigits[digit], kClearDigit );
}

void driveLEDs()
{
  for( int i=0 ; i<=8 ; i++ ) /* segment a .. segment f, dp */
  {
    pinMode( ledSegments[i], OUTPUT );
    digitalWrite( ledSegments[i], LOW );
  }

  for( int j=0 ; j <=7 ; j++ ) /* digit 0 .. digit 6 */
  {
    pinMode( ledDigits[j], OUTPUT );
    digitalWrite( ledDigits[j], LOW );
  }

  for( int digit =0 ; digit < 8 ; digit++ )
  {
    byte pattern = 0;
    if( millis() >= (long)textTimeout ) { // not elegant, but good for now
      // display the internal digits
      if( digit == kDisplayAddrOffset   ) pattern = segmentLookup[threeHex[0][0]];
      if( digit == kDisplayAddrOffset+1 ) pattern = segmentLookup[threeHex[0][1]];
      if( digit == kDisplayAddrOffset+2 ) pattern = segmentLookup[threeHex[1][0]];
      if( digit == kDisplayAddrOffset+3 ) pattern = segmentLookup[threeHex[1][1]];
      
      if( digit == kDisplayDataOffset   ) pattern = segmentLookup[threeHex[2][0]];
      if( digit == kDisplayDataOffset+1 ) pattern = segmentLookup[threeHex[2][1]];
    } 
    else 
    {
      // display the temporary text
      pattern = segmentLookup[textHex[digit]];
    }
    
    // patch the digits
    if( (digit == kDisplayShift)  &&  shiftKey ) pattern |= 0x02;  // shift indicator
    if( digit == kDisplayDot ) pattern |= 0x80;  // decimal point separator
    
    displayPattern( digit, pattern );
  }
}

}