
#include "timer.h"

#include "sh7091/sh7091.hpp"
#include "sh7091/sh7091_bits.hpp"

static uint32_t timer_type = TIMER_TYPE_NONE;

void timer_init(uint32_t type, uint32_t start_value)
{
	using namespace sh7091;
	using sh7091::sh7091;

	if (type < TIMER_TYPE_COUNT_DOWN || type > TIMER_TYPE_COUNT_UP)
		return;

	timer_type = type;

	sh7091.TMU.TSTR = 0; // stop all timers
	sh7091.TMU.TOCR = tmu::tocr::tcoe::tclk_is_external_clock_or_input_capture;
	sh7091.TMU.TCR0 = tmu::tcr0::tpsc::p_phi_256; // 256 / 50MHz = 5.12 μs ; underflows in ~1 hour
	sh7091.TMU.TCOR0 = 0xFFFFFFFF;
	sh7091.TMU.TCNT0 = timer_type == TIMER_TYPE_COUNT_UP ? 0xFFFFFFFF - start_value : start_value;
	sh7091.TMU.TSTR = tmu::tstr::str0::counter_start;
}

uint32_t timer_read(void)
{
	using namespace sh7091;
	using sh7091::sh7091;

	switch (timer_type)
	{
		case TIMER_TYPE_COUNT_DOWN: return sh7091.TMU.TCNT0;
		case TIMER_TYPE_COUNT_UP: return 0xFFFFFFFF - sh7091.TMU.TCNT0;
		default: return 0;
	}
}

uint32_t timer_ticks_per_second(void)
{
	using namespace sh7091;
	using sh7091::sh7091;

	switch (sh7091.TMU.TCR0 & tmu::tcr0::tpsc::bit_mask)
	{
		case tmu::tcr0::tpsc::p_phi_4: return (4.0f / 50.0f) * 1e+6f;
		case tmu::tcr0::tpsc::p_phi_16: return (16.0f / 50.0f) * 1e+6f;
		case tmu::tcr0::tpsc::p_phi_64: return (64.0f / 50.0f) * 1e+6f;
		case tmu::tcr0::tpsc::p_phi_256: return (256.0f / 50.0f) * 1e+6f;
		case tmu::tcr0::tpsc::p_phi_1024: return (1024.0f / 50.0f) * 1e+6f;
		case tmu::tcr0::tpsc::rtc_output:
		case tmu::tcr0::tpsc::external:
		default: return 1;
	}
}
