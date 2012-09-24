/*
 * Copyright 2011 Google Inc. All rights reserved.
 * Copyright 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file is intended to be included in another C++ file where the character
// types are defined. This allows us to write mostly generic code, but not have
// templace bloat because everything is inlined when anybody calls any of our
// functions.

#ifndef URLCanonInternal_h
#define URLCanonInternal_h

#include "URLCanon.h"
#include "URLCharacterTypes.h"
#include <stdlib.h>
#include <wtf/HexNumber.h>

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

// Appends the given string to the output, escaping characters that do not
// match the given |type| in SharedCharTypes.
void appendStringOfType(const char* source, int length, URLCharacterTypes::CharacterTypes urlComponentType, URLBuffer<char>& output);
void appendStringOfType(const UChar* source, int length, URLCharacterTypes::CharacterTypes urlComponentType, URLBuffer<char>& output);

// This lookup table allows fast conversion between ASCII hex letters and their
// corresponding numerical value. The 8-bit range is divided up into 8
// regions of 0x20 characters each. Each of the three character types (numbers,
// uppercase, lowercase) falls into different regions of this range. The table
// contains the amount to subtract from characters in that range to get at
// the corresponding numerical value.
//
// See HexDigitToValue for the lookup.
extern const char kCharToHexLookup[8];

// Assumes the input is a valid hex digit! Call isHexChar before using this.
inline unsigned char hexCharToValue(unsigned char character)
{
    return character - kCharToHexLookup[character / 0x20];
}

// Indicates if the given character is a dot or dot equivalent, returning the
// number of characters taken by it. This will be one for a literal dot, 3 for
// an escaped dot. If the character is not a dot, this will return 0.
template<typename CharacterType>
inline int isDot(const CharacterType* spec, int offset, int end)
{
    if (spec[offset] == '.')
        return 1;

    if (spec[offset] == '%' && offset + 3 <= end && spec[offset + 1] == '2' && (spec[offset + 2] == 'e' || spec[offset + 2] == 'E')) {
        // Found "%2e"
        return 3;
    }
    return 0;
}

// Returns the canonicalized version of the input character according to scheme
// rules. This is implemented alongside the scheme canonicalizer, and is
// required for relative URL resolving to test for scheme equality.
//
// Returns 0 if the input character is not a valid scheme character.
char canonicalSchemeChar(UChar);

// Write a single character, escaped, to the output. This always escapes: it
// does no checking that thee character requires escaping.
// Escaping makes sense only 8 bit chars, so code works in all cases of
// input parameters (8/16bit).
template<typename InChar, typename OutChar>
inline void appendURLEscapedCharacter(InChar character, URLBuffer<OutChar>& buffer)
{
    buffer.append('%');
    buffer.append(WTF::Internal::upperHexDigits[character >> 4]);
    buffer.append(WTF::Internal::upperHexDigits[character & 0xf]);
}

// The character we'll substitute for undecodable or invalid characters.
extern const UChar kUnicodeReplacementCharacter;

// UTF-8 functions ------------------------------------------------------------

// Reads one character in UTF-8 starting at |*begin| in |str| and places
// the decoded value into |*codePoint|. If the character is valid, we will
// return true. If invalid, we'll return false and put the
// kUnicodeReplacementCharacter into |*codePoint|.
//
// |*begin| will be updated to point to the last character consumed so it
// can be incremented in a loop and will be ready for the next character.
// (for a single-byte ASCII character, it will not be changed).
//
// Implementation is in URLCanonicalizer_icu.cc.
bool readUTFChar(const char* str, int* begin, int length, unsigned* codePointOut);

// Generic To-UTF-8 converter. This will call the given append method for each
// character that should be appended, with the given output method. Wrappers
// are provided below for escaped and non-escaped versions of this.
//
// The charactervalue must have already been checked that it's a valid Unicode
// character.
template<class Output, void appendFunction(unsigned char, Output&)>
inline void doAppendUTF8(unsigned charactervalue, Output& output)
{
    if (charactervalue <= 0x7f) {
        appendFunction(static_cast<unsigned char>(charactervalue), output);
    } else if (charactervalue <= 0x7ff) {
        // 110xxxxx 10xxxxxx
        appendFunction(static_cast<unsigned char>(0xC0 | (charactervalue >> 6)), output);
        appendFunction(static_cast<unsigned char>(0x80 | (charactervalue & 0x3f)), output);
    } else if (charactervalue <= 0xffff) {
        // 1110xxxx 10xxxxxx 10xxxxxx
        appendFunction(static_cast<unsigned char>(0xe0 | (charactervalue >> 12)), output);
        appendFunction(static_cast<unsigned char>(0x80 | ((charactervalue >> 6) & 0x3f)), output);
        appendFunction(static_cast<unsigned char>(0x80 | (charactervalue & 0x3f)), output);
    } else if (charactervalue <= 0x10FFFF) { // Max unicode code point.
        // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        appendFunction(static_cast<unsigned char>(0xf0 | (charactervalue >> 18)), output);
        appendFunction(static_cast<unsigned char>(0x80 | ((charactervalue >> 12) & 0x3f)), output);
        appendFunction(static_cast<unsigned char>(0x80 | ((charactervalue >> 6) & 0x3f)), output);
        appendFunction(static_cast<unsigned char>(0x80 | (charactervalue & 0x3f)), output);
    } else {
        // Invalid UTF-8 character (>20 bits).
        ASSERT_NOT_REACHED();
    }
}

// Helper used by AppendUTF8Value below. We use an unsigned parameter so there
// are no funny sign problems with the input, but then have to convert it to
// a regular char for appending.
inline void AppendCharToOutput(unsigned char character, URLBuffer<char>& output)
{
    output.append(static_cast<char>(character));
}

// Writes the given character to the output as UTF-8. This does NO checking
// of the validity of the unicode characters; the caller should ensure that
// the value it is appending is valid to append.
inline void AppendUTF8Value(unsigned charactervalue, URLBuffer<char>& output)
{
    doAppendUTF8<URLBuffer<char>, AppendCharToOutput>(charactervalue, output);
}

// Writes the given character to the output as UTF-8, escaping ALL
// characters (even when they are ASCII). This does NO checking of the
// validity of the unicode characters; the caller should ensure that the value
// it is appending is valid to append.
inline void AppendUTF8EscapedValue(unsigned charactervalue, URLBuffer<char>& output)
{
    doAppendUTF8<URLBuffer<char>, appendURLEscapedCharacter>(charactervalue, output);
}

// UTF-16 functions -----------------------------------------------------------

// Reads one character in UTF-16 starting at |*begin| in |str| and places
// the decoded value into |*codePoint|. If the character is valid, we will
// return true. If invalid, we'll return false and put the
// kUnicodeReplacementCharacter into |*codePoint|.
//
// |*begin| will be updated to point to the last character consumed so it
// can be incremented in a loop and will be ready for the next character.
// (for a single-16-bit-word character, it will not be changed).
//
// Implementation is in URLCanonicalizer_icu.cc.
bool readUTFChar(const UChar* str, int* begin, int length, unsigned* codePoint);

// Equivalent to U16_APPEND_UNSAFE in ICU but uses our output method.
inline void AppendUTF16Value(unsigned codePoint, URLBuffer<UChar>& output)
{
    if (codePoint > 0xffff) {
        output.append(static_cast<UChar>((codePoint >> 10) + 0xd7c0));
        output.append(static_cast<UChar>((codePoint & 0x3ff) | 0xdc00));
    } else
        output.append(static_cast<UChar>(codePoint));
}

// Escaping functions ---------------------------------------------------------

// Writes the given character to the output as UTF-8, escaped. Call this
// function only when the input is wide. Returns true on success. Failure
// means there was some problem with the encoding, we'll still try to
// update the |*begin| pointer and add a placeholder character to the
// output so processing can continue.
//
// We will append the character starting at ch[begin] with the buffer ch
// being |length|. |*begin| will be updated to point to the last character
// consumed (we may consume more than one for UTF-16) so that if called in
// a loop, incrementing the pointer will move to the next character.
//
// Every single output character will be escaped. This means that if you
// give it an ASCII character as input, it will be escaped. Some code uses
// this when it knows that a character is invalid according to its rules
// for validity. If you don't want escaping for ASCII characters, you will
// have to filter them out prior to calling this function.
//
// Assumes that ch[begin] is within range in the array, but does not assume
// that any following characters are.
inline bool AppendUTF8EscapedChar(const UChar* str, int* begin, int length, URLBuffer<char>& output)
{
    // UTF-16 input. ReadUChar will handle invalid characters for us and give
    // us the kUnicodeReplacementCharacter, so we don't have to do special
    // checking after failure, just pass through the failure to the caller.
    unsigned charactervalue;
    bool success = readUTFChar(str, begin, length, &charactervalue);
    AppendUTF8EscapedValue(charactervalue, output);
    return success;
}

// Handles UTF-8 input. See the wide version above for usage.
inline bool AppendUTF8EscapedChar(const char* str, int* begin, int length, URLBuffer<char>& output)
{
    // ReadUTF8Char will handle invalid characters for us and give us the
    // kUnicodeReplacementCharacter, so we don't have to do special checking
    // after failure, just pass through the failure to the caller.
    unsigned ch;
    bool success = readUTFChar(str, begin, length, &ch);
    AppendUTF8EscapedValue(ch, output);
    return success;
}

// Given a '%' character at |*begin| in the string |spec|, this will decode
// the escaped value and put it into |*unescapedValue| on success (returns
// true). On failure, this will return false, and will not write into
// |*unescapedValue|.
//
// |*begin| will be updated to point to the last character of the escape
// sequence so that when called with the index of a for loop, the next time
// through it will point to the next character to be considered. On failure,
// |*begin| will be unchanged.
inline bool Is8BitChar(char)
{
    return true; // this case is specialized to avoid a warning
}
inline bool Is8BitChar(UChar c)
{
    return c <= 255;
}

template<typename CHAR>
inline bool DecodeEscaped(const CHAR* spec, int* begin, int end, unsigned char* unescapedValue)
{
    if (*begin + 3 > end || !Is8BitChar(spec[*begin + 1]) || !Is8BitChar(spec[*begin + 2])) {
        // Invalid escape sequence because there's not enough room, or the
        // digits are not ASCII.
        return false;
    }

    unsigned char first = static_cast<unsigned char>(spec[*begin + 1]);
    unsigned char second = static_cast<unsigned char>(spec[*begin + 2]);
    if (!URLCharacterTypes::isHexChar(first) || !URLCharacterTypes::isHexChar(second)) {
        // Invalid hex digits, fail.
        return false;
    }

    // Valid escape sequence.
    *unescapedValue = (hexCharToValue(first) << 4) + hexCharToValue(second);
    *begin += 2;
    return true;
}

// Appends the given substring to the output, escaping "some" characters that
// it feels may not be safe. It assumes the input values are all contained in
// 8-bit although it allows any type.
//
// This is used in error cases to append invalid output so that it looks
// approximately correct. Non-error cases should not call this function since
// the escaping rules are not guaranteed!
void AppendInvalidNarrowString(const char* spec, int begin, int end, URLBuffer<char>& output);
void AppendInvalidNarrowString(const UChar* spec, int begin, int end, URLBuffer<char>& output);

// Misc canonicalization helpers ----------------------------------------------

// Converts between UTF-8 and UTF-16, returning true on successful conversion.
// The output will be appended to the given canonicalizer output (so make sure
// it's empty if you want to replace).
//
// On invalid input, this will still write as much output as possible,
// replacing the invalid characters with the "invalid character". It will
// return false in the failure case, and the caller should not continue as
// normal.
bool ConvertUTF16ToUTF8(const UChar* input, int inputLength, URLBuffer<char>& output);
bool ConvertUTF8ToUTF16(const char* input, int inputLength, URLBuffer<UChar>& output);

// Converts from UTF-16 to 8-bit using the character set converter. If the
// converter is null, this will use UTF-8.
void ConvertUTF16ToQueryEncoding(const UChar* input, const URLComponent& query, CharsetConverter*, URLBuffer<char>& output);

// Applies the replacements to the given component source. The component source
// should be pre-initialized to the "old" base. That is, all pointers will
// point to the spec of the old URL, and all of the Parsed components will
// be indices into that string.
//
// The pointers and components in the |source| for all non-null strings in the
// |repl| (replacements) will be updated to reference those strings.
// Canonicalizing with the new |source| and |parsed| can then combine URL
// components from many different strings.
void SetupOverrideComponents(const char* base,
                             const Replacements<char>&,
                             URLComponentSource<char>*,
                             URLSegments* parsed);

// Like the above 8-bit version, except that it additionally converts the
// UTF-16 input to UTF-8 before doing the overrides.
//
// The given utf8Buffer is used to store the converted components. They will
// be appended one after another, with the parsed structure identifying the
// appropriate substrings. This buffer is a parameter because the source has
// no storage, so the buffer must have the same lifetime as the source
// parameter owned by the caller.
//
// THE CALLER MUST NOT ADD TO THE |utf8Buffer| AFTER THIS CALL. Members of
// |source| will point into this buffer, which could be invalidated if
// additional data is added and the CanonOutput resizes its buffer.
//
// Returns true on success. Fales means that the input was not valid UTF-16,
// although we will have still done the override with "invalid characters" in
// place of errors.
bool SetupUTF16OverrideComponents(const char* base,
                                  const Replacements<UChar>&,
                                  URLBuffer<char>& utf8Buffer,
                                  URLComponentSource<char>*,
                                  URLSegments* parsed);

// Implemented in URLCanonicalizer_path.cc, these are required by the relative URL
// resolver as well, so we declare them here.
bool CanonicalizePartialPath(const char* spec,
                             const URLComponent& path,
                             int pathBeginInOutput,
                             URLBuffer<char>& output);
bool CanonicalizePartialPath(const UChar* spec,
                             const URLComponent& path,
                             int pathBeginInOutput,
                             URLBuffer<char>& output);

#if !OS(WINDOWS)
// Implementations of Windows' int-to-string conversions
int _itoa_s(int value, char* buffer, size_t sizeInChars, int radix);
int _itow_s(int value, UChar* buffer, size_t sizeInChars, int radix);

// Secure template overloads for these functions
template<size_t N>
inline int _itoa_s(int value, char (&buffer)[N], int radix)
{
    return _itoa_s(value, buffer, N, radix);
}

template<size_t N>
inline int _itow_s(int value, UChar (&buffer)[N], int radix)
{
    return _itow_s(value, buffer, N, radix);
}

// _strtoui64 and strtoull behave the same
inline unsigned long long _strtoui64(const char* nptr, char** endptr, int base)
{
    return strtoull(nptr, endptr, base);
}
#endif // OS(WINDOWS)

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)

#endif // URLCanonInternal_h
