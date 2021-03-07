#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG
#include <stdio.h>
#define dbg(format, arg ...) do { printf("DEBUG: (%s):" format, __FUNCTION__, ##arg); } while (0)
#define out(format, arg ...) do { printf("" format, ##arg); } while (0)
#else
#define dbg(format, arg ...) /* */
#define out(format, arg ...) do { printf("" format, ##arg); } while (0)
#endif /* DEBUG */

#endif // DEBUG_H
