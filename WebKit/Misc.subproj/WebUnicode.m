/*	
        WebUnicode.m
	Copyright 2001, 2002, Apple Computer, Inc.
*/
#import <WebKit/WebUnicode.h>
#import <WebCore/WebCoreUnicode.h>


static int _unicodeDigitValue(UniChar c)
{
    const char *dec_row = decimal_info[WK_ROW(c)];
    if( !dec_row )
	return -1;
    return dec_row[WK_CELL(c)];
}

static WebCoreUnicodeCategory _unicodeCategory(UniChar c)
{
    return (WebCoreUnicodeCategory)(unicode_info[WK_ROW(c)][WK_CELL(c)]);
}

static WebCoreUnicodeDirection _unicodeDirection(UniChar c)
{
    const unsigned char *rowp = direction_info[WK_ROW(c)];
    
    if(!rowp) 
        return DirectionL;
    return (WebCoreUnicodeDirection) ( *(rowp+WK_CELL(c)) &0x1f );
}

static WebCoreUnicodeJoining _unicodeJoining(UniChar c)
{
    const unsigned char *rowp = direction_info[WK_ROW(c)];
    if ( !rowp )
	return JoiningOther;
    return (WebCoreUnicodeJoining) ((*(rowp+WK_CELL(c)) >> 5) &0x3);
}

static WebCoreUnicodeDecomposition _unicodeDecompositionTag(UniChar c)
{
    const unsigned short *r = decomposition_info[WK_ROW(c)];
    if(!r)
        return DecompositionSingle;

    unsigned short pos = r[WK_CELL(c)];
    if(!pos)
        return DecompositionSingle;

    return (WebCoreUnicodeDecomposition) decomposition_map[pos];
}

static bool _unicodeMirrored(UniChar c)
{
    const unsigned char *rowp = direction_info[WK_ROW(c)];
    if ( !rowp )
	return FALSE;
    return *(rowp+WK_CELL(c))>128;
}

static UniChar _unicodeMirroredChar(UniChar c)
{
    if(!_unicodeMirrored(c))
        return c;

    int i;
    for (i = 0; i < symmetricPairsSize; i ++) {
	if (symmetricPairs[i] == c)
	  return symmetricPairs[(i%2) ? (i-1) : (i+1)];
    }
    return c;
}

static WebCoreUnicodeCombiningClass _unicodeCombiningClass (UniChar c)
{
    const unsigned char *rowp = combining_info[WK_ROW(c)];
    if ( !rowp )
	return 0;
    return *(rowp+WK_CELL(c));
}

static UniChar _unicodeLower(UniChar c)
{
    if ( _unicodeCategory(c) != Letter_Uppercase )
	return c;
    unsigned short lower = *( case_info[WK_ROW(c)] + WK_CELL(c) );
    if ( lower == 0 )
	return c;
    return lower;
}

static UniChar _unicodeUpper(UniChar c)
{
    if ( _unicodeCategory(c) != Letter_Lowercase )
	return c;
    unsigned short upper = *(case_info[WK_ROW(c)]+WK_CELL(c));
    if ( upper == 0 )
	return c;
    return upper;
}

void WebKitInitializeUnicode(void)
{
    WebCoreUnicodeDigitValueFunction = _unicodeDigitValue;
    WebCoreUnicodeCategoryFunction = _unicodeCategory;
    WebCoreUnicodeDirectionFunction = _unicodeDirection;
    WebCoreUnicodeJoiningFunction = _unicodeJoining;
    WebCoreUnicodeDecompositionTagFunction = _unicodeDecompositionTag;
    WebCoreUnicodeMirroredFunction = _unicodeMirrored;
    WebCoreUnicodeMirroredCharFunction = _unicodeMirroredChar;
    WebCoreUnicodeCombiningClassFunction = _unicodeCombiningClass;
    WebCoreUnicodeLowerFunction = _unicodeLower;
    WebCoreUnicodeUpperFunction = _unicodeUpper;
}

