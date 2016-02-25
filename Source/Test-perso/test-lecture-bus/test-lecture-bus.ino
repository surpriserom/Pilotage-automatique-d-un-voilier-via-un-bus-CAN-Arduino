//Programme permettant de lire  le message seatalk,
//nécessite une carte arduino mega 
//la librairie Hardwareserial modifié doit etre installer

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial2.begin(4800, SERIAL_9N1);
}

void loop() {
  // put your main code here, to run repeatedly:
  int c;
  
  if(Serial2.available())
  {
    c = Serial2.read();
    if(c >= 0x100)
    {
      Serial.println("");
    }
     Serial.print(c,HEX);
     Serial.print(" ");
  }
}
