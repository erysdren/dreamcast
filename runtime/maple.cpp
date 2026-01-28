
#include "maple.h"

#include "maple/maple_bits.hpp"
#include "maple/maple_bus_bits.hpp"
#include "maple/maple_bus_commands.hpp"
#include "maple/maple_bus_ft0.hpp"
#include "maple/maple_bus_ft6.hpp"
#include "maple/maple_bus_ft6_scan_code.hpp"
#include "maple/maple_host.hpp"

static uint8_t maple_send_buf[1024] __attribute__((aligned(32)));
static uint8_t maple_recv_buf[1024] __attribute__((aligned(32)));

static inline uint32_t align_32byte(uint32_t mem)
{
	return (mem + 31) & ~31;
}

static void ports_for_num(uint32_t port, uint32_t& host_instruction_port, uint32_t& port_select_port)
{
	using namespace maple;

	switch (port)
	{
		case 0:
			host_instruction_port = host_instruction::port_select::a;
			port_select_port = ap::port_select::a;
			break;
		case 1:
			host_instruction_port = host_instruction::port_select::b;
			port_select_port = ap::port_select::b;
			break;
		case 2:
			host_instruction_port = host_instruction::port_select::c;
			port_select_port = ap::port_select::c;
			break;
		case 3:
			host_instruction_port = host_instruction::port_select::d;
			port_select_port = ap::port_select::d;
			break;
	}
}

static void maple_device_request(uint8_t* send_buf, uint8_t* recv_buf, uint32_t port, bool end)
{
	using namespace maple;

	uint32_t host_instruction_port = 0;
	uint32_t port_select_port = 0;

	ports_for_num(port, host_instruction_port, port_select_port);

	using host_command = host_command<device_request::data_fields>;

	host_command* command = (host_command*)send_buf;

	command->host_instruction = host_instruction::pattern::normal
									| host_instruction_port
									| host_instruction::transfer_length(sizeof(command->data) / 4);

	if (end)
		command->host_instruction |= host_instruction::end_flag;

	command->receive_data_address = ((uint32_t)recv_buf) & receive_data_address::mask;

	command->header.command_code = device_request::command_code;

	command->header.destination_ap = ap::de::device
									| port_select_port;

	command->header.source_ap = ap::de::port
								| port_select_port;

	command->header.data_size = sizeof(command->data) / 4;
}

static void maple_get_condition(uint8_t* send_buf, uint8_t* recv_buf, uint32_t port, bool end)
{
	using namespace maple;

	uint32_t host_instruction_port = 0;
	uint32_t port_select_port = 0;

	ports_for_num(port, host_instruction_port, port_select_port);

	using host_command = host_command<get_condition::data_fields>;

	host_command* command = (host_command*)send_buf;

	command->host_instruction = host_instruction::pattern::normal
									| host_instruction_port
									| host_instruction::transfer_length(sizeof(command->data) / 4);

	if (end)
		command->host_instruction |= host_instruction::end_flag;

	command->receive_data_address = ((uint32_t)recv_buf) & receive_data_address::mask;

	command->header.command_code = get_condition::command_code;

	command->header.destination_ap = ap::de::device
									| port_select_port;

	command->header.source_ap = ap::de::port
								| port_select_port;

	command->header.data_size = sizeof(command->data) / 4;

	command->data.function_type = __builtin_bswap32(0x00000001);
}

// system bus interface
volatile uint32_t* ISTNRM = (volatile uint32_t*)(0xa05f6800 + 0x100);

#define ISTNRM__END_OF_DMA_MAPLE_DMA (1 << 12)

// Maple interface
volatile uint32_t* MDSTAR = (volatile uint32_t*)(0xa05f6c00 + 0x04);
volatile uint32_t* MDTSEL = (volatile uint32_t*)(0xa05f6c00 + 0x10);
volatile uint32_t* MDEN   = (volatile uint32_t*)(0xa05f6c00 + 0x14);
volatile uint32_t* MDST   = (volatile uint32_t*)(0xa05f6c00 + 0x18);
volatile uint32_t* MSYS   = (volatile uint32_t*)(0xa05f6c00 + 0x80);
volatile uint32_t* MDAPRO = (volatile uint32_t*)(0xa05f6c00 + 0x8c);

