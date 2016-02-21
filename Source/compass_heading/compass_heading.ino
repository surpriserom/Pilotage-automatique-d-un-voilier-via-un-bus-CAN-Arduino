
const int led = 13;
boolean state = false;

void setup() 
{
  Serial.begin(4800, SERIAL_9N1);
  while(!Serial){};
  
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);
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
    // 89  U2  VW  XY  2Z  Compass heading sent by ST40 compass instrument
    c = 0x89;
    Serial.write9(c, true); //c => charactere emi, true => bit de commande a 1 pour nouvelle trame
   if(Serial.available())
   {
     volatile uint16_t d = Serial.read();
     if(d == c)
     {
       tempo = true;
     }
     else
     {
       tempo = false;
     }
   }
   
   if(timer_led <= time)
   {
     timer_led += (tempo ? 250 : 500);
     digitalWrite(led, (state ? HIGH : LOW));
     state = !state;
   }
  }

}
