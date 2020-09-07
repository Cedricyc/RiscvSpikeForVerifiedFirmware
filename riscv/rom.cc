#include "devices.h"

rom_device_t::rom_device_t(std::vector<char> data) : data(data) {}

// semantic:
// input addr,len
// output data of addr of length of len to output array: bytes
bool rom_device_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  if (addr + len > data.size())
    return false;
  memcpy(bytes, &data[addr], len);
  return true;
}
// not ok to store for rom
bool rom_device_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  return false;
}
