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
extern const unsigned char * const _WebDirectionInfo[];
extern const unsigned short * const decomposition_info[];
extern const unsigned short decomposition_map[];
extern const unsigned short symmetricPairs[];
extern int symmetricPairsSize;
extern const unsigned short * const case_info[];
extern const unsigned short * const ligature_info[];
extern const unsigned short ligature_map[];
extern UniChar *shapedString(UniChar *uc, int stringLength, int from, int len, int dir, int *lengthOut);
#ifdef __cplusplus
}
#endif

#define WK_CELL(ucs) ((unsigned char)(ucs))
#define WK_ROW(ucs) ((unsigned char)(ucs>>8))

// surrogate ranges
enum {
    HighSurrogateRangeStart  = 0xD800,
    HighSurrogateRangeEnd    = 0xDBFF,
    LowSurrogateRangeStart   = 0xDC00,
    LowSurrogateRangeEnd     = 0xDFFF
};

#define UnicodeValueForSurrogatePair(h,l) (( ( h - HighSurrogateRangeStart ) << 10 ) + ( l - LowSurrogateRangeStart ) + 0x0010000)
#define HighSurrogatePair(c) (((c - 0x10000)>>10) + 0xd800)
#define LowSurrogatePair(c) (((c - 0x10000)&0x3ff) + 0xdc00)
#define IsHighSurrogatePair(c)  (( c & 0xFC00 ) == HighSurrogateRangeStart )
#define IsLowSurrogatePair(c)  (( c & 0xFC00 ) == LowSurrogateRangeStart )

