// Arduino program to write Seatalk test messages using HardwareSerial
// Source code: http://www.arribasail.com/src/SeatalkTest
// Notes:
//   SeaTalk electrical signals are non-inverted (i.e., TTL-like), so OK to use TX output as is.
//   SeaTalk uses 9 bits per byte, so the 9-bit version of Hardware Serial is required.
//   Tested only on Arduino Uno and Arduino MEGA 2560.
//   Seatalk Reference: http://www.thomasknauf.de/rap/seatalk2.htm
//
// Author: Alan Noble
// Created: 23 August 2015 (with messages DBT, MSU, STW, SOG, HDG, AWA and AWS)

#include <string.h>
#include <ctype.h>
#include <stdarg.h>

// macros
#define M_TO_FT(m) ((m) * 3.2808399)

// constants
const char * progname = "SeatalkTest v1";

namespace seatalk {

  // Seatalk field types
  // NB: Nibbles must be followed by a second nibble & order is most significant nibble first.
  enum FieldType {
    Null =   0,     // terminates a datagram specification spec
    Cmd =    1,     // command byte
    Att =    2,     // attribute byte
    Nibble = 3,     // data nibble (4 bits)
    Byte =   4,     // data byte (8 bits)
    Int =    5,     // data int (16 bits)
    Int10 =  6      // data int scaled by 10 (NOT 10 ints)
  };

  // corresponding field sizes in nibbles (to calculate datagram size)
  float fieldSize[] = { 0, 2, 2, 1, 2, 4, 4 };

  // Seatalk datagrams are specified as arrays of alternating field types and field values, terminated by seatalk::Null
 
  // Depth below Transducer (DBT): 00 02 YZ XX XX
  //   Y = 0 for feet (default), 4 for meters
  //   Z = 1 for shallow depth alarm, 4 for defective transducter
  //   XXXX = depth*10
  uint16_t dbtSpec[] = { Cmd, 0x00, Att, 0x02, Nibble, 0, Nibble, 0, Int10, 0, Null };
  #define DBT_Y 5
  #define DBT_Z 7
  #define DBT_DEPTH 9

  // Mileage and Speed Units (MSU): 24 02 00 00 XX
  //   XX: 00=nm/knots, 06=mile/mph, 86=km/kmh
  uint16_t msuSpec[] = { Cmd, 0x24, Att, 0x02, Byte, 0, Byte, 0, Byte, 0, Null };
  #define MSU_UNITS 9
  
  // Speed through Water (STW): 20 01 XX XX
  //   XXXX = speed*10
  uint16_t stwSpec[] = { Cmd, 0x20, Att, 0x01, Int10, 0, Null };
  #define STW_SPEED 5

  // Speed over Ground (SOG): 52 01 XX XX
  //   XXXX = speed*10
  uint16_t sogSpec[] = { Cmd, 0x52, Att, 0x01, Int10, 0, Null };
  #define SOG_SPEED 5

  // Compass Heading (HDG): 89 U2 VW XY 2Z
  //   heading = (U & 0x3) * 90 + (VW & 0x3F) * 2 + (U & 0xC) / 2
  uint16_t hdgSpec[] = { Cmd, 0x89, Nibble, 0, Nibble, 0x2, Byte, 0, Byte, 0, Nibble, 0x2, Nibble, 0, Null };
  #define HDG_U 3
  #define HDG_VW 7
  #define HDG_XY 9
  #define HDG_Z 13
  
  // Apparent Wind Angle (AWA): 10 01 XX YY
  uint16_t awaSpec[] = { Cmd, 0x10, Att, 0x01, Byte, 0, Byte, 0, Null };
  #define AWA_XX 5
  #define AWA_YY 7

  // Apparent Wind Speed (AWS): 11 01 XX 0Y 
  //   (XX & 0x7F) + YY/10 = speed
  //   XX&0x80=0           = knots (default)
  //   XX&0x80=0x80        = meters/second
  uint16_t awsSpec[] = { Cmd, 0x11, Att, 0x01, Byte, 0, Nibble, 0, Nibble, 0, Null };
  #define AWS_XX 5
  #define AWS_Y 9

  // functions
  int sizeofDatagram(uint16_t data[]) {
    // compute size of datagram in nibbles; divide by 2 for bytes
    int size = 0;
    for (int ii = 0; ; ii += 2) {
      switch (data[ii]) {
      case Null:
        return size;
      default:
        size += fieldSize[data[ii]];
      }
    }
  }

  boolean writeDatagram(uint16_t data[]) {
    // send a datagram; last element of data must be seatalk::Null
    for (int ii = 0; ; ii += 2) {
      switch (data[ii]) {
      case Null:
        return true;

      case Cmd:
        // NB: set command bit
        Serial.write9(data[ii + 1] | 0x0100);     
        break;

      case Nibble:
        // NB: nibbles must come in pairs, with most significant first
        if (data[ii + 2] != Nibble) return false;
        Serial.write9((data[ii + 1] << 4) | (data[ii + 3] & 0x0f));
        ii += 2; // skip over next nibble which we've already consumed
        break;

      case Att:
      case Byte:
        Serial.write9(data[ii + 1] & 0x00ff);     // mask high byte
        break;

      case Int:
      case Int10:
        Serial.write9(data[ii + 1] & 0x00ff);    // LSB first
        Serial.write9(data[ii + 1]  >> 8);       // MSB second
        break;

      default:
  return false;
      }
    }
    return true;
  }

} // end seatalk namespace
    
void setup() {
  Serial.begin(4800, true); // enable 9 bit mode
  delay(2000);
}

// simulated depths & speeds
float Depth[] = {8.1, 7.9, 7.7, 7.8, 8.0}; // meters
float Speed[] = {6.2, 6.4, 6.6, 6.3, 6.1}; // knots
int Cnt = 0;

void loop () {
  
  
  // NB: some delay is required after each datagram to allow the Seatalk bus to return HIGH

  // Mileage units = nm; Speed units = knots
  seatalk::msuSpec[MSU_UNITS] = 0;
  seatalk::writeDatagram(seatalk::msuSpec);
  delay(100);
  
  seatalk::dbtSpec[DBT_Y] = 4;
  seatalk::dbtSpec[DBT_DEPTH] = (uint16_t)(M_TO_FT(Depth[Cnt]) * 10);
  seatalk::writeDatagram(seatalk::dbtSpec);
  delay(100);

  seatalk::stwSpec[STW_SPEED] = (uint16_t)(Speed[Cnt] * 10);           
  seatalk::writeDatagram(seatalk::stwSpec);
  delay(100);

  // HDG = 291 degrees
  // (HDG_U & 0x3) * 90 + (HDG_VW & 0x3F) * 2 + (HDG_U & 0xC) / 2 = 3 * 90 + 10 * 2 + 2/2) = 291
  seatalk::hdgSpec[HDG_U] = 0xB;
  seatalk::hdgSpec[HDG_VW] = 10;
  seatalk::writeDatagram(seatalk::hdgSpec);
  delay(100);
  
  // AWA = 60 degrees, stb
  seatalk::awaSpec[AWA_XX] = 0;
  seatalk::awaSpec[AWA_YY] = (60 * 2);
  seatalk::writeDatagram(seatalk::awaSpec);
  delay(100);
  
  // AWS = 9.5 knots
  seatalk::awsSpec[AWS_XX] = 9;
  seatalk::awsSpec[AWS_Y] = 5;
  seatalk::writeDatagram(seatalk::awsSpec);
  
  Cnt = (Cnt + 1) % 5;
  delay(random(1000, 5000));
}

