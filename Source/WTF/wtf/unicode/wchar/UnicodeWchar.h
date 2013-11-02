/*
 * Copyright (C) 2012 Patrick Gansterer <paroga@paroga.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_UnicodeWchar_h
#define WTF_UnicodeWchar_h

#include <stdint.h>
#include <wchar.h>
#include <wtf/unicode/ScriptCodesFromICU.h>
#include <wtf/unicode/UnicodeMacrosFromICU.h>

typedef wchar_t UChar;
typedef uint32_t UChar32;

enum UBlockCode { UBLOCK_UNKNOWN, UBLOCK_ARABIC };

enum UCharCategory {
    U_CHAR_CATEGORY_UNKNOWN,
    U_PARAGRAPH_SEPARATOR,
    U_SPACE_SEPARATOR
};

enum UCharDirection {
    U_LEFT_TO_RIGHT,
    U_RIGHT_TO_LEFT,
    U_EUROPEAN_NUMBER,
    U_EUROPEAN_NUMBER_SEPARATOR,
    U_EUROPEAN_NUMBER_TERMINATOR,
    U_ARABIC_NUMBER,
    U_COMMON_NUMBER_SEPARATOR,
    U_BLOCK_SEPARATOR,
    U_SEGMENT_SEPARATOR,
    U_WHITE_SPACE_NEUTRAL,
    U_OTHER_NEUTRAL,
    U_LEFT_TO_RIGHT_EMBEDDING,
    U_LEFT_TO_RIGHT_OVERRIDE,
    U_RIGHT_TO_LEFT_ARABIC,
    U_RIGHT_TO_LEFT_EMBEDDING,
    U_RIGHT_TO_LEFT_OVERRIDE,
    U_POP_DIRECTIONAL_FORMAT,
    U_DIR_NON_SPACING_MARK,
    U_BOUNDARY_NEUTRAL
};

enum UDecompositionType { U_DT_NONE, U_DT_COMPAT, U_DT_FONT };

enum UErrorCode { U_ZERO_ERROR = 0, U_ERROR };

enum ULineBreak { U_LB_UNKNOWN, U_LB_COMPLEX_CONTEXT, U_LB_CONDITIONAL_JAPANESE_STARTER, U_LB_IDEOGRAPHIC };

enum UNormalizationMode { UNORM_NFC };

enum UProperty { UCHAR_DECOMPOSITION_TYPE, UCHAR_LINE_BREAK };

enum { UNORM_UNICODE_3_2 };

// FIXME: Would be good to define all of these in terms of UCharCategory constants, but until u_charType
// is implemented, that's not really worth the time.

#define U_GC_CC_MASK 0
#define U_GC_CS_MASK 0
#define U_GC_CF_MASK 0
#define U_GC_CN_MASK 0
#define U_GC_LL_MASK 0
#define U_GC_LM_MASK 0
#define U_GC_LO_MASK 0
#define U_GC_LT_MASK 0
#define U_GC_LU_MASK 0
#define U_GC_MC_MASK 0
#define U_GC_MN_MASK 0
#define U_GC_ND_MASK 0
#define U_GC_NL_MASK 0
#define U_GC_PC_MASK 0
#define U_GC_PE_MASK 0
#define U_GC_PF_MASK 0
#define U_GC_PI_MASK 0
#define U_GC_PO_MASK 0
#define U_GC_PS_MASK 0
#define U_GC_ZL_MASK 0
#define U_GC_ZP_MASK 0

#define U_GC_L_MASK 0
#define U_GC_M_MASK 0
#define U_GC_Z_MASK 0

#define U_SUCCESS(status) ((status) <= 0)
#define U_FAILURE(status) ((status) > 0)

#define U_FOLD_CASE_DEFAULT 0

#define U_GET_GC_MASK(c) U_MASK(u_charType(c))

inline UBlockCode ublock_getCode(UChar32 character) { return (character & ~0xFF) == 0x600 ? UBLOCK_ARABIC : UBLOCK_UNKNOWN; }
WTF_EXPORT_PRIVATE int unorm_normalize(const UChar* source, int32_t sourceLength, UNormalizationMode mode, int32_t options, UChar* result, int32_t resultLength, UErrorCode* status);
WTF_EXPORT_PRIVATE UCharDirection u_charDirection(UChar32);
WTF_EXPORT_PRIVATE UChar32 u_charMirror(UChar32);
WTF_EXPORT_PRIVATE int8_t u_charType(UChar32);
inline UChar32 u_foldCase(UChar32 character, unsigned options) { ASSERT_UNUSED(options, options == U_FOLD_CASE_DEFAULT); return towlower(character); }
WTF_EXPORT_PRIVATE uint8_t u_getCombiningClass(UChar32);
WTF_EXPORT_PRIVATE int u_getIntPropertyValue(UChar32, UProperty);
inline bool u_isalnum(UChar32 character) { return iswalnum(character); }
inline bool u_isprint(UChar32 character) { return iswprint(character); }
inline bool u_ispunct(UChar32 character) { return iswpunct(character); }
inline bool u_isspace(UChar32 character) { return iswspace(character); }
WTF_EXPORT_PRIVATE int u_memcasecmp(const UChar*, const UChar*, int sourceLength, unsigned options);
inline bool u_print(UChar32 character) { return iswprint(character); }
WTF_EXPORT_PRIVATE int u_strFoldCase(UChar* result, int resultLength, const UChar* source, int sourceLength, unsigned options, UErrorCode*);
WTF_EXPORT_PRIVATE int u_strToLower(UChar* result, int resultLength, const UChar* source, int sourceLength, const char* locale, UErrorCode*);
WTF_EXPORT_PRIVATE int u_strToUpper(UChar* result, int resultLength, const UChar* source, int sourceLength, const char* locale, UErrorCode*);
inline UChar32 u_tolower(UChar32 character) { return towlower(character); }
inline UChar32 u_totitle(UChar32 character) { return towupper(character); }
inline UChar32 u_toupper(UChar32 character) { return towupper(character); }

#endif // WTF_UnicodeWchar_h
