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
    (TRACE>=0x03 ? fprintf(stderr,"%s:setupGPIO() Setup Encoder Push\n",PROGRAMID) : _NOP);
    gpioSetMode(GPIO_SW, PI_INPUT);
    gpioSetPullUpDown(GPIO_SW,PI_PUD_UP);
    gpioSetAlertFunc(GPIO_SW,updateSW);
    usleep(100000);

    (TRACE>=0x03 ? fprintf(stderr,"%s:setupGPIO() Setup Encoder\n",PROGRAMID) : _NOP);
    gpioSetMode(GPIO_CLK, PI_INPUT);
    gpioSetPullUpDown(GPIO_CLK,PI_PUD_UP);
    usleep(100000);
    gpioSetISRFunc(GPIO_CLK, FALLING_EDGE,0,updateEncoders);

    gpioSetMode(GPIO_DT, PI_INPUT);
    gpioSetPullUpDown(GPIO_DT,PI_PUD_UP);
    usleep(100000);

    (TRACE>=0x03 ? fprintf(stderr,"%s:setupGPIO() Setup GPIO Handler\n",PROGRAMID) : _NOP);
    for (int i=0;i<64;i++) {

        gpioSetSignalFunc(i,sighandler);

    }
    (TRACE>=0x03 ? fprintf(stderr,"%s:setupGPIO() End of setup for GPIO Handler\n",PROGRAMID) : _NOP);


}