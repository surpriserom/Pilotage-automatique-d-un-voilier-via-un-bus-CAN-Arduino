//base sur la demo du shield pour lla reception
//recupere les message sur le bus et compare les identifiants
//affiche les message des identifiant decoder et leur valeur


#include <SPI.h>
#include "mcp_can.h"
//#include "gps_parser.h" //non utilise dans l'exemple
#include "parseCan.h"


const int SPI_CS_PIN = 9;
const int led = 13;
boolean state = false;

ParseCan parser(true);

MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin

void setup()
{
    Serial1.begin(115200);
    
      while (!Serial1) {
        ; // wait for Serial1 port to connect. Needed for Leonardo only
      }
      
  //set the test signal led
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

START_INIT:

    if(CAN_OK == CAN.begin(CAN_500KBPS))                   // init can bus : baudrate = 500k
    {
        Serial1.println("CAN BUS Shield init ok!");
    }
    else
    {
        Serial1.println("CAN BUS Shield init fail");
        Serial1.println("Init CAN BUS Shield again");
        delay(100);
        goto START_INIT;
    }
}

void loop()
{
    unsigned char len = 0;
    char buf[80];
    static unsigned long time = 0;
    unsigned long time2 = 0;
    
    if(CAN_MSGAVAIL == CAN.checkReceive())            // check if data coming
    {
        CAN.readMsgBuf(&len,(unsigned char *) buf);    // read data,  len: data length, buf: data buf

        unsigned char canId = CAN.getCanId();
        
        switch(canId)
        {
          case MSG_GPRMC_LAT_LONG : 
                Serial1.println("MSG_GPRMC_LAT_LONG");
                Serial1.print("lat:");
                Serial1.print(parser.ucharToFloat((unsigned char *)buf,0),6);
                Serial1.print(" long:");
                Serial1.println(parser.ucharToFloat((unsigned char *)buf,4),6);
            break;
          case MSG_GPRMC_VIT_DATE : 
                Serial1.println("MSG_GPRMC_VIT_DATE");
                Serial1.print("vitesse:");
                Serial1.print(parser.ucharToFloat((unsigned char *)buf,0),4);
                Serial1.print("noeud, date:");
                Serial1.print((unsigned char)buf[4],DEC);
                Serial1.print("/");
                Serial1.print((unsigned char)buf[5],DEC);
                Serial1.print("/");
                Serial1.println((unsigned char)buf[6],DEC);
            break;
          case MSG_GYRO_X_Y_Z :
                parser.set_int_GYRO((unsigned char *) buf);
                Serial1.println("MSG_GYRO_X_Y_Z");
                Serial1.print("GYRO x:");
                Serial1.print(parser.get_int_GYRO_X());
                Serial1.print(" y:");
                Serial1.print(parser.get_int_GYRO_Y());
                Serial1.print(" z:");
                Serial1.println(parser.get_int_GYRO_Z());
            break;
          case MSG_IMU_PHI_THETA_PSI :
                Serial1.println("MSG_IMU_PHI_THETA_PSI");
                Serial1.print("IMU phi:");
                Serial1.print(parser.ucharToInt((unsigned char *) buf,0));
                Serial1.print(" theta:");
                Serial1.print(parser.ucharToInt((unsigned char *) buf,2));
                Serial1.print(" psi:");
                Serial1.println(parser.ucharToInt((unsigned char *) buf,4));
            break;
            default: //par defaut on affiche le code hexa que l'on a reçus
              Serial1.print("recus id: ");
              Serial1.println(canId,HEX);
            break;
        }
    }
    
    time2 = millis();
    if(time2 > time )
    {
      //creer un clignotement asynchrone pour avoir une information visuel de debogage
      state = !state;
      digitalWrite(led, (state ? HIGH : LOW));
      time = time + (state ? 500 : 1000);
    }
    
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
