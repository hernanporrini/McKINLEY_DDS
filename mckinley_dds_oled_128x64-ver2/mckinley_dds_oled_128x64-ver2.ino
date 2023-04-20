#include <EEPROM.h>             //The Arduino EEPROM library
#include <Adafruit_GFX.h>       //Adafruit GFX https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>   //Adafruit SSD1306 https://github.com/adafruit/Adafruit_SSD1306
#include <Wire.h>               //IDE Standard
#include <Rotary.h>             //Ben Buxton https://github.com/brianlow/Rotary
#include <si5351.h>             //Etherkit https://github.com/etherkit/Si5351Arduino

#define XT_CAL_F    127000      //Si5351 calibration factor, adjust to get exatcly 10MHz. 
#define fint        10695000
#define offset_ssb  1500
#define key_baja    2
#define key_sube    3
#define key_paso    4
#define key_clar    5
#define pot_clar    A0
#define sig_meter   A1
#define key_ptt     A2
#define key_mode    9
    

Rotary r = Rotary(key_baja, key_sube);
Si5351 si5351(0x60); //Si5351 I2C Address 0x60

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);

long cal = XT_CAL_F;
unsigned long frec = 27000000;
unsigned long frecold = 0;
long fpaso;
int  paso = 2;
long clarif= 0;
long clarifold= 0;
byte x;
byte y;
int banda=4;
int canal=11;
int modo=0; // modo: 0=AM/FM, 1=LSB, 2=USB
bool mode=1; // mode: 0=canalizado, 1=banda corrida
/*
String band[10]   ={"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"};

String channel[]={  "1",         "2", "3","e", "4", "5",       "6", "7","e", "8", "9",      "10", 
			        "11", "11","12","13",     "14","15","15","16","17",     "18","19","19","20",
		   	        "21",       "22","23",     "24","25",      "26","27",     "28","29",      "30",
		   	        "31",       "32","33",     "34","35",      "36","37",     "38","39",      "40"};
*/
void setup() 
  {
  EEPROM.get(0, frec);             //Restore the frequency from EEPROMWire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
  pinMode(key_baja, INPUT);
  pinMode(key_sube, INPUT);
  pinMode(key_paso, INPUT);
  pinMode(key_ptt,  INPUT_PULLUP);
  pinMode(key_clar, INPUT_PULLUP);
  pinMode(key_mode, INPUT_PULLUP);
  pinMode(13, OUTPUT);
  texto_inicial();  //If you hang on startup, comment
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.set_correction(cal, SI5351_PLL_INPUT_XO);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 1);                  //1 - Enable / 0 - Disable CLK
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 0);
  r.begin();
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  fpaso = 10000;
  frec=27065000;  
  //EEPROM.get(0, frec);
  generarfrecuencia();
  }

void loop() 
  {
  if (digitalRead(key_clar))
    clarif=0; 
  else 
    clarif=map(analogRead(pot_clar),0,1023,-500,500);
  if (clarifold!=clarif)
    {
    generarfrecuencia();
    clarifold=clarif;
    }  
    
  if (frecold!=frec)
    {
    generarfrecuencia();
    frecold=frec;
    }
  if (digitalRead(key_paso) == LOW) 
    {
    if (!mode) 
      {
      banda++;    
      if (banda>10) {banda= 1;}
      frec=25155000+450000*long(banda)+10000*long(canal);
      }
      else
      {
      switch (paso)    
        {
        case 1: paso = 2; fpaso = 10000;  break;
        case 2: paso = 3; fpaso = 100000; break;
        case 3: paso = 1; fpaso = 1000;   break;
        }
      }
    delay(300);
    }
  if (digitalRead(key_mode) == LOW) 
    {
    mode=!mode;
    delay(300);
    }
  
  mostrarfrecuencia();
  }

void texto_inicial()
  {
  display.setTextSize(2);
  display.setCursor(0, 2);
  display.print(" McKINLEY ");
  display.setTextSize(1);
  display.setCursor(6, 25);
  display.print("Hernan LW1EHP 2023");
  display.display(); 
  delay(5000);
  }

ISR(PCINT2_vect) 
  {
  char result = r.process();
  if (mode)
    {
    if (result == DIR_CW) frec = frec + fpaso;
    else if (result == DIR_CCW) frec = frec - fpaso;
    if (frec >= 31000000) frec = 31000000;
    if (frec <= 25000000) frec = 25000000;
    }
    else
    {
    if (result == DIR_CW) {canal++;if (canal>45) {canal=1; banda++;}}
    else if (result == DIR_CCW) {canal--; if (canal<1) {canal=45; banda--;}} 
    if (banda<1)  {banda=10;}
    if (banda>10) {banda= 1;}
    frec=25155000+450000*long(banda)+10000*long(canal);
    }
  EEPROM.put(0, frec);
  }
  
