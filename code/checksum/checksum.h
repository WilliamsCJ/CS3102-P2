//
// Created by cjdw1 on 30/03/2022.
//

#ifndef CS3102_P2_CHECKSUM_H
#define CS3102_P2_CHECKSUM_H

uint16_t ipv4_header_checksum(void *data, uint32_t size);
uint16_t rdt_checksum(void* data, uint32_t size);

#endif //CS3102_P2_CHECKSUM_H
