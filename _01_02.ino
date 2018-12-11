#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <Streaming.h>
#include <SoftwareSerial.h>
#include <Keypad.h>
#include "RTClib.h"

RTC_DS1307 rtc;


const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
char keys[ROWS][COLS] = {       
  {'1','2','3', 'A'},           
  {'4','5','6', 'B'},           
  {'7','8','9', 'C'},           
  {'*','0','#', 'D'}            
};                                                                                                        //arduinoPins-->membranePins
byte rowPins[ROWS] = {5, 4, 3, 2}; //felső | 2. | 3. | alsó |----connect to the row pinouts of the keypad   2->5  3->6  4->7  5->8
byte colPins[COLS] = {9, 8, 7, 6}; //bal | 2. | 3. | jobb |---connect to the column pinouts of the keypad   6->1  7->2  8->3  9->4
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


byte menu[4] = {1,0,0,0};

const byte progRows = 30;
byte programs[progRows][5]; //EEPROM address starts from 6

byte shownProgID = 0; 
byte paramID = 0;
byte shownZoneID = 0;

const byte zoneNo = 8;
bool activeZones[zoneNo];

byte tempParamVal = 0;
bool intro = 0;

bool justRefreshed = false; 
uint32_t secC; 

uint32_t eepromCounter;
int updateTime;

bool BTControll = false; 

int sensorSens; //EEPROM address: 0-1
bool sensorOK = 1; 
int seasonAdj;  //EEPROM address: 2-3


LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial bt(11, 10);


void setup() {
//Loading data from eeprom

  EEPROM.get(0, sensorSens);
  EEPROM.get(2, seasonAdj);

  int k = 6; //EEPROM address index from 6
  for (int i = 0; i < progRows; i++)
  {
    for (int j = 0; j < 5; j++)
    {
      programs[i][j] = EEPROM.read(k);
      k++;
    }
  }

  updateTime = 900; //How often data should be backed up to EEPROM, in seconds
  eepromCounter = 0;
  
//-------------------------
  
  
  Serial.begin(19200);
  bt.begin(9600);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  for (byte i = 37; i > 29; i--) //define relay pins
  {
    pinMode(i, OUTPUT);
    digitalWrite(i, HIGH);
  }

  
  
  while(!Serial)
  {
    ;
  }
  
  lcd.begin();
  lcd.clear();  
  
  Serial.print("SerialOK");
  bt.println("AT");
  if (bt.available())
  {
    Serial.write(bt.read());
  }

  //INTRO
  if (intro)
  {
    lcd.clear();
    lcd.print("HOME WATERING");
    lcd.setCursor(0,1);
    lcd.print("Version 0.1");
    delay(1000);
    lcd.clear();
    lcd.print("Booting");
    for (int i=0; i < 3; i++)
    {
      lcd.print(".");
      delay(400);
    }
    lcd.clear();
  }

lcd.clear(); lcd.print("AUTOMATIC MODE");

resetZones();

secC = 0;
}

