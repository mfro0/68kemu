#ifndef SV_ACCESS_H
#define SV_ACCESS_H
#include <stdbool.h>

bool is_super(void);
unsigned int read_super(unsigned address, int length);
void write_super(unsigned address, unsigned value);
#endif // SV_ACCESS_H
