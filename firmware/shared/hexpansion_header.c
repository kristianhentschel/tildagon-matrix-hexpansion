#include "hexpansion_header.h"

void hexpansion_header_fill_checksum(hexpansion_header_t *header) {
  header->checksum = 0x55;

  for (uint8_t i = 0; i < 30; i++) {
    header->checksum ^= ((uint8_t *)header)[1 + i]; // access the header bytes one by one 
  }
}