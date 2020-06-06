//--------------------------------------------------------------------------------------------------
// Turn backlight on/off
//--------------------------------------------------------------------------------------------------
void setBacklight(bool v) {

    if (lcd==nullptr) return;
    lcd->backlight(v);
    lcd->setCursor(0,0);
    if (backlight!=0) { TBACKLIGHT=backlight; }
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

        setBacklight(true);

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
//* Interrupt handler routine for Squelch
//*--------------------------------------------------------------------------------------------------
void updateSQL(int gpio, int level, uint32_t tick)
{

     setBacklight(true);

     if (level != 0) {
        endSQL = std::chrono::system_clock::now();
        int lapSQL=std::chrono::duration_cast<std::chrono::milliseconds>(endSQL - startSQL).count();
        pushSQL=1;
        if (d!=nullptr) {setWord(&d->dra[m].STATUS,SQ,false);}
        setWord(&GSW,FSQ,true);
        return;
     }

     startSQL = std::chrono::system_clock::now();
     pushSQL=0;
     if (d!=nullptr) {setWord(&d->dra[m].STATUS,SQ,true);}
     setWord(&GSW,FSQ,true);

}
//*--------------------------[Rotary Encoder Interrupt Handler]--------------------------------------
//* Interrupt handler routine for Microphone PTT Push button
//*--------------------------------------------------------------------------------------------------
void updateMICPTT(int gpio, int level, uint32_t tick)
{

     setBacklight(true);
     if (level != 0) {
        endPTT = std::chrono::system_clock::now();
    int lapPTT=std::chrono::duration_cast<std::chrono::milliseconds>(endPTT - startPTT).count();
        pushPTT=1;
       (TRACE>=0x03 ? fprintf(stderr,"%s:updateMICPTT() GPIO level up pushPTT(%d)\n",PROGRAMID,pushPTT) : _NOP);
        setWord(&GSW,FPTT,true);
        if (watchdog!=0) {
            TWATCHDOG=watchdog;
        }
        return;
     }
     startPTT = std::chrono::system_clock::now();
     pushPTT=gpioRead(GPIO_MICPTT);
     pushPTT=0;
     setWord(&GSW,FPTT,true);
    (TRACE>=0x03 ? fprintf(stderr,"%s:updateMICPTT() GPIO level down pushPTT(%d)\n",PROGRAMID,pushPTT) : _NOP);
     setWord(&vfo->FT817,WATCHDOG,false);
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
    gpioSetISRFunc(GPIO_MICPTT, EITHER_EDGE,0,updateMICPTT);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() SQL\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_SQL, PI_INPUT);
    gpioSetPullUpDown(GPIO_SQL,PI_PUD_UP);
    usleep(100000);
    gpioSetISRFunc(GPIO_SQL, EITHER_EDGE,0,updateSQL);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() PTT\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_PTT, PI_OUTPUT);
    gpioSetPullUpDown(GPIO_PTT,PI_PUD_UP);
    usleep(100000);
    gpioWrite(GPIO_PTT,1);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() PD\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_PD, PI_OUTPUT);
    gpioSetPullUpDown(GPIO_PD,PI_PUD_UP);
    usleep(100000);
    gpioWrite(GPIO_PD,1);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() HL\n",PROGRAMID) : _NOP);

    gpioSetMode(GPIO_HL, PI_OUTPUT);
    gpioSetPullUpDown(GPIO_PD,PI_PUD_UP);
    usleep(100000);
    gpioWrite(GPIO_HL,0);

    (TRACE>=0x02 ? fprintf(stderr,"%s:setupGPIO() Setup GPIO signal Handler\n",PROGRAMID) : _NOP);
    for (int i=0;i<64;i++) {

        gpioSetSignalFunc(i,sighandler);

    }
    (TRACE>=0x03 ? fprintf(stderr,"%s:setupGPIO() End of setup for GPIO Handler\n",PROGRAMID) : _NOP);


}
void DRAchangePTT() {

    (TRACE>=0x02 ? fprintf(stderr,"%s:DRAchangePTT() Process PTT change request PTT(%s)\n",PROGRAMID,BOOL2CHAR(d->getPTT())) : _NOP);
    (d->getPTT()==false ? gpioWrite(GPIO_PTT,1) : gpioWrite(GPIO_PTT,0));

}
void DRAchangeHL() {

    (TRACE>=0x02 ? fprintf(stderr,"%s:DRAchangeHL() Process HL change request HL(%s)\n",PROGRAMID,BOOL2CHAR(d->getHL())) : _NOP);
    (d->getHL()==false ? gpioWrite(GPIO_HL,0) : gpioWrite(GPIO_HL,1));

}
void DRAchangePD() {

    (TRACE>=0x02 ? fprintf(stderr,"%s:DRAchangeHL() Process PD change request HL(%s)\n",PROGRAMID,BOOL2CHAR(d->getPD())) : _NOP);
    (d->getPD()==false ? gpioWrite(GPIO_PD,0) : gpioWrite(GPIO_PD,1));

}
void showMeter();
void DRAchangeRSSI(float rssi) {

    (TRACE>=0x03 ? fprintf(stderr,"%s:DRAchangeRSSI() Signal report RSSI(%f)\n",PROGRAMID,rssi) : _NOP);
    RSSI=rssi;
    if (RSSI!=RSSIant) {
       showMeter();
       RSSIant=RSSI;
    }

}
//*--------------------------------------------------------------------------------------------------
//* setupDRA818V setup the DRA818V definitions
//*--------------------------------------------------------------------------------------------------
void setupDRA818V() {

//*---- setup  DRA818V

    d=new DRA818V(DRAchangePTT,DRAchangePD,DRAchangeHL,DRAchangeRSSI);
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

    setWord(&d->dra[m].STATUS,PD,bPD);
    setWord(&d->dra[m].STATUS,PEF,bPFE);
    setWord(&d->dra[m].STATUS,HPF,bHPF);
    setWord(&d->dra[m].STATUS,LPF,bLPF);
    setWord(&d->dra[m].STATUS,HL,bHL);

    return;
}
//====================================================================================================================== 
// showMenu()
//====================================================================================================================== 
void showMenu() {

MMS*  menu=root->curr;
int   i=menu->mVal;
char* m=menu->mText;

     sprintf(LCD_Buffer," %02d %s",i,m);
     lcd->println(0,0,LCD_Buffer);

char* t;

     MMS*  child=menu->curr;
     if (child!=NULL) {
         t=child->mText;
         sprintf(LCD_Buffer," %s",t);
         lcd->println(1,1,t);
     } else {
         t=NULL;
         return;
     }

     if (getWord(MSW,CMD)==true && getWord(MSW,GUI)==true) {
        lcd->setCursor(0,1);
        lcd->typeChar((char)126);
     }
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
//*==================================================================================================
void showFrequency() {

     if (vfo==nullptr) {return;}

     if (vfo->getPTT() == false) {
        sprintf(LCD_Buffer,"%6.2f",vfo->get(VFOA)/1000000.0);
        lcd->println(2,0,LCD_Buffer);

        sprintf(LCD_Buffer,"%6.2f",vfo->get(VFOB)/1000000.0);
        lcd->println(2,1,LCD_Buffer);
        return;
     }

     if (vfo->vfo==VFOA) {
        sprintf(LCD_Buffer,"%6.2f",(vfo->get(VFOA)+vfo->getShift(VFOA))/1000000.0);
        lcd->println(2,0,LCD_Buffer);

        sprintf(LCD_Buffer,"%6.2f",vfo->get(VFOB)/1000000.0);
        lcd->println(2,1,LCD_Buffer);

     } else {

        sprintf(LCD_Buffer,"%6.2f",vfo->get(VFOA)/1000000.0);
        lcd->println(2,0,LCD_Buffer);

        sprintf(LCD_Buffer,"%6.2f",(vfo->get(VFOB)+vfo->getShift(VFOB))/1000000.0);
        lcd->println(2,1,LCD_Buffer);

     }
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
//*==================================================================================================

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
//*==================================================================================================
void showMeter() {

     if (getWord(MSW,CMD)==true) {return;}

     if (vfo==nullptr) {
        (TRACE>=0x02 ? fprintf(stderr,"%s:showMeter() vfo pointer is NULL, request ignored\n",PROGRAMID) : _NOP);
        return;
     }

     if (d==nullptr) {return;}
     if (lcd==nullptr) {return;}

     lcd->setCursor(13,1);
int  n=0;
    
     if (RSSI<55) {n=0;}
     if (RSSI>=55 && RSSI <60 )   {n=1;}
     if (RSSI>=61 && RSSI <67 )   {n=2;}
     if (RSSI>=67 && RSSI <73 )   {n=3;}
     if (RSSI>=73 && RSSI <79 )   {n=4;}
     if (RSSI>=79 && RSSI <85 )   {n=5;}
     if (RSSI>=85 && RSSI <91 )   {n=6;}
     if (RSSI>=91 && RSSI <97 )   {n=7;}
     if (RSSI>=97 && RSSI <103 )  {n=8;}
     if (RSSI>=103 && RSSI <109 ) {n=9;}
     if (RSSI>=109 && RSSI <115 ) {n=10;}
     if (RSSI>=115 && RSSI <120 ) {n=11;}
     if (RSSI>=120 && RSSI <125 ) {n=12;}
     if (RSSI>=125 && RSSI <130 ) {n=13;}
     if (RSSI>=130 && RSSI <135 ) {n=14;}
     if (RSSI>=135 && RSSI <135 ) {n=15;}

     if (nant!=-1) {
        if (n==nant) {return;}
     }
     nant=n;

     switch(n) {
       case  0: {lcd->print(" ");lcd->print(" ");lcd->print(" ");break;}
       case  1: {lcd->write(byte(1));lcd->print(" ");lcd->print(" ");break;}
       case  2: {lcd->write(byte(2));lcd->print(" ");lcd->print(" ");break;}
       case  3: {lcd->write(byte(3));lcd->print(" ");lcd->print(" ");break;}
       case  4: {lcd->write(byte(4));lcd->print(" ");lcd->print(" ");break;}
       case  5: {lcd->write(byte(255));lcd->print(" ");lcd->print(" ");break;}
       case  6: {lcd->write(byte(255));lcd->write(byte(1));lcd->print(" ");}
       case  7: {lcd->write(byte(255));lcd->write(byte(2));lcd->print(" ");break;}
       case  8: {lcd->write(byte(255));lcd->write(byte(3));lcd->print(" ");break;}
       case  9: {lcd->write(byte(255));lcd->write(byte(4));lcd->print(" ");break;}
       case 10: {lcd->write(byte(255));lcd->write(byte(255));lcd->print(" ");break;}
       case 11: {lcd->write(byte(255));lcd->write(byte(255));lcd->write(byte(1));break;}
       case 12: {lcd->write(byte(255));lcd->write(byte(255));lcd->write(byte(2));break;}
       case 13: {lcd->write(byte(255));lcd->write(byte(255));lcd->write(byte(3));break;}
       case 14: {lcd->write(byte(255));lcd->write(byte(255));lcd->write(byte(4));break;}
       case 15: {lcd->write(byte(255));lcd->write(byte(255));lcd->write(byte(255));break;}
     }
     usleep(10000);
     return;
}
//*==================================================================================================

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
//*==================================================================================================
void showCTCSS() {

    if (d==nullptr) {return;}

    if (d->getRxCTCSS()>0) {
       strcpy(LCD_Buffer,"T");
    } else {
       strcpy(LCD_Buffer," ");
    }
    lcd->println(10,0,LCD_Buffer);
}
//*==================================================================================================
void showHL() {
    if (d==nullptr) {return;}

    if (d->getHL()==true) {
       strcpy(LCD_Buffer,"H");
    } else {
       strcpy(LCD_Buffer,"L");
    }
    lcd->println(12,0,LCD_Buffer);

}
//*==================================================================================================
void showPD() {
    if (d==nullptr) {return;}

    if (d->getPD()==true) {
       strcpy(LCD_Buffer,"*");
    } else {
       strcpy(LCD_Buffer," ");
    }
    lcd->println(13,0,LCD_Buffer);

}
//*==================================================================================================
void showVFOMEM() {

//*--- Mockup

    strcpy(LCD_Buffer,"VFO");
    lcd->println(10,1,LCD_Buffer);

}
//*==================================================================================================
//* Show the entire VFO panel at once
//*==================================================================================================
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
    nant=-1;
    showMeter();
}

//*--------------------------------------------------------------------------------------------------
//* Handlers for DRA818 Frequency events
//*--------------------------------------------------------------------------------------------------
void changeFrequency(float f) {

    showFrequency();
    showChange();
    TVFO=3000;
    setWord(&GSW,FBLINK,true);

    if (d==nullptr) {return;}
    d->setRFW(vfo->get()/1000000.0);
    d->setTFW(d->getRFW()+(vfo->getShift()/1000000));
    d->sendSetGroup();

}
//*=====================================================================================================================
//* hook to manage changes in status (SPLIT, RIT, PTT, etc)
//*=====================================================================================================================
void changeVfoHandler(byte S) {

   if (getWord(S,SPLIT)==true) {
      if(vfo==nullptr) {return;}
      (TRACE>=0x02 ? fprintf(stderr,"%s:changeVfoHandler() change SPLIT S(%s) On\n",PROGRAMID,BOOL2CHAR(getWord(vfo->FT817,SPLIT))) : _NOP);
   }
   if (getWord(S,RITX)==true) {

   }
   if (getWord(S,PTT)==true) {
      if (vfo==nullptr) {return;}
      if (d==nullptr) {return;}

      (TRACE>=0x02 ? fprintf(stderr,"%s:changeVfoHandler() change PTT S(%s) On\n",PROGRAMID,BOOL2CHAR(getWord(vfo->FT817,PTT))) : _NOP);
      if (getWord(vfo->FT817,WATCHDOG) == false) {
          d->setPTT(getWord(vfo->FT817,PTT));
      } else {
          vfo->setPTT(false);
         (TRACE>=0x00 ? fprintf(stderr,"%s:changeVfoHandler() watchdog activated PTT S(%s) disabled\n",PROGRAMID,BOOL2CHAR(getWord(vfo->FT817,PTT))) : _NOP);
      }
      showPTT();
      showFrequency();

   }
   if (getWord(S,VFO)==true) {
      if (vfo==nullptr) {return;}
      (TRACE>=0x02 ? fprintf(stderr,"%s:changeVfoHandler() change VFO S(%s) On\n",PROGRAMID,BOOL2CHAR(getWord(vfo->FT817,VFO))) : _NOP);

      if (d==nullptr) {return;}
      d->setRFW(vfo->get()/1000000.0);
      d->setTFW(d->getRFW()+(vfo->getShift()/1000000));
      d->sendSetGroup();

      showVFO();
      showChange();
      showFrequency();
   }

}
//*=====================================================================================================================
//* hook to manage changes in frequency
//*=====================================================================================================================
void freqVfoHandler(float f) {  // handler to receiver VFO upcalls for frequency changes

char* b;

   if (vfo==nullptr) {return;}

   b=(char*)malloc(128);
   vfo->vfo2str(vfo->vfo,b);
   showFrequency();
   showChange();
   //if (cat!=nullptr) {
   //   cat->f=vfo->get(vfo->vfo);
   //}
   (TRACE>=0x02 ? fprintf(stderr,"%s:freqVfoHandler() VFO(%s) f(%5.0f) fA(%5.0f) fB(%5.0f) PTT(%s)\n",PROGRAMID,b,f,vfo->get(VFOA),vfo->get(VFOB),BOOL2CHAR(getWord(vfo->FT817,PTT))) : _NOP);

}


void nextMenu(int dir) {
     root->move(dir);
}

void nextChild(int dir) {
     root->curr->move(dir);
}

//*---  backup menu

void backupMenu() {
     (TRACE>=0x02 ? fprintf(stderr,"%s:backupMenu() Saving mVal(%d) mText(%s)\n",PROGRAMID,root->curr->mVal,root->curr->mText) : _NOP);
     root->curr->backup();
}
//*---  save menu

void saveMenu() {
     (TRACE>=0x02 ? fprintf(stderr,"%s:saveMenu() Saving mVal(%d) mText(%s)\n",PROGRAMID,root->curr->mVal,root->curr->mText) : _NOP);
     root->curr->save();
}
//*---  restore menu

void restoreMenu() {
     (TRACE>=0x02 ? fprintf(stderr,"%s:restoreMenu() Saving mVal(%d) mText(%s)\n",PROGRAMID,root->curr->mVal,root->curr->mText) : _NOP);
     root->curr->restore();
}

//*--------------------------------------------------------------------------------------------------
//* processGUI() handles the update of the main panel, the menu panel or the item panel
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

        if (getWord(GSW,FSW)==true) {
           setWord(&GSW,FSW,false);
           if (vfo!=nullptr) {
              vfo->swapVFO();
           }

        }

        if (getWord(GSW,FSWL)==true) {   // switch to menu mode
           setWord(&GSW,FSWL,false);
           setWord(&MSW,CMD,true);
           setWord(&MSW,GUI,false);
           lcd->clear();
           showMenu();
        }

        if (getWord(GSW,FSQ)==true) {
            setWord(&GSW,FSQ,false);
           (TRACE>=0x02 ? fprintf(stderr,"%s:processGUI() SQL activation SQL(%s)\n",PROGRAMID,BOOL2CHAR(getWord(d->dra[m].STATUS,SQ))) : _NOP);
            showMeter();
        }

        if (getWord(GSW,FPTT)==true) {
            setWord(&GSW,FPTT,false);
           (pushPTT==0x00 ? vfo->setPTT(true) : vfo->setPTT(false));
        }
     }
