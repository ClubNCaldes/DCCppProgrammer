/**********************************************************************

KeyPadLcd.h
COPYRIGHT (c) 2019 Dani Guisado Serra

Add-on for DCC++ BASE STATION for the Arduino

**********************************************************************/

#ifndef KeyPadLcd_h
#define KeyPadLcd_h

#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "PacketRegister.h"
#include "CurrentMonitor.h"

#define  MAX_COMMAND_LENGTH         30

enum CmdStatus {halt, loco, go, progread, progwrite};

struct KeyPadLcd{
  static CmdStatus mStatus;
  static int mCV;
  static int mCVValue;
  static int mInputLen;
  static int mLoco;
  static int mSpeed;
  static int mDirection;
  static bool mFunctions[10];
  static unsigned long mMillis;
  static char commandString[MAX_COMMAND_LENGTH+1];
  static volatile RegisterList *mRegs, *pRegs;
  static CurrentMonitor *mMonitor;
  static Keypad *mCustomKeypad;
  static LiquidCrystal_I2C *mLcd;
  static void init(volatile RegisterList *, volatile RegisterList *, CurrentMonitor *, Keypad *, LiquidCrystal_I2C *);
  static void parse(char *);
  static void process();
  static void displayStatus();
  static void printBits(int n);
}; // SerialCommand
  
#endif
