/* cd into the kwq directory, and use this command:

make && g++ can-convert.mm -framework Carbon && ./a.out && rm a.out

*/

#include "config.h"
#include <Carbon/Carbon.h>

const int NoEncodingFlags = 0;
const int BigEndian = 0;
const int IsJapanese = 0;
const int LittleEndian = 0;
const int VisualOrdering = 0;

typedef struct {
    const char *charset;
    TextEncoding encoding;
    int flags;
} CharsetEntry;

#define kCFStringEncodingBig5_DOSVariant (kTextEncodingBig5 | (kBig5_DOSVariant << 16))
#define kCFStringEncodingEUC_CN_DOSVariant (kTextEncodingEUC_CN | (kEUC_CN_DOSVariant << 16))
#define kCFStringEncodingEUC_KR_DOSVariant (kTextEncodingEUC_KR | (kEUC_KR_DOSVariant << 16))
#define kCFStringEncodingISOLatin10 kTextEncodingISOLatin10
#define kCFStringEncodingKOI8_U kTextEncodingKOI8_U
#define kCFStringEncodingShiftJIS_DOSVariant (kTextEncodingShiftJIS | (kShiftJIS_DOSVariant << 16))

#include "KWQCharsetData.c"

int main()
{
    int i;
    for (i = 0; table[i].charset; ++i) {
        TECObjectRef c;
        if (table[i].encoding != kCFStringEncodingUnicode
            && TECCreateConverter(&c, table[i].encoding,
                CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat)) != noErr) {
            printf("can't do %s\n", table[i].charset);
        }
    }
}