//*====================================================================================================
//*-------------- Process main menu Panel (CMD=true GUI=false)
//*====================================================================================================

    if (getWord(MSW,CMD)==true && getWord(MSW,GUI)==false) {

        if (getWord(GSW,FSW)==true) {  //return to VFO mode
           setWord(&GSW,FSW,false);
           setWord(&MSW,CMD,false);
           showPanel();
        }

        if (getWord(GSW,ECW)==true) {  //while in menu mode turn knob clockwise
           setWord(&GSW,ECW,false);
           nextMenu(+1);
	   lcd->clear();
           showMenu();
        }

        if (getWord(GSW,ECCW)==true) {  //while in menu mode turn knob counterclockwise
           setWord(&GSW,ECCW,false);
           nextMenu(-1);
           lcd->clear();
           showMenu();
        }


        if (getWord(GSW,FSWL)==true) {  //while in menu mode transition to GUI mode (backup)
           setWord(&GSW,FSWL,false);
           setWord(&MSW,GUI,true);
           lcd->clear();
           backupMenu();
           showMenu();
        }

        if (getWord(SSW,FSAVE)==true) { //restore menu after saving message
           setWord(&SSW,FSAVE,false);
           lcd->clear();
           showMenu();
        }
     }

//*====================================================================================================
//*-------------- Process main menu Panel (CMD=true GUI=true)
//*====================================================================================================

   if (getWord(MSW,CMD)==true && getWord(MSW,GUI)==true) {

        if (getWord(GSW,ECW)==true) {  //while in menu mode turn knob clockwise
           setWord(&GSW,ECW,false);
           nextChild(+1);
	   lcd->clear();
           showMenu();
        }

        if (getWord(GSW,ECCW)==true) {  //while in menu mode turn knob counterclockwise
           setWord(&GSW,ECCW,false);
           nextChild(-1);
           lcd->clear();
           showMenu();
        }

        if (getWord(GSW,FSW)==true) {  //in GUI mode return to menu mode without commit of changes
           setWord(&GSW,FSW,false);
           setWord(&MSW,GUI,false);
           lcd->clear();
           restoreMenu();
           showMenu();
        }

        if (getWord(GSW,FSWL)==true) {  //in GUI mode return to menu mode with commit of changes
           setWord(&GSW,FSWL,false);
           setWord(&MSW,GUI,false);
           lcd->clear();
           strcpy(LCD_Buffer,"Saving..");
           lcd->println(0,0,LCD_Buffer);
           saveMenu();
           TSAVE=3000;

        }
     }

}
//*=====================================================================================================
void procUpdateBW(MMS* p) {
     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateBW() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 1) {
        p->mVal=1;
     }
     if (d==nullptr) {return;}
     (p->mVal == 1 ? (void)d->setGBW(true) : (void)d->setGBW(false));
     d->sendSetGroup();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateBW() GBW is now(%d)\n",PROGRAMID,p->mVal) : _NOP);


}
void procUpdateVol(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateVol() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 8) {
        p->mVal=8;
     }
     if (d==nullptr) {return;}
     d->setVol(p->mVal);
     d->sendSetVolume();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateVol() Vol is now(%d)\n",PROGRAMID,p->mVal) : _NOP);
     return;

}
void procUpdateSql(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateSql() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 8) {
        p->mVal=8;
     }
     if (d==nullptr) {return;}
     d->setSQL(p->mVal);
     d->sendSetGroup();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateVol() SQL is now(%d)\n",PROGRAMID,p->mVal) : _NOP);
     return;
}
void procUpdateRxCTCSS(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateRxCTCSS() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 38) {
        p->mVal=38;
     }
     if (d==nullptr) {return;}
     d->setRxCTCSS(p->mVal);
     d->sendSetGroup();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateRxCTCSS() CTCSS is now(%d)\n",PROGRAMID,p->mVal) : _NOP);

}
void procUpdateTxCTCSS(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateTxCTCSS() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 38) {
        p->mVal=38;
     }
     if (d==nullptr) {return;}
     d->setTxCTCSS(p->mVal);
     d->sendSetGroup();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateTxCTCSS() CTCSS is now(%d)\n",PROGRAMID,p->mVal) : _NOP);
}
void procUpdateOfs(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateOfs() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 2) {
        p->mVal=2;
     }
     if (vfo==nullptr) {return;}
     switch(p->mVal) {
	case 0 : {
		  vfo->setShift(0.0);
 		  break;
		 }
	case 1 : {
		  vfo->setShift(+600000.0);
 		  break;
		 }
	case 2 : {
		  vfo->setShift(-600000.0);
 		  break;
		 }
     }
     if (d==nullptr) {return;}
     d->setTFW(d->getRFW()+(vfo->getShift()/1000000));
     d->sendSetGroup();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateTxCTCSS() Offseet is now(%d)\n",PROGRAMID,p->mVal) : _NOP);
}
void procUpdatePFE(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdatePFE() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 1) {
        p->mVal=1;
     }
     if (d==nullptr) {return;}
    (p->mVal == 1 ? d->setPEF(true) : d->setPEF(false));
     d->sendSetFilter();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdatePEF() PEF is now(%d)\n",PROGRAMID,p->mVal) : _NOP);


}
void procUpdateLPF(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateLPF() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 1) {
        p->mVal=1;
     }
     if (d==nullptr) {return;}
     (p->mVal == 1 ? d->setLPF(true) : d->setLPF(false));
     d->sendSetFilter();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateLPF() LPF is now(%d)\n",PROGRAMID,p->mVal) : _NOP);



}
void procUpdateHPF(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateHPF() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 1) {
        p->mVal=1;
     }
     if (d==nullptr) {return;}
     (p->mVal == 1 ? d->setHPF(true) : d->setHPF(false));
     d->sendSetFilter();
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateHPF() HPF is now(%d)\n",PROGRAMID,p->mVal) : _NOP);


}
void procUpdateHL(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateHL() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 1) {
        p->mVal=1;
     }
     if (d==nullptr) {return;}
     (p->mVal == 1 ? d->setHL(true) : d->setHL(false));
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateHPF() HL is now(%d)\n",PROGRAMID,p->mVal) : _NOP);

}
void procUpdatePD(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateHL() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 1) {
        p->mVal=1;
     }
     if (d==nullptr) {return;}
     (p->mVal == 1 ? d->setPD(true) : d->setPD(false));
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateHPF() PD is now(%d)\n",PROGRAMID,p->mVal) : _NOP);
}
void procUpdateBacklight(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateBacklight() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 60) {
        p->mVal=60;
     }
     backlight=p->mVal*1000;
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateBacklight() Backlight is now(%d)\n",PROGRAMID,p->mVal) : _NOP);

}
void procUpdateStep(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateStep() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 1) {
        p->mVal=1;
     }
     if (vfo==nullptr) {return;}
     switch(p->mVal) {
	case 0 : {
		  vfo->setVFOStep(VFOA,VFO_STEP_10KHz);
		  vfo->setVFOStep(VFOB,VFO_STEP_10KHz);
 		  break;
		 }
	case 1 : {
		  vfo->setVFOStep(VFOA,VFO_STEP_5KHz);
		  vfo->setVFOStep(VFOB,VFO_STEP_5KHz);
 		  break;
		 }
     }
     if (d==nullptr) {return;}
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateStep() Step is now(%d)\n",PROGRAMID,p->mVal) : _NOP);

}
void procUpdateWatchdog(MMS* p) {

     (TRACE>=0x03 ? fprintf(stderr,"%s:procUpdateWatchdog() \n",PROGRAMID) : _NOP);
     if (p->mVal < 0) {
        p->mVal=0;
     }
     if (p->mVal > 60) {
        p->mVal=60;
     }
     watchdog=p->mVal*1000;
     (TRACE>=0x02 ? fprintf(stderr,"%s:procUpdateWatchdog() Watchdog is now(%d)\n",PROGRAMID,p->mVal) : _NOP);


}
//*------- Procedure to manage content of the display
void procChangeVol(MMS* p) {

char f[9];
char lev[2];
int  k = 255;
char c = k;

     (TRACE>=0x03 ? fprintf(stderr,"%s:procChangeVol %d [%s] \n",PROGRAMID,p->mVal,f) : _NOP);

     sprintf(lev,"%c", c); //prints A
     if (p->mVal>8) { p->mVal=8;}
     if (p->mVal<0) { p->mVal=0;}

     strcpy(f,"        ");
     for (int i=0;i<p->mVal;i++) {
         f[i]=lev[0];
     }
     (TRACE>=0x02 ? fprintf(stderr,"%s:procChangeVol %d [%s] \n",PROGRAMID,p->mVal,f) : _NOP);
     sprintf(p->mText," %d [%s]",p->mVal,f);

}
void procChangeSql(MMS* p) {

char f[9];
char lev[2];
int  k = 255;
char c = k;

     sprintf(lev,"%c", c); //prints A
     if (p->mVal>8) { p->mVal=8;}
     if (p->mVal<0) { p->mVal=0;}

     strcpy(f,"        ");
     for (int i=0;i<p->mVal;i++) {
         f[i]=lev[0];
     }
     (TRACE>=0x02 ? fprintf(stderr,"%s:procChangeSql %d [%s] \n",PROGRAMID,p->mVal,f) : _NOP);
     sprintf(p->mText," %d [%s]",p->mVal,f);

}

