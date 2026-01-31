#include <cstdint>
#include <type_traits>

#include "sh7091/cache.hpp"
#include "sh7091/store_queue_transfer.hpp"
#include "sh7091/serial.hpp"

#include "tinyalloc/tinyalloc.h"

extern uint32_t __text_link_start __asm("__text_link_start");
extern uint32_t __text_link_end __asm("__text_link_end");
extern uint32_t __text_load_start __asm("__text_load_start");

extern uint32_t __data_link_start __asm("__data_link_start");
extern uint32_t __data_link_end __asm("__data_link_end");
extern uint32_t __data_load_start __asm("__data_load_start");

extern uint32_t __rodata_link_start __asm("__rodata_link_start");
extern uint32_t __rodata_link_end __asm("__rodata_link_end");
extern uint32_t __rodata_load_start __asm("__rodata_load_start");

extern uint32_t __ctors_link_start __asm("__ctors_link_start");
extern uint32_t __ctors_link_end __asm("__ctors_link_end");

extern uint32_t __bss_link_start __asm("__bss_link_start");
extern uint32_t __bss_link_end __asm("__bss_link_end");

typedef void(init_t)(void);

static void __attribute__((section(".text.startup.copy"))) copy(uint32_t* start, const uint32_t* end, uint32_t* load)
{
	if (start != load) {
		while (start < end) {
			*start++ = *load++;
		}
	}
}

void interrupts_init();

// 4mb heap
static uint8_t heap[4 * 1024 * 1024] __attribute__((aligned(32)));

extern "C" void __attribute__((section(".text.startup.runtime_init"))) runtime_init()
{
	// init sh7091 cache
	sh7091::cache::init();

	// relocate text (if necessary)
	copy(&__text_link_start, &__text_link_end, &__text_load_start);

	// relocate data (if necessary)
	copy(&__data_link_start, &__data_link_end, &__data_load_start);

	// relocate rodata (if necessary)
	copy(&__rodata_link_start, &__rodata_link_end, &__rodata_load_start);

	// clear BSS
	uint32_t* bss_start = &__bss_link_start;
	uint32_t* bss_end = &__bss_link_end;
	uint32_t bss_length = bss_end - bss_start;
	sh7091::store_queue_transfer::zeroize(bss_start, bss_length, 0);

	// call ctors
	uint32_t* ctors_start = &__ctors_link_start;
	uint32_t* ctors_end = &__ctors_link_end;
	while (ctors_start < ctors_end) {
		((init_t*)(*ctors_start++))();
	}

	// init serial interface
	sh7091::serial::init(0);

	// initialize heap
	ta_init(heap, heap + sizeof(heap), 256, 16, sizeof(void *));

	// initialize interrupts
	interrupts_init();
}
