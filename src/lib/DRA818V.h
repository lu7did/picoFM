//--------------------------------------------------------------------------------------------------
// DRA818V controllerr   (HEADER CLASS)
// wrapper to call the Dorji DRA818V chipset thru the serial protocol to activate and configure
// different functions available
//--------------------------------------------------------------------------------------------------
// receive class implementation of a simple USB receiver
// Solo para uso de radioaficionados, prohibido su utilizacion comercial
// Copyright 2018 Dr. Pedro E. Colla (LU7DID)
//--------------------------------------------------------------------------------------------------

#ifndef DRA818V_h
#define DRA818V_h

#include<unistd.h>
#include<sys/wait.h>
#include<sys/prctl.h>
#include<signal.h>
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<fcntl.h> 
#include <termios.h>
#include "../picoFM/picoFM.h"
#include <iostream>
#include <fstream>
using namespace std;

#include <sys/types.h>
#include <sys/stat.h>




typedef unsigned char byte;
typedef bool boolean;
typedef void (*CALLBACK)();

bool getWord (unsigned char SysWord, unsigned char v);
void setWord(unsigned char* SysWord,unsigned char v, bool val);


#define  GBW     0B00000001
#define  PEF     0B00000010
#define  HPF     0B00000100
#define  LPF     0B00001000
#define  HL      0B00010000
#define  PD      0B00100000
#define  READY   0B01000000
#define  PT      0B10000000


struct DRA818V_response
{
        char     command[128];
        char     response[128];
        bool     active;
        bool     serviced;
        char     rc[32];
        int      timeout;

};

struct DRA 
{

        float    TFW;
        float    OFS;
        float    RFW;
        int      SQL;
        byte     STATUS;
        int      Tx_CTCSS;
        int      Rx_CTCSS;
        byte     Vol;

};
//---------------------------------------------------------------------------------------------------
// gpio CLASS
//---------------------------------------------------------------------------------------------------
class DRA818V {

  public: 
  
         DRA818V(CALLBACK ptt,CALLBACK pdt,CALLBACK hlt);

// --- public methods

CALLBACK changePTT=NULL;
CALLBACK changePD=NULL;
CALLBACK changeHL=NULL;

     int start();
    void stop();
     int set_interface_attribs (int fd, int speed, int parity);
    void set_blocking (int fd, int should_block);
     int read_data(char* buffer,int len);
    void send_data(char* s);

    void processCommand();
    void parseCommand();

   float getRFW();
   float getRFW(byte m);

   void  setRFW(float f);
   void  setRFW(byte m, float f);

   float getTFW();
   float getTFW(byte m);

   void  setTFW(float f);
   void  setTFW(byte m,float f);

   int   getTxCTCSS();
   int   getTxCTCSS(byte m);

   void  setTxCTCSS(int t);
   void  setTxCTCSS(byte m, int t);

   int   getRxCTCSS();
   int   getRxCTCSS(byte m);
   void  setRxCTCSS(int t);
   void  setRxCTCSS(byte m,int t);

   int   getSQL();
   int   getSQL(byte m);
   void  setSQL(byte v);
   void  setSQL(byte m,byte v);

   bool  getPD();
   void  setPD(bool v);

   bool  getHL();
   void  setHL(bool v);

   int   getVol();
   int   getVol(byte m);

   void  setVol(byte v);
   void  setVol(byte m,byte v);

   bool  getGBW();
   bool  getGBW(byte m);
   void  setGBW(byte m,bool g);
   void  setGBW(bool g);

   bool  getPEF();
   bool  getPEF(byte m);
   void  setPEF(byte m,bool g);
   void  setPEF(bool g);

   bool  getHPF();
   bool  getHPF(byte m);
   void  setHPF(byte m,bool g);
   void  setHPF(bool g);

   bool  getLPF();
   bool  getLPF(byte m);
   void  setLPF(byte m,bool g);
   void  setLPF(bool g);

