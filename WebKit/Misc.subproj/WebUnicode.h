/*	
        WebUnicode.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Private header file.
*/
#ifdef __cplusplus
extern "C" {
#endif
extern void composeLigatures(UniChar *str, unsigned int stringLength);
extern void WebKitInitializeUnicode(void);
extern const unsigned char * const combining_info[];
extern const char * const decimal_info[];
extern const unsigned char * const unicode_info[];
extern const unsigned char * const direction_info[];
extern const unsigned short * const decomposition_info[];
extern const unsigned short decomposition_map[];
extern const unsigned short symmetricPairs[];
extern int symmetricPairsSize;
extern const unsigned short * const case_info[];
extern const unsigned short * const ligature_info[];
extern const unsigned short ligature_map[];
#ifdef __cplusplus
}
#endif

#define WK_CELL(ucs) ((unsigned char) ucs & 0xff)
#define WK_ROW(ucs) ((unsigned char) (ucs>>8)&0xff)

