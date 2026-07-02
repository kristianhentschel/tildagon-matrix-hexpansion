#ifndef HEXPANSION_HEADER_H
#define HEXPANSION_HEADER_H
#include <stdint.h>

// See https://tildagon.badge.emfcamp.org/hexpansions/eeprom/#eeprom-format
typedef struct hexpansion_header {
  volatile char magic[4]; // ASCII "THEX"
  volatile char manifest_version[4]; // ASCII "2024"
  volatile struct hexpansion_header_filesystem_info {
    volatile uint16_t offset;
    volatile uint16_t page_size;
    volatile uint32_t total_size;
  } filesystem_info;
  volatile uint16_t vendor_id;
  volatile uint16_t product_id;
  volatile uint16_t unique_id;
  volatile char friendly_name[9];
  volatile uint8_t checksum;
} hexpansion_header_t;

void hexpansion_header_fill_checksum(hexpansion_header_t *header);
#endif