void procChangeRxCTCSS(MMS* p) {

char buffer[16];

     (TRACE>=0x03 ? fprintf(stderr,"%s:procChangeRxCTCSS() \n",PROGRAMID) : _NOP);
     if (p->mVal<0) {p->mVal=0;}
     if (p->mVal>38) {p->mVal=38;}
     if (p->mVal!=0) {
         sprintf(buffer,"%5.1f",d->CTCSStoTone(p->mVal));
         sprintf(p->mText,"%s Hz",buffer);
     } else {
         sprintf(p->mText,"No tone");
     }

}
void procChangeTxCTCSS(MMS* p) {

char buffer[16];

     (TRACE>=0x03 ? fprintf(stderr,"%s:procChangeTxCTCSS() \n",PROGRAMID) : _NOP);
     if (p->mVal<0) {p->mVal=0;}
     if (p->mVal>38) {p->mVal=38;}
     if (p->mVal!=0) {
        sprintf(buffer,"%5.1f",d->CTCSStoTone(p->mVal));
        sprintf(p->mText,"%s Hz",buffer);
     } else {
        sprintf(p->mText,"No tone");
     }

}


void setupMMS() {

     root=new MMS(0,(char*)"root",NULL,NULL);
     root->TRACE=TRACE;

     mnu_BW  =  new MMS(1,(char*)"Bandwidth",NULL,procUpdateBW);
     mnu_Vol =  new MMS(2,(char*)"Volume",procChangeVol,procUpdateVol);
     mnu_Sql =  new MMS(3,(char*)"Squelch",procChangeSql,procUpdateSql);
     mnu_Rx_CTCSS= new MMS(4,(char*)"Rx CTCSS",procChangeRxCTCSS,procUpdateRxCTCSS);
     mnu_Tx_CTCSS= new MMS(5,(char*)"Tx CTCSS",procChangeTxCTCSS,procUpdateTxCTCSS);
     mnu_Ofs =  new MMS(6,(char*)"Offset",NULL,procUpdateOfs);
     mnu_PFE =  new MMS(7,(char*)"PEF",NULL,procUpdatePFE);
     mnu_LPF =  new MMS(8,(char*)"LPF",NULL,procUpdateLPF);
     mnu_HPF =  new MMS(9,(char*)"HPF",NULL,procUpdateHPF);
     mnu_HL  =  new MMS(10,(char*)"Power",NULL,procUpdateHL);
     mnu_PD  =  new MMS(11,(char*)"Pwr Saving",NULL,procUpdatePD);
     mnu_Backlight = new MMS(12,(char*)"Backlight",NULL,procUpdateBacklight);
     mnu_Step=  new MMS(13,(char*)"Step",NULL,procUpdateStep);
     mnu_Watchdog = new MMS(14,(char*)"Watchdog",NULL,procUpdateWatchdog);

     root->add(mnu_BW);
     root->add(mnu_Vol);
     root->add(mnu_Sql);
     root->add(mnu_Rx_CTCSS);
     root->add(mnu_Tx_CTCSS);
     root->add(mnu_Ofs);
     root->add(mnu_PFE);
     root->add(mnu_LPF);
     root->add(mnu_HPF);
     root->add(mnu_LPF);
     root->add(mnu_HL);
     root->add(mnu_PD);
     root->add(mnu_Backlight);
     root->add(mnu_Step);
     root->add(mnu_Watchdog);

//*--- 


     mnu_BW_12KHZ = new MMS(0,(char*)"12.5 KHz",NULL,NULL);
     mnu_BW_25KHZ = new MMS(1,(char*)"25.0 KHz",NULL,NULL);

     mnu_BW->add(mnu_BW_12KHZ);
     mnu_BW->add(mnu_BW_25KHZ);

     mnu_Vol_sp=new MMS(7,(char*)"*",NULL,NULL);
     mnu_Sql_sp=new MMS(1,(char*)"*",NULL,NULL);

     mnu_Rx_CTCSS_sp=new MMS(0,(char*)"*",NULL,NULL);
     mnu_Tx_CTCSS_sp=new MMS(0,(char*)"*",NULL,NULL);

     mnu_Rx_CTCSS->add(mnu_Rx_CTCSS_sp);
     mnu_Tx_CTCSS->add(mnu_Tx_CTCSS_sp);

     mnu_Rx_CTCSS->lowerLimit(0);
     mnu_Rx_CTCSS->upperLimit(38);

     mnu_Tx_CTCSS->lowerLimit(0);
     mnu_Tx_CTCSS->upperLimit(38);

     mnu_Vol->add(mnu_Vol_sp);
     mnu_Vol->lowerLimit(0);
     mnu_Vol->upperLimit(8);

     mnu_Sql->add(mnu_Sql_sp);
     mnu_Sql->lowerLimit(0);
     mnu_Sql->upperLimit(8);

     mnu_Ofs_None= new MMS(0,(char*)"Simplex",NULL,NULL);
     mnu_Ofs_Plus= new MMS(1,(char*)"+600 KHz",NULL,NULL);
     mnu_Ofs_Minus=new MMS(2,(char*)"-600 KHz",NULL,NULL);

     mnu_Ofs->add(mnu_Ofs_None);
     mnu_Ofs->add(mnu_Ofs_Plus);
     mnu_Ofs->add(mnu_Ofs_Minus);


     mnu_PFE_Off = new MMS(0,(char*)"Off",NULL,NULL);
     mnu_PFE_On  = new MMS(1,(char*)"On",NULL,NULL);

     mnu_LPF_Off = new MMS(0,(char*)"Off",NULL,NULL);
     mnu_LPF_On  = new MMS(1,(char*)"On",NULL,NULL);

     mnu_HPF_Off = new MMS(0,(char*)"Off",NULL,NULL);
     mnu_HPF_On  = new MMS(1,(char*)"On",NULL,NULL);

     mnu_HL_Off = new MMS(0,(char*)"Low",NULL,NULL);
     mnu_HL_On  = new MMS(1,(char*)"High",NULL,NULL);

     mnu_PD_Off = new MMS(0,(char*)"Off",NULL,NULL);
     mnu_PD_On  = new MMS(1,(char*)"On",NULL,NULL);

     mnu_Backlight_Off = new MMS(0,(char*)"Off",NULL,NULL);
     mnu_Backlight_On  = new MMS(1,(char*)"On",NULL,NULL);

     mnu_Step_10KHZ = new MMS(0,(char*)"10 KHz",NULL,NULL);
     mnu_Step_5KHZ  = new MMS(1,(char*)" 5 KHz",NULL,NULL);

     mnu_Watchdog_Off = new MMS(0,(char*)"Off",NULL,NULL);
     mnu_Watchdog_On  = new MMS(1,(char*)"On",NULL,NULL);

     mnu_PFE->add(mnu_PFE_Off);
     mnu_PFE->add(mnu_PFE_On);

     mnu_LPF->add(mnu_LPF_Off);
     mnu_LPF->add(mnu_LPF_On);

     mnu_HPF->add(mnu_HPF_Off);
     mnu_HPF->add(mnu_HPF_On);

     mnu_HL->add(mnu_HL_Off);
     mnu_HL->add(mnu_HL_On);

     mnu_PD->add(mnu_PD_Off);
     mnu_PD->add(mnu_PD_On);

     mnu_Backlight->add(mnu_Backlight_Off);
     mnu_Backlight->add(mnu_Backlight_On);

     mnu_Watchdog->add(mnu_Watchdog_Off);
     mnu_Watchdog->add(mnu_Watchdog_On);

     mnu_Step->add(mnu_Step_10KHZ);
     mnu_Step->add(mnu_Step_5KHZ);


     (getWord(d->dra[m].STATUS,PEF)==false ? mnu_PFE->setChild(0) : mnu_PFE->setChild(1));
     (getWord(d->dra[m].STATUS,HPF)==false ? mnu_HPF->setChild(0) : mnu_HPF->setChild(1));
     (getWord(d->dra[m].STATUS,LPF)==false ? mnu_LPF->setChild(0) : mnu_LPF->setChild(1));
     (getWord(d->dra[m].STATUS,HL)==false ? mnu_HL->setChild(0) : mnu_HL->setChild(1));
     (getWord(d->dra[m].STATUS,PD)==false ? mnu_PD->setChild(0) : mnu_PD->setChild(1));

}