void loop() {

  DateTime timeNow = rtc.now();
  checkSensor();
  checkActivePrograms(timeNow);
  switchZones();
  

  if ((timeNow.unixtime() - eepromCounter) > updateTime   ) 
  {
    updateEEPROM();
    eepromCounter = timeNow.unixtime();
  }


  
  char key = keypad.getKey();
  if (key == 'B') {sMenu(2,1,0,0); printProgram(shownProgID); resetZones();}
  else if (key == 'C') {sMenu(3,1,0,0); listZones();}
  else if (key == 'D') {sMenu(4,0,0,0); lcd.clear(); lcd.print("WATERING DISABLED");}
  else if (key == 'A') {sMenu(1,0,0,0); resetZones(); lcd.clear(); lcd.print("AUTOMATIC MODE"); lcd.setCursor(0,1); 
                        lcd.print(String(timeNow.month()) + "-" + String(timeNow.day()) + " " + String(timeNow.hour()) + ":" + String(timeNow.minute()) + " Se:" + String(sensorOK));}

  if (BTControll)
  {
    if (!justRefreshed)
    {
      secC = timeNow.unixtime();
      lcd.clear();
      lcd.print("BLUETOOTH CONN");
      lcd.setCursor(0,1);
      lcd.print("No manual setup");
      justRefreshed = true;
    }
    if (timeNow.unixtime() - secC >= 15 && justRefreshed)
    {
      justRefreshed = false; 
    }
  }
  else if (menu[0] == byte(1)) //AUTOMATIC MODE
  {
    if ((timeNow.unixtime() - secC) >= 15)
    {
      secC = timeNow.unixtime();
      lcd.clear();
      lcd.print("AUTOMATIC MODE");
      lcd.setCursor(0,1);
      lcd.print(String(timeNow.month()) + "-" + String(timeNow.day()) + " " + String(timeNow.hour()) + ":" + String(timeNow.minute()) + " Se:" + String(sensorOK));
    }
    else if (timeNow.second() == 0 && !justRefreshed)
    {
      secC = timeNow.unixtime();
      lcd.clear();
      lcd.print("AUTOMATIC MODE");
      lcd.setCursor(0,1);
      lcd.print(String(timeNow.month()) + "-" + String(timeNow.day()) + " " + String(timeNow.hour()) + ":" + String(timeNow.minute()) + " Se:" + String(sensorOK));
      justRefreshed = true;
    }

    if (timeNow.unixtime() - secC >= 1 && justRefreshed)
    {
      justRefreshed = false; 
    }
  }
  else if (menu[0] == byte(2) && menu[1] == byte(1)) //PROGRAMMING MODE
  {
    if (menu[2] == byte(0) && menu[3] == byte(0)) //PROGRAMMING MODE MAIN: list programs
    {
      if (key == '2') //DISPLAY THE UPPER ROW OF PROGRAM
      {
         if (shownProgID == 0){;}
         else {shownProgID--; printProgram(shownProgID);}
      }
      else if (key == '8') //DISPLAY THE NEXT ROW
      {
        if (shownProgID == progRows-1) {;}
        else {shownProgID++; printProgram(shownProgID);}
      }
      else if (key == '#') //SELECT A PROGRAM TO MODIFY 
      {
        sMenu(2,1,2,0);
        paramID = 0; 
        showParam();
      }
      else if (key == '*') //DELETE THE SHOWN PROGRAM
      {
        sMenu(2,1,1,0);
        lcd.clear();
        lcd.print("Delete program:");
        lcd.setCursor(0,1);
        lcd.print(String(shownProgID) + "?");
      }
    }
    else if (menu[2] == byte(1) && menu[3] == byte(0))  //DELETE the selected program
    {
      if (key == '#')
      {
        deleteProg(shownProgID);  //DELETE THE SHOWN PROGRAM 
        lcd.clear();
        lcd.print("Prog " + String(shownProgID) + " deleted");
        printProgram(shownProgID);
      }
      else if (key == '*') //GOES BACK 
      {
        sMenu(2,1,0,0);
        printProgram(shownProgID);
      }
    }
    else if (menu[2] == byte(2) && menu[3] == byte(0)) //CHOOSE PARAMETER of a program to modify
    {
      if (key == '6') //SELECT THE NEXT PARAMETER
      {
        if (paramID == 4) {}
        else {paramID++; showParam();}
      }
      else if (key == '4')  //SELECT THE PARAMETER BEFORE 
      {
        if (paramID == 0) {}
        else {paramID--; showParam();}
      }
      else if (key == '*')  //GOES BACK
      {
        sMenu(2,1,0,0);
        printProgram(shownProgID);
      }
      else if (key == '#') //CHOOSES THE SELECTED PARAMETER TO EDIT 
      {
        sMenu(2,1,2,1);
        tempParamVal = programs[shownProgID][paramID];
        printSetParam();
      }
    }
    else if (menu[2] == byte(2) && menu[3] == byte(1)) //SET PARAMETER
    {
      if (key)
      {
        if (key == '#') 
        {
          programs[shownProgID][paramID] = tempParamVal; 
          byte zone = programs[shownProgID][0];
          activeZones[zone-1] = 0; 
          sMenu(2,1,2,0); 
          showParam();
        }
        else if (key == '*') {sMenu(2,1,2,0); showParam();}
        else
        {
          int i = key - '0';
          byte b = (byte)i;

          if (paramID == 0)
          {
            if (b > byte(8) || b == 0) {}
            else {tempParamVal = b; printSetParam();}
          }
          else if (paramID == 1)
          {
            if (((10 * tempParamVal) + b) > 23) {tempParamVal = b; printSetParam();}
            else {tempParamVal = ((tempParamVal * 10) + b); printSetParam();}
          }
          else if (paramID == 2)
          {
            if (((tempParamVal*10) + b) > 59) {tempParamVal = b; printSetParam();}
            else {tempParamVal = ((tempParamVal * 10) + b); printSetParam();}
          }
          else if (paramID == 3)
          {
            if (((tempParamVal * 10) + b) > 99 && b != 0) {tempParamVal = b; printSetParam();}
            else if (((tempParamVal * 10) + b) > 99 && b == 0) {}
            else {tempParamVal = ((tempParamVal * 10) + b); printSetParam();}
          }
          else if (paramID == 4)
          {
            if (b > byte(1)) {}
            else {tempParamVal = b; printSetParam();}
          }
        }
      }
    }
  }
  else if (menu[0] == byte(3) && menu[1] == byte(1)) //MANUAL MODE
  {
    if (menu[2] == byte(0) && menu[3] == byte(0)) //LIST ZONES
    {
      if (key == '2')
      {
        if (shownZoneID == 0) {;}
        else {shownZoneID--; listZones();}
      }
      else if (key == '8')
      {
        if (shownZoneID == 6) {;}
        else {shownZoneID++; listZones();}
      }
      else if (key == '#')
      {
        modZoneStat(); //MODIFIY STATUS
      }
    }
  }
  else if (menu[0] == byte(4) && menu[1] == byte(0) && menu[2] == byte(0) && menu[3] == byte(0)){} //DISABLE WATERING


  while (bt.available())
  {
    String toPrint = String(readBT());
    callBTFunc(toPrint);
  }
}

