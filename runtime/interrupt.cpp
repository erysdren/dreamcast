#include <cstdint>

#include "runtime.h"

#include "sh7091/serial.hpp"

#include "sh7091/sh7091.hpp"
#include "sh7091/sh7091_bits.hpp"
#include "sh7091/vbr.hpp"
#include "systembus/systembus.hpp"

void vbr100()
{
	using sh7091::sh7091;
	print_cstring("vbr100\n");
	print_cstring("expevt ");
	printf("0x%08x", sh7091.CCN.EXPEVT);
	print_char('\n');
	print_cstring("intevt ");
	printf("0x%08x", sh7091.CCN.INTEVT);
	print_char('\n');
	print_cstring("tra ");
	printf("0x%08x", sh7091.CCN.TRA);
	print_char('\n');
	uint32_t spc;
	uint32_t ssr;
	asm volatile ("stc spc,%0"
			: "=r" (spc)
			);
	asm volatile ("stc ssr,%0"
			: "=r" (ssr)
			);

	print_cstring("spc ");
	printf("0x%08x", spc);
	print_char('\n');
	print_cstring("ssr ");
	printf("0x%08x", ssr);
	print_char('\n');
	while (1);
}

void vbr400()
{
	print_cstring("vbr400\n");
	while (1);
}

void vbr600()
{
	print_cstring("vbr600\n");
	while (1);
}

void interrupts_init()
{
	using sh7091::sh7091;
	using namespace sh7091;
	using systembus::systembus;

	print_cstring("main\n");

	uint32_t vbr = reinterpret_cast<uint32_t>(&__vbr_link_start) - 0x100;

	systembus.IML2NRM = 0;
	systembus.IML2ERR = 0;
	systembus.IML2EXT = 0;

	systembus.IML4NRM = 0;
	systembus.IML4ERR = 0;
	systembus.IML4EXT = 0;

	systembus.IML6NRM = 0;
	systembus.IML6ERR = 0;
	systembus.IML6EXT = 0;

	sh7091.CCN.INTEVT = 0;
	sh7091.CCN.EXPEVT = 0;

	uint32_t zero = 0;
	asm volatile ("ldc %0,spc"
			:
			: "r" (zero));

	asm volatile ("ldc %0,ssr"
			:
			: "r" (zero));

	asm volatile ("ldc %0,vbr"
			:
			: "r" (vbr));


	uint32_t sr;
	asm volatile ("stc sr,%0"
			: "=r" (sr));

	print_cstring("sr ");
	printf("0x%08x", sr);
	print_char('\n');

	sr &= ~sh::sr::bl; // BL
	sr |= sh::sr::imask(15); // imask

	print_cstring("sr ");
	printf("0x%08x", sr);
	print_char('\n');

	asm volatile ("ldc %0,sr"
			:
			: "r" (sr));

	print_cstring("vbr ");
	printf("0x%08x", vbr);
	print_char('\n');
	print_cstring("vbr100 ");
	printf("0x%08x", reinterpret_cast<uint32_t>(&vbr100));
	print_char('\n');

	uint32_t r15;
	asm volatile ("mov r15,%0" : "=r" (r15) );
	print_cstring("r15 ");
	printf("0x%08x", r15);
	print_char('\n');
}
