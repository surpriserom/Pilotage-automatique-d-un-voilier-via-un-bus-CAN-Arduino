//Programme permettant de lire  le message seatalk,
//nécessite une carte arduino mega 
//la librairie Hardwareserial modifié doit etre installer

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial1.begin(4800, SERIAL_9N1);
}

void loop() {
  // put your main code here, to run repeatedly:
  int c;
  static bool newMessage = false;
  if(Serial1.available())
  {
    c = Serial1.read();
    if(c != 0)
    {
      newMessage = true;
      Serial.print(c,HEX);
      Serial.print(" ");
    }
    if(c == 0 && newMessage == true)
    {
      newMessage=false;
      Serial.print("\n");
    }
  }
}