void checkActivePrograms(DateTime nw)
{
  if (menu[0] == 1 || menu[0] == 2)
  {
    for (byte i = 0; i < progRows; i++)
    {
      if (programs[i][0] != 0)
      {
        byte pHour = programs[i][1];
        byte pMin = programs[i][2];
        byte pDur = programs[i][3];
        byte zone = programs[i][0];
        
        DateTime progStart (nw.year(), nw.month(), nw.day(), pHour, pMin, 0);
        DateTime progStop = (progStart.unixtime() + pDur*60*seasonAdj/100);

        if (programs[i][4] == 0)
        {
          if (nw.unixtime() < progStop.unixtime() && nw.unixtime() >= progStart.unixtime()){activeZones[zone - 1] = 1; }
          else if ((nw.unixtime() - progStop.unixtime()) <= 30) {activeZones[zone - 1] = 0; }
          else {}
        }
        else 
        {
          if (nw.unixtime() < progStop.unixtime() && nw.unixtime() >= progStart.unixtime() && sensorOK){activeZones[zone - 1] = 1; }
          else if ((nw.unixtime() - progStop.unixtime()) <= 30 || !sensorOK) {activeZones[zone - 1] = 0; }
          else {}
        }
      } 
    }
  }
}

void showParam()
{
  byte ID = shownProgID;
  lcd.clear();
  String uR;

  if (paramID == 0) {uR = "o ! hh:mm-ms Se";}
  else if (paramID == 1) {uR = "o Z !!:mm-ms Se";}
  else if (paramID == 2) {uR = "o Z hh:!!-ms Se";}
  else if (paramID == 3) {uR = "o Z hh:mm-!! Se";}
  else if (paramID == 4) {uR = "o Z hh:mm-ms !!";}
  
  String lR;
  
  String hh;
  if (programs[ID][1] < 10) {hh = "0" + String(programs[ID][1]);}
  else {hh = String(programs[ID][1]);}
  
  String mm;
  if (programs[ID][2] < 10) {mm = "0" + String(programs[ID][2]);}
  else {mm = String(programs[ID][2]);}
  
  String ms;
  if (programs[ID][3] < 10) {ms = "0" + String(programs[ID][3]);}
  else {ms = String(programs[ID][3]);}
  
  lR = String(ID) + ' ' + String(programs[ID][0]) + " " + hh + ":" + mm + "-" + ms + "  " + String(programs[ID][4]);

  lcd.print(uR);
  lcd.setCursor(0,1);
  lcd.print(lR);
}

