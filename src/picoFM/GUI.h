void setBacklight(bool v) {
    if (lcd==nullptr) return;
    lcd->backlight(v);
    lcd->setCursor(0,0);
}
//--------------------------------------------------------------------------------------------------
// returns the time in a string format
//--------------------------------------------------------------------------------------------------
char* getTime() {

       time_t theTime = time(NULL);
       struct tm *aTime = localtime(&theTime);
       int hour=aTime->tm_hour;
       int min=aTime->tm_min;
       int sec=aTime->tm_sec;
       sprintf(timestr,"%02d:%02d:%02d",hour,min,sec);
       return (char*) &timestr;

}
//*--------------------------------------------------------------------------------------------------
//* setupLCD setup the LCD definitions
//*--------------------------------------------------------------------------------------------------
void setupLCD() {

//*--- setup LCD configuration


   lcd=new LCDLib(NULL);
   (TRACE>=0x01 ? fprintf(stderr,"%s:setupLCD() LCD system initialization\n",PROGRAMID) : _NOP);

   lcd->begin(16,2);
   lcd->clear();

   lcd->createChar(0,TX);
   lcd->createChar(1,S1);
   lcd->createChar(2,S2);
   lcd->createChar(3,S3);
   lcd->createChar(4,S4);
   lcd->createChar(5,NS);
   lcd->createChar(6,NA);
   lcd->createChar(7,NB);

//   lcd->createChar(0,TX);

   lcd_light=LCD_ON;
   lcd->backlight(true);
   lcd->setCursor(0,0);
   lcd->print("Loading...");

}

//*--------------------------[Rotary Encoder Interrupt Handler]--------------------------------------
//* Interrupt handler for Rotary Encoder CW and CCW control
//*--------------------------------------------------------------------------------------------------
void updateEncoders(int gpio, int level, uint32_t tick)
{
        if (level != 0) {  //ignore non falling part of the interruption
           return;
        }

        //setBacklight(true);

        if (getWord(GSW,ECW)==true || getWord(GSW,ECCW) ==true) { //exit if pending to service a previous one
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateEnconders() Last CW/CCW signal pending processsing, ignored!\n",PROGRAMID) : _NOP);
           return;
        }

        int clkState=gpioRead(GPIO_CLK);
        int dtState= gpioRead(GPIO_DT);

        endEncoder = std::chrono::system_clock::now();

        int lapEncoder=std::chrono::duration_cast<std::chrono::milliseconds>(endEncoder - startEncoder).count();

        if ( lapEncoder  < MINENCLAP )  {
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateEnconders() CW/CCW signal too close from last, ignored lap(%d)!\n",PROGRAMID,lapEncoder) : _NOP);
           return;
        }

        if (dtState != clkState) {
          counter++;
          setWord(&GSW,ECCW,true);
        } else {
          counter--;
          setWord(&GSW,ECW,true);
        }

        clkLastState=clkState;        
        startEncoder = std::chrono::system_clock::now();

}

