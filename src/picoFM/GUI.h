
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
