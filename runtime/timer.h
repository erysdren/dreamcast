#ifndef _TIMER_H_
#define _TIMER_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

enum : uint32_t {
	TIMER_TYPE_NONE,
	TIMER_TYPE_COUNT_DOWN,
	TIMER_TYPE_COUNT_UP
};

void timer_init(uint32_t type, uint32_t start_value);
uint32_t timer_read(void);
uint32_t timer_ticks_per_second(void);

#ifdef __cplusplus
}
#endif
#endif // _TIMER_H_