//*--------------------------[Rotary Encoder Interrupt Handler]--------------------------------------
//* Interrupt handler routine for Rotary Encoder Push button
//*--------------------------------------------------------------------------------------------------
void updateSW(int gpio, int level, uint32_t tick)
{

     setBacklight(true);
     if (level != 0) {
        endPush = std::chrono::system_clock::now();
        int lapPush=std::chrono::duration_cast<std::chrono::milliseconds>(endPush - startPush).count();
        if (getWord(GSW,FSW)==true || getWord(GSW,FSWL)==true) {
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateSW() Last SW signal pending processsing, ignored!\n",PROGRAMID) : _NOP);
           return;
        }
        if (lapPush < MINSWPUSH) {
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateSW() SW pulsetoo short! ignored!\n",PROGRAMID) : _NOP) ;
           return;
        } else {
           if (lapPush > MAXSWPUSH) {
              (TRACE>=0x02 ? fprintf(stderr,"%s:updateSW() SW long pulse detected lap(%d)\n",PROGRAMID,lapPush) : _NOP);
              setWord(&GSW,FSWL,true);
           } else {
              (TRACE>=0x02 ? fprintf(stderr,"%s:updateSW() SW brief pulse detected lap(%d)\n",PROGRAMID,lapPush) : _NOP);
              setWord(&GSW,FSW,true);
           }
           return;
        }
     }
     startPush = std::chrono::system_clock::now();
     int pushSW=gpioRead(GPIO_SW);
}
//*--------------------------[Rotary Encoder Interrupt Handler]--------------------------------------
//* Interrupt handler routine for Rotary Encoder Push button
//*--------------------------------------------------------------------------------------------------
void updateSQL(int gpio, int level, uint32_t tick)
{

     if (level != 0) {
        endSQL = std::chrono::system_clock::now();
        int lapSQL=std::chrono::duration_cast<std::chrono::milliseconds>(endSQL - startSQL).count();
        if (getWord(GSW,FSQ)==true) {
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateSQL() Last SQL signal pending processsing, ignored!\n",PROGRAMID) : _NOP);
           return;
        }
        if (lapSQL < MINSWPUSH) {
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateSQL() SQL pulsetoo short! ignored!\n",PROGRAMID) : _NOP) ;
           return;
        } else {
           setWord(&GSW,FSQ,true);
        }
        return;
     }
     startSQL = std::chrono::system_clock::now();
     int pushSQL=gpioRead(GPIO_SQL);
}
//*--------------------------[Rotary Encoder Interrupt Handler]--------------------------------------
//* Interrupt handler routine for Rotary Encoder Push button
//*--------------------------------------------------------------------------------------------------
void updateMICPTT(int gpio, int level, uint32_t tick)
{

     setBacklight(true);
     if (level != 0) {
        endPTT = std::chrono::system_clock::now();
        int lapPTT=std::chrono::duration_cast<std::chrono::milliseconds>(endPTT - startPTT).count();
        if (getWord(GSW,FPTT)==true) {
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateMICPTT() Last signal pending processsing, ignored!\n",PROGRAMID) : _NOP);
           return;
        }
        if (lapPTT < MINSWPUSH) {
           (TRACE>=0x02 ? fprintf(stderr,"%s:updateMICPTT() PTT pulsetoo short! ignored!\n",PROGRAMID) : _NOP) ;
           return;
        } else {
          setWord(&GSW,FPTT,true);
        }
        return;
     }
     startPTT = std::chrono::system_clock::now();
     int pushPTT=gpioRead(GPIO_MICPTT);
}
//*--------------------------------------------------------------------------------------------------
//* setupGPIO setup the GPIO definitions
//*--------------------------------------------------------------------------------------------------
void setupGPIO() {

    (TRACE>=0x00 ? fprintf(stderr,"%s:setupGPIO() Starting....\n",PROGRAMID) : _NOP);
    gpioCfgClock(5, 0, 0);
    if(gpioInitialise()<0) {
        (TRACE>=0x00 ? fprintf(stderr,"%s:setupGPIO() Cannot initialize GPIO\n",PROGRAMID) : _NOP);
        exit(16);
    }

//*---- Configure Encoder

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() Setup Encoder Push\n",PROGRAMID) : _NOP);
    gpioSetMode(GPIO_SW, PI_INPUT);
    gpioSetPullUpDown(GPIO_SW,PI_PUD_UP);
    gpioSetAlertFunc(GPIO_SW,updateSW);
    usleep(100000);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() Setup Encoder\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_CLK, PI_INPUT);
    gpioSetPullUpDown(GPIO_CLK,PI_PUD_UP);
    usleep(100000);
    gpioSetISRFunc(GPIO_CLK, FALLING_EDGE,0,updateEncoders);

    gpioSetMode(GPIO_DT, PI_INPUT);
    gpioSetPullUpDown(GPIO_DT,PI_PUD_UP);
    usleep(100000);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() MIC PTT\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_MICPTT, PI_INPUT);
    gpioSetPullUpDown(GPIO_MICPTT,PI_PUD_UP);
    usleep(100000);
    gpioSetISRFunc(GPIO_MICPTT, FALLING_EDGE,0,updateMICPTT);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() SQL\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_SQL, PI_INPUT);
    gpioSetPullUpDown(GPIO_SQL,PI_PUD_UP);
    usleep(100000);
    gpioSetISRFunc(GPIO_SQL, FALLING_EDGE,0,updateSQL);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() PTT\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_PTT, PI_OUTPUT);
    gpioSetPullUpDown(GPIO_PTT,PI_PUD_UP);
    usleep(100000);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() PD\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_PD, PI_OUTPUT);
    gpioSetPullUpDown(GPIO_PD,PI_PUD_UP);
    usleep(100000);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() HL\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_HL, PI_OUTPUT);
    gpioSetPullUpDown(GPIO_PD,PI_PUD_UP);
    usleep(100000);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() Setup GPIO signal Handler\n",PROGRAMID) : _NOP);
    for (int i=0;i<64;i++) {

        gpioSetSignalFunc(i,sighandler);

    }
    (TRACE>=0x03 ? fprintf(stderr,"%s:setupGPIO() End of setup for GPIO Handler\n",PROGRAMID) : _NOP);


}
//*--------------------------------------------------------------------------------------------------
//* setupDRA818V setup the DRA818V definitions
//*--------------------------------------------------------------------------------------------------
void setupDRA818V() {

//*---- setup  DRA818V

    d=new DRA818V(NULL,NULL,NULL);
    d->start();

    d->setRFW(f/1000000.0);
    d->setTFW(d->getRFW()+(ofs/1000000));
    d->setVol(vol);
    d->setGBW(0);
    d->setPEF(0);
    d->setHPF(0);
    d->setLPF(0);
    d->setSQL(sql);
    d->setPD(bPD);
    d->setHL(bHL);
   

    d->setTxCTCSS(d->TonetoCTCSS(tx_ctcss));
    d->setRxCTCSS(d->TonetoCTCSS(rx_ctcss));

    d->sendSetGroup();
    d->sendSetVolume();
    d->sendSetFilter();

    d->setPTT(false);
    return;
}
//*==================================================================================================
//
//*==================================================================================================
void showMenu() {
    lcd->clear();
}
//*==================================================================================================
void showVFO() {

   if (vfo==nullptr) {return;}

   if (vfo->vfo == VFOA) {
      lcd->setCursor(0,0);
      lcd->write(byte(6));
      strcpy(LCD_Buffer,"B");
      lcd->println(0,1,LCD_Buffer);
   } else {
      strcpy(LCD_Buffer,"A");
      lcd->println(0,0,LCD_Buffer);
      lcd->setCursor(0,1);
      lcd->write(byte(7));
   }

   return;

}
//*==================================================================================================
void showChange() {

int row=0;
int alt=0;

     if (getWord(MSW,CMD)==true) {return;}
     if (vfo==nullptr) {return;}

     if (vfo->vfo == VFOA) {
        row=0;
        alt=1;
     } else {
        row=1;
        alt=0;
     }

     if (getWord(GSW,FBLINK)==true) {

        lcd->setCursor(1,row);

        (vfo->vfodir == 1 ? lcd->typeChar((char)126) : lcd->typeChar((char)127) );
        strcpy(LCD_Buffer," ");
        lcd->println(1,alt,LCD_Buffer);

     } else {

        strcpy(LCD_Buffer," ");
        lcd->println(1,0,LCD_Buffer);
        lcd->println(1,1,LCD_Buffer);

     }

}
//*----------------------
void showFrequency() {

     if (vfo==nullptr) {return;}

     //strcpy(LCD_Buffer," ");
     //lcd->println(9,0,LCD_Buffer);
     //lcd->println(9,1,LCD_Buffer);

     sprintf(LCD_Buffer,"%6.2f",vfo->get(VFOA)/1000000.0);
     lcd->println(2,0,LCD_Buffer);

     sprintf(LCD_Buffer,"%6.2f",vfo->get(VFOB)/1000000.0);
     lcd->println(2,1,LCD_Buffer);

}