   bool  getPTT();
   void  setPTT(bool p);

   void  sendSetGroup();
   void  sendSetVolume();
   void  sendSetFilter();


   float CTCSStoTone(byte t);
   int   TonetoCTCSS(float t);

// -- public attributes

    byte TRACE=0x02;
struct   DRA818V_response   d[16];
struct   DRA      dra[16];
     int pR=0;
     int pW=0;

    byte  m=0;
     int pRead=0;
     int pWrite=0;
     int fd=0;
    byte MSW = 0;
    char portname[64];
    char command[128];
    char buffer[128];
    char commandQueue[128];

//-------------------- GLOBAL VARIABLES ----------------------------
const char   *PROGRAMID="DRA818V";
const char   *PROG_VERSION="1.0";
const char   *PROG_BUILD="00";
const char   *COPYRIGHT="(c) LU7DID 2019,2020";


float CTCSS[38+1]={0.0,67.0,71.9,74.4,77.0,79.7,82.5,85.4,88.5,91.5,94.8,97.4,100.0,103.5,107.2,110.9,114.8,118.8,123.0,127.3,131.8,136.5,141.3,146.2,151.4,156.7,162.2,167.9,173.8,179.9,186.2,192.8,203.5,210.7,218.1,225.7,233.6,241.8,250.3};


private:


};