void generarfrecuencia()
  {
  switch(modo)
  {
    case 0:  si5351.set_freq((frec-fint+clarif) * 100ULL, SI5351_CLK0); break;      //modo AM/FM
    case 1:  si5351.set_freq((frec-fint+clarif-offset_ssb) * 100ULL, SI5351_CLK0); break; //modo LSB
    case 2:  si5351.set_freq((frec-fint+clarif+offset_ssb) * 100ULL, SI5351_CLK0); break; //modo USB
  }
  //si5351.set_freq(10000000 * 100ULL, SI5351_CLK1);
  }

void mostrarfrecuencia() 
  {
  unsigned int m = (frec+clarif) / 1000000;
  unsigned int k = ((frec+clarif) % 1000000) / 1000;
  //unsigned int h = ((frec+clarif) % 1000) / 1;
  display.clearDisplay();
  display.setTextSize(3);
  char buffer[15] = "";
  display.setCursor(10, 0); 
  //sprintf(buffer, "%2d.%003d.%003d", m, k, h);
  sprintf(buffer, "%2d.%003d", m, k);
  display.print(buffer);
  display.setTextColor(WHITE);
  if (mode)
    {
    if (paso == 1) {display.drawLine(102, 22,112, 22, WHITE);
                    display.drawLine(101, 23,113, 23, WHITE);
                    display.drawLine(100, 24,114, 24, WHITE);}
    if (paso == 2) {display.drawLine(84, 22, 94, 22, WHITE); 
                    display.drawLine(83, 23, 95, 23, WHITE);
                    display.drawLine(82, 24, 96, 24, WHITE);} 
    if (paso == 3) {display.drawLine(66, 22, 76, 22, WHITE);
                    display.drawLine(65, 23, 77, 23, WHITE);
                    display.drawLine(64, 24, 78, 24, WHITE);} 
    }
    else
    {
    display.setTextSize(4);
    
    display.setCursor(10, 34); 
    switch (canal)
      {
      case  1: display.print("01"); break;
      case  2: display.print("02"); break;
      case  3: display.print("03"); break;
      case  4: display.print("xx"); break;
      case  5: display.print("04"); break;
      case  6: display.print("05"); break;
      case  7: display.print("06"); break;
      case  8: display.print("07"); break;
      case  9: display.print("xx"); break;
      case 10: display.print("08"); break;
      case 11: display.print("09"); break;
      case 12: display.print("10"); break;
      case 13: display.print("11"); break;
      case 14: display.print("xx"); break;
      case 15: display.print("12"); break;
      case 16: display.print("13"); break;
      case 17: display.print("14"); break;
      case 18: display.print("15"); break;
      case 19: display.print("xx"); break;
      case 20: display.print("16"); break;
      case 21: display.print("17"); break;
      case 22: display.print("18"); break;
      case 23: display.print("19"); break;
      case 24: display.print("xx"); break;
      case 25: display.print("20"); break;
      case 26: display.print("21"); break;
      case 27: display.print("22"); break;
      case 28: display.print("23"); break;
      case 29: display.print("24"); break;
      case 30: display.print("25"); break;
      case 31: display.print("26"); break;
      case 32: display.print("27"); break;
      case 33: display.print("28"); break;
      case 34: display.print("29"); break;
      case 35: display.print("30"); break;
      case 36: display.print("31"); break;
      case 37: display.print("32"); break;
      case 38: display.print("33"); break;
      case 39: display.print("34"); break;
      case 40: display.print("35"); break;
      case 41: display.print("36"); break;
      case 42: display.print("37"); break;
      case 43: display.print("38"); break;
      case 44: display.print("39"); break;
      case 45: display.print("40"); break;
      }
    display.setCursor(60, 41); 
    display.setTextSize(3);
    switch (banda)
      {
      case  1: display.print("A"); break;
      case  2: display.print("B"); break;
      case  3: display.print("C"); break;
      case  4: display.print("D"); break;
      case  5: display.print("E"); break;
      case  6: display.print("F"); break;
      case  7: display.print("G"); break;
      case  8: display.print("H"); break;
      case  9: display.print("I"); break;
      case 10: display.print("J"); break;
      }
    }
  if (!digitalRead(key_clar))
    {
    display.setTextSize(1);
    display.setCursor(60, 27);
    display.print("RIT");
    if (clarif<0) display.setCursor(80, 27); else display.setCursor(85, 27); 
    display.print(clarif);
    display.print("Hz");
    }
  display.display();
  }
