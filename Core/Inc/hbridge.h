#ifndef HBRIDGE_H
#define HBRIDGE_H

#include "main.h"

void hbridge_init(GPIO_TypeDef *en_port, uint16_t en_pin, GPIO_TypeDef *dir_port, uint16_t dir_pin);
void coil_enable(uint8_t on);
void coil_set_dir(int dir);

#endif /* HBRIDGE_H */
