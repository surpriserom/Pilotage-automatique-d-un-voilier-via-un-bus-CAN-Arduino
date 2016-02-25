
const int led = 13;
boolean state = false;

void setup() 
{
  volatile 
    uint16_t tab[] = {0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00};
  volatile unsigned int i;
  Serial.begin(115200);
  Serial1.begin(4800, SERIAL_9N1);
  Serial2.begin(4800, SERIAL_9N1);
  while(!Serial1){};
  
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

//on vérifie que le port est libre
 if(Serial2.available() ==  0)
 {
  volatile uint16_t c;
  //83  07  XX  00  00  00  00  00  80  00  00 Sent by course computer.
  for(i=0; i< 10; i++)
  {
    Serial1.write9(tab[i], false);
    c = Serial2.read();
    Serial.print(c,HEX);
  }
 }
 Serial.println("");
}

void loop() 
{
  static unsigned int timer_emit = 0;
  static unsigned int  timer_led = 0;
  static boolean tempo = false;
  volatile unsigned int time = millis();

  if(Serial2.available())
  {
      volatile uint16_t c_lecture;
      c_lecture = Serial2.read();
      Serial.print(c_lecture,HEX);
  }
  //on emet toute les seconde et seulement quand le bus est libre
  if(timer_emit <= time && Serial2.available() ==  0)
  {
    uint16_t c_ecriture, c_lecture;
    timer_emit = timer_emit + 1000;
    // 84  U6  VW  XY 0Z 0M RR SS TT  Compass heading  Autopilot course and 
    c_ecriture = 0x84;
    Serial1.write9(c_ecriture, true); //c => charactere emi, true => bit de commande a 1 pour nouvelle trame
      c_ecriture = 0x06; //u6
      Serial1.write9(c_ecriture, false);
        c_ecriture = 0x22;//vw
        Serial1.write9(c_ecriture, false);
          c_ecriture = 0x00;//xy
          Serial1.write9(c_ecriture, false);
            c_ecriture = 0x00;//0Z
            Serial1.write9(c_ecriture, false);
                c_ecriture = 0x00;//0M -< 0x4 set allarme
                Serial1.write9(c_ecriture, false);
                  c_ecriture = 0xFE;//rr
                  Serial1.write9(c_ecriture, false);
                    c_ecriture = 0x00;//ss
                    Serial1.write9(c_ecriture, false);
                      c_ecriture = 0x00;//tt
                      Serial1.write9(c_ecriture, false);
    Serial.println(" ");  
  }
  
   if(timer_led <= time)
   {
     timer_led += (tempo ? 250 : 500);
     digitalWrite(led, (state ? HIGH : LOW));
     state = !state;
   }

}
