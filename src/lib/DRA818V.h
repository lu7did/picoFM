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

struct DRA818V_response
{
        char     command[128];
        bool     active;
        bool     serviced;
        int      timeout;

};
//---------------------------------------------------------------------------------------------------
// gpio CLASS
//---------------------------------------------------------------------------------------------------
class DRA818V {

  public: 
  
         DRA818V(CALLBACK c);

// --- public methods

CALLBACK changeResponse=NULL;

     int start();
    void stop();
     int set_interface_attribs (int fd, int speed, int parity);
    void set_blocking (int fd, int should_block);
     int read_data(char* buffer,int len);
    void send_data(char* s);

// -- public attributes

    byte TRACE=0x02;
struct   DRA818V_response   d[16];

     int pRead=0;
     int pWrite=0;
     int fd=0;
    byte MSW = 0;
    char portname[64];
    char command[64];
    char buffer[128];

//-------------------- GLOBAL VARIABLES ----------------------------
const char   *PROGRAMID="DRA818V";
const char   *PROG_VERSION="1.0";
const char   *PROG_BUILD="00";
const char   *COPYRIGHT="(c) LU7DID 2019,2020";

private:


};

#endif
//---------------------------------------------------------------------------------------------------
// gpio CLASS Implementation
//--------------------------------------------------------------------------------------------------
DRA818V::DRA818V(CALLBACK c){


   if (c!=NULL) {changeResponse=c;}

// --- initial definitions

   setWord(&MSW,RUN,false);
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
// send operations (fork processes) Implementation
//--------------------------------------------------------------------------------------------------
void DRA818V::send_data(char* s) {

    strcat(s,"\r\n");
    write(fd,s,strlen(s)); 

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
                   fprintf(stderr,"error %d from tcgetattr", errno);
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
                   fprintf(stderr,"error %d from tcsetattr", errno);
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
                   fprintf(stderr,"error %d from tggetattr", errno);
                   return;
           }

           tty.c_cc[VMIN]  = should_block ? 1 : 0;
           tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

           if (tcsetattr (fd, TCSANOW, &tty) != 0)
                   fprintf(stderr,"error %d setting term attributes", errno);
}
