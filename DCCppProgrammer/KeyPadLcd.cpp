/**********************************************************************

SerialCommand.cpp
COPYRIGHT (c) 2013-2016 Gregg E. Berman

Part of DCC++ BASE STATION for the Arduino

**********************************************************************/

// DCC++ BASE STATION COMMUNICATES VIA THE SERIAL PORT USING SINGLE-CHARACTER TEXT COMMANDS
// WITH OPTIONAL PARAMTERS, AND BRACKETED BY < AND > SYMBOLS.  SPACES BETWEEN PARAMETERS
// ARE REQUIRED.  SPACES ANYWHERE ELSE ARE IGNORED.  A SPACE BETWEEN THE SINGLE-CHARACTER
// COMMAND AND THE FIRST PARAMETER IS ALSO NOT REQUIRED.

// See SerialCommand::parse() below for defined text commands.
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "KeyPadLcd.h"
#include "DCCppProgrammer.h"
#include "Accessories.h"
#include "Sensor.h"
#include "Outputs.h"
#include "EEStore.h"
#include "Comm.h"

extern int __heap_start, *__brkval;

///////////////////////////////////////////////////////////////////////////////

CmdStatus KeyPadLcd::mStatus=halt;
int KeyPadLcd::mCV=1;
int KeyPadLcd::mCVValue=3;
int KeyPadLcd::mInputLen=0;
int KeyPadLcd::mLoco=3;
int KeyPadLcd::mSpeed=0;
int KeyPadLcd::mDirection=1;
bool KeyPadLcd::mFunctions[]={0,0,0,0,0,0,0,0,0,0};
unsigned long KeyPadLcd::mMillis=millis();
char KeyPadLcd::commandString[MAX_COMMAND_LENGTH+1];
volatile RegisterList *KeyPadLcd::mRegs;
volatile RegisterList *KeyPadLcd::pRegs;
CurrentMonitor *KeyPadLcd::mMonitor;
Keypad *KeyPadLcd::mCustomKeypad;
LiquidCrystal_I2C *KeyPadLcd::mLcd;

///////////////////////////////////////////////////////////////////////////////

void KeyPadLcd::init(volatile RegisterList *_mRegs, volatile RegisterList *_pRegs, CurrentMonitor *_mMonitor, Keypad *_mCustomKeypad, LiquidCrystal_I2C *_mLcd){
  mRegs=_mRegs;
  pRegs=_pRegs;
  mMonitor=_mMonitor;
  mCustomKeypad=_mCustomKeypad;
  mLcd=_mLcd;
  sprintf(commandString,"");

  // Initialize LCD
  mLcd->init();    
  mLcd->backlight();

  mLcd->setCursor(0, 0);
  mLcd->print("DCC++ Club N Caldes");
  
  mLcd->setCursor(0, 1);
  mLcd->print(ARDUINO_TYPE);
  mLcd->print(" / ");
  mLcd->print(MOTOR_SHIELD_NAME);
  
  mLcd->setCursor(0, 2);
  mLcd->print("Ver. ");
  mLcd->print(VERSION);
  
  mLcd->setCursor(0, 3);
  mLcd->print(COMM_TYPE);
  mLcd->print(": ");
  #if COMM_TYPE == 0
    mLcd->print("SERIAL");
  #elif COMM_TYPE == 1
    mLcd->print(Ethernet.localIP());    
  #endif

} // KeyPadLcd::init

///////////////////////////////////////////////////////////////////////////////

