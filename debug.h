#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG
#ifdef __mcoldfire__
#include <stdio.h>
#define dbg(format, arg ...) do { printf("DEBUG: (%s):" format, __FUNCTION__, ##arg); } while (0)
#define out(format, arg ...) do { printf("" format, ##arg); } while (0)
#else
#include "natfeats.h"
#define dbg(format, arg ...) do { nf_printf("DEBUG: (%s):" format, __FUNCTION__, ##arg); } while (0)
#define out(format, arg ...) do { nf_printf("DEBUG: (%s):" format, __FUNCTION__, ##arg); } while (0)
#endif
#else
#define dbg(format, arg ...) /* */
#define out(format, arg ...) do { printf("" format, ##arg); } while (0)
#endif /* DEBUG */

#endif // DEBUG_H