//*==================================================================================================
void showDRA818V() {

   if (d==nullptr) {return;}
   if (getWord(d->MSW,RUN)==true) {
       lcd->setCursor(8,0);
       lcd->write(byte(5));
     } else {
       strcpy(LCD_Buffer," ");
       lcd->println(8,0,LCD_Buffer);
     }
}

void showPTT() {

   if (vfo==nullptr) {return; }
   if (vfo->getPTT()==false) {  //inverted for testing
      strcpy(LCD_Buffer," ");
      lcd->println(9,0,LCD_Buffer);
      return;
   }
   lcd->setCursor(9,0);
   lcd->write(0);
}

void showMeter() {

     if (getWord(MSW,CMD)==true) {return;}

     lcd->setCursor(13,1);   //Placeholder Meter till a routine is developed for it

     if (vfo==nullptr) {
        (TRACE>=0x02 ? fprintf(stderr,"%s:showMeter() vfo pointer is NULL, request ignored\n",PROGRAMID) : _NOP);
        return;
     }

     if (d==nullptr) {return;}
     
int  n=0;
     (getWord(d->dra[d->m].STATUS,SQ)==true ? n=10 : n=0);
     (TRACE>=0x02 ? fprintf(stderr,"%s:showMeter() segments(%d)\n",PROGRAMID,n) : _NOP);

     switch(n) {

      case 0 : {lcd->write(byte(32))    ;lcd->write(byte(32))  ;lcd->write(byte(32));break;}
      case 1 : {lcd->write(byte(1))    ;lcd->write(byte(32))  ;lcd->write(byte(32));break;}
      case 2 : {lcd->write(byte(2))    ;lcd->write(byte(32))  ;lcd->write(byte(32));break;}
      case 3 : {lcd->write(byte(3))    ;lcd->write(byte(32))  ;lcd->write(byte(32));break;}
      case 4 : {lcd->write(byte(4))    ;lcd->write(byte(32))  ;lcd->write(byte(32));break;}
      case 5 : {lcd->write(byte(255))  ;lcd->write(byte(32))  ;lcd->write(byte(32));break;}
      case 6 : {lcd->write(byte(255))  ;lcd->write(byte(1))   ;lcd->write(byte(32));break;}
      case 7 : {lcd->write(byte(255))  ;lcd->write(byte(2))   ;lcd->write(byte(32));break;}
      case 8 : {lcd->write(byte(255))  ;lcd->write(byte(3))   ;lcd->write(byte(32));break;}
      case 9 : {lcd->write(byte(255))  ;lcd->write(byte(4))   ;lcd->write(byte(32));break;}
      case 10 : {lcd->write(byte(255))  ;lcd->write(byte(255)) ;lcd->write(byte(32));break;}
      case 11: {lcd->write(byte(255))  ;lcd->write(byte(255)) ;lcd->write(byte(1));break;}
      case 12: {lcd->write(byte(255))  ;lcd->write(byte(255)) ;lcd->write(byte(2));break;}
      case 13: {lcd->write(byte(255))  ;lcd->write(byte(255)) ;lcd->write(byte(3));break;}
      case 14: {lcd->write(byte(255))  ;lcd->write(byte(255)) ;lcd->write(byte(4));break;}
      case 15: {lcd->write(byte(255))  ;lcd->write(byte(255)) ;lcd->write(byte(255));break;}
     }

}

