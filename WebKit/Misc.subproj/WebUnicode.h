/*	
        WebUnicode.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Private header file.
*/
extern void WebKitInitializeUnicode(void);

extern const unsigned char * const *combining_info;
extern const char * const *decimal_info;
extern const unsigned char *const *unicode_info;
extern const unsigned char * const *direction_info;
extern const unsigned short * const *decomposition_info;
extern const unsigned short *decomposition_map;
extern const unsigned short *symmetricPairs;
extern int symmetricPairsSize;
extern const unsigned short * const *case_info;