void printSetParam()
{
  byte ID = shownProgID;
  lcd.clear();
  String uR;
  byte values[5];
  
  for (int i = 0; i < 5; i++)
  {
    values[i] = programs[ID][i];
  }
  values[paramID] = tempParamVal;

  if (paramID == 0) {uR = "o !             ";}
  else if (paramID == 1) {uR = "o   !!          ";}
  else if (paramID == 2) {uR = "o      !!       ";}
  else if (paramID == 3) {uR = "o         !!    ";}
  else if (paramID == 4) {uR = "o             !!";}
  
  String lR;
  
  String hh;
  if (values[1] < 10) {hh = "0" + String(values[1]);}
  else {hh = String(values[1]);}
  
  String mm;
  if (values[2] < 10) {mm = "0" + String(values[2]);}
  else {mm = String(values[2]);}
  
  String ms;
  if (values[3] < 10) {ms = "0" + String(values[3]);}
  else {ms = String(values[3]);}
  
  lR = String(ID) + ' ' + String(values[0]) + " " + hh + ":" + mm + "-" + ms + "  " + String(values[4]);

  lcd.print(uR);
  lcd.setCursor(0,1);
  lcd.print(lR); 
}

void printProgram(byte ID)
{
  lcd.clear();
  String uR = " Z hh:mm-ms Se";
  String lR;
  
  String hh;
  if (programs[ID][1] < 10) {hh = "0" + String(programs[ID][1]);}
  else {hh = String(programs[ID][1]);}
  
  String mm;
  if (programs[ID][2] < 10) {mm = "0" + String(programs[ID][2]);}
  else {mm = String(programs[ID][2]);}
  
  String ms;
  if (programs[ID][3] < 10) {ms = "0" + String(programs[ID][3]);}
  else {ms = String(programs[ID][3]);}
  
  lR = String(ID+1) + ' ' + String(programs[ID][0]) + " " + hh + ":" + mm + "-" + ms + "  " + String(programs[ID][4]);
  lcd.print(char(223) + uR);
  lcd.setCursor(0,1);
  lcd.print(lR);
}

void deleteProg(byte ID)
{
  for (int i = 1; i < 5; i++)
  {
    programs[ID][i] = 0;
  }
  lcd.clear();
  lcd.print("Prog " + String(ID+1) + " deleted");
  delay(1000);
  sMenu(2,1,0,0);
}

void sMenu(byte a, byte b, byte c, byte d)
{
  menu[0] = a;
  menu[1] = b;
  menu[2] = c;
  menu[3] = d;
}

String readBT()
{
  String readed;
  while (bt.available())
  {
    char c = char(bt.read());
    if (c == char('\r') || c == char('\n') || c == char('&')){}
    else {readed += c;}
  }
  return readed; 
}

