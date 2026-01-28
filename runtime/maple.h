#ifndef _MAPLE_H_
#define _MAPLE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "runtime.h"

enum : uint32_t {
	MAPLE_PORT_A = 0,
	MAPLE_PORT_B = 1,
	MAPLE_PORT_C = 2,
	MAPLE_PORT_D = 3
};

typedef struct maple_ft0_data {
	uint16_t digital_button;
	uint8_t analog_coordinate_axis[6];
} maple_ft0_data_t;

void maple_init(void);
bool maple_read_ft0(maple_ft0_data_t *data, uint32_t port);

#ifdef __cplusplus
}
#endif
#endif // _MAPLE_H_