void KeyPadLcd::process(){
    
  //KeyPad check
  char customKey = mCustomKeypad->getKey();
  char command[20];
  int mNewSpeed=0;

  //Checks the potentiometer for speed change
  if ((millis()-mMillis>500) && (mStatus==go))
  {
    mMillis=millis();
    mNewSpeed=map(analogRead(A8),0,1023,0,126);
    if (mNewSpeed!=mSpeed)
    {
      mSpeed=mNewSpeed;
      displayStatus();
      // <t REGISTER CAB SPEED DIRECTION>
      sprintf(command,"1 %i %i %i",mLoco,mSpeed,mDirection);
      mRegs->setThrottle(command);
    }
  }
  
  
  if (!customKey) return;

  //If in STOP mode only '*' key is allowed
  if (mStatus==halt && customKey!='*') 
  {
    displayStatus();
    return;
  }  
  
  switch (customKey)
  {
    /***** TURN ON POWER FROM MOTOR SHIELD TO TRACKS  ****/    
    case '*':      
     digitalWrite(SIGNAL_ENABLE_PIN_PROG,HIGH);
     digitalWrite(SIGNAL_ENABLE_PIN_MAIN,HIGH);
     INTERFACE.print("<p1>");
     mStatus=go;
     displayStatus();
     break;
          
    /***** TURN OFF POWER FROM MOTOR SHIELD TO TRACKS  ****/    
    case '#':
     digitalWrite(SIGNAL_ENABLE_PIN_PROG,LOW);
     digitalWrite(SIGNAL_ENABLE_PIN_MAIN,LOW);
     INTERFACE.print("<p0>");
     mStatus=halt;
     displayStatus();
     break;

    /***** MODE RUN ****/
    case 'F':
      if (mStatus==go)
      {
        mStatus=loco;
        mInputLen=0;
        displayStatus();
        break;
      }
      else
      {
        mStatus=go;
        displayStatus();
        break;
      }

    /***** MODE PROG *****/
    case 'G':      
      mStatus=progread;
      mInputLen=0;
      displayStatus();
      break;

    /**** Esc Key ****/
    case 'S':
      if (mStatus==progread)
      {
        mCV=1;
        mInputLen=0;
        displayStatus();
      }
      if (mStatus==progwrite)
      {
        mCVValue=0;
        mInputLen=0;
        displayStatus();
      }
      break;
    /**** Enter Key ****/
    case 'E':
      if (mStatus==progread && mCV>0)
      {
        // <R CV CALLBACKNUM CALLBACKSUB>
        sprintf(command,"%i 100 101",mCV);
        mCVValue=pRegs->readCV(command);
        if (mCV==1 && mCVValue>0) mLoco=mCVValue;
        displayStatus();
        break;
      }
      if (mStatus==progwrite && mCVValue>0)
      {
        // <W CV VALUE CALLBACKNUM CALLBACKSUB>
        sprintf(command,"%i %i 102 103",mCV,mCVValue);
        mCVValue=pRegs->writeCVByte(command);
        displayStatus();
        break;
      }
      if (mStatus==loco && mLoco>0)
      {
        mStatus=go;
        displayStatus();
        break;
      }
      break;
      
    /**** Right Arrow ****/
    case 'R':
      if (mStatus==progread)
      {
        mStatus=progwrite;
        if (mCVValue>99)
          mInputLen=3;
        else if (mCVValue>9)
          mInputLen=2;
        else
          mInputLen=1;
        displayStatus();
        break;        
      }
      if (mStatus==go)
      {
        mDirection=1;
        displayStatus();
        // <t REGISTER CAB SPEED DIRECTION>
        sprintf(command,"1 %i %i %i",mLoco,mSpeed,mDirection);
        mRegs->setThrottle(command);
        break;
      }
      break;
    /**** Left Arrow ****/
    case 'L':
      if (mStatus==progread && mInputLen>0)
      {
        if (mCV>9) mCV=mCV/10;
        mInputLen=mInputLen-1;        
        displayStatus();        
        break;
      }
      if (mStatus==progwrite && mInputLen==0)
      {
        mStatus=progread;
        if (mCV>99)
          mInputLen=3;
        else if (mCV>9)
          mInputLen=2;
        else
          mInputLen=1;
        displayStatus();
        break;
      }
      if (mStatus==progwrite && mInputLen>0)
      {
        if (mCVValue>9) mCVValue=mCVValue/10;
        mInputLen=mInputLen-1;        
        displayStatus();        
        break;
      }
      if (mStatus==go)
      {
        mDirection=0;
        displayStatus();
        // <t REGISTER CAB SPEED DIRECTION>
        sprintf(command,"1 %i %i %i",mLoco,mSpeed,mDirection);
        mRegs->setThrottle(command);
        break;
      }
      break;
    /***** Up Arrow *****/
    case 'U':
      if (mStatus==progread && mCV<255) {
        mCV++;
        displayStatus();
      }
      if (mStatus==progwrite && mCVValue<255) {
        mCVValue++;
        displayStatus();
      }
      break;
    /***** Down Arrow *****/
    case 'D':
      if (mStatus==progread && mCV>1) {
        mCV--;
        displayStatus();
      }
      if (mStatus==progwrite && mCVValue>0) {
        mCVValue--;
        displayStatus();
      }
      break;
    case '0':
      if (mStatus==go)
      {
        mFunctions[0]=!mFunctions[0];
        sprintf(command,"%i %i",mLoco,128+mFunctions[1]+mFunctions[2]*2+mFunctions[3]*4+mFunctions[4]*8+mFunctions[0]*16);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '1':
      if (mStatus==go)
      {
        mFunctions[1]=!mFunctions[1];
        sprintf(command,"%i %i",mLoco,128+mFunctions[1]+mFunctions[2]*2+mFunctions[3]*4+mFunctions[4]*8+mFunctions[0]*16);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '2':
      if (mStatus==go)
      {
        mFunctions[2]=!mFunctions[2];
        sprintf(command,"%i %i",mLoco,128+mFunctions[1]+mFunctions[2]*2+mFunctions[3]*4+mFunctions[4]*8+mFunctions[0]*16);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '3':
      if (mStatus==go)
      {
        mFunctions[3]=!mFunctions[3];
        sprintf(command,"%i %i",mLoco,128+mFunctions[1]+mFunctions[2]*2+mFunctions[3]*4+mFunctions[4]*8+mFunctions[0]*16);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '4':      
      if (mStatus==go)
      {
        mFunctions[4]=!mFunctions[4];
        sprintf(command,"%i %i",mLoco,128+mFunctions[1]+mFunctions[2]*2+mFunctions[3]*4+mFunctions[4]*8+mFunctions[0]*16);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '5':
      if (mStatus==go)
      {
        mFunctions[5]=!mFunctions[5];
        sprintf(command,"%i %i",mLoco,176+mFunctions[5]+mFunctions[6]*2+mFunctions[7]*4+mFunctions[8]*8);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '6':
      if (mStatus==go)
      {
        mFunctions[6]=!mFunctions[6];
        sprintf(command,"%i %i",mLoco,176+mFunctions[5]+mFunctions[6]*2+mFunctions[7]*4+mFunctions[8]*8);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '7':
      if (mStatus==go)
      {
        mFunctions[7]=!mFunctions[7];
        sprintf(command,"%i %i",mLoco,176+mFunctions[5]+mFunctions[6]*2+mFunctions[7]*4+mFunctions[8]*8);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '8':
      if (mStatus==go)
      {
        mFunctions[8]=!mFunctions[8];
        sprintf(command,"%i %i",mLoco,176+mFunctions[5]+mFunctions[6]*2+mFunctions[7]*4+mFunctions[8]*8);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
    case '9':
      if (mStatus==go)
      {
        mFunctions[9]=!mFunctions[9];
        sprintf(command,"%i %i",mLoco,160+mFunctions[9]);
        mRegs->setFunction(command);
        displayStatus();
        break;
      }
      if (mStatus==loco)
      {
        if (mInputLen==0)
        {
          mLoco=customKey - '0';
          mInputLen=1;
        }
        else
        {
          mLoco=mLoco*10+(customKey - '0');
          mInputLen++;
        }    
      }
      if (mStatus==progread)
      {
        if (mInputLen==0)
        {
          mCV=customKey - '0';
          mInputLen=1;
        }
        else
        {
          mCV=mCV*10+(customKey - '0');
          mInputLen++;
        }    
      }
      if (mStatus==progwrite)
      {
        if (mInputLen==0)
        {
          mCVValue=customKey - '0';
          mInputLen=1;
        }
        else
        {
          mCVValue=mCVValue*10+(customKey - '0');
          mInputLen++;
        }    
      }
      
      if (mCV>255) 
      {
        mCV=1;
        mInputLen=0;
      }
      if (mCVValue>255)
      {
        mCVValue=1;
        mInputLen=0;
      }
      if (mLoco>255)
      {
        mLoco=3;
        mInputLen=0;
      }
      displayStatus();
      break;
  } //switch

  
  //mLcd->setCursor(0, 0);
  //mLcd->print(analogRead(A8));mLcd->print("   ");

} // SerialCommand:process
   
///////////////////////////////////////////////////////////////////////////////

void KeyPadLcd::parse(char *com)
{
  
  
}; // SerialCommand::parse

void KeyPadLcd::displayStatus()
{
  char cadena[21];
  
  switch (mStatus)
  {
    case halt:
      mLcd->noBlink();
      mLcd->clear();
      mLcd->setCursor(0, 1);
      mLcd->print("####################");
      mLcd->setCursor(0, 2);
      mLcd->print("##     S T O P    ##");
      mLcd->setCursor(0, 3);
      mLcd->print("####################");
      break;
    case loco:
      mLcd->clear();
      mLcd->setCursor(0, 0);      
      mLcd->print("  Press loco + Ent");
      mLcd->setCursor(0, 2);
      mLcd->print(" Locomotive: ");mLcd->print(mLoco);
      mLcd->setCursor(13+mInputLen, 2);
      mLcd->blink();
      break;
    case go:
      mLcd->noBlink();
      mLcd->clear();
      mLcd->setCursor(0, 0);
      mLcd->print("  ##     RUN    ##");
      mLcd->setCursor(0, 1);
      mLcd->print("   LOCOMOTIVE: "); mLcd->print(mLoco);
      mLcd->setCursor(0, 2);
      mLcd->print("  SPEED:  ");mLcd->print(mSpeed);
      if (mDirection==1)
        mLcd->print("% ==> ");
      else
        mLcd->print("% <== ");
        
      mLcd->setCursor(0, 3);
      sprintf(cadena,"%i %i %i %i %i %i %i %i %i %i",mFunctions[0],mFunctions[1],mFunctions[2],mFunctions[3],mFunctions[4],
                                                    mFunctions[5],mFunctions[6],mFunctions[7],mFunctions[8],mFunctions[9]);
      mLcd->print(cadena);
      break;
    case progread:
      mLcd->clear();
      mLcd->setCursor(0, 0);
      mLcd->print("  ** Prog Mode **");
      mLcd->setCursor(0, 1);
      mLcd->print("CV:     Value:     ");
      mLcd->setCursor(0, 1);
      mLcd->print("CV: "); mLcd->print(mCV); 
      mLcd->setCursor(8, 1);
      mLcd->print("Value: "); mLcd->print(mCVValue);
      mLcd->setCursor(11, 2);
      printBits(mCVValue);

      mLcd->setCursor(0,3);
      mLcd->print("                    ");
      mLcd->setCursor(0,3);
      switch (mCV){
        case 1: mLcd->print(" Adress number"); break;
        case 2: mLcd->print(" Start voltage"); break;
        case 3: mLcd->print(" Acceleration rate"); break;
        case 4: mLcd->print(" Deceleration rate"); break;
        case 5: mLcd->print(" Maximum speed"); break;
        case 6: mLcd->print(" Medium speed"); break;
        case 7: mLcd->print(" Manufacturer Ver."); break;
        case 8: mLcd->print(" Manufacturer Id"); break;
        case 29: mLcd->print(" Config register"); break;
        case 33: mLcd->print(" Cab light FL(f)"); break;
        case 34: mLcd->print(" Cab light FL(r)"); break;
      }
      
      mLcd->setCursor(4+mInputLen, 1);
      mLcd->blink();     
      break;
    case progwrite:
      mLcd->clear();
      mLcd->setCursor(0,3);
      if (mCVValue==-1)
      {
        mCVValue=0;
        mInputLen=0;
        mLcd->print("Write CV ERROR");
      }
              
      mLcd->setCursor(0, 0);
      mLcd->print("  ** Prog Mode **");
      mLcd->setCursor(0, 1);
      mLcd->print("CV:     Value:     ");
      mLcd->setCursor(0, 1);
      mLcd->print("CV: "); mLcd->print(mCV); 
      mLcd->setCursor(8, 1);
      mLcd->print("Value: "); mLcd->print(mCVValue); 
      mLcd->setCursor(11, 2);
      printBits(mCVValue);
      mLcd->setCursor(15+mInputLen, 1);
      mLcd->blink();
      break;
  }
}; //displayStatus

void KeyPadLcd::printBits(int n) {  
  char b;  
  for (byte i = 0; i < 8; i++) {    
    b = (n & (1 << (7 - i))) > 0 ? '1' : '0';
    mLcd->print(b);    
  }
}

///////////////////////////////////////////////////////////////////////////////
