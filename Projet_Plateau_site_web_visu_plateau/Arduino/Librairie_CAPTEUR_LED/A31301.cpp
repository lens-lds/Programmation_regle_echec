#include "Arduino.h"
#include "A31301.h"
#include <Wire.h>
#include "config.h"
//uint8_t A31301_ADDR[16]={0x04,0x03,0x02,0x63,0x05,0x06,0x07,0x08,0x0C,0x0B,0x0A,0x09,0x0D,0x0E,0x0F,0x10};    // carte n°1 (A31301_ADDR[4] = 0x63 et A31301_ADDR[16] = 0x10)
//int8_t A31301_ADDR[16]={0x14,0x13,0x12,0x11,0x15,0x16,0x17,0x18,0x1C,0x1B,0x1A,0x19,0x1D,0x1E,0x1F,0x20};     //  carte n°2
//uint8_t A31301_ADDR[16]={0x24,0x23,0x22,0x21,0x25,0x26,0x27,0x28,0x2C,0x2B,0x2A,0x29,0x2D,0x2E,0x2F,0x30};    //  carte n°3
//uint8_t A31301_ADDR[16]={0x34,0x33,0x32,0x31,0x35,0x36,0x37,0x38,0x3C,0x3B,0x3A,0x39,0x3D,0x3E,0x41,0x40};      //  carte n°4

//int16_t SEUIL_CAPT[16]={-10,20,38,-25,-60,-35,2,-4,-4,7,3,-16,9,-10,-7,12};             // carte n°1
///int16_t SEUIL_CAPT[16]={-15,-15,-18,-14,-46,-26,0,2,-12,-18,-6,6,-10,-5,-40,0};         // carte n°2
//int16_t SEUIL_CAPT[16]={-2,-30,-8,-36,-25,-13,-5,-15,8,-12,8,-38,8,-8,4,-8};            // carte n°3
//int16_t SEUIL_CAPT[16]={-12,-16,-28,-24,-26,-20,-10,-12,-15,-25,-4,-2,-6,-20,0,-18};      // carte n°4

// X-axis register
#define REGISTER_MSB_X 0x1E
#define REGISTER_LSB_X 0x1F

// Y-axis register
#define REGISTER_MSB_Y 0x20
#define REGISTER_LSB_Y 0x21
// Z-axis register
#define REGISTER_MSB_Z 0x22
#define REGISTER_LSB_Z 0x23

/** 
 * \brief Read and return a byte from the given register 
 *
 * \param address A31301 sensor I²C address
 * \param registerAddress Register to read address
 * \return The byte read in the register
 */
uint8_t Request_info(uint8_t address, uint8_t registerAddress) {
  // Configure the I²C write/read
  // 1. Write the register address
  // 2. Read the register content
  Wire.beginTransmission(address);
  // Set register address
  Wire.write(registerAddress);
  Wire.endTransmission(false);  
  // Read register content
  Wire.requestFrom(address, 1);

  // Return the byte read at the register address
  return Wire.read();
}
/*
 * \brief Read and return the X-axis magnitude
 *
 * \param address A31301 sensor I²C address
 * \return signed X-axis magnitude on 16 bits
 */
int16_t getX(uint8_t address) {
    address=A31301_ADDR[address];
  // Read MSB and LSB for X-axis
  uint8_t msb = Request_info(address, REGISTER_MSB_X);
  uint8_t lsb = Request_info(address, REGISTER_LSB_X);

  // Merge MSB and LSB on 15 bits
  int16_t combined = ((msb & 0x7F) << 8) | lsb;

  // Check the 15th bit to duplicate the sign on the 16 bit (0=positive / 1=negative)
  if (combined & 0x4000) combined |= 0x8000; 

  return combined;
}


/** 
 * \brief Read and return the Y-axis magnitude
 *
 * \param address A31301 sensor I²C address
 * \return signed Y-axis magnitude on 16 bits
 */
int16_t getY(uint8_t address) {
    address=A31301_ADDR[address];
  // Read MSB and LSB for Y-axis
  uint8_t msb = Request_info(address, REGISTER_MSB_Y);
  uint8_t lsb = Request_info(address, REGISTER_LSB_Y);

  // Merge MSB and LSB on 15 bits
  int16_t combined = ((msb & 0x7F) << 8) | lsb;

  // Check the 15th bit to duplicate the sign on the 16 bit (0=positive / 1=negative)
  if (combined & 0x4000) combined |= 0x8000; 

  return combined;
}

/** 
 * \brief Read and return the Z-axis magnitude
 *
 * \param address A31301 sensor I²C address
 * \return signed Z-axis magnitude on 16 bits
 */
int16_t getZ(uint8_t address) {
    address=A31301_ADDR[address];
  // Read MSB and LSB for Z-axis
  uint8_t msb = Request_info(address, REGISTER_MSB_Z);
  uint8_t lsb = Request_info(address, REGISTER_LSB_Z);

  // Merge MSB and LSB on 15 bits
  int16_t combined = ((msb & 0x7F) << 8) | lsb;

  // Check the 15th bit to duplicate the sign on the 16 bit (0=positive / 1=negative)
  if (combined & 0x4000) combined |= 0x8000; 

  return combined;
}


bool presence_pion_blanc(uint8_t num_capt){
  if (getZ(num_capt)<(SEUIL_CAPT[num_capt]-40)){
    return true;
  }
  else{
    return false;
  }
}

bool presence_pion_noir(uint8_t num_capt){
  if (getZ(num_capt)>(SEUIL_CAPT[num_capt]+40)){
    return true;
  }
  else{
    return false;
  }
}




