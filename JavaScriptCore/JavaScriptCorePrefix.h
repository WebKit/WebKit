#ifdef __cplusplus
#define new ("if you use new/delete make sure to include config.h at the top of the file"()) 
#define delete ("if you use new/delete make sure to include config.h at the top of the file"()) 
#endif

#ifdef __cplusplus
#define NULL __null
#else
#define NULL ((void *)0)
#endif

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>

#ifdef __cplusplus

// Work around bug 3553309 by re-including <ctype.h>.
#include <cctype>
#define isalnum(c)      __istype((c), (_CTYPE_A|_CTYPE_D))
#define isalpha(c)      __istype((c), _CTYPE_A)
#define iscntrl(c)      __istype((c), _CTYPE_C)
#define isdigit(c)      __isctype((c), _CTYPE_D)	/* ANSI -- locale independent */
#define isgraph(c)      __istype((c), _CTYPE_G)
#define islower(c)      __istype((c), _CTYPE_L)
#define isprint(c)      __istype((c), _CTYPE_R)
#define ispunct(c)      __istype((c), _CTYPE_P)
#define isspace(c)      __istype((c), _CTYPE_S)
#define isupper(c)      __istype((c), _CTYPE_U)
#define isxdigit(c)     __isctype((c), _CTYPE_X)	/* ANSI -- locale independent */
#define tolower(c)      __tolower(c)
#define toupper(c)      __toupper(c)

#include <list>
#include <typeinfo>

#endif
