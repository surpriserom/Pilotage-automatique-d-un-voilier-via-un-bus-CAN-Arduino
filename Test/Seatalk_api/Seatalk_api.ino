#include "SeaTalk.h"

const int led = 13;
boolean state = false;

SeaTalk_API seatalk_api;

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
  c = 0x83;
  Serial1.write9(c, true);
  c = Serial2.read();
  Serial.print(c,HEX);
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
	  /*
      volatile uint16_t c_lecture;
      c_lecture = Serial2.read();
		if (c_lecture >= 0x100)
		{
			Serial.println("");
		}
      Serial.print(c_lecture,HEX);
      Serial.print(' ');
	  */
	  char buffer[23];
	 seatalk_api. read_seatalk_input(&Serial2,buff);
		Serial.write(buff);
		Serial.println(" ");
		if(buff[0] == SeaTalk_Heading_Rudder)
		{
			Serial.write("SeaTalk_Heading_Rudder");
			Serial.println("");
		}
		
  }
  //on emet toute les seconde et seulement quand le bus est libre
  if(timer_emit <= time && Serial2.available() ==  0)
  {
    uint16_t c;
    timer_emit = timer_emit + 1000;
    seatalk_api.send_heading_rudder(&Serial1, &Serial2, 68, -2);
    Serial.println(" ");  
  }
  
   if(timer_led <= time)
   {
     timer_led += (tempo ? 250 : 500);
     digitalWrite(led, (state ? HIGH : LOW));
     state = !state;
   }

}
