#pragma once

#include "sh7091/sh7091.hpp"
#include "sh7091/pref.hpp"
#include "memorymap.h"

namespace sh7091::store_queue_transfer {

static inline void __attribute__((always_inline)) copy(void* out_addr, const void* src, int length)
{
	uint32_t out = reinterpret_cast<uint32_t>(out_addr);
	sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(out);
	sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(out);

	volatile uint32_t* base = (volatile uint32_t*)&store_queue[(out & 0x03ffffe0)];
	const uint32_t* src32 = reinterpret_cast<const uint32_t*>(src);

	length = (length + 31) & ~31; // round up to nearest multiple of 32
	while (length > 0)
	{
		base[0] = src32[0];
		base[1] = src32[1];
		base[2] = src32[2];
		base[3] = src32[3];
		base[4] = src32[4];
		base[5] = src32[5];
		base[6] = src32[6];
		base[7] = src32[7];
		pref(&base[0])
		length -= 32;
		base += 8;
		src32 += 8;
	}
}

static inline void __attribute__((always_inline)) zeroize(void* out_addr, int length, const uint32_t value)
{
	uint32_t out = reinterpret_cast<uint32_t>(out_addr);
	sh7091.CCN.QACR0 = sh7091::ccn::qacr0::address(out);
	sh7091.CCN.QACR1 = sh7091::ccn::qacr1::address(out);

	volatile uint32_t* base = (volatile uint32_t*)&store_queue[(out & 0x03ffffe0)];

	length = (length + 31) & ~31; // round up to nearest multiple of 32
	while (length > 0) {
		base[0] = value;
		base[1] = value;
		base[2] = value;
		base[3] = value;
		base[4] = value;
		base[5] = value;
		base[6] = value;
		base[7] = value;
		pref(&base[0]);
		length -= 32;
		base += 8;
	}
}

} // namespace sh7091::store_queue_transfer
