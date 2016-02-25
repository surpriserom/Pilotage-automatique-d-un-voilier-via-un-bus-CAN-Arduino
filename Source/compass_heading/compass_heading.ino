
const int led = 13;
boolean state = false;

void setup() 
{
  volatile 
    uint16_t tab[] = {0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00};
  volatile unsigned int i;
  Serial.begin(4800, SERIAL_9N1);
  while(!Serial){};
  
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  //83  07  XX  00  00  00  00  00  80  00  00 Sent by course computer.
  for(i=0; i< 10; i++)
  {
    Serial.write9(tab[i], false);
  }
}

void loop() 
{
  static unsigned int timer_emit = 0;
  static unsigned int  timer_led = 0;
  static boolean tempo = false;
  volatile unsigned int time = millis();
  
  //on emet toute les seconde et seulement quand le bus est libre
  if(timer_emit <= time && Serial.available() ==  0)
  {
    uint16_t c;
    timer_emit = timer_emit + 1000;
    // 84  U6  VW  XY 0Z 0M RR SS TT  Compass heading  Autopilot course and 
    c = 0x84;
    Serial.write9(c, true); //c => charactere emi, true => bit de commande a 1 pour nouvelle trame
    c = 0x06; //u6
    Serial.write9(c, false);
    c = 0x22;//vw
    Serial.write9(c, false);
    c = 0x00;//xy
    Serial.write9(c, false);
    c = 0x00;//0Z
    Serial.write9(c, false);
    c = 0x04;//0M
    Serial.write9(c, false);
    c = 0xFE;//rr
    Serial.write9(c, false);
    c = 0x00;//ss
    Serial.write9(c, false);
    c = 0x00;//tt
    Serial.write9(c, false);
  }
   if(timer_led <= time)
   {
     timer_led += (tempo ? 250 : 500);
     digitalWrite(led, (state ? HIGH : LOW));
     state = !state;
   }

}
