// C64MultiKernel.c Rev 2.0 (2022-01-03)
// coded by BWACK in mikroC, rewritten by TEBL for single LED.
// SX-64 reset logic (inspired by SX-64 Ultra Reset) by Jonni


// Multikernel switcher for the C64 breadbin/longboard
// a 2332 ROM to 27C256 ROM adapter with four kernels
// The microcontroller program flips through the four kernels by holding down
// the RESTORE-key, in other words its a 'switchless' design. Red LED will flash
// according to action, counting the flashes with 1 being reset, 2 is next 
// kernal, 3 will toggle drive number and the subsequent will jump to a specific
// kernal.

// MCU: PIC12F629 or PIC12F675
// EasyPIC5 development board
// use any pic-programmer and load the .hex file with same name as your PIC

// Project settings:
// 4MHz Intosc - I/O on GP4
// All functions disabled (MCLR, brownout etc.)


// changes:
//   2022-01-03 Rev 2.0  - Rewritten for SX-64 reset logic
//   2019-06-17 Rev 1.14 - Rewritten for single RGB
//   2017-11-02 Rev 1.13.1 - Added support for PIC12F675
//   2016-10-22 Rev 1.13 - putting the mcu to sleep
//   2016-09-17 Rev 1.12 - removed kernal=0. Switch-case state machine.

//#define DEBUG // uncomment this before debugging

// Inputs:
#define RESET_N GP3_bit
// Outputs:
#define RED_LED   GP2_bit
#define INTRST_N  GP1_bit // open-collector
// Addresses on GPIO B4 and B5

// finite state machine
#define IDLE_STATE 0
#define WAIT_RELEASE 1
#define KERNAL_TOGGLE 2
#define KERNAL_SET 3
#define RESET 4
#define DRIVE_TOGGLE 5

char STATE=IDLE_STATE;
char cycle=0,buttontimer=0, old_button;
char kernalno=0;
char driveno=0;

void setkernal(char _kernal) {
  GP4_bit=0;
  GP5_bit=0;
  GPIO|=kernalno<<4;
#ifndef DEBUG
  EEPROM_Write(0x00,kernalno);
#endif
}
void setdrive(char _drive) {
  GP0_bit=0;
  GPIO|=driveno;
#ifndef DEBUG
  EEPROM_Write(0x01,driveno);
#endif
}

void toggleKernal(void) {
  kernalno++;
  kernalno&=0x03;
  setkernal(kernalno);
}

void toggleDrive(void) {
  driveno++;
  driveno&=0x01;
  setdrive(driveno);
}

void intres(void) {
  INTRST_N=1;
  RED_LED=~RED_LED;
  delay_ms(50);
  RED_LED=~RED_LED;
  delay_ms(200); // was 500
  INTRST_N=0;
}

void setLED(void) {
  RED_LED = 0;    // default off
}

void blinkLED(void) {
  RED_LED=~RED_LED;
  delay_ms(100);
  RED_LED=~RED_LED;
  delay_ms(100);
}

void init(void) {
  char _i;
  OPTION_REG=0;
//  OPTION_REG=0b0001110; // about 1s watchdog timer.
  WPU.WPU1=1;
  CMCON=0x07; // digital IO
#ifdef P12F675
  ANSEL=0;
#endif
  
//ANSEL=0; // only defined for pic12f675
  TRISIO=0b00001000;
  INTRST_N=0;
  RESET_N=1;
  RED_LED=0;
#ifndef DEBUG
  kernalno=EEPROM_READ(0x00);
  driveno=EEPROM_READ(0x01);
#endif
  GP3_bit=1; // presetting inputs for the debugger
  if(kernalno>3) kernalno=0; // incase EEPROM garbage.
  if(driveno>1) driveno=0; // incase EEPROM garbage.
  setkernal(kernalno);
  setdrive(driveno);
  intres();

  IOC = 0b00001000; // GPIO interrupt-on-change mask
  GPIE_bit=1;       // GPIO Interrupt enable, on
  GIE_bit=0;        // Globale interrupt enable, off

  for(_i=0; _i<5; _i++) {
    //asm { CLRWDT };
    RED_LED=1;
    delay_ms(50);
    RED_LED=0;
    delay_ms(50);
  }
}

void main() {
  char i;
//  asm { CLRWDT };
  init();

  while(1) {
    setLED();

    switch(STATE) {
      case IDLE_STATE:
        if(!RESET_N ) {
          buttontimer++;
          delay_ms(100);
        } else {
          if (cycle > 0) {
            STATE=WAIT_RELEASE; old_button=0; buttontimer=0;
          } else {
            buttontimer=0;
            asm {sleep};
            GPIF_bit=0;

            delay_ms(100);
          }
        }

        // Bump cycle counter
        if (buttontimer > 10) {
          cycle++; buttontimer=0;
          blinkLED();
        }
        break;

      case WAIT_RELEASE:
        switch (cycle) {
          case 1:                  // 1-2 seconds
            STATE=RESET;
            break;
          case 2:                  // 2-3 seconds
            STATE=KERNAL_TOGGLE;
            break;
          case 3:                  // 2-3 seconds
            STATE=DRIVE_TOGGLE;
            break;
          case 4:
            STATE=KERNAL_SET;
            kernalno=0;
            break;
          case 5:
            STATE=KERNAL_SET;
            kernalno=1;
            break;
          case 6:
            STATE=KERNAL_SET;
            kernalno=2;
            break;
          case 7:
            STATE=KERNAL_SET;
            kernalno=3;
            break;
          default:
            STATE=RESET;
            break;
        }
        
        cycle=0;
        break;

      case KERNAL_TOGGLE:
        STATE=RESET;
        toggleKernal();
        delay_ms(20);
        break;

      case DRIVE_TOGGLE:
        STATE=RESET;
        toggleDrive();
        delay_ms(20);
        break;

      case KERNAL_SET:
        STATE=RESET;
        setkernal(kernalno);
        delay_ms(20);
        break;

      case RESET:
        STATE=IDLE_STATE;
        intres();
        buttontimer=0;
        break;

      default:
        break;
    }
  }
}