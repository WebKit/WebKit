#ifndef STDINT_WIN32_H
#define STDINT_WIN32_H

/* This file emulates enough of unix's stdint.h on Windows
   to make WebCore compile */
   
#ifndef WIN32
#error "This stdint.h file should only be compiled under Windows"
#endif
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned __int64 uint64_t;

#ifndef CASSERT
#define CASSERT( exp, name ) typedef int dummy##name [ (exp ) ? 1 : -1 ];
#endif

CASSERT( sizeof(int16_t) == 2, int16_t_is_two_bytes )
CASSERT( sizeof(uint16_t) == 2, uint16_t_is_two_bytes )
CASSERT( sizeof(uint32_t) == 4, uint32_t_is_four_bytes )
CASSERT( sizeof(int32_t) == 4, int32_t_is_four_bytes )

#endif