void showShift() {

    if (vfo==nullptr) {return;}
    
    if (vfo->getShift()<0) {
       strcpy(LCD_Buffer,"-");
    } else {
      if (vfo->getShift()>0) {
          strcpy(LCD_Buffer,"+");
      } else {
          strcpy(LCD_Buffer," ");
      }
    }
    lcd->println(10,0,LCD_Buffer);

}

void showCTCSS() {

    if (d==nullptr) {return;}

    if (d->getRxCTCSS()>0) {
       strcpy(LCD_Buffer,"T");
    } else {
       strcpy(LCD_Buffer," ");
    }
    lcd->println(10,0,LCD_Buffer);
}
void showHL() {
    if (d==nullptr) {return;}

    if (d->getHL()==true) {
       strcpy(LCD_Buffer,"H");
    } else {
       strcpy(LCD_Buffer,"L");
    }
    lcd->println(12,0,LCD_Buffer);

}
void showPD() {
    if (d==nullptr) {return;}

    if (d->getPD()==true) {
       strcpy(LCD_Buffer,"*");
    } else {
       strcpy(LCD_Buffer," ");
    }
    lcd->println(13,0,LCD_Buffer);

}
void showVFOMEM() {

//*--- Mockup

    strcpy(LCD_Buffer,"VFO");
    lcd->println(10,1,LCD_Buffer);

}

