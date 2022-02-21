/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Mathias Bynens (mathias@qiwi.be)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "LiteralParser.h"

#include "CodeBlock.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSONAtomStringCacheInlines.h"
#include "Lexer.h"
#include "ObjectConstructor.h"
#include <wtf/ASCIICType.h>
#include <wtf/dtoa.h>
#include <wtf/text/StringConcatenate.h>

#include "KeywordLookup.h"

namespace JSC {

template<typename CharType>
ALWAYS_INLINE bool compare3Chars(const CharType* source, CharType c0, CharType c1, CharType c2)
{
    if constexpr (sizeof(CharType) == 1)
        return COMPARE_3CHARS(source, c0, c1, c2);
    else
        return COMPARE_3UCHARS(source, c0, c1, c2);
}

template<typename CharType>
ALWAYS_INLINE bool compare4Chars(const CharType* source, CharType c0, CharType c1, CharType c2, CharType c3)
{
    if constexpr (sizeof(CharType) == 1)
        return COMPARE_4CHARS(source, c0, c1, c2, c3);
    else
        return COMPARE_4UCHARS(source, c0, c1, c2, c3);
}

template <typename CharType>
bool LiteralParser<CharType>::tryJSONPParse(Vector<JSONPData>& results, bool needsFullSourceInfo)
{
    VM& vm = m_globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (m_lexer.next() != TokIdentifier)
        return false;
    do {
        Vector<JSONPPathEntry> path;
        // Unguarded next to start off the lexer
        Identifier name = Identifier::fromString(vm, m_lexer.currentToken()->start, m_lexer.currentToken()->end - m_lexer.currentToken()->start);
        JSONPPathEntry entry;
        if (name == vm.propertyNames->varKeyword) {
            if (m_lexer.next() != TokIdentifier)
                return false;
            entry.m_type = JSONPPathEntryTypeDeclareVar;
            entry.m_pathEntryName = Identifier::fromString(vm, m_lexer.currentToken()->start, m_lexer.currentToken()->end - m_lexer.currentToken()->start);
            path.append(entry);
        } else {
            entry.m_type = JSONPPathEntryTypeDot;
            entry.m_pathEntryName = Identifier::fromString(vm, m_lexer.currentToken()->start, m_lexer.currentToken()->end - m_lexer.currentToken()->start);
            path.append(entry);
        }
        if (isLexerKeyword(entry.m_pathEntryName))
            return false;
        TokenType tokenType = m_lexer.next();
        if (entry.m_type == JSONPPathEntryTypeDeclareVar && tokenType != TokAssign)
            return false;
        while (tokenType != TokAssign) {
            switch (tokenType) {
            case TokLBracket: {
                entry.m_type = JSONPPathEntryTypeLookup;
                if (m_lexer.next() != TokNumber)
                    return false;
                double doubleIndex = m_lexer.currentToken()->numberToken;
                int index = (int)doubleIndex;
                if (index != doubleIndex || index < 0)
                    return false;
                entry.m_pathIndex = index;
                if (m_lexer.next() != TokRBracket)
                    return false;
                break;
            }
            case TokDot: {
                entry.m_type = JSONPPathEntryTypeDot;
                if (m_lexer.next() != TokIdentifier)
                    return false;
                entry.m_pathEntryName = Identifier::fromString(vm, m_lexer.currentToken()->start, m_lexer.currentToken()->end - m_lexer.currentToken()->start);
                break;
            }
            case TokLParen: {
                if (path.last().m_type != JSONPPathEntryTypeDot || needsFullSourceInfo)
                    return false;
                path.last().m_type = JSONPPathEntryTypeCall;
                entry = path.last();
                goto startJSON;
            }
            default:
                return false;
            }
            path.append(entry);
            tokenType = m_lexer.next();
        }
    startJSON:
        m_lexer.next();
        results.append(JSONPData());
        JSValue startParseExpressionValue = parse(StartParseExpression);
        RETURN_IF_EXCEPTION(scope, false);
        results.last().m_value.set(vm, startParseExpressionValue);
        if (!results.last().m_value)
            return false;
        results.last().m_path.swap(path);
        if (entry.m_type == JSONPPathEntryTypeCall) {
            if (m_lexer.currentToken()->type != TokRParen)
                return false;
            m_lexer.next();
        }
        if (m_lexer.currentToken()->type != TokSemi)
            break;
        m_lexer.next();
    } while (m_lexer.currentToken()->type == TokIdentifier);
    return m_lexer.currentToken()->type == TokEnd;
}

template <typename CharType>
ALWAYS_INLINE Identifier LiteralParser<CharType>::makeIdentifier(VM& vm, typename Lexer::LiteralParserTokenPtr token)
{
    if (token->stringIs8Bit)
        return Identifier::fromString(vm, vm.jsonAtomStringCache.makeIdentifier(token->stringToken8, token->stringLength));
    return Identifier::fromString(vm, vm.jsonAtomStringCache.makeIdentifier(token->stringToken16, token->stringLength));
}

template <typename CharType>
ALWAYS_INLINE JSString* LiteralParser<CharType>::makeJSString(VM& vm, typename Lexer::LiteralParserTokenPtr token)
{
    constexpr unsigned maxAtomizeStringLength = 10;
    if (token->stringIs8Bit) {
        if (token->stringLength > maxAtomizeStringLength)
            return jsString(vm, String(token->stringToken8, token->stringLength));
        return jsString(vm, Identifier::fromString(vm, token->stringToken8, token->stringLength).string());
    }
    if (token->stringLength > maxAtomizeStringLength)
        return jsString(vm, String(token->stringToken16, token->stringLength));
    return jsString(vm, Identifier::fromString(vm, token->stringToken16, token->stringLength).string());
}

static ALWAYS_INLINE bool cannotBeIdentPartOrEscapeStart(LChar)
{
    RELEASE_ASSERT_NOT_REACHED();
}

static ALWAYS_INLINE bool cannotBeIdentPartOrEscapeStart(UChar)
{
    RELEASE_ASSERT_NOT_REACHED();
}

// 256 Latin-1 codes
// The JSON RFC 4627 defines a list of allowed characters to be considered
// insignificant white space: http://www.ietf.org/rfc/rfc4627.txt (2. JSON Grammar).
static constexpr const TokenType tokenTypesOfLatin1Characters[256] = {
/*   0 - Null               */ TokError,
/*   1 - Start of Heading   */ TokError,
/*   2 - Start of Text      */ TokError,
/*   3 - End of Text        */ TokError,
/*   4 - End of Transm.     */ TokError,
/*   5 - Enquiry            */ TokError,
/*   6 - Acknowledgment     */ TokError,
/*   7 - Bell               */ TokError,
/*   8 - Back Space         */ TokError,
/*   9 - Horizontal Tab     */ TokErrorSpace,
/*  10 - Line Feed          */ TokErrorSpace,
/*  11 - Vertical Tab       */ TokError,
/*  12 - Form Feed          */ TokError,
/*  13 - Carriage Return    */ TokErrorSpace,
/*  14 - Shift Out          */ TokError,
/*  15 - Shift In           */ TokError,
/*  16 - Data Line Escape   */ TokError,
/*  17 - Device Control 1   */ TokError,
/*  18 - Device Control 2   */ TokError,
/*  19 - Device Control 3   */ TokError,
/*  20 - Device Control 4   */ TokError,
/*  21 - Negative Ack.      */ TokError,
/*  22 - Synchronous Idle   */ TokError,
/*  23 - End of Transmit    */ TokError,
/*  24 - Cancel             */ TokError,
/*  25 - End of Medium      */ TokError,
/*  26 - Substitute         */ TokError,
/*  27 - Escape             */ TokError,
/*  28 - File Separator     */ TokError,
/*  29 - Group Separator    */ TokError,
/*  30 - Record Separator   */ TokError,
/*  31 - Unit Separator     */ TokError,
/*  32 - Space              */ TokErrorSpace,
/*  33 - !                  */ TokError,
/*  34 - "                  */ TokString,
/*  35 - #                  */ TokError,
/*  36 - $                  */ TokIdentifier,
/*  37 - %                  */ TokError,
/*  38 - &                  */ TokError,
/*  39 - '                  */ TokString,
/*  40 - (                  */ TokLParen,
/*  41 - )                  */ TokRParen,
/*  42 - *                  */ TokError,
/*  43 - +                  */ TokError,
/*  44 - ,                  */ TokComma,
/*  45 - -                  */ TokNumber,
/*  46 - .                  */ TokDot,
/*  47 - /                  */ TokError,
/*  48 - 0                  */ TokNumber,
/*  49 - 1                  */ TokNumber,
/*  50 - 2                  */ TokNumber,
/*  51 - 3                  */ TokNumber,
/*  52 - 4                  */ TokNumber,
/*  53 - 5                  */ TokNumber,
/*  54 - 6                  */ TokNumber,
/*  55 - 7                  */ TokNumber,
/*  56 - 8                  */ TokNumber,
/*  57 - 9                  */ TokNumber,
/*  58 - :                  */ TokColon,
/*  59 - ;                  */ TokSemi,
/*  60 - <                  */ TokError,
/*  61 - =                  */ TokAssign,
/*  62 - >                  */ TokError,
/*  63 - ?                  */ TokError,
/*  64 - @                  */ TokError,
/*  65 - A                  */ TokIdentifier,
/*  66 - B                  */ TokIdentifier,
/*  67 - C                  */ TokIdentifier,
/*  68 - D                  */ TokIdentifier,
/*  69 - E                  */ TokIdentifier,
/*  70 - F                  */ TokIdentifier,
/*  71 - G                  */ TokIdentifier,
/*  72 - H                  */ TokIdentifier,
/*  73 - I                  */ TokIdentifier,
/*  74 - J                  */ TokIdentifier,
/*  75 - K                  */ TokIdentifier,
/*  76 - L                  */ TokIdentifier,
/*  77 - M                  */ TokIdentifier,
/*  78 - N                  */ TokIdentifier,
/*  79 - O                  */ TokIdentifier,
/*  80 - P                  */ TokIdentifier,
/*  81 - Q                  */ TokIdentifier,
/*  82 - R                  */ TokIdentifier,
/*  83 - S                  */ TokIdentifier,
/*  84 - T                  */ TokIdentifier,
/*  85 - U                  */ TokIdentifier,
/*  86 - V                  */ TokIdentifier,
/*  87 - W                  */ TokIdentifier,
/*  88 - X                  */ TokIdentifier,
/*  89 - Y                  */ TokIdentifier,
/*  90 - Z                  */ TokIdentifier,
/*  91 - [                  */ TokLBracket,
/*  92 - \                  */ TokError,
/*  93 - ]                  */ TokRBracket,
/*  94 - ^                  */ TokError,
/*  95 - _                  */ TokIdentifier,
/*  96 - `                  */ TokError,
/*  97 - a                  */ TokIdentifier,
/*  98 - b                  */ TokIdentifier,
/*  99 - c                  */ TokIdentifier,
/* 100 - d                  */ TokIdentifier,
/* 101 - e                  */ TokIdentifier,
/* 102 - f                  */ TokIdentifier,
/* 103 - g                  */ TokIdentifier,
/* 104 - h                  */ TokIdentifier,
/* 105 - i                  */ TokIdentifier,
/* 106 - j                  */ TokIdentifier,
/* 107 - k                  */ TokIdentifier,
/* 108 - l                  */ TokIdentifier,
/* 109 - m                  */ TokIdentifier,
/* 110 - n                  */ TokIdentifier,
/* 111 - o                  */ TokIdentifier,
/* 112 - p                  */ TokIdentifier,
/* 113 - q                  */ TokIdentifier,
/* 114 - r                  */ TokIdentifier,
/* 115 - s                  */ TokIdentifier,
/* 116 - t                  */ TokIdentifier,
/* 117 - u                  */ TokIdentifier,
/* 118 - v                  */ TokIdentifier,
/* 119 - w                  */ TokIdentifier,
/* 120 - x                  */ TokIdentifier,
/* 121 - y                  */ TokIdentifier,
/* 122 - z                  */ TokIdentifier,
/* 123 - {                  */ TokLBrace,
/* 124 - |                  */ TokError,
/* 125 - }                  */ TokRBrace,
/* 126 - ~                  */ TokError,
/* 127 - Delete             */ TokError,
/* 128 - Cc category        */ TokError,
/* 129 - Cc category        */ TokError,
/* 130 - Cc category        */ TokError,
/* 131 - Cc category        */ TokError,
/* 132 - Cc category        */ TokError,
/* 133 - Cc category        */ TokError,
/* 134 - Cc category        */ TokError,
/* 135 - Cc category        */ TokError,
/* 136 - Cc category        */ TokError,
/* 137 - Cc category        */ TokError,
/* 138 - Cc category        */ TokError,
/* 139 - Cc category        */ TokError,
/* 140 - Cc category        */ TokError,
/* 141 - Cc category        */ TokError,
/* 142 - Cc category        */ TokError,
/* 143 - Cc category        */ TokError,
/* 144 - Cc category        */ TokError,
/* 145 - Cc category        */ TokError,
/* 146 - Cc category        */ TokError,
/* 147 - Cc category        */ TokError,
/* 148 - Cc category        */ TokError,
/* 149 - Cc category        */ TokError,
/* 150 - Cc category        */ TokError,
/* 151 - Cc category        */ TokError,
/* 152 - Cc category        */ TokError,
/* 153 - Cc category        */ TokError,
/* 154 - Cc category        */ TokError,
/* 155 - Cc category        */ TokError,
/* 156 - Cc category        */ TokError,
/* 157 - Cc category        */ TokError,
/* 158 - Cc category        */ TokError,
/* 159 - Cc category        */ TokError,
/* 160 - Zs category (nbsp) */ TokError,
/* 161 - Po category        */ TokError,
/* 162 - Sc category        */ TokError,
/* 163 - Sc category        */ TokError,
/* 164 - Sc category        */ TokError,
/* 165 - Sc category        */ TokError,
/* 166 - So category        */ TokError,
/* 167 - So category        */ TokError,
/* 168 - Sk category        */ TokError,
/* 169 - So category        */ TokError,
/* 170 - Ll category        */ TokError,
/* 171 - Pi category        */ TokError,
/* 172 - Sm category        */ TokError,
/* 173 - Cf category        */ TokError,
/* 174 - So category        */ TokError,
/* 175 - Sk category        */ TokError,
/* 176 - So category        */ TokError,
/* 177 - Sm category        */ TokError,
/* 178 - No category        */ TokError,
/* 179 - No category        */ TokError,
/* 180 - Sk category        */ TokError,
/* 181 - Ll category        */ TokError,
/* 182 - So category        */ TokError,
/* 183 - Po category        */ TokError,
/* 184 - Sk category        */ TokError,
/* 185 - No category        */ TokError,
/* 186 - Ll category        */ TokError,
/* 187 - Pf category        */ TokError,
/* 188 - No category        */ TokError,
/* 189 - No category        */ TokError,
/* 190 - No category        */ TokError,
/* 191 - Po category        */ TokError,
/* 192 - Lu category        */ TokError,
/* 193 - Lu category        */ TokError,
/* 194 - Lu category        */ TokError,
/* 195 - Lu category        */ TokError,
/* 196 - Lu category        */ TokError,
/* 197 - Lu category        */ TokError,
/* 198 - Lu category        */ TokError,
/* 199 - Lu category        */ TokError,
/* 200 - Lu category        */ TokError,
/* 201 - Lu category        */ TokError,
/* 202 - Lu category        */ TokError,
/* 203 - Lu category        */ TokError,
/* 204 - Lu category        */ TokError,
/* 205 - Lu category        */ TokError,
/* 206 - Lu category        */ TokError,
/* 207 - Lu category        */ TokError,
/* 208 - Lu category        */ TokError,
/* 209 - Lu category        */ TokError,
/* 210 - Lu category        */ TokError,
/* 211 - Lu category        */ TokError,
/* 212 - Lu category        */ TokError,
/* 213 - Lu category        */ TokError,
/* 214 - Lu category        */ TokError,
/* 215 - Sm category        */ TokError,
/* 216 - Lu category        */ TokError,
/* 217 - Lu category        */ TokError,
/* 218 - Lu category        */ TokError,
/* 219 - Lu category        */ TokError,
/* 220 - Lu category        */ TokError,
/* 221 - Lu category        */ TokError,
/* 222 - Lu category        */ TokError,
/* 223 - Ll category        */ TokError,
/* 224 - Ll category        */ TokError,
/* 225 - Ll category        */ TokError,
/* 226 - Ll category        */ TokError,
/* 227 - Ll category        */ TokError,
/* 228 - Ll category        */ TokError,
/* 229 - Ll category        */ TokError,
/* 230 - Ll category        */ TokError,
/* 231 - Ll category        */ TokError,
/* 232 - Ll category        */ TokError,
/* 233 - Ll category        */ TokError,
/* 234 - Ll category        */ TokError,
/* 235 - Ll category        */ TokError,
/* 236 - Ll category        */ TokError,
/* 237 - Ll category        */ TokError,
/* 238 - Ll category        */ TokError,
/* 239 - Ll category        */ TokError,
/* 240 - Ll category        */ TokError,
/* 241 - Ll category        */ TokError,
/* 242 - Ll category        */ TokError,
/* 243 - Ll category        */ TokError,
/* 244 - Ll category        */ TokError,
/* 245 - Ll category        */ TokError,
/* 246 - Ll category        */ TokError,
/* 247 - Sm category        */ TokError,
/* 248 - Ll category        */ TokError,
/* 249 - Ll category        */ TokError,
/* 250 - Ll category        */ TokError,
/* 251 - Ll category        */ TokError,
/* 252 - Ll category        */ TokError,
/* 253 - Ll category        */ TokError,
/* 254 - Ll category        */ TokError,
/* 255 - Ll category        */ TokError
};

// 256 Latin-1 codes
static constexpr const bool safeStringLatin1CharactersInStrictJSON[256] = {
/*   0 - Null               */ false,
/*   1 - Start of Heading   */ false,
/*   2 - Start of Text      */ false,
/*   3 - End of Text        */ false,
/*   4 - End of Transm.     */ false,
/*   5 - Enquiry            */ false,
/*   6 - Acknowledgment     */ false,
/*   7 - Bell               */ false,
/*   8 - Back Space         */ false,
/*   9 - Horizontal Tab     */ false,
/*  10 - Line Feed          */ false,
/*  11 - Vertical Tab       */ false,
/*  12 - Form Feed          */ false,
/*  13 - Carriage Return    */ false,
/*  14 - Shift Out          */ false,
/*  15 - Shift In           */ false,
/*  16 - Data Line Escape   */ false,
/*  17 - Device Control 1   */ false,
/*  18 - Device Control 2   */ false,
/*  19 - Device Control 3   */ false,
/*  20 - Device Control 4   */ false,
/*  21 - Negative Ack.      */ false,
/*  22 - Synchronous Idle   */ false,
/*  23 - End of Transmit    */ false,
/*  24 - Cancel             */ false,
/*  25 - End of Medium      */ false,
/*  26 - Substitute         */ false,
/*  27 - Escape             */ false,
/*  28 - File Separator     */ false,
/*  29 - Group Separator    */ false,
/*  30 - Record Separator   */ false,
/*  31 - Unit Separator     */ false,
/*  32 - Space              */ true,
/*  33 - !                  */ true,
/*  34 - "                  */ false,
/*  35 - #                  */ true,
/*  36 - $                  */ true,
/*  37 - %                  */ true,
/*  38 - &                  */ true,
/*  39 - '                  */ true,
/*  40 - (                  */ true,
/*  41 - )                  */ true,
/*  42 - *                  */ true,
/*  43 - +                  */ true,
/*  44 - ,                  */ true,
/*  45 - -                  */ true,
/*  46 - .                  */ true,
/*  47 - /                  */ true,
/*  48 - 0                  */ true,
/*  49 - 1                  */ true,
/*  50 - 2                  */ true,
/*  51 - 3                  */ true,
/*  52 - 4                  */ true,
/*  53 - 5                  */ true,
/*  54 - 6                  */ true,
/*  55 - 7                  */ true,
/*  56 - 8                  */ true,
/*  57 - 9                  */ true,
/*  58 - :                  */ true,
/*  59 - ;                  */ true,
/*  60 - <                  */ true,
/*  61 - =                  */ true,
/*  62 - >                  */ true,
/*  63 - ?                  */ true,
/*  64 - @                  */ true,
/*  65 - A                  */ true,
/*  66 - B                  */ true,
/*  67 - C                  */ true,
/*  68 - D                  */ true,
/*  69 - E                  */ true,
/*  70 - F                  */ true,
/*  71 - G                  */ true,
/*  72 - H                  */ true,
/*  73 - I                  */ true,
/*  74 - J                  */ true,
/*  75 - K                  */ true,
/*  76 - L                  */ true,
/*  77 - M                  */ true,
/*  78 - N                  */ true,
/*  79 - O                  */ true,
/*  80 - P                  */ true,
/*  81 - Q                  */ true,
/*  82 - R                  */ true,
/*  83 - S                  */ true,
/*  84 - T                  */ true,
/*  85 - U                  */ true,
/*  86 - V                  */ true,
/*  87 - W                  */ true,
/*  88 - X                  */ true,
/*  89 - Y                  */ true,
/*  90 - Z                  */ true,
/*  91 - [                  */ true,
/*  92 - \                  */ false,
/*  93 - ]                  */ true,
/*  94 - ^                  */ true,
/*  95 - _                  */ true,
/*  96 - `                  */ true,
/*  97 - a                  */ true,
/*  98 - b                  */ true,
/*  99 - c                  */ true,
/* 100 - d                  */ true,
/* 101 - e                  */ true,
/* 102 - f                  */ true,
/* 103 - g                  */ true,
/* 104 - h                  */ true,
/* 105 - i                  */ true,
/* 106 - j                  */ true,
/* 107 - k                  */ true,
/* 108 - l                  */ true,
/* 109 - m                  */ true,
/* 110 - n                  */ true,
/* 111 - o                  */ true,
/* 112 - p                  */ true,
/* 113 - q                  */ true,
/* 114 - r                  */ true,
/* 115 - s                  */ true,
/* 116 - t                  */ true,
/* 117 - u                  */ true,
/* 118 - v                  */ true,
/* 119 - w                  */ true,
/* 120 - x                  */ true,
/* 121 - y                  */ true,
/* 122 - z                  */ true,
/* 123 - {                  */ true,
/* 124 - |                  */ true,
/* 125 - }                  */ true,
/* 126 - ~                  */ true,
/* 127 - Delete             */ true,
/* 128 - Cc category        */ true,
/* 129 - Cc category        */ true,
/* 130 - Cc category        */ true,
/* 131 - Cc category        */ true,
/* 132 - Cc category        */ true,
/* 133 - Cc category        */ true,
/* 134 - Cc category        */ true,
/* 135 - Cc category        */ true,
/* 136 - Cc category        */ true,
/* 137 - Cc category        */ true,
/* 138 - Cc category        */ true,
/* 139 - Cc category        */ true,
/* 140 - Cc category        */ true,
/* 141 - Cc category        */ true,
/* 142 - Cc category        */ true,
/* 143 - Cc category        */ true,
/* 144 - Cc category        */ true,
/* 145 - Cc category        */ true,
/* 146 - Cc category        */ true,
/* 147 - Cc category        */ true,
/* 148 - Cc category        */ true,
/* 149 - Cc category        */ true,
/* 150 - Cc category        */ true,
/* 151 - Cc category        */ true,
/* 152 - Cc category        */ true,
/* 153 - Cc category        */ true,
/* 154 - Cc category        */ true,
/* 155 - Cc category        */ true,
/* 156 - Cc category        */ true,
/* 157 - Cc category        */ true,
/* 158 - Cc category        */ true,
/* 159 - Cc category        */ true,
/* 160 - Zs category (nbsp) */ true,
/* 161 - Po category        */ true,
/* 162 - Sc category        */ true,
/* 163 - Sc category        */ true,
/* 164 - Sc category        */ true,
/* 165 - Sc category        */ true,
/* 166 - So category        */ true,
/* 167 - So category        */ true,
/* 168 - Sk category        */ true,
/* 169 - So category        */ true,
/* 170 - Ll category        */ true,
/* 171 - Pi category        */ true,
/* 172 - Sm category        */ true,
/* 173 - Cf category        */ true,
/* 174 - So category        */ true,
/* 175 - Sk category        */ true,
/* 176 - So category        */ true,
/* 177 - Sm category        */ true,
/* 178 - No category        */ true,
/* 179 - No category        */ true,
/* 180 - Sk category        */ true,
/* 181 - Ll category        */ true,
/* 182 - So category        */ true,
/* 183 - Po category        */ true,
/* 184 - Sk category        */ true,
/* 185 - No category        */ true,
/* 186 - Ll category        */ true,
/* 187 - Pf category        */ true,
/* 188 - No category        */ true,
/* 189 - No category        */ true,
/* 190 - No category        */ true,
/* 191 - Po category        */ true,
/* 192 - Lu category        */ true,
/* 193 - Lu category        */ true,
/* 194 - Lu category        */ true,
/* 195 - Lu category        */ true,
/* 196 - Lu category        */ true,
/* 197 - Lu category        */ true,
/* 198 - Lu category        */ true,
/* 199 - Lu category        */ true,
/* 200 - Lu category        */ true,
/* 201 - Lu category        */ true,
/* 202 - Lu category        */ true,
/* 203 - Lu category        */ true,
/* 204 - Lu category        */ true,
/* 205 - Lu category        */ true,
/* 206 - Lu category        */ true,
/* 207 - Lu category        */ true,
/* 208 - Lu category        */ true,
/* 209 - Lu category        */ true,
/* 210 - Lu category        */ true,
/* 211 - Lu category        */ true,
/* 212 - Lu category        */ true,
/* 213 - Lu category        */ true,
/* 214 - Lu category        */ true,
/* 215 - Sm category        */ true,
/* 216 - Lu category        */ true,
/* 217 - Lu category        */ true,
/* 218 - Lu category        */ true,
/* 219 - Lu category        */ true,
/* 220 - Lu category        */ true,
/* 221 - Lu category        */ true,
/* 222 - Lu category        */ true,
/* 223 - Ll category        */ true,
/* 224 - Ll category        */ true,
/* 225 - Ll category        */ true,
/* 226 - Ll category        */ true,
/* 227 - Ll category        */ true,
/* 228 - Ll category        */ true,
/* 229 - Ll category        */ true,
/* 230 - Ll category        */ true,
/* 231 - Ll category        */ true,
/* 232 - Ll category        */ true,
/* 233 - Ll category        */ true,
/* 234 - Ll category        */ true,
/* 235 - Ll category        */ true,
/* 236 - Ll category        */ true,
/* 237 - Ll category        */ true,
/* 238 - Ll category        */ true,
/* 239 - Ll category        */ true,
/* 240 - Ll category        */ true,
/* 241 - Ll category        */ true,
/* 242 - Ll category        */ true,
/* 243 - Ll category        */ true,
/* 244 - Ll category        */ true,
/* 245 - Ll category        */ true,
/* 246 - Ll category        */ true,
/* 247 - Sm category        */ true,
/* 248 - Ll category        */ true,
/* 249 - Ll category        */ true,
/* 250 - Ll category        */ true,
/* 251 - Ll category        */ true,
/* 252 - Ll category        */ true,
/* 253 - Ll category        */ true,
/* 254 - Ll category        */ true,
/* 255 - Ll category        */ true,
};

template <typename CharType>
static ALWAYS_INLINE bool isJSONWhiteSpace(const CharType& c)
{
    return isLatin1(c) && tokenTypesOfLatin1Characters[c] == TokErrorSpace;
}

template <typename CharType>
ALWAYS_INLINE TokenType LiteralParser<CharType>::Lexer::lex(LiteralParserToken<CharType>& token)
{
#if ASSERT_ENABLED
    m_currentTokenID++;
#endif

    while (m_ptr < m_end && isJSONWhiteSpace(*m_ptr))
        ++m_ptr;

    ASSERT(m_ptr <= m_end);
    if (m_ptr == m_end) {
        token.type = TokEnd;
        token.start = token.end = m_ptr;
        return TokEnd;
    }
    ASSERT(m_ptr < m_end);
    token.type = TokError;
    token.start = m_ptr;
    CharType character = *m_ptr;
    if (LIKELY(isLatin1(character))) {
        TokenType tokenType = tokenTypesOfLatin1Characters[character];
        switch (tokenType) {
        case TokString:
            if (UNLIKELY(character == '\'' && m_mode == StrictJSON)) {
                m_lexErrorMessage = "Single quotes (\') are not allowed in JSON"_s;
                return TokError;
            }
            return lexString(token, character);

        case TokIdentifier: {
            switch (character) {
            case 't':
                if (m_end - m_ptr >= 4 && compare3Chars<CharType>(m_ptr + 1, 'r', 'u', 'e')) {
                    m_ptr += 4;
                    token.type = TokTrue;
                    token.end = m_ptr;
                    return TokTrue;
                }
                break;
            case 'f':
                if (m_end - m_ptr >= 5 && compare4Chars<CharType>(m_ptr + 1, 'a', 'l', 's', 'e')) {
                    m_ptr += 5;
                    token.type = TokFalse;
                    token.end = m_ptr;
                    return TokFalse;
                }
                break;
            case 'n':
                if (m_end - m_ptr >= 4 && compare3Chars<CharType>(m_ptr + 1, 'u', 'l', 'l')) {
                    m_ptr += 4;
                    token.type = TokNull;
                    token.end = m_ptr;
                    return TokNull;
                }
                break;
            }
            return lexIdentifier(token);
        }

        case TokNumber:
            return lexNumber(token);

        case TokError:
        case TokErrorSpace:
            break;

        default:
            ASSERT(tokenType == TokLBracket
                || tokenType == TokRBracket
                || tokenType == TokLBrace
                || tokenType == TokRBrace
                || tokenType == TokColon
                || tokenType == TokLParen
                || tokenType == TokRParen
                || tokenType == TokComma
                || tokenType == TokDot
                || tokenType == TokAssign
                || tokenType == TokSemi);
            token.type = tokenType;
            token.end = ++m_ptr;
            return tokenType;
        }
    }
    m_lexErrorMessage = makeString("Unrecognized token '", StringView { m_ptr, 1 }, '\'');
    return TokError;
}

template <>
ALWAYS_INLINE TokenType LiteralParser<LChar>::Lexer::lexIdentifier(LiteralParserToken<LChar>& token)
{
    while (m_ptr < m_end && (isASCIIAlphanumeric(*m_ptr) || *m_ptr == '_' || *m_ptr == '$'))
        m_ptr++;
    token.stringIs8Bit = 1;
    token.stringToken8 = token.start;
    token.stringLength = m_ptr - token.start;
    token.type = TokIdentifier;
    token.end = m_ptr;
    return TokIdentifier;
}

template <>
ALWAYS_INLINE TokenType LiteralParser<UChar>::Lexer::lexIdentifier(LiteralParserToken<UChar>& token)
{
    while (m_ptr < m_end && (isASCIIAlphanumeric(*m_ptr) || *m_ptr == '_' || *m_ptr == '$' || *m_ptr == 0x200C || *m_ptr == 0x200D))
        m_ptr++;
    token.stringIs8Bit = 0;
    token.stringToken16 = token.start;
    token.stringLength = m_ptr - token.start;
    token.type = TokIdentifier;
    token.end = m_ptr;
    return TokIdentifier;
}

template <typename CharType>
TokenType LiteralParser<CharType>::Lexer::next()
{
    TokenType result = lex(m_currentToken);
    ASSERT(m_currentToken.type == result);
    return result;
}

template <>
ALWAYS_INLINE void setParserTokenString<LChar>(LiteralParserToken<LChar>& token, const LChar* string)
{
    token.stringIs8Bit = 1;
    token.stringToken8 = string;
}

template <>
ALWAYS_INLINE void setParserTokenString<UChar>(LiteralParserToken<UChar>& token, const UChar* string)
{
    token.stringIs8Bit = 0;
    token.stringToken16 = string;
}

enum class SafeStringCharacterSet { Strict, NonStrict };

template <SafeStringCharacterSet set>
static ALWAYS_INLINE bool isSafeStringCharacter(LChar c, LChar terminator)
{
    if constexpr (set == SafeStringCharacterSet::Strict)
        return safeStringLatin1CharactersInStrictJSON[c];
    else
        return (c >= ' ' && c != '\\' && c != terminator) || (c == '\t');
}

template <SafeStringCharacterSet set>
static ALWAYS_INLINE bool isSafeStringCharacter(UChar c, UChar terminator)
{
    if constexpr (set == SafeStringCharacterSet::Strict) {
        if (!isLatin1(c))
            return true;
        return isSafeStringCharacter<set>(static_cast<LChar>(c), static_cast<LChar>(terminator));
    } else
        return (c >= ' ' && isLatin1(c) && c != '\\' && c != terminator) || (c == '\t');
}

template <typename CharType>
ALWAYS_INLINE TokenType LiteralParser<CharType>::Lexer::lexString(LiteralParserToken<CharType>& token, CharType terminator)
{
    ++m_ptr;
    const CharType* runStart = m_ptr;

    if (m_mode == StrictJSON) {
        ASSERT(terminator == '"');
        while (m_ptr < m_end && isSafeStringCharacter<SafeStringCharacterSet::Strict>(*m_ptr, '"'))
            ++m_ptr;
    } else {
        while (m_ptr < m_end && isSafeStringCharacter<SafeStringCharacterSet::NonStrict>(*m_ptr, terminator))
            ++m_ptr;
    }

    if (LIKELY(m_ptr < m_end && *m_ptr == terminator)) {
        setParserTokenString<CharType>(token, runStart);
        token.stringLength = m_ptr - runStart;
        token.type = TokString;
        token.end = ++m_ptr;
        return TokString;
    }
    return lexStringSlow(token, runStart, terminator);
}

template <typename CharType>
TokenType LiteralParser<CharType>::Lexer::lexStringSlow(LiteralParserToken<CharType>& token, const CharType* runStart, CharType terminator)
{
    m_builder.clear();
    goto slowPathBegin;
    do {
        runStart = m_ptr;
        if (m_mode == StrictJSON) {
            while (m_ptr < m_end && isSafeStringCharacter<SafeStringCharacterSet::Strict>(*m_ptr, terminator))
                ++m_ptr;
        } else {
            while (m_ptr < m_end && isSafeStringCharacter<SafeStringCharacterSet::NonStrict>(*m_ptr, terminator))
                ++m_ptr;
        }

        if (!m_builder.isEmpty())
            m_builder.appendCharacters(runStart, m_ptr - runStart);

slowPathBegin:
        if ((m_mode != NonStrictJSON) && m_ptr < m_end && *m_ptr == '\\') {
            if (m_builder.isEmpty() && runStart < m_ptr)
                m_builder.appendCharacters(runStart, m_ptr - runStart);
            ++m_ptr;
            if (m_ptr >= m_end) {
                m_lexErrorMessage = "Unterminated string"_s;
                return TokError;
            }
            switch (*m_ptr) {
                case '"':
                    m_builder.append('"');
                    m_ptr++;
                    break;
                case '\\':
                    m_builder.append('\\');
                    m_ptr++;
                    break;
                case '/':
                    m_builder.append('/');
                    m_ptr++;
                    break;
                case 'b':
                    m_builder.append('\b');
                    m_ptr++;
                    break;
                case 'f':
                    m_builder.append('\f');
                    m_ptr++;
                    break;
                case 'n':
                    m_builder.append('\n');
                    m_ptr++;
                    break;
                case 'r':
                    m_builder.append('\r');
                    m_ptr++;
                    break;
                case 't':
                    m_builder.append('\t');
                    m_ptr++;
                    break;

                case 'u':
                    if ((m_end - m_ptr) < 5) { 
                        m_lexErrorMessage = "\\u must be followed by 4 hex digits"_s;
                        return TokError;
                    } // uNNNN == 5 characters
                    for (int i = 1; i < 5; i++) {
                        if (!isASCIIHexDigit(m_ptr[i])) {
                            m_lexErrorMessage = makeString("\"\\", StringView { m_ptr, 5 }, "\" is not a valid unicode escape");
                            return TokError;
                        }
                    }
                    m_builder.append(JSC::Lexer<CharType>::convertUnicode(m_ptr[1], m_ptr[2], m_ptr[3], m_ptr[4]));
                    m_ptr += 5;
                    break;

                default:
                    if (*m_ptr == '\'' && m_mode != StrictJSON) {
                        m_builder.append('\'');
                        m_ptr++;
                        break;
                    }
                    m_lexErrorMessage = makeString("Invalid escape character ", StringView { m_ptr, 1 });
                    return TokError;
            }
        }
    } while ((m_mode != NonStrictJSON) && m_ptr != runStart && (m_ptr < m_end) && *m_ptr != terminator);

    if (m_ptr >= m_end || *m_ptr != terminator) {
        m_lexErrorMessage = "Unterminated string"_s;
        return TokError;
    }

    if (m_builder.isEmpty()) {
        setParserTokenString<CharType>(token, runStart);
        token.stringLength = m_ptr - runStart;
    } else {
        if (m_builder.is8Bit()) {
            token.stringIs8Bit = 1;
            token.stringToken8 = m_builder.characters8();
        } else {
            token.stringIs8Bit = 0;
            token.stringToken16 = m_builder.characters16();
        }
        token.stringLength = m_builder.length();
    }
    token.type = TokString;
    token.end = ++m_ptr;
    return TokString;
}

template <typename CharType>
TokenType LiteralParser<CharType>::Lexer::lexNumber(LiteralParserToken<CharType>& token)
{
    // ES5 and json.org define numbers as
    // number
    //     int
    //     int frac? exp?
    //
    // int
    //     -? 0
    //     -? digit1-9 digits?
    //
    // digits
    //     digit digits?
    //
    // -?(0 | [1-9][0-9]*) ('.' [0-9]+)? ([eE][+-]? [0-9]+)?

    if (m_ptr < m_end && *m_ptr == '-') // -?
        ++m_ptr;
    
    // (0 | [1-9][0-9]*)
    if (m_ptr < m_end && *m_ptr == '0') // 0
        ++m_ptr;
    else if (m_ptr < m_end && *m_ptr >= '1' && *m_ptr <= '9') { // [1-9]
        ++m_ptr;
        // [0-9]*
        while (m_ptr < m_end && isASCIIDigit(*m_ptr))
            ++m_ptr;
    } else {
        m_lexErrorMessage = "Invalid number"_s;
        return TokError;
    }

    // ('.' [0-9]+)?
    const int NumberOfDigitsForSafeInt32 = 9;  // The numbers from -99999999 to 999999999 are always in range of Int32.
    if (m_ptr < m_end && *m_ptr == '.') {
        ++m_ptr;
        // [0-9]+
        if (m_ptr >= m_end || !isASCIIDigit(*m_ptr)) {
            m_lexErrorMessage = "Invalid digits after decimal point"_s;
            return TokError;
        }

        ++m_ptr;
        while (m_ptr < m_end && isASCIIDigit(*m_ptr))
            ++m_ptr;
    } else if (m_ptr < m_end && (*m_ptr != 'e' && *m_ptr != 'E') && (m_ptr - token.start) <= NumberOfDigitsForSafeInt32) {
        int32_t result = 0;
        token.type = TokNumber;
        token.end = m_ptr;
        const CharType* digit = token.start;
        bool negative = false;
        if (*digit == '-') {
            negative = true;
            digit++;
        }
        
        ASSERT((m_ptr - digit) <= NumberOfDigitsForSafeInt32);
        while (digit < m_ptr)
            result = result * 10 + (*digit++) - '0';

        if (!negative)
            token.numberToken = result;
        else {
            if (!result)
                token.numberToken = -0.0;
            else
                token.numberToken = -result;
        }
        return TokNumber;
    }

    //  ([eE][+-]? [0-9]+)?
    if (m_ptr < m_end && (*m_ptr == 'e' || *m_ptr == 'E')) { // [eE]
        ++m_ptr;

        // [-+]?
        if (m_ptr < m_end && (*m_ptr == '-' || *m_ptr == '+'))
            ++m_ptr;

        // [0-9]+
        if (m_ptr >= m_end || !isASCIIDigit(*m_ptr)) {
            m_lexErrorMessage = "Exponent symbols should be followed by an optional '+' or '-' and then by at least one number"_s;
            return TokError;
        }
        
        ++m_ptr;
        while (m_ptr < m_end && isASCIIDigit(*m_ptr))
            ++m_ptr;
    }
    
    token.type = TokNumber;
    token.end = m_ptr;
    size_t parsedLength;
    token.numberToken = parseDouble(token.start, token.end - token.start, parsedLength);
    return TokNumber;
}

template <typename CharType>
void LiteralParser<CharType>::setErrorMessageForToken(TokenType tokenType)
{
    switch (tokenType) {
    case TokRBrace:
        m_parseErrorMessage = "Expected '}'"_s;
        break;
    case TokRBracket:
        m_parseErrorMessage = "Expected ']'"_s;
        break;
    case TokColon:
        m_parseErrorMessage = "Expected ':' before value in object property definition"_s;
        break;
    default: {
        RELEASE_ASSERT_NOT_REACHED();
    }
    }
}

template <typename CharType>
ALWAYS_INLINE JSValue LiteralParser<CharType>::parsePrimitiveValue(VM& vm)
{
    switch (m_lexer.currentToken()->type) {
    case TokString: {
        JSString* result = makeJSString(vm, m_lexer.currentToken());
        m_lexer.next();
        return result;
    }
    case TokNumber: {
        JSValue result = jsNumber(m_lexer.currentToken()->numberToken);
        m_lexer.next();
        return result;
    }
    case TokNull:
        m_lexer.next();
        return jsNull();
    case TokTrue:
        m_lexer.next();
        return jsBoolean(true);
    case TokFalse:
        m_lexer.next();
        return jsBoolean(false);
    case TokRBracket:
        m_parseErrorMessage = "Unexpected token ']'"_s;
        return { };
    case TokRBrace:
        m_parseErrorMessage = "Unexpected token '}'"_s;
        return { };
    case TokIdentifier: {
        auto token = m_lexer.currentToken();

        auto tryMakeErrorString = [&] (unsigned length) -> String {
            bool addEllipsis = length != token->stringLength;
            if (token->stringIs8Bit)
                return tryMakeString("Unexpected identifier \"", StringView { token->stringToken8, length }, addEllipsis ? "..." : "", '"');
            return tryMakeString("Unexpected identifier \"", StringView { token->stringToken16, length }, addEllipsis ? "..." : "", '"');
        };

        constexpr unsigned maxLength = 200;

        String errorString = tryMakeErrorString(std::min(token->stringLength, maxLength));
        if (!errorString) {
            constexpr unsigned shortLength = 10;
            if (token->stringLength > shortLength)
                errorString = tryMakeErrorString(shortLength);
            if (!errorString)
                errorString = "Unexpected identifier";
        }

        m_parseErrorMessage = errorString;
        return { };
    }
    case TokColon:
        m_parseErrorMessage = "Unexpected token ':'"_s;
        return { };
    case TokLParen:
        m_parseErrorMessage = "Unexpected token '('"_s;
        return { };
    case TokRParen:
        m_parseErrorMessage = "Unexpected token ')'"_s;
        return { };
    case TokComma:
        m_parseErrorMessage = "Unexpected token ','"_s;
        return { };
    case TokDot:
        m_parseErrorMessage = "Unexpected token '.'"_s;
        return { };
    case TokAssign:
        m_parseErrorMessage = "Unexpected token '='"_s;
        return { };
    case TokSemi:
        m_parseErrorMessage = "Unexpected token ';'"_s;
        return { };
    case TokEnd:
        m_parseErrorMessage = "Unexpected EOF"_s;
        return { };
    case TokError:
    default:
        // Error
        m_parseErrorMessage = "Could not parse value expression"_s;
        return { };
    }
}

template <typename CharType>
JSValue LiteralParser<CharType>::parse(ParserState initialState)
{
    VM& vm = m_globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    ParserState state = initialState;
    MarkedArgumentBuffer objectStack;
    JSValue lastValue;
    Vector<ParserState, 16, UnsafeVectorOverflow> stateStack;
    Vector<Identifier, 16, UnsafeVectorOverflow> identifierStack;
    HashSet<JSObject*> visitedUnderscoreProto;
    while (1) {
        switch(state) {
        startParseArray:
        case StartParseArray: {
            JSArray* array = constructEmptyArray(m_globalObject, nullptr);
            RETURN_IF_EXCEPTION(scope, { });
            objectStack.appendWithCrashOnOverflow(array);
        }
        doParseArrayStartExpression:
        FALLTHROUGH;
        case DoParseArrayStartExpression: {
            TokenType lastToken = m_lexer.currentToken()->type;
            if (m_lexer.next() == TokRBracket) {
                if (UNLIKELY(lastToken == TokComma)) {
                    m_parseErrorMessage = "Unexpected comma at the end of array expression"_s;
                    return { };
                }
                m_lexer.next();
                lastValue = objectStack.takeLast();
                break;
            }

            stateStack.append(DoParseArrayEndExpression);
            goto startParseExpression;
        }
        case DoParseArrayEndExpression: {
            JSArray* array = asArray(objectStack.last());
            array->putDirectIndex(m_globalObject, array->length(), lastValue);
            RETURN_IF_EXCEPTION(scope, { });

            if (m_lexer.currentToken()->type == TokComma)
                goto doParseArrayStartExpression;

            if (UNLIKELY(m_lexer.currentToken()->type != TokRBracket)) {
                setErrorMessageForToken(TokRBracket);
                return { };
            }
            
            m_lexer.next();
            lastValue = objectStack.takeLast();
            break;
        }
        startParseObject:
        case StartParseObject: {
            JSObject* object = constructEmptyObject(m_globalObject);

            TokenType type = m_lexer.next();
            if (type == TokString || (m_mode != StrictJSON && type == TokIdentifier)) {
                while (true) {
                    Identifier ident = makeIdentifier(vm, m_lexer.currentToken());

                    if (UNLIKELY(m_lexer.next() != TokColon)) {
                        setErrorMessageForToken(TokColon);
                        return { };
                    }

                    TokenType nextType = m_lexer.next();
                    if (nextType == TokLBrace || nextType == TokLBracket) {
                        objectStack.appendWithCrashOnOverflow(object);
                        identifierStack.append(WTFMove(ident));
                        stateStack.append(DoParseObjectEndExpression);
                        if (nextType == TokLBrace)
                            goto startParseObject;
                        ASSERT(nextType == TokLBracket);
                        goto startParseArray;
                    }

                    // Leaf object construction fast path.
                    JSValue primitive = parsePrimitiveValue(vm);
                    if (UNLIKELY(!primitive))
                        return { };

                    if (m_mode != StrictJSON && ident == vm.propertyNames->underscoreProto) {
                        if (UNLIKELY(!visitedUnderscoreProto.add(object).isNewEntry)) {
                            m_parseErrorMessage = "Attempted to redefine __proto__ property"_s;
                            return { };
                        }
                        PutPropertySlot slot(object, m_nullOrCodeBlock ? m_nullOrCodeBlock->ownerExecutable()->isInStrictContext() : false);
                        JSValue(object).put(m_globalObject, ident, primitive, slot);
                        RETURN_IF_EXCEPTION(scope, { });
                    } else {
                        if (std::optional<uint32_t> index = parseIndex(ident)) {
                            object->putDirectIndex(m_globalObject, index.value(), primitive);
                            RETURN_IF_EXCEPTION(scope, { });
                        } else
                            object->putDirect(vm, ident, primitive);
                    }

                    if (m_lexer.currentToken()->type != TokComma)
                        break;

                    nextType = m_lexer.next();
                    if (UNLIKELY(nextType != TokString && (m_mode == StrictJSON || nextType != TokIdentifier))) {
                        m_parseErrorMessage = "Property name must be a string literal"_s;
                        return { };
                    }
                }

                if (UNLIKELY(m_lexer.currentToken()->type != TokRBrace)) {
                    setErrorMessageForToken(TokRBrace);
                    return { };
                }
                m_lexer.next();
                lastValue = object;
                break;
            }

            if (UNLIKELY(type != TokRBrace)) {
                setErrorMessageForToken(TokRBrace);
                return { };
            }

            m_lexer.next();
            lastValue = object;
            break;
        }
        doParseObjectStartExpression:
        case DoParseObjectStartExpression: {
            TokenType type = m_lexer.next();
            if (UNLIKELY(type != TokString && (m_mode == StrictJSON || type != TokIdentifier))) {
                m_parseErrorMessage = "Property name must be a string literal"_s;
                return { };
            }
            identifierStack.append(makeIdentifier(vm, m_lexer.currentToken()));

            // Check for colon
            if (UNLIKELY(m_lexer.next() != TokColon)) {
                setErrorMessageForToken(TokColon);
                return { };
            }

            m_lexer.next();
            stateStack.append(DoParseObjectEndExpression);
            goto startParseExpression;
        }
        case DoParseObjectEndExpression:
        {
            JSObject* object = asObject(objectStack.last());
            Identifier ident = identifierStack.takeLast();
            if (m_mode != StrictJSON && ident == vm.propertyNames->underscoreProto) {
                if (UNLIKELY(!visitedUnderscoreProto.add(object).isNewEntry)) {
                    m_parseErrorMessage = "Attempted to redefine __proto__ property"_s;
                    return { };
                }
                PutPropertySlot slot(object, m_nullOrCodeBlock ? m_nullOrCodeBlock->ownerExecutable()->isInStrictContext() : false);
                JSValue(object).put(m_globalObject, ident, lastValue, slot);
                RETURN_IF_EXCEPTION(scope, { });
            } else {
                if (std::optional<uint32_t> index = parseIndex(ident)) {
                    object->putDirectIndex(m_globalObject, index.value(), lastValue);
                    RETURN_IF_EXCEPTION(scope, { });
                } else
                    object->putDirect(vm, ident, lastValue);
            }
            if (m_lexer.currentToken()->type == TokComma)
                goto doParseObjectStartExpression;
            if (UNLIKELY(m_lexer.currentToken()->type != TokRBrace)) {
                setErrorMessageForToken(TokRBrace);
                return { };
            }
            m_lexer.next();
            lastValue = objectStack.takeLast();
            break;
        }
        startParseExpression:
        case StartParseExpression: {
            TokenType type = m_lexer.currentToken()->type;
            if (type == TokLBracket)
                goto startParseArray;
            if (type == TokLBrace)
                goto startParseObject;
            lastValue = parsePrimitiveValue(vm);
            if (UNLIKELY(!lastValue))
                return { };
            break;
        }
        case StartParseStatement: {
            switch (m_lexer.currentToken()->type) {
            case TokLBracket:
            case TokNumber:
            case TokString: {
                lastValue = parsePrimitiveValue(vm);
                if (UNLIKELY(!lastValue))
                    return { };
                break;
            }

            case TokLParen: {
                m_lexer.next();
                stateStack.append(StartParseStatementEndStatement);
                goto startParseExpression;
            }
            case TokRBracket:
                m_parseErrorMessage = "Unexpected token ']'"_s;
                return { };
            case TokLBrace:
                m_parseErrorMessage = "Unexpected token '{'"_s;
                return { };
            case TokRBrace:
                m_parseErrorMessage = "Unexpected token '}'"_s;
                return { };
            case TokIdentifier:
                m_parseErrorMessage = "Unexpected identifier"_s;
                return { };
            case TokColon:
                m_parseErrorMessage = "Unexpected token ':'"_s;
                return { };
            case TokRParen:
                m_parseErrorMessage = "Unexpected token ')'"_s;
                return { };
            case TokComma:
                m_parseErrorMessage = "Unexpected token ','"_s;
                return { };
            case TokTrue:
                m_parseErrorMessage = "Unexpected token 'true'"_s;
                return { };
            case TokFalse:
                m_parseErrorMessage = "Unexpected token 'false'"_s;
                return { };
            case TokNull:
                m_parseErrorMessage = "Unexpected token 'null'"_s;
                return { };
            case TokEnd:
                m_parseErrorMessage = "Unexpected EOF"_s;
                return { };
            case TokDot:
                m_parseErrorMessage = "Unexpected token '.'"_s;
                return { };
            case TokAssign:
                m_parseErrorMessage = "Unexpected token '='"_s;
                return { };
            case TokSemi:
                m_parseErrorMessage = "Unexpected token ';'"_s;
                return { };
            case TokError:
            default:
                m_parseErrorMessage = "Could not parse statement"_s;
                return { };
            }
            break;
        }
        case StartParseStatementEndStatement: {
            ASSERT(stateStack.isEmpty());
            if (m_lexer.currentToken()->type != TokRParen)
                return { };
            if (m_lexer.next() == TokEnd)
                return lastValue;
            m_parseErrorMessage = "Unexpected content at end of JSON literal"_s;
            return { };
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        if (stateStack.isEmpty())
            return lastValue;
        state = stateStack.takeLast();
        continue;
    }
}

// Instantiate the two flavors of LiteralParser we need instead of putting most of this file in LiteralParser.h
template class LiteralParser<LChar>;
template class LiteralParser<UChar>;

}