static void maple_dma_start(void* command_buf)
{
	using namespace maple;

	// disable Maple DMA and abort any possibly-in-progress transfers
	*MDEN = mden::dma_enable::abort;
	while ((*MDST & mdst::start_status::bit_mask) != 0);

	// clear Maple DMA end status
	*ISTNRM = ISTNRM__END_OF_DMA_MAPLE_DMA;

	// 20nsec * 0xc350 = 1ms
	uint32_t one_msec = 0xc350;
	// set the Maple bus controller timeout and transfer rate
	*MSYS = msys::time_out_counter(one_msec)
			| msys::sending_rate::_2M;

	// MDAPRO controls which (system memory) addresses are considered valid for
	// Maple DMA. An attempt to use an address outside of this range will cause an
	// error interrupt in from the Maple DMA unit.
	//
	// 0x40 through 0x7F allows for transfers from 0x0c000000 to 0x0fffffff
	// System Memory is 0x0c000000 through 0x0cffffff (16MB)
	//
	// It is probably possible to do strange things such as use texture memory as
	// a Maple DMA recieve buffer, but TOP_ADDRESS would need to be lowered
	// accordingly.
	*MDAPRO = mdapro::security_code
			| mdapro::top_address(0x40)
			| mdapro::bottom_address(0x7f);

	// SOFTWARE_INITIATION allows a Maple DMA transfer to be initiated by a write
	// to the MDST register.
	*MDTSEL = mdtsel::trigger_select::v_blank_initiation;

	// the Maple DMA start address must be 32-byte aligned
	*MDSTAR = mdstar::table_address((uint32_t)command_buf);

	// re-enable Maple DMA
	*MDEN = mden::dma_enable::enable;
}

static uint8_t *maple_recv_buf_port_address[4] = {NULL, NULL, NULL, NULL};

void maple_init(void)
{
	using namespace maple;

	uint8_t* send_buf = maple_send_buf;
	uint8_t* recv_buf = maple_recv_buf;

	memset(maple_recv_buf, 0, sizeof(maple_recv_buf));

	// setup device requests for all 4 ports
	for (int i = 0; i < 4; i++)
	{
		maple_device_request(send_buf, recv_buf, i, false);
		send_buf += sizeof(host_command<device_request::data_fields>);
		recv_buf += sizeof(host_response<device_status::data_fields>);
	}

	// setup get condition requests for all 4 ports
	for (int i = 0; i < 4; i++)
	{
		maple_get_condition(send_buf, recv_buf, i, i == 3 ? true : false);
		maple_recv_buf_port_address[i] = recv_buf;
		send_buf += sizeof(host_command<get_condition::data_fields>);
		recv_buf += sizeof(host_response<data_transfer<ft0::data_transfer::data_format>::data_fields>);
	}

	// write back operand cache blocks for command buffer prior to starting DMA
	for (uint32_t i = 0; i < align_32byte(sizeof(maple_send_buf)) / 32; i++)
	{
		asm volatile (
			"ocbwb @%0"
			:
			: "r" ((uint32_t)(&maple_send_buf[32 * i]))
		);
	}

	// start dma
	maple_dma_start(maple_send_buf);

	// invalidate operand cache block for recv buffer, prior to returning to the caller
	for (uint32_t i = 0; i < align_32byte(sizeof(maple_recv_buf)) / 32; i++)
	{
		asm volatile (
			"ocbi @%0"
			:
			: "r" ((uint32_t)(&maple_recv_buf[32 * i]))
		);
	}
}

bool maple_read_ft0(maple_ft0_data_t *data, uint32_t port)
{
	using namespace maple;
	using host_response = host_response<data_transfer<ft0::data_transfer::data_format>::data_fields>;

	if (port >= 4)
		return false;

	host_response* response = (host_response*)maple_recv_buf_port_address[port];

	if (__builtin_bswap32(response->data.function_type) != 1)
		return false;

	data->digital_button = response->data.data.digital_button;
	for (int i = 0; i < 6; i++)
		data->analog_coordinate_axis[i] = response->data.data.analog_coordinate_axis[i];

	return true;
}
