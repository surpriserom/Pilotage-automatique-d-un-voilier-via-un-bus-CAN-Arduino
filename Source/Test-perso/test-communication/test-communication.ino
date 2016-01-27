


void setup()
{
  Serial.begin(4800,SERIAL_9N1);
}


void loop()
{
  uint8_t c;
  //apparent wind angle => 10 01 XX YY //XXYY/2 angle en degrees
  c = 0x10;
  Serial.write9(c, true); //premier bit du message 9eme bit a 1
  c = 0x01;
  Serial.write9(c, false);
  c = 0x01;
  Serial.write9(c, false);
  c = 0x80;
  Serial.write9(c, false);

 delay(100);
 //apparent wind speed => 11 01 XX 0Y //(XX & 0x7F) + Y/10 Knots
  c = 0x11;
  Serial.write9(c, true); //premier bit du message 9eme bit a 1
  c = 0x01;
  Serial.write9(c, false);
  c = 0x01;
  Serial.write9(c, false);
  c = 0x08;
  Serial.write9(c, false); 
  
  delay(100);
}
