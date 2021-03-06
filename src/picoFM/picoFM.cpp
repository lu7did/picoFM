/*
 * picoFM
 * Raspberry Pi based VHF transceiver based on the DRA818V chipseet
 *---------------------------------------------------------------------
 * Created by Pedro E. Colla (lu7did@gmail.com)
 * Code excerpts from several packages:
 *
 * Also libraries
 *
 * ---------------------------------------------------------------------
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

//----------------------------------------------------------------------------
//  includes
//----------------------------------------------------------------------------

//*---- Generic includes

#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <pigpio.h>
#include <wiringPiI2C.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <wiringSerial.h>
#include <sstream>
#include <iomanip>
#include <assert.h>
#include <termios.h>
#include <unistd.h>
//*---- Program specific includes
#include "./picoFM.h"
#include "../lib/DRA818V.h"
#include "/home/pi/OrangeThunder/src/lib/CAT817.h"
#include "/home/pi/OrangeThunder/src/lib/CallBackTimer.h"
#include "/home/pi/OrangeThunder/src/lib/genVFO.h"
#include "/home/pi/PixiePi/src/lib/LCDLib.h"
#include "/home/pi/PixiePi/src/lib/MMS.h"

#include <iostream>
#include <cstdlib>    // for std::rand() and std::srand()
#include <ctime>      // for std::time()

#include <chrono>
#include <future>
#define SIGTERM_MSG "SIGTERM received.\n"

byte  TRACE=0x02;
byte  MSW=0x00;
byte  GSW=0x00;
byte  SSW=0x00;

void setPTT(bool f);
static void sighandler(int signum);
//-------------------- GLOBAL VARIABLES ----------------------------
const char   *PROGRAMID="picoFM";
const char   *PROG_VERSION="1.0";
const char   *PROG_BUILD="00";
const char   *COPYRIGHT="(c) LU7DID 2018,2020";

int       a;
int       anyargs;
int       lcd_light;
char      cmd[256];

DRA818V   *d=nullptr;
LCDLib    *lcd=nullptr;
genVFO    *vfo=nullptr;

char*     LCD_Buffer;
char      timestr[32];
// *----------------------------------------------------------------*
// *                  GPIO support processing                       *
// *----------------------------------------------------------------*
//*--- debouncing logic setup
auto startEncoder=std::chrono::system_clock::now();
auto endEncoder=std::chrono::system_clock::now();

auto startPush=std::chrono::system_clock::now();
auto endPush=std::chrono::system_clock::now();

auto startPTT=std::chrono::system_clock::now();
auto endPTT=std::chrono::system_clock::now();

auto startSQL=std::chrono::system_clock::now();
auto endSQL=std::chrono::system_clock::now();

int value=0;
int lastEncoded=0;
int counter=0;
int clkLastState=0; 
int pushPTT=0;
int pushSQL=0;

//* --- Define minIni related parameters (configuration persistence)

char inifile[80];
char iniStr[100];
long nIni;
int  sIni,kIni;
char iniSection[50];

// *----------------------------------------------------------------*
// *                Manu Subsytem definitions                       *
// *----------------------------------------------------------------*
MMS* root;
MMS* mnu_BW;
MMS* mnu_Vol;
MMS* mnu_Sql;
MMS* mnu_Rx_CTCSS;
MMS* mnu_Tx_CTCSS;
MMS* mnu_Ofs;
MMS* mnu_PFE;
MMS* mnu_LPF;
MMS* mnu_HPF;
MMS* mnu_Mode;
MMS* mnu_HL;
MMS* mnu_PD;
MMS* mnu_Backlight;
MMS* mnu_Step;
MMS* mnu_Watchdog;

MMS* mnu_BW_12KHZ;
MMS* mnu_BW_25KHZ;

MMS* mnu_Vol_sp;
MMS* mnu_Sql_sp;

MMS* mnu_Rx_CTCSS_sp;
MMS* mnu_Tx_CTCSS_sp;

MMS* mnu_Ofs_None;
MMS* mnu_Ofs_Plus;
MMS* mnu_Ofs_Minus;

MMS* mnu_PFE_Off;
MMS* mnu_PFE_On;

MMS* mnu_LPF_Off;
MMS* mnu_LPF_On;

MMS* mnu_HPF_Off;
MMS* mnu_HPF_On;

MMS* mnu_HL_Off;
MMS* mnu_HL_On;

MMS* mnu_PD_Off;
MMS* mnu_PD_On;

MMS* mnu_Step_10KHZ;
MMS* mnu_Step_5KHZ;

MMS* mnu_Backlight_Off;
MMS* mnu_Backlight_On;

MMS* mnu_Watchdog_Off;
MMS* mnu_Watchdog_On;




int  TBCK=0;
int  TSAVE=0;
int  TVFO=0;
int  TRSSI=0;
int  TBACKLIGHT=0;
int  TWATCHDOG=0;
// *----------------------------------------------------------------*
// *               Initial setup values                             *
// *----------------------------------------------------------------*
byte  m=MFM;
float f=147120000.0;
float ofs=600000.0;
int   vol=5;
int   sql=1;
char  callsign[16];
char  grid[16];
int   backlight=0;
int   watchdog=0;
bool  cooler=false;
float rx_ctcss=0;
float tx_ctcss=0;
bool  bPD=true;
bool  bHL=false;
bool  bPFE=false;
bool  bLPF=false;
bool  bHPF=false;
int   RSSI=135;
int   RSSIant=135;
int   nant=-1;
byte  col=0;
struct sigaction sigact;
CallBackTimer* masterTimer;
char portname[32]; 

//*--------------------------[System Word Handler]---------------------------------------------------
//* getWord Return status according with the setting of the argument bit onto the SW
//*--------------------------------------------------------------------------------------------------
bool getWord (unsigned char SysWord, unsigned char v) {

  return SysWord & v;

}
//*--------------------------------------------------------------------------------------------------
//* setWord Sets a given bit of the system status Word (SSW)
//*--------------------------------------------------------------------------------------------------
void setWord(unsigned char* SysWord,unsigned char v, bool val) {

  *SysWord = ~v & *SysWord;
  if (val == true) {
    *SysWord = *SysWord | v;
  }

}
// ======================================================================================================================
// sighandler
// ======================================================================================================================
static void sighandler(int signum)
{

   if (signum==28) {
      (TRACE >= 0x00 ? fprintf(stderr, "\n%s:sighandler() SIG(%d), ignored!\n",PROGRAMID,signum) : _NOP);
      return;
   }

   (TRACE >= 0x00 ? fprintf(stderr, "\n%s:sighandler() Signal caught(%d), exiting!\n",PROGRAMID,signum) : _NOP);
   setWord(&MSW,RUN,false);
   if (getWord(MSW,RETRY)==true) {
      (TRACE >= 0x00 ? fprintf(stderr, "\n%s:sighandler() Re-entering SIG(%d), force!\n",PROGRAMID,signum) : _NOP);
      exit(16);
   }
   setWord(&MSW,RETRY,true);

}
#include "./GUI.h"

//--------------------------------------------------------------------------------------------------
// FT8ISR - interrupt service routine, keep track of the FT8 sequence windows
//--------------------------------------------------------------------------------------------------
void ISRHandler() {


   if (TVFO!=0) {
       TVFO--;
       if (TVFO==0) {
           setWord(&SSW,FVFO,true);
       }
   }

   if (TRSSI!=0) {
       TRSSI--;
       if (TRSSI==0) {
          if (d!=nullptr) {
              d->sendRSSI();
          }
          TRSSI=1000;
       }
   }


   if (TBACKLIGHT!=0) {
       TBACKLIGHT--;
       if (TBACKLIGHT==0) {
          setBacklight(false);
       }
   }

   if (TWATCHDOG!=0) {
       TWATCHDOG--;
       if (TWATCHDOG==0) {
          if (vfo!=nullptr) {setWord(&vfo->FT817,WATCHDOG,true); vfo->setPTT(false);}
       }
   }

   return;

}
//*-------------------------------------------------------------------------------------------------
//* print_usage
//* help message at program startup
//*-------------------------------------------------------------------------------------------------
void print_usage(void)
{

fprintf(stderr,"\n%s version %s build (%s)\n"
"Usage:\npicoFM  [-f frequency {144000000..147999999 Hz}]\n"
"                [-o offset (+/-Hz) default=0)]\n"
"                [-v volume (0..8 default=5)]\n"
"                [-b backlight (0..60 default=0)]\n"
"                [-w watchdog (0..90 default=0)]\n"
"                [-p high power]\n"
"                [-z low energy]\n"
"                [-1 pre-emphasis]\n"
"                [-2 low pass filter]\n"
"                [-3 high pass filter]\n"
"                [-s squelch(0..8 default=5)]\n"
"                [-r Rx CTCSS (0..38 default=0)]\n"
"                [-t Tx CTCSS (0..38 default=0)]\n"
"                [-x Verbose {0..2} default=0}]\n",PROGRAMID,PROG_VERSION,PROG_BUILD);

}

//*--------------------------------------------------------------------------------------------------
//* main execution of the program
//*--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
     std::srand(static_cast<unsigned int>(std::time(nullptr))); // set initial seed value to system clock    
     fprintf(stderr,"%s Version %s Build(%s) %s\n",PROGRAMID,PROG_VERSION,PROG_BUILD,COPYRIGHT);

     for (int i = 0; i < 64; i++) {
        if (i != SIGALRM && i != 17) {
           signal(i,sighandler);
        }
     }

//*--------- Process arguments to override persistence

while(true)
        {
                a = getopt(argc, argv, "o:s:r:t:x:v:b:w:f:hzp123?");

                if(a == -1) 
                {
                        if(anyargs) {
                           break; 
                        } else {
                           break;
                        }
                }
                anyargs = 1;    

                switch(a)
                {

                case 'f': 
	                f=atof(optarg);
                        fprintf(stderr,"%s:main() args(frequency)=%5.0f\n",PROGRAMID,f);
                        break;
                case 'o': 
	                ofs=atof(optarg);
                        fprintf(stderr,"%s:main() args(offset)=%5.0f\n",PROGRAMID,ofs);
                        break;
                case 'v':
                        vol=atoi(optarg);
                        fprintf(stderr,"%s:main() args(volume)=%d\n",PROGRAMID,vol);
                        break;
                case 'b':
                        backlight=atoi(optarg);
                        fprintf(stderr,"%s:main() args(backlight)=%d\n",PROGRAMID,backlight);
                        break;
                case 'w':
                        watchdog=atoi(optarg);
                        fprintf(stderr,"%s:main() args(watchdog)=%d\n",PROGRAMID,watchdog);
                        break;
                case 's':
                        sql=atoi(optarg);
                        fprintf(stderr,"%s:main() args(squelch)=%d\n",PROGRAMID,sql);
                        break;
                case 'z':
                        bPD=true;
                        fprintf(stderr,"%s:main() args(power saving)=%s\n",PROGRAMID,BOOL2CHAR(bPD));
                        break;
                case '1':
                        bPFE=true;
                        fprintf(stderr,"%s:main() args(pre-emphasis)=%s\n",PROGRAMID,BOOL2CHAR(bPFE));
                        break;
                case '2':
                        bLPF=true;
                        fprintf(stderr,"%s:main() args(low pass filter)=%s\n",PROGRAMID,BOOL2CHAR(bLPF));
                        break;
                case '3':
                        bHPF=true;
                        fprintf(stderr,"%s:main() args(high pass filter)=%s\n",PROGRAMID,BOOL2CHAR(bHPF));
                        break;
                case 'p':
                        bHL=true;
                        fprintf(stderr,"%s:main() args(power high)=%s\n",PROGRAMID,BOOL2CHAR(bHL));
                        break;
                case 't':
                        tx_ctcss=atof(optarg);
                        fprintf(stderr,"%s:main() args(Rx CTCSS)=%5.1f\n",PROGRAMID,tx_ctcss);
                        break;
                case 'r':
                        rx_ctcss=atof(optarg);
                        fprintf(stderr,"%s:main() args(Tx CTCSS)=%5.1f\n",PROGRAMID,rx_ctcss);
                        break;
                case 'x':
                        TRACE=atoi(optarg);
                        fprintf(stderr,"%s:main() args(TRACE)=%d\n",PROGRAMID,TRACE);
                        break;
                case '?':
                        print_usage();
                        exit(1);
                        break;
                default:
                        print_usage();
                        exit(1);
                        break;
                }
        }


//*--- Create memory resources

    (TRACE>=0x01 ? fprintf(stderr,"%s:main() Memory resources acquired\n",PROGRAMID) : _NOP);
     LCD_Buffer=(char*) malloc(32);

//*--- Define and initialize LCD interface

    (TRACE>=0x01 ? fprintf(stderr,"%s:main() LCD sub-system initialized\n",PROGRAMID) : _NOP);
     setupLCD();
     sprintf(LCD_Buffer,"%s %s(%s)",PROGRAMID,PROG_VERSION,PROG_BUILD);
     lcd->println(0,0,LCD_Buffer);
     sprintf(LCD_Buffer,"%s","Booting..");
     lcd->println(0,1,LCD_Buffer);

//*--- Establish master clock

    (TRACE>=0x01 ? fprintf(stderr,"%s:main() Master timer enabled\n",PROGRAMID) : _NOP);
     masterTimer=new CallBackTimer();
     masterTimer->start(1,ISRHandler);
     TVFO=500;

    (TRACE>=0x01 ? fprintf(stderr,"%s:main() VFO sub-system initialized\n",PROGRAMID) : _NOP);
     vfo=new genVFO(changeFrequency,NULL,NULL,changeVfoHandler);
     vfo->TRACE=TRACE;
     vfo->FT817=0x00;
     vfo->MODE=m;
     //vfo->POWER=DDS_MAXLEVEL;
     vfo->setBand(VFOA,vfo->getBand(f));
     vfo->setBand(VFOB,vfo->getBand(f));
     vfo->set(VFOA,f);
     vfo->set(VFOB,f);
     vfo->setSplit(false);
     vfo->setRIT(VFOA,false);
     vfo->setRIT(VFOB,false);
     vfo->setVFO(VFOA);
     vfo->setPTT(false);
     vfo->setLock(false);
     vfo->setShift(ofs);
     vfo->setVFOStep(VFOA,VFO_STEP_10KHz);
     vfo->setVFOStep(VFOB,VFO_STEP_10KHz);

//*--- Setup GPIO

    (TRACE>=0x01 ? fprintf(stderr,"%s:main() Setup GPIO sub-system\n",PROGRAMID) : _NOP);
     setupGPIO();


//*---- setup  DRA818V
    (TRACE>=0x01 ? fprintf(stderr,"%s:main() Setup DRA818V chipset sub-system\n",PROGRAMID) : _NOP);
    setupDRA818V();

    setupMMS();

    (TRACE>=0x01 ? fprintf(stderr,"%s:main() Display main panel\n",PROGRAMID) : _NOP);
    showPanel();

    TRSSI=1000;

char buf [100];

//--------------------------------------------------------------------------------------------------
// Main program loop
//--------------------------------------------------------------------------------------------------
     (TRACE>=0x00 ? fprintf(stderr,"%s:main() Starting operation\n",PROGRAMID) : _NOP);
     setWord(&MSW,RUN,true);
     while(getWord(MSW,RUN)==true) {

//*--- Read and process events coming from the CAT subsystem

         d->processCommand();    //Process DRA818 responses
         processGUI();           //Process GUI 
         usleep(100000);         //Reduce the CPU load by doing it more slowly

     }

//*--- Turn off timer sub-system

 (TRACE>=0x00 ? fprintf(stderr,"%s:main() Stopping master timer sub-system\n",PROGRAMID) : _NOP);
  masterTimer->stop();
  delete(masterTimer);

//*--- Turn off LCD

 (TRACE>=0x00 ? fprintf(stderr,"%s:main() Stopping LCD sub-system\n",PROGRAMID) : _NOP);
  lcd->backlight(false);
  lcd->setCursor(0,0);
  lcd->clear();
  delete(lcd);

//*--- Turn off gpio

 (TRACE>=0x00 ? fprintf(stderr,"%s:main() Terminate GPIO sub-system\n",PROGRAMID) : _NOP);
  gpioTerminate();

//*--- Close serial port

 (TRACE>=0x00 ? fprintf(stderr,"%s:main() Stopping DRA818V sub-system\n",PROGRAMID) : _NOP);
  d->stop();
  delete(d);

 (TRACE>=0x00 ? fprintf(stderr,"%s:main() Stopping VFO sub-system\n",PROGRAMID) : _NOP);
  delete(vfo);

  exit(0);
}