void showPanel() {
    if (lcd==nullptr) {return;}

    lcd->clear();

    showVFO();
    showFrequency();
    showChange();

    showDRA818V();
    showPTT();
    showShift();
    showCTCSS();
    showHL();
    showPD();
    showVFOMEM();
    showMeter();
}

//*--------------------------------------------------------------------------------------------------
//* Handlers for VFO events
//*--------------------------------------------------------------------------------------------------
void changeFrequency(float f) {

    showFrequency();
    showChange();
    TVFO=3000;
    setWord(&GSW,FBLINK,true);

}
//*--------------------------------------------------------------------------------------------------
//* processGUI()
//*--------------------------------------------------------------------------------------------------
void processGUI() {


//*-------------- Process main display Panel (CMD=false GUI=*)

      if (getWord(MSW,CMD)==false) {

        if (getWord(GSW,ECW)==true) {  //increase f
           setWord(&GSW,ECW,false);
           setWord(&GSW,ECCW,false);
           if (vfo->getPTT()==false) { 
              f=vfo->up();
              TVFO=3000;
              setWord(&GSW,FBLINK,true);
           }
        }


        if (getWord(GSW,ECCW)==true) {  //decrease f
           setWord(&GSW,ECCW,false);
           setWord(&GSW,ECW,false);
           if (vfo->getPTT()==false) { 
              f=vfo->down();
              TVFO=3000;
              setWord(&GSW,FBLINK,true);
           }
        }

        if (getWord(SSW,FVFO)==true) {  // manage blinking
           setWord(&GSW,FBLINK,false);
           setWord(&SSW,FVFO,false);
           showChange();
        }


        if (getWord(GSW,FSW)==true) {   // switch to menu mode
           setWord(&GSW,FSW,false);
           setWord(&MSW,CMD,true);
           setWord(&MSW,GUI,false);
           //lcd->clear();
           showMenu();

        }

     }

//*====================================================================================================


//*-------------- Process main menu Panel (CMD=true GUI=false)

    if (getWord(MSW,CMD)==true && getWord(MSW,GUI)==false) {

        if (getWord(GSW,FSW)==true) {  //return to VFO mode
           setWord(&GSW,FSW,false);
           setWord(&MSW,CMD,false);
           showPanel();
        }

        if (getWord(GSW,ECW)==true) {  //while in menu mode turn knob clockwise
           setWord(&GSW,ECW,false);
           //nextMenu(+1);
	   lcd->clear();
           showMenu();
        }

        if (getWord(GSW,ECCW)==true) {  //while in menu mode turn knob counterclockwise
           setWord(&GSW,ECCW,false);
           //nextMenu(-1);
           lcd->clear();
           showMenu();
        }


        if (getWord(GSW,FSWL)==true) {  //while in menu mode transition to GUI mode (backup)
           setWord(&GSW,FSWL,false);
           setWord(&MSW,GUI,true);
           lcd->clear();
           //backupMenu();
           showMenu();
        }

        if (getWord(SSW,FSAVE)==true) { //restore menu after saving message
           setWord(&SSW,FSAVE,false);
           //lcd->clear();
           showMenu();
        }
     }


//*=====================================================================================================
}