#endif
//---------------------------------------------------------------------------------------------------
// gpio CLASS Implementation
//--------------------------------------------------------------------------------------------------
DRA818V::DRA818V(CALLBACK ptt,CALLBACK pdt,CALLBACK hlt) {

   if (ptt!=NULL) {changePTT=ptt;}
   if (pdt!=NULL) {changePD=pdt;}
   if (hlt!=NULL) {changeHL=hlt;}

// --- initial definitions

   setWord(&MSW,RUN,false);
}
//---------------------------------------------------------------------------------------------------
// CTCSStoTone Implementation
//--------------------------------------------------------------------------------------------------
float DRA818V::CTCSStoTone(byte t) {

    if (t<0 || t>38) {
      (TRACE>=0x00 ? fprintf(stderr,"%s:CTCSStoTone() Tone(%d) invalid, ignored!\n",PROGRAMID,t) : _NOP);
       return 0.0;
    }

   (TRACE>=0x02 ? fprintf(stderr,"%s:CTCSStoTone() Tone(%d) returning Tone(%3.0f)\n",PROGRAMID,t,CTCSS[t]) : _NOP);
    return CTCSS[t];

}
//---------------------------------------------------------------------------------------------------
// TonetoCTCSS Implementation
//--------------------------------------------------------------------------------------------------
int DRA818V::TonetoCTCSS(float t) {

   if (t==0.0) {
     (TRACE>=0x02 ? fprintf(stderr,"%s:TonetoCTCSS() Tone(%3.0f) is no tone\n",PROGRAMID,t) : _NOP);
      return 0;
   }

   for (int i=0;i<39;i++) {
     if (CTCSS[i]==t) {
        (TRACE>=0x02 ? fprintf(stderr,"%s:TonetoCTCSS() Tone(%3.0f) found index(%03d)\n",PROGRAMID,t,i) : _NOP);
         return i;
     }
   }
   return 0.0;
}
//---------------------------------------------------------------------------------------------------
// parseCommand Implementation
//--------------------------------------------------------------------------------------------------
void DRA818V::parseCommand() {

char  cmd[128];
char* token; 
char* p;
char* val;
    val=(char*)malloc(128);
    p=(char*)malloc(128);
    sprintf(cmd,"%s",commandQueue);
    strcpy(d[pR].response,cmd);

    if (strstr(cmd,"+DMOCONNECT:") != NULL || strstr(cmd,"DMOSETGROUP:") != NULL || strstr(cmd,"DMOSETFILTER:")!=NULL || strstr(cmd,"DMOSETVOLUME:")!=NULL) {
       token = strtok(cmd, ":");
       strcpy(p,token);
       while (token!=NULL) {
         token=strtok(NULL,":");
         if (token!=NULL) {
            strcpy(val,token);
            strcpy(d[pR].rc,val);
         }
       } 
       return;
    }


    if (strstr(commandQueue,"S=") != NULL) {

    }
}
//---------------------------------------------------------------------------------------------------
// processCommand Implementation
//--------------------------------------------------------------------------------------------------
void DRA818V::processCommand() {
char buffer[128];
char* c;

     if (d[pR].active==true) {
         d[pR].active=false;
         strcpy(buffer,d[pR].command);
         strcat(buffer,"\r\n");
         write(fd,buffer,strlen(buffer)); 
         (TRACE>=0x03 ? fprintf(stderr,"%s:processCommand() Write Command[%s]\n",PROGRAMID,d[pR].command) : _NOP);
     }

c=(char*)malloc(16);
int  n=read(fd,buffer,128);
     if(n==0) return;
     for (int i=0;i<n;i++) {
         c[0]=buffer[i];
         c[1]=0x00;
         if (strcmp(c,"\r")!=0 && strcmp(c,"\n")!=0) {

             commandQueue[pWrite]=buffer[i];
             pWrite++;         
         } else {
             if (strcmp(c,"\n")==0) {
                commandQueue[pWrite]=0x00;
                if (strlen(commandQueue)==0) {
                    return;
                }
                (TRACE>=0x03 ? fprintf(stderr,"%s:processCommand() Buffer response identified[%s]\n",PROGRAMID,commandQueue) : _NOP);
                parseCommand();
                pWrite=0x00;
                d[pR].serviced=true;
                if(strlen(d[pR].command)!=0) {
                  (TRACE>=0x02 ? fprintf(stderr,"%s:processCommand() Command(%s) Response(%s) serviced rc(%s)\n",PROGRAMID,d[pR].command,d[pR].response,d[pR].rc) : _NOP);
                  if (strcmp(d[pR].command,"AT+DMOCONNECT")==0 && strcmp(d[pR].response,"+DMOCONNECT:0")==0) {
                     (TRACE>=0x02 ? fprintf(stderr,"%s:processCommand() Response to DMOCONNECT command recognized, receiver online\n",PROGRAMID) : _NOP);
                     setWord(&dra[m].STATUS,READY,true);
                  }
                  pR++;
                  if (pR>15) {
                      pR=0;
                  }
                }
              }
         }
     }
}
//---------------------------------------------------------------------------------------------------
// send operations (fork processes) Implementation
//--------------------------------------------------------------------------------------------------
void DRA818V::send_data(char* s) {

    strcpy(d[pW].command,s);
    d[pW].active=true;
    d[pW].serviced=false;
    d[pW].timeout=2000;
    pW++;
    if (pW>15) {
        pW=0;
    }

}
//---------------------------------------------------------------------------------------------------
// read operations (fork processes) Implementation
//--------------------------------------------------------------------------------------------------
int DRA818V::read_data(char* buffer,int len) {
int n = read (fd, buffer, len);
    if (n!=0) {
       buffer[n]=0x00;
       return n;
    }
    return 0;
}
//---------------------------------------------------------------------------------------------------
// start operations (fork processes) Implementation
//--------------------------------------------------------------------------------------------------
int DRA818V::start() {

     strcpy(portname,"/dev/ttyS0");
     fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
     if (fd < 0) {
        (TRACE>=0x00 ? fprintf(stderr,"%s::start() error %d opening %s: %s", PROGRAMID,errno, portname, strerror (errno)) : _NOP);
        return -1;
     }

     set_interface_attribs (fd, B9600, 0);  // set speed to 115,200 bps, 8n1 (no parity)
     set_blocking (fd, 0);                // set no blocking
     strcpy(command,"AT+DMOCONNECT");
     this->send_data(command);
     setWord(&MSW,RUN,true);
    (TRACE>=0x00 ? fprintf(stderr,"%s::start() serial interface(%s) active\n",PROGRAMID,portname) : _NOP);

    return 0;
}
//---------------------------------------------------------------------------------------------------
// stop()  CLASS Implementation
//--------------------------------------------------------------------------------------------------
void DRA818V::stop() {

  close(fd);
 (TRACE>=0x00 ? fprintf(stderr,"%s::stop() connection with DRA818V chipset terminated\n",PROGRAMID) : _NOP);

  return;
}
//---------------------------------------------------------------------------------------------------
// set_interface_attribs()  CLASS Implementation
//--------------------------------------------------------------------------------------------------
int DRA818V::set_interface_attribs (int fd, int speed, int parity)
{
           struct termios tty;
           memset (&tty, 0, sizeof tty);
           if (tcgetattr (fd, &tty) != 0)
           {
                   (TRACE>=0x00 ? fprintf(stderr,"error %d from tcgetattr", errno) : _NOP);
                   return -1;
           }

           cfsetospeed (&tty, speed);
           cfsetispeed (&tty, speed);

           tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
           // disable IGNBRK for mismatched speed tests; otherwise receive break
           // as \000 chars
           tty.c_iflag &= ~IGNBRK;         // disable break processing
           tty.c_lflag = 0;                // no signaling chars, no echo,
                                           // no canonical processing
           tty.c_oflag = 0;                // no remapping, no delays
           tty.c_cc[VMIN]  = 0;            // read doesn't block
           tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

           tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

           tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                           // enable reading
           tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
           tty.c_cflag |= parity;
           tty.c_cflag &= ~CSTOPB;
           tty.c_cflag &= ~CRTSCTS;

           if (tcsetattr (fd, TCSANOW, &tty) != 0)
           {
                   (TRACE>=0x00 ? fprintf(stderr,"error %d from tcsetattr", errno) : _NOP);
                   return -1;
           }
           return 0;
}
//---------------------------------------------------------------------------------------------------
// set_blocking()  CLASS Implementation
//--------------------------------------------------------------------------------------------------
void DRA818V::set_blocking (int fd, int should_block)
{
           struct termios tty;
           memset (&tty, 0, sizeof tty);
           if (tcgetattr (fd, &tty) != 0)
           {
                   (TRACE>=0x00 ? fprintf(stderr,"error %d from tggetattr", errno) : _NOP);
                   return;
           }

           tty.c_cc[VMIN]  = should_block ? 1 : 0;
           tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

           if (tcsetattr (fd, TCSANOW, &tty) != 0)
                   (TRACE>=0x00 ? fprintf(stderr,"error %d setting term attributes", errno) : _NOP);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::sendSetGroup() {

     sprintf(command,"AT+DMOSETGROUP=%d,%3.4f,%3.4f,%04d,%d,%04d",getWord(dra[m].STATUS,GBW),dra[m].TFW,dra[m].RFW,dra[m].Rx_CTCSS,dra[m].SQL,dra[m].Tx_CTCSS);
     this->send_data(command);

}
//--------------------------------------------------------------------------------------------------
void DRA818V::sendSetVolume() {

     sprintf(command,"AT+DMOSETVOLUME=%d",dra[m].Vol);
     this->send_data(command);

}
//--------------------------------------------------------------------------------------------------
void DRA818V::sendSetFilter() {

     sprintf(command,"AT+SETFILTER=%d,%d,%d",getWord(dra[m].STATUS,PEF),getWord(dra[m].STATUS,HPF),getWord(dra[m].STATUS,LPF));
     this->send_data(command);

}
//--------------------------------------------------------------------------------------------------
float DRA818V::getRFW(byte m) {

    if (m<0 || m>15) return 0.0;

   (TRACE>=0x00 ? fprintf(stderr,"%s::getRFW() RFW(%5.1f)\n",PROGRAMID,dra[m].RFW) : _NOP);
    return dra[m].RFW;
}
//--------------------------------------------------------------------------------------------------
float DRA818V::getRFW() {
   return this->getRFW(this->m);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setRFW(byte m,float f) {

    if (m<0 || m>15) return;

    dra[m].RFW=f;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setRFW() TFW(%5.1f)\n",PROGRAMID,dra[m].RFW) : _NOP);
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setRFW(float f) {
    this->setRFW(this->m,f);
    return;
}
//--------------------------------------------------------------------------------------------------
float DRA818V::getTFW(byte m) {
    if (m<0 || m>15) return 0.0;

   (TRACE>=0x00 ? fprintf(stderr,"%s::getTFW() TFW(%5.1f)\n",PROGRAMID,dra[m].TFW) : _NOP);
    return dra[m].TFW;
}
//--------------------------------------------------------------------------------------------------
float DRA818V::getTFW() {
   return this->getTFW(this->m);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setTFW(float f) {
    this->setTFW(this->m,f);
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setTFW(byte m,float f) {
    if (m<0 || m>15) return;
    dra[m].TFW=f;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setTFW() TFW(%5.1f)\n",PROGRAMID,dra[m].TFW) : _NOP);
    return;
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getTxCTCSS(byte m) {

    if (m<0 || m>15) return 0;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getTxCTCSS() CTCSS(%x)\n",PROGRAMID,dra[m].Tx_CTCSS) : _NOP);
   return dra[m].Tx_CTCSS;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setTxCTCSS(byte m,int t) {
   if (m<0 || m>15) return;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setTxCTCSS() CTCSS(%x)\n",PROGRAMID,t) : _NOP);
   dra[m].Tx_CTCSS=t;
   return;   
}
void DRA818V::setTxCTCSS(int t) {
   this->setTxCTCSS(0,t);
   return;
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getTxCTCSS() {
   return this->getTxCTCSS(this->m);
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getRxCTCSS(byte m) {

    if (m<0 || m>15) return 0;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getRxCTCSS() CTCSS(%x)\n",PROGRAMID,dra[m].Rx_CTCSS) : _NOP);
   return dra[m].Rx_CTCSS;
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getRxCTCSS() {
   return this->getRxCTCSS(this->m);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setRxCTCSS(byte m,int t){
   if (m<0 || m>15) return;
   dra[m].Rx_CTCSS=t;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setRxCTCSS() CTCSS(%d)\n",PROGRAMID,t) : _NOP);
   return;   
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setRxCTCSS(int t) {
   return setRxCTCSS(this->m,t);
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getVol(byte m) {
    if (m<0 || m>15) return 0;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getVol() Vol(%d)\n",PROGRAMID,dra[m].Vol) : _NOP);
   return dra[m].Vol;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setPD(bool v) {
   setWord(&dra[m].STATUS,PD,v);
   (TRACE>=0x00 ? fprintf(stderr,"%s::setPD() PD(%s)\n",PROGRAMID,BOOL2CHAR(getWord(dra[m].STATUS,PD))) : _NOP);
   if(changePD!=NULL) {changePD();}
   return;
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getPD() {
   return getWord(dra[m].STATUS,PD);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setHL(bool v) {
   setWord(&dra[m].STATUS,HL,v);
   (TRACE>=0x00 ? fprintf(stderr,"%s::setHL() HL(%s)\n",PROGRAMID,BOOL2CHAR(getWord(dra[m].STATUS,HL))) : _NOP);
   if (changeHL!=NULL) {changeHL();}
   return;
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getHL() {
   return getWord(dra[m].STATUS,HL);
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getVol() {
   return dra[this->m].Vol;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setVol(byte m,byte v) {
    if (m<0 || m>15) return;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setVol() Vol(%d)\n",PROGRAMID,v) : _NOP);
    dra[m].Vol=v;
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setVol(byte v) {
    this->setVol(this->m,v);
    return;
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getSQL(byte m) {
    if (m<0 || m>15) return 0;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getSQL() SQL(%d)\n",PROGRAMID,dra[m].SQL) : _NOP);
   return dra[m].SQL;
}
//--------------------------------------------------------------------------------------------------
int DRA818V::getSQL() {
   return dra[this->m].SQL;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setSQL(byte m,byte v) {
    if (m<0 || m>15) return;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setSQL() SQL(%d)\n",PROGRAMID,v) : _NOP);
    dra[m].SQL=v;
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setSQL(byte v) {
    this->setSQL(this->m,v);
    return;
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getGBW(byte m) {
    if (m<0 || m>15) return false;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getGBW() GBW(%s)\n",PROGRAMID,BOOL2CHAR(getWord(dra[m].STATUS,GBW))) : _NOP);
    return getWord(dra[m].STATUS,GBW);
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getGBW() {
     return this->getGBW(this->m);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setGBW(byte m,bool v) {
    if (m<0 || m>15) return;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setGBW() PEF(%s)\n",PROGRAMID,BOOL2CHAR(v)) : _NOP);
    setWord(&dra[m].STATUS,GBW,v);
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setGBW(bool v) {
    this->setGBW(this->m,v);
    return;
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getPEF(byte m) {
    if (m<0 || m>15) return false;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getPEF() PEF(%s)\n",PROGRAMID,BOOL2CHAR(getWord(dra[m].STATUS,PEF))) : _NOP);
    return getWord(dra[m].STATUS,PEF);
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getPEF() {
     return this->getPEF(this->m);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setPEF(byte m,bool v) {
    if (m<0 || m>15) return;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setPEF() PEF(%s)\n",PROGRAMID,BOOL2CHAR(v)) : _NOP);
    setWord(&dra[m].STATUS,PEF,v);
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setPEF(bool v) {
    this->setPEF(this->m,v);
    return;
}
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
bool DRA818V::getHPF(byte m) {
    if (m<0 || m>15) return false;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getHPF() HPF(%s)\n",PROGRAMID,BOOL2CHAR(getWord(dra[m].STATUS,HPF))) : _NOP);
    return getWord(dra[m].STATUS,HPF);
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getHPF() {
     return this->getHPF(this->m);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setHPF(byte m,bool v) {
    if (m<0 || m>15) return;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setHPF() HPF(%s)\n",PROGRAMID,BOOL2CHAR(v)) : _NOP);
    setWord(&dra[m].STATUS,HPF,v);
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setHPF(bool v) {
    this->setHPF(this->m,v);
    return;
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getLPF(byte m) {
    if (m<0 || m>15) return false;
   (TRACE>=0x00 ? fprintf(stderr,"%s::getLPF() LPF(%s)\n",PROGRAMID,BOOL2CHAR(getWord(dra[m].STATUS,LPF))) : _NOP);
    return getWord(dra[m].STATUS,LPF);
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getLPF() {
     return this->getLPF(this->m);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setLPF(byte m,bool v) {
    if (m<0 || m>15) return;
   (TRACE>=0x00 ? fprintf(stderr,"%s::setLPF() LPF(%s)\n",PROGRAMID,BOOL2CHAR(getWord(dra[m].STATUS,LPF))) : _NOP);
    setWord(&dra[m].STATUS,LPF,v);
    return;
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setLPF(bool v) {
    this->setLPF(this->m,v);
    return;
}
//--------------------------------------------------------------------------------------------------
bool DRA818V::getPTT() {
    return getWord(dra[0].STATUS,PT);
}
//--------------------------------------------------------------------------------------------------
void DRA818V::setPTT(bool v) {
    setWord(&dra[0].STATUS,PT,v);
    if (changePTT!=NULL) {changePTT();}
    return;
}