void callBTFunc(String input)
{
  byte error = 0;
  
  if (input.length() < 2){bt.println("1");}
  else 
  {
  
    String commandS = String(input[0]) + String(input[1]) + String(input[2]);
    String arg = "";
    
    for (int i = 3; i < input.length(); i++) 
    {
      arg += input[i];
    }
  
    if (!BTControll && commandS != "CN+" && commandS != "DC+")
    {
      error = 2; 
    }
    else if (commandS == "CN+") //log into BT control with password
    {
      if (arg == "password"){BTControll = true; bt.println("Y");}
      else {bt.println("N");}
    }  
    else if (commandS == "GS+") //reads the arduino's state
    {
      DateTime timeNow = rtc.now(); 
      bt.println(String(menu[0]) + "/" + String(timeNow.unixtime()) + "/" + String(sensorOK) + "/" + String(sensorSens) + "/" + String(seasonAdj));
    }
    else if (commandS == "SM+") //set mode
    {
        if (arg.length() != 1) {error = 3;}
        else 
        {
          int i = arg[0] - '0';
          byte b = (byte)i;
    
          sMenu(b, 0, 0, 0); 
          if (b != 3) {resetZones();}
    
          DateTime timeNow = rtc.now(); 
          bt.println(String(menu[0]) + "/" + String(timeNow.unixtime()) + "/" + String(sensorOK) + "/" + String(sensorSens));
        }
    }
    else if (commandS == "SC+") //set clock 
    {
      if (arg.length() != 17) {error = 3;}
      else
      {
        int Y;
        int M; 
        int D;
        int h;
        int m;
        
              
        char * strtokIndx;
        char cArg[arg.length()];
        arg.toCharArray(cArg, arg.length());
  
        strtokIndx = strtok(cArg,"/");      
        Y = atoi(strtokIndx);
        
        strtokIndx = strtok(NULL, "/");
        M = atoi(strtokIndx);
        
        strtokIndx = strtok(NULL, "/"); 
        D = atoi(strtokIndx); 
        
        strtokIndx = strtok(NULL, "/"); 
        h = atoi(strtokIndx);
        
        strtokIndx = strtok(NULL, "/"); 
        m = atoi(strtokIndx);
  
        rtc.adjust(DateTime(Y, M, D, h, m, 0));
        bt.println("Clock setted to: " + String(Y) + "-" + String(M) + "-" + String(D) + " " + String(h) + ":" + String(m));
      }
    }
    else if (commandS == "GP+") //read the choosen program's parameters
    {
      if (arg.length() != 1){error = 3;}
      else
      {
        int i = arg[0] - '0';
        byte row = (byte)i;
  
        byte Z = programs[row][0];
        byte stH = programs[row][1];
        byte stM = programs[row][2];
        byte dur = programs[row][3];
        byte sens = programs[row][4];
  
        bt.println(String(Z) + "/" + String(stH) + "/" + String(stM) + "/" + String(dur) + "/" + String(sens));
      }
    }
    else if (commandS == "SP+") //set the program's parameters
    {
      if (arg.length() != 18){error = 3;}
      else 
      {
        byte id;
        byte zone;
        byte stH;
        byte stM;
        byte dur;
        byte sens; 
        
        char * strtokIndx;
        char cArg[arg.length()];
        arg.toCharArray(cArg, arg.length());
  
        strtokIndx = strtok(cArg,"/");      
        id = (byte)atoi(strtokIndx);
        
        strtokIndx = strtok(NULL, "/");
        zone = (byte)atoi(strtokIndx);
        
        strtokIndx = strtok(NULL, "/"); 
        stH = (byte)atoi(strtokIndx); 
        
        strtokIndx = strtok(NULL, "/"); 
        stM = (byte)atoi(strtokIndx);
        
        strtokIndx = strtok(NULL, "/"); 
        dur = (byte)atoi(strtokIndx);
  
        strtokIndx = strtok(NULL, "/"); 
        sens = (byte)atoi(strtokIndx);
  
        programs[id][0] = zone;
        programs[id][1] = stH;
        programs[id][2] = stM;
        programs[id][3] = dur;
        programs[id][4] = sens;
        activeZones[zone-1] = 0; 
        
        bt.println(id + ". program beállítva: /" + String(zone) + "/" + String(stH) + "/" + String(stM) + "/" + String(dur) + "/" + String(sens));  
      }
    }
    else if (commandS == "GZ+") //read the choosen zone's state
    {
      bt.println(String(activeZones[0]) + "/" + String(activeZones[1]) + "/" + String(activeZones[2]) + "/" + String(activeZones[3]) + "/" + String(activeZones[4]) + 
      "/" + String(activeZones[5]) + "/" + String(activeZones[6]) + "/" + String(activeZones[7]) + "/" + String(activeZones[8]));
    }
    else if (commandS == "SZ+") //set the given zone to given state
    {
      if (arg.length() != 5){error = 3;}
      else 
      {
        int id;
        int state;
        bool stateB;
        
        char * strtokIndx;
        char cArg[arg.length()];
        arg.toCharArray(cArg, arg.length());
  
        strtokIndx = strtok(cArg,"/");      
        id = atoi(strtokIndx);
        
        strtokIndx = strtok(NULL, "/");
        state = atoi(strtokIndx);
  
        if (state == 1) {stateB = true;}
        else if (state == 0) {stateB = false;}
        else {error = 3;}
  
         activeZones[id] = stateB;
  
         bt.println(String(activeZones[id]));
      }
    }
    else if (commandS == "DC+") //log out from BT control mode
    {
      BTControll = false; 
      bt.println("LO");
    }
    else if (commandS == "RS+") //read sensor sensitivity 
    { 
      bt.println(String(sensorSens));
    }
    else if (commandS == "SS+") //set sensor sens.
    {
      if (arg.length()<2 || arg.length()>3 ) {error = 3;}
      else 
      {
        int i = arg.toInt();
        sensorSens = i;
  
        resetZones();

        bt.println(String(sensorSens));
      }
    }
    else if (commandS == "GA+") //get season adjustment
    {
      bt.println(String(seasonAdj));
    }
    else if (commandS == "SA+") //set season adjustment
    {
      if (arg.length() != 3) {error = 3;}
      else 
      {
        int i = arg.toInt();
        if (i < 25 || i > 300) {error = 8; Serial.println(String(i));}
        else {seasonAdj = i;}
  
        resetZones();

        bt.println(String(seasonAdj));
      }
    }
    else {error = 4;}
    
    if (error > 0)
    {
      bt.println(String(error));
    }
  }
}

