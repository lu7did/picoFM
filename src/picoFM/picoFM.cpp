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
#include "/home/pi/OrangeThunder/src/lib/CallBackTimer.h"
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

DRA818V   *dra=nullptr;
LCDLib    *lcd=nullptr;

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

auto startAux=std::chrono::system_clock::now();
auto endAux=std::chrono::system_clock::now();

auto startLeft=std::chrono::system_clock::now();
auto endLeft=std::chrono::system_clock::now();

auto startRight=std::chrono::system_clock::now();
auto endRight=std::chrono::system_clock::now();

int value=0;
int lastEncoded=0;
int counter=0;
int clkLastState=0; 

//* --- Define minIni related parameters (configuration persistence)

char inifile[80];
char iniStr[100];
long nIni;
int  sIni,kIni;
char iniSection[50];

MMS* straight;
MMS* iambicA;
MMS* iambicB;
MMS* spval;
MMS* stval;
MMS* shval;
MMS* drval;
MMS* modval;
MMS* backval;
MMS* coolon;
MMS* coolof;
MMS* bcnon;
MMS* bcnoff;
MMS* paddle;
MMS* padrev;
MMS* paddir;

int  TBCK=0;
int  TSAVE=0;

int  TVFO=0;

// *----------------------------------------------------------------*
// *               Initial setup values                             *
// *----------------------------------------------------------------*
float f=7030000;
byte  s=20;
byte  m=MCW;
char  callsign[16];
char  grid[16];
byte  l=7;
int   b=0;
float x=600.0;
byte  k=0;
int   backlight=0;
bool  cooler=false;
byte  st=3;
byte  rev=0;

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


   if (TVFO==0) return;
   TVFO--;
   if (TVFO==0) {setWord(&SSW,FVFO,true);}

   return;

}
//*-------------------------------------------------------------------------------------------------
//* print_usage
//* help message at program startup
//*-------------------------------------------------------------------------------------------------
void print_usage(void)
{

fprintf(stderr,"\n%s version %s build (%s)\n"
"Usage:\nPixiaPi [-f frequency {Hz}]\n"
"                [-s keyer speed {5..50 wpm default=20}]\n"
"                [-S tunning step {0..11 default=3 (100Hz)}]\n"
"                [-m mode {0=LSB,1=USB,2=CW,3=CWR,4=AM,5=FM,6=WFM,7=PKT,8=DIG default=CW}]\n"
"                [-c cooler activated {defaul not, argument turns it on!}]\n"
"                [-r reverse keyer paddles {defaul not}]\n"
"                [-l {power level (0..7 default=7}]\n"
"                [-p {CAT port}]\n"
"                [-v Verbose {0,1,2,3 default=0}]\n"
"                [-x Shift {600..800Hz default=600}]\n"
"                [-b Backlight timeout {0..60 secs default=0 (not activated)}]\n"
"                [-k Keyer {0=Straight,1=Iambic A,2=Iambic B default=0}]\n",PROGRAMID,PROG_VERSION,PROG_BUILD);

}

//*--------------------------------------------------------------------------------------------------
//* main execution of the program
//*--------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
     std::srand(static_cast<unsigned int>(std::time(nullptr))); // set initial seed value to system clock    
     fprintf(stderr,"%s Version %s Build(%s) %s\n",PROGRAMID,PROG_VERSION,PROG_BUILD,COPYRIGHT);

     for (int i = 0; i < 64; i++) {
        if (i != SIGALRM && i != 17 && i != 28) {
           signal(i,sighandler);
        }
     }

//*--------- Process arguments to override persistence

while(true)
        {
                a = getopt(argc, argv, "f:h?");

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
                        fprintf(stderr,"%s:main() args(f)=%5.0f\n",PROGRAMID,f);
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



//*---


//*--- Create memory resources

    (TRACE>=0x01 ? fprintf(stderr,"%s:main() Memory resources acquired\n",PROGRAMID) : _NOP);
     LCD_Buffer=(char*) malloc(32);

//*--- Define and initialize LCD interface

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

//*--- Setup GPIO
    (TRACE>=0x01 ? fprintf(stderr,"%s:main() Setup GPIO\n",PROGRAMID) : _NOP);
     setupGPIO();


    dra=new DRA818V(NULL);
    dra->start();

char buf [100];

//--------------------------------------------------------------------------------------------------
// Main program loop
//--------------------------------------------------------------------------------------------------
     (TRACE>=0x00 ? fprintf(stderr,"%s:main() Starting operation\n",PROGRAMID) : _NOP);
     setWord(&MSW,RUN,true);
     while(getWord(MSW,RUN)==true) {

//*--- Read and process events coming from the CAT subsystem
         
         int n = dra->read_data(buf,100);
         if (n!=0) {
           buf[n]=0x00;
           fprintf(stderr,"Buffer[%s]\n",buf);
         }

         if (getWord(GSW,ECW)==true) {
            setWord(&GSW,ECW,false);
            strcpy(LCD_Buffer,"ECW rotary         ");
            lcd->println(0,0,LCD_Buffer);
            write(fd,"AT+DMOSETVOLUME=5\r\n",19);
            fprintf(stderr,"Message DMOSETVOLUME=5 sent\n");
         }
         if (getWord(GSW,ECCW)==true) {
            setWord(&GSW,ECCW,false);
            strcpy(LCD_Buffer,"ECCW rotary        ");
            lcd->println(0,0,LCD_Buffer);
            write(fd,"AT+DMOSETVOLUME=1\r\n",19);
            fprintf(stderr,"Message DMOSETVOLUME=1 sent\n");
         }
         if (getWord(GSW,FSW)==true) {
            setWord(&GSW,FSW,false);
            strcpy(LCD_Buffer,"SW PUSH detected   ");
            lcd->println(0,0,LCD_Buffer);
         }
         if (getWord(GSW,FSWL)==true) {
            setWord(&GSW,FSWL,false);
            strcpy(LCD_Buffer,"SW PUSH long       ");
            lcd->println(0,0,LCD_Buffer);
         }


         if (getWord(SSW,FVFO)==true) {
            setWord(&SSW,FVFO,false);
            TVFO=2000;
            lcd->setCursor(col,1);
            lcd->write(byte(255));         
            if (col!=0) {
               strcpy(LCD_Buffer," ");
               lcd->println(col-1,1,LCD_Buffer);
            }
            col++;
            if (col>15) {col=0;}
         }
         usleep(100000);
     }
 

//*--- Turn off LCD

  lcd->backlight(false);
  lcd->setCursor(0,0);
  lcd->clear();
  exit(0);
}


