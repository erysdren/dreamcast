#pragma once

namespace sh7091::serial {

void init_wait();

void reset_txrx();

void init(uint8_t bit_rate);

} // sh7091::serial
