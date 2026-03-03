#ifndef A31301_H
#define A31301_H

int16_t getZ(uint8_t address);
int16_t getX(uint8_t address); 
int16_t getY(uint8_t address);
uint8_t Request_info(uint8_t address, uint8_t registerAddress);
bool presence_pion_blanc(uint8_t num_capt);
bool presence_pion_noir(uint8_t num_capt);
#endif