void switchZones()
{
  if (menu[0] != 4)
  {
    for (int i = 0; i < 8; i++)
    {
      if (activeZones[i] == 1) {digitalWrite(i + 30, LOW);}
      else if (activeZones[i] == 0) {digitalWrite(i + 30, HIGH);} 
    }
  }
  else 
  {
    for (int i = 0; i < 8; i++)
    {
      digitalWrite(i + 30, HIGH);
    }
  }
}

void listZones()
{
  lcd.clear();
  lcd.print("Zone ID: " + String(shownZoneID + 1));
  lcd.setCursor(0,1);
  String stat;
  if (activeZones[shownZoneID] == 1){stat = "ACTIVE";}
  else if (activeZones[shownZoneID] == 0){stat = "INACTIVE";}
  lcd.print("Status:"+stat);
}

void modZoneStat()
{
  if (activeZones[shownZoneID] == 0)
  {
    activeZones[shownZoneID] = 1;
  }
  else if (activeZones[shownZoneID] == 1)
  {
    activeZones[shownZoneID] = 0;
  }
  listZones();
}

void checkSensor()
{
  int sVal = analogRead(A0);  
  if (sVal < sensorSens) {sensorOK = 0;}
  else {sensorOK = 1; }
}

void resetZones()
{
  for (byte i = 0; i < zoneNo; i++)
  {
    activeZones[i] = false; 
  }
}

void updateEEPROM()
{
  Serial.println("cycle");
  
  int EEPROMSensorSens;
  EEPROM.get(0, EEPROMSensorSens);
  if (sensorSens == EEPROMSensorSens){}
  else {
   Serial.println("0," + String(sensorSens));
   EEPROM.put(0, sensorSens);
  }

  int EEPROMSeasonAdj;
  EEPROM.get(2, EEPROMSeasonAdj);
  if (seasonAdj == EEPROMSeasonAdj){}
  else {
  Serial.println("2," + String(seasonAdj));
  EEPROM.put(2, seasonAdj);
  }


  int k = 6; //EEPROM address index from 6
  for (int i = 0; i < progRows; i++)
  {
    for (int j = 0; j < 5; j++)
    {
      EEPROM.update(k, programs[i][j]);
      k++;
    }
  }
}
