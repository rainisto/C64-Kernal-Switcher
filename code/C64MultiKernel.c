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
//   2022-02-28 Rev 2.0.3 - Added config for mute toggle
//   2022-01-27 Rev 2.0.2 - Added config for wait time toggle
//   2022-01-26 Rev 2.0.1 - Added config for start- and reset-sound toggle
//   2022-01-03 Rev 2.0  - Rewritten for SX-64 reset logic
//   2019-06-17 Rev 1.14 - Rewritten for single RGB
//   2017-11-02 Rev 1.13.1 - Added support for PIC12F675
//   2016-10-22 Rev 1.13 - putting the mcu to sleep
//   2016-09-17 Rev 1.12 - removed kernal=0. Switch-case state machine.

//#define DEBUG // uncomment this before debugging

// Inputs:
#define RESET_N   GP3_bit
// Outputs:
#define RED_LED   GP2_bit
#define INTRST_N  GP1_bit // open-collector
// Addresses on GPIO B4 and B5

// finite state machine
#define IDLE_STATE        0
#define WAIT_RELEASE      1
#define KERNAL_TOGGLE     2
#define KERNAL_SET        3
#define RESET             4
#define DRIVE_TOGGLE      5
#define RESETSOUND_TOGGLE 6
#define STARTSOUND_TOGGLE 7
#define WAITTIME_TOGGLE   8
#define MUTE_TOGGLE       9

char STATE=IDLE_STATE;
char cycle=0,buttontimer=0, old_button;
char kernalno=0;
char driveno=0;
char resetsound=1;
char startsound=1;
char waittime=0;
char mute=0;

void setKernal(char _kernal) {
  GP4_bit=0;
  GP5_bit=0;
  GPIO|=kernalno<<4;
#ifndef DEBUG
  EEPROM_Write(0x00,kernalno);
#endif
}
void setDrive(char _drive) {
  GP0_bit=0;
  GPIO|=driveno;
#ifndef DEBUG
  EEPROM_Write(0x01,driveno);
#endif
}

void setResetSound(char _sound) {
  EEPROM_Write(0x02,resetsound);
}

void setStartSound(char _sound) {
  EEPROM_Write(0x03,startsound);
}

void setWaitTime(char _waittime) {
  EEPROM_Write(0x04,waittime);
}

void setMuteSounds(char _waittime) {
  EEPROM_Write(0x05,mute);
}

void toggleKernal(void) {
  kernalno++;
  kernalno&=0x03;
  setKernal(kernalno);
}

void toggleDrive(void) {
  driveno++;
  driveno&=0x01;
  setDrive(driveno);
}

void toggleResetSound(void) {
  resetsound++;
  resetsound&=0x01;
  setResetSound(resetsound);
}

void toggleStartSound(void) {
  startsound++;
  startsound&=0x01;
  setStartSound(startsound);
}

void toggleMuteSounds(void) {
  mute++;
  mute&=0x01;
  setMuteSounds(mute);
}

void toggleWaitTime(void) {
  waittime++;
  waittime&=0x01;
  setWaitTime(waittime);
}

void intres(void) {
  INTRST_N=1;
  if(resetsound && !mute) RED_LED=~RED_LED;
  delay_ms(200); // 200ms reset sound/blink
  if(resetsound && !mute) RED_LED=~RED_LED;
  delay_ms(50);
  INTRST_N=0;
}

void setLED(void) {
  RED_LED = 0;    // default off
}

void blinkLED(void) {
  if(!mute) RED_LED=~RED_LED;
  delay_ms(100);
  if(!mute) RED_LED=~RED_LED;
  delay_ms(100);
}

void init(void) {
  char _i;
  OPTION_REG=0;
  // OPTION_REG=0b0001110; // about 1s watchdog timer.
  // WPU.WPU1=1;           // Weak pull-up register
  CMCON=0x07;              // digital IO
#ifdef P12F675
  ANSEL=0;
#endif
  
  // ANSEL=0;              // only defined for pic12f675
  TRISIO=0b00001000;
  INTRST_N=0;
  RESET_N=1;
  RED_LED=0;
#ifndef DEBUG
  kernalno=EEPROM_READ(0x00);
  driveno=EEPROM_READ(0x01);
#endif
  resetsound=EEPROM_READ(0x02);
  startsound=EEPROM_READ(0x03);
  waittime=EEPROM_READ(0x04);
  mute=EEPROM_READ(0x05);
  GP3_bit=1; // presetting inputs for the debugger
  if(kernalno>3) kernalno=0;     // incase EEPROM garbage.
  if(driveno>1) driveno=0;       // incase EEPROM garbage.
  if(resetsound>1) resetsound=1; // incase EEPROM garbage.
  if(startsound>1) startsound=1; // incase EEPROM garbage.
  if(waittime>1) waittime=0;     // incase EEPROM garbage.
  if(mute>1) mute=0;             // incase EEPROM garbage.
  setKernal(kernalno);
  setDrive(driveno);
  intres();

  IOC = 0b00001000; // GPIO interrupt-on-change mask
  GPIE_bit=1;       // GPIO Interrupt enable, on
  GIE_bit=0;        // Globale interrupt enable, off

  if(startsound && !mute) {
    for(_i=0; _i<5; _i++) {
      //asm { CLRWDT };
      RED_LED=1;
      delay_ms(50);
      RED_LED=0;
      delay_ms(50);
    }
  }
}

void main() {
  char i;
  // asm { CLRWDT };
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
        if (buttontimer > 10+waittime*10) {
          if(cycle < 11) cycle++;
          buttontimer=0;
          blinkLED();
        }
        break;

      case WAIT_RELEASE:
        switch (cycle) {
          case 1:                  // 1 led/piezo blink cycles
            STATE=RESET;
            break;
          case 2:                  // 2 blinks
            STATE=KERNAL_TOGGLE;
            break;
          case 3:                  // 3 blinks
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
          case 8:
            STATE=RESETSOUND_TOGGLE;
            break;
          case 9:
            STATE=STARTSOUND_TOGGLE;
            break;
          case 10:
            STATE=WAITTIME_TOGGLE;
            break;
          case 11:
            STATE=MUTE_TOGGLE;
            break;
          default:
            STATE=RESET;
            break;
        }
        
        cycle=0;
        break;

      case RESETSOUND_TOGGLE:
        STATE=RESET;
        toggleResetSound();
        delay_ms(20);
        break;

      case STARTSOUND_TOGGLE:
        STATE=RESET;
        toggleStartSound();
        delay_ms(20);
        break;

      case WAITTIME_TOGGLE:
        STATE=RESET;
        toggleWaitTime();
        delay_ms(20);
        break;

      case MUTE_TOGGLE:
        STATE=RESET;
        toggleMuteSounds();
        delay_ms(20);
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
        setKernal(kernalno);
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