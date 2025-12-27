# ifndef TACHIMETER_H
# define TACHIMETER_H
# include <stdint.h>
float tachometer_get_angle_degrees(uint32_t time_micro_seconds);
void init_tachometer(uint32_t stack_size, uint8_t priority, uint8_t core_id);
# endif // TACHIMETER_H