#include <HardwareSerial.h>


void setup()
{
  Serial1.begin(4800,SERIAL_9N1);
}


void loop()
{
  uint16_t c;
  //apparent wind angle => 10 01 XX YY //XXYY/2 angle en degrees
  c = 0x10;
  Serial1.write9(c, true); //premier bit du message 9eme bit a 1
  c = 0x01;
  Serial1.write9(c, false);
  c = 0x01;
  Serial1.write9(c, false);
  c = 0x80;
  Serial1.write9(c, false);

// Serial1.write(0);//pour la lecture du bus on attend un 0
 delay(100);
 //apparent wind speed => 11 01 XX 0Y //(XX & 0x7F) + Y/10 Knots
  c = 0x11;
  Serial1.write9(c, true); //premier bit du message 9eme bit a 1
  c = 0x01;
  Serial1.write9(c, false);
  c = 0x01;
  Serial1.write9(c, false);
  c = 0x08;
  Serial1.write9(c, false); 
  
//  Serial1.write(0); 
  delay(100);
 
  delay(1000);
}
