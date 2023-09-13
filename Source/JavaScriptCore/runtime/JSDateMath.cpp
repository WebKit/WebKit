/*
 * Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010 &yet, LLC. (nate@andyet.net)
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.

 * Copyright 2006-2012 the V8 project authors. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Google Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#include "config.h"
#include "JSDateMath.h"

#include "ExceptionHelpers.h"
#include "VM.h"
#include <limits>
#include <wtf/DateMath.h>
#include <wtf/Language.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#if U_ICU_VERSION_MAJOR_NUM >= 69 || (U_ICU_VERSION_MAJOR_NUM == 68 && USE(APPLE_INTERNAL_SDK))
#define HAVE_ICU_C_TIMEZONE_API 1
#ifdef U_HIDE_DRAFT_API
#undef U_HIDE_DRAFT_API
#endif
#include <unicode/ucal.h>
#else
// icu::TimeZone and icu::BasicTimeZone features are only available in ICU C++ APIs.
// We use these C++ APIs as an exception.
#undef U_SHOW_CPLUSPLUS_API
#define U_SHOW_CPLUSPLUS_API 1
#include <unicode/basictz.h>
#include <unicode/locid.h>
#include <unicode/timezone.h>
#include <unicode/unistr.h>
#undef U_SHOW_CPLUSPLUS_API
#define U_SHOW_CPLUSPLUS_API 0
#endif

namespace JSC {
constexpr int kMaxInt = 0x7FFFFFFF;

constexpr intptr_t kIntptrAllBitsSet = intptr_t { -1 };
constexpr uintptr_t kUintptrAllBitsSet = static_cast<uintptr_t>(kIntptrAllBitsSet);
static constexpr intptr_t kSmiMinValue = static_cast<intptr_t>(kUintptrAllBitsSet << (32 - 1));
static constexpr intptr_t kSmiMaxValue = -(kSmiMinValue + 1);
static constexpr int kMaxValue = static_cast<int>(kSmiMaxValue);

template<typename T, typename U>
inline constexpr bool IsInRange(T value, U lower_limit, U higher_limit)
{
    // DCHECK_LE(lower_limit, higher_limit);
    static_assert(sizeof(U) <= sizeof(T));
    using unsigned_T = typename std::make_unsigned<T>::type;
    // Use static_cast to support enum classes.
    return static_cast<unsigned_T>(static_cast<unsigned_T>(value) - static_cast<unsigned_T>(lower_limit)) <= static_cast<unsigned_T>(static_cast<unsigned_T>(higher_limit) - static_cast<unsigned_T>(lower_limit));
}

inline constexpr int AsciiAlphaToLower(uint32_t c) { return c | 0x20; }

inline constexpr bool IsDecimalDigit(uint32_t c)
{
    // ECMA-262, 3rd, 7.8.3 (p 16)
    return IsInRange(c, '0', '9');
}

bool IsLineTerminator(unsigned int c)
{
    return c == 0x000A || c == 0x000D || c == 0x2028 || c == 0x2029;
}

/***** src/date/dateparser.h *****/
class DateParser {
public:
    enum {
        YEAR,
        MONTH,
        DAY,
        HOUR,
        MINUTE,
        SECOND,
        MILLISECOND,
        UTC_OFFSET,
        OUTPUT_SIZE
    };

    // Parse the string as a date. If parsing succeeds, return true after
    // filling out the output array as follows (all integers are Smis):
    // [0]: year
    // [1]: month (0 = Jan, 1 = Feb, ...)
    // [2]: day
    // [3]: hour
    // [4]: minute
    // [5]: second
    // [6]: millisecond
    // [7]: UTC offset in seconds, or null value if no timezone specified
    // If parsing fails, return false (content of output array is not defined).
    // template<typename Char>
    // static bool Parse(Isolate* isolate, base::Vector<Char> str, double* output);
    static bool Parse(void* isolate, const char* str, size_t size, double* output);

private:
    // Range testing
    static inline bool Between(int x, int lo, int hi)
    {
        return static_cast<unsigned>(x - lo) <= static_cast<unsigned>(hi - lo);
    }

    // Indicates a missing value.
    static const int kNone = kMaxInt;

    // Maximal number of digits used to build the value of a numeral.
    // Remaining digits are ignored.
    static const int kMaxSignificantDigits = 9;

    // InputReader provides basic string parsing and character classification.
    // template<typename Char>
    class InputReader {
    public:
        explicit InputReader(const char* s, size_t size)
            : index_(0)
            , buffer_(s, size)
        {
            Next();
        }

        int position() { return index_; }

        // Advance to the next character of the string.
        void Next()
        {
            ch_ = (index_ < static_cast<int>(buffer_.size())) ? buffer_[index_] : 0;
            index_++;
        }

        // Read a string of digits as an unsigned number. Cap value at
        // kMaxSignificantDigits, but skip remaining digits if the numeral
        // is longer.
        int ReadUnsignedNumeral()
        {
            int n = 0;
            int i = 0;
            // First, skip leading zeros
            while (ch_ == '0')
                Next();
            // And then, do the conversion
            while (IsAsciiDigit()) {
                if (i < kMaxSignificantDigits)
                    n = n * 10 + ch_ - '0';
                i++;
                Next();
            }
            return n;
        }

        // Read a word (sequence of chars. >= 'A'), fill the given buffer with a
        // lower-case prefix, and pad any remainder of the buffer with zeroes.
        // Return word length.
        int ReadWord(uint32_t* prefix, int prefix_size)
        {
            int len;
            for (len = 0; IsAsciiAlphaOrAbove() && !IsWhiteSpaceChar();
                 Next(), len++) {
                if (len < prefix_size)
                    prefix[len] = AsciiAlphaToLower(ch_);
            }
            for (int i = len; i < prefix_size; i++)
                prefix[i] = 0;
            return len;
        }

        // The skip methods return whether they actually skipped something.
        bool Skip(uint32_t c)
        {
            if (ch_ == c) {
                Next();
                return true;
            }
            return false;
        }

        inline bool SkipWhiteSpace();
        inline bool SkipParentheses();

        // Character testing/classification. Non-ASCII digits are not supported.
        bool Is(uint32_t c) const { return ch_ == c; }
        bool IsEnd() const { return ch_ == 0; }
        bool IsAsciiDigit() const { return IsDecimalDigit(ch_); }
        bool IsAsciiAlphaOrAbove() const { return ch_ >= 'A'; }
        bool IsWhiteSpaceChar() const { return isASCIIWhitespace(ch_); }
        bool IsAsciiSign() const { return ch_ == '+' || ch_ == '-'; }

        // Return 1 for '+' and -1 for '-'.
        int GetAsciiSignValue() const { return 44 - static_cast<int>(ch_); }

    private:
        int index_;
        WTF::Vector<char> buffer_;
        uint32_t ch_;
    };

    enum KeywordType {
        INVALID,
        MONTH_NAME,
        TIME_ZONE_NAME,
        TIME_SEPARATOR,
        AM_PM
    };

    struct DateToken {
    public:
        bool IsInvalid() { return tag_ == kInvalidTokenTag; }
        bool IsUnknown() { return tag_ == kUnknownTokenTag; }
        bool IsNumber() { return tag_ == kNumberTag; }
        bool IsSymbol() { return tag_ == kSymbolTag; }
        bool IsWhiteSpace() { return tag_ == kWhiteSpaceTag; }
        bool IsEndOfInput() { return tag_ == kEndOfInputTag; }
        bool IsKeyword() { return tag_ >= kKeywordTagStart; }

        int length() { return length_; }

        int number()
        {
            // DCHECK(IsNumber());
            return value_;
        }
        KeywordType keyword_type()
        {
            // DCHECK(IsKeyword());
            return static_cast<KeywordType>(tag_);
        }
        int keyword_value()
        {
            // DCHECK(IsKeyword());
            return value_;
        }
        char symbol()
        {
            // DCHECK(IsSymbol());
            return static_cast<char>(value_);
        }
        bool IsSymbol(char symbol)
        {
            return IsSymbol() && this->symbol() == symbol;
        }
        bool IsKeywordType(KeywordType tag) { return tag_ == tag; }
        bool IsFixedLengthNumber(int length)
        {
            return IsNumber() && length_ == length;
        }
        bool IsAsciiSign()
        {
            return tag_ == kSymbolTag && (value_ == '-' || value_ == '+');
        }
        int ascii_sign()
        {
            // DCHECK(IsAsciiSign());
            return 44 - value_;
        }
        bool IsKeywordZ()
        {
            return IsKeywordType(TIME_ZONE_NAME) && length_ == 1 && value_ == 0;
        }
        bool IsUnknown(int character) { return IsUnknown() && value_ == character; }
        // Factory functions.
        static DateToken Keyword(KeywordType tag, int value, int length)
        {
            return DateToken(tag, length, value);
        }
        static DateToken Number(int value, int length)
        {
            return DateToken(kNumberTag, length, value);
        }
        static DateToken Symbol(char symbol)
        {
            return DateToken(kSymbolTag, 1, symbol);
        }
        static DateToken EndOfInput() { return DateToken(kEndOfInputTag, 0, -1); }
        static DateToken WhiteSpace(int length)
        {
            return DateToken(kWhiteSpaceTag, length, -1);
        }
        static DateToken Unknown() { return DateToken(kUnknownTokenTag, 1, -1); }
        static DateToken Invalid() { return DateToken(kInvalidTokenTag, 0, -1); }

    private:
        enum TagType {
            kInvalidTokenTag = -6,
            kUnknownTokenTag = -5,
            kWhiteSpaceTag = -4,
            kNumberTag = -3,
            kSymbolTag = -2,
            kEndOfInputTag = -1,
            kKeywordTagStart = 0
        };
        DateToken(int tag, int length, int value)
            : tag_(tag)
            , length_(length)
            , value_(value)
        {
        }

        int tag_;
        int length_; // Number of characters.
        int value_;
    };

    // template<typename char>
    class DateStringTokenizer {
    public:
        explicit DateStringTokenizer(InputReader* in)
            : in_(in)
            , next_(Scan())
        {
        }
        DateToken Next()
        {
            DateToken result = next_;
            next_ = Scan();
            return result;
        }

        DateToken Peek() { return next_; }
        bool SkipSymbol(char symbol)
        {
            if (next_.IsSymbol(symbol)) {
                next_ = Scan();
                return true;
            }
            return false;
        }

    private:
        DateToken Scan();

        InputReader* in_;
        DateToken next_;
    };

    static int ReadMilliseconds(DateToken number);

    // KeywordTable maps names of months, time zones, am/pm to numbers.
    class KeywordTable {
    public:
        // Look up a word in the keyword table and return an index.
        // 'pre' contains a prefix of the word, zero-padded to size kPrefixLength
        // and 'len' is the word length.
        static int Lookup(const uint32_t* pre, int len);
        // Get the type of the keyword at index i.
        static KeywordType GetType(int i)
        {
            return static_cast<KeywordType>(array[i][kTypeOffset]);
        }
        // Get the value of the keyword at index i.
        static int GetValue(int i) { return array[i][kValueOffset]; }

        static const int kPrefixLength = 3;
        static const int kTypeOffset = kPrefixLength;
        static const int kValueOffset = kTypeOffset + 1;
        static const int kEntrySize = kValueOffset + 1;
        static const int8_t array[][kEntrySize];
    };

    class TimeZoneComposer {
    public:
        TimeZoneComposer()
            : sign_(kNone)
            , hour_(kNone)
            , minute_(kNone)
        {
        }
        void Set(int offset_in_hours)
        {
            sign_ = offset_in_hours < 0 ? -1 : 1;
            hour_ = offset_in_hours * sign_;
            minute_ = 0;
        }
        void SetSign(int sign) { sign_ = sign < 0 ? -1 : 1; }
        void SetAbsoluteHour(int hour) { hour_ = hour; }
        void SetAbsoluteMinute(int minute) { minute_ = minute; }
        bool IsExpecting(int n) const
        {
            return hour_ != kNone && minute_ == kNone && TimeComposer::IsMinute(n);
        }
        bool IsUTC() const { return hour_ == 0 && minute_ == 0; }
        bool Write(double* output);
        bool IsEmpty() { return hour_ == kNone; }

    private:
        int sign_;
        int hour_;
        int minute_;
    };

    class TimeComposer {
    public:
        TimeComposer()
            : index_(0)
            , hour_offset_(kNone)
        {
        }
        bool IsEmpty() const { return index_ == 0; }
        bool IsExpecting(int n) const
        {
            return (index_ == 1 && IsMinute(n)) || (index_ == 2 && IsSecond(n)) || (index_ == 3 && IsMillisecond(n));
        }
        bool Add(int n)
        {
            return index_ < kSize ? (comp_[index_++] = n, true) : false;
        }
        bool AddFinal(int n)
        {
            if (!Add(n))
                return false;
            while (index_ < kSize)
                comp_[index_++] = 0;
            return true;
        }
        void SetHourOffset(int n) { hour_offset_ = n; }
        bool Write(double* output);

        static bool IsMinute(int x) { return Between(x, 0, 59); }
        static bool IsHour(int x) { return Between(x, 0, 23); }
        static bool IsSecond(int x) { return Between(x, 0, 59); }

    private:
        static bool IsHour12(int x) { return Between(x, 0, 12); }
        static bool IsMillisecond(int x) { return Between(x, 0, 999); }

        static const int kSize = 4;
        int comp_[kSize];
        int index_;
        int hour_offset_;
    };

    class DayComposer {
    public:
        DayComposer()
            : index_(0)
            , named_month_(kNone)
            , is_iso_date_(false)
        {
        }
        bool IsEmpty() const { return index_ == 0; }
        bool Add(int n)
        {
            if (index_ < kSize) {
                comp_[index_] = n;
                index_++;
                return true;
            }
            return false;
        }
        void SetNamedMonth(int n) { named_month_ = n; }
        bool Write(double* output);
        void set_iso_date() { is_iso_date_ = true; }
        static bool IsMonth(int x) { return Between(x, 1, 12); }
        static bool IsDay(int x) { return Between(x, 1, 31); }

    private:
        static const int kSize = 3;
        int comp_[kSize];
        int index_;
        int named_month_;
        // If set, ensures that data is always parsed in year-month-date order.
        bool is_iso_date_;
    };

    // Tries to parse an ES5 Date Time String. Returns the next token
    // to continue with in the legacy date string parser. If parsing is
    // complete, returns DateToken::EndOfInput(). If terminally unsuccessful,
    // returns DateToken::Invalid(). Otherwise parsing continues in the
    // legacy parser.
    // template<typename Char>
    static DateParser::DateToken ParseES5DateTime(
        DateStringTokenizer* scanner, DayComposer* day, TimeComposer* time,
        TimeZoneComposer* tz);
};

/***** src/date/dateparser.cc *****/

bool DateParser::DayComposer::Write(double* output)
{
    if (index_ < 1)
        return false;
    // Day and month defaults to 1.
    while (index_ < kSize) {
        comp_[index_++] = 1;
    }

    int year = 0; // Default year is 0 (=> 2000) for KJS compatibility.
    int month = kNone;
    int day = kNone;

    if (named_month_ == kNone) {
        if (is_iso_date_ || (index_ == 3 && !IsDay(comp_[0]))) {
            // YMD
            year = comp_[0];
            month = comp_[1];
            day = comp_[2];
        } else {
            // MD(Y)
            month = comp_[0];
            day = comp_[1];
            if (index_ == 3)
                year = comp_[2];
        }
    } else {
        month = named_month_;
        if (index_ == 1) {
            // MD or DM
            day = comp_[0];
        } else if (!IsDay(comp_[0])) {
            // YMD, MYD, or YDM
            year = comp_[0];
            day = comp_[1];
        } else {
            // DMY, MDY, or DYM
            day = comp_[0];
            year = comp_[1];
        }
    }

    if (!is_iso_date_) {
        if (Between(year, 0, 49))
            year += 2000;
        else if (Between(year, 50, 99))
            year += 1900;
    }

    // if (!Smi::IsValid(year) || !IsMonth(month) || !IsDay(day))
    //     return false;
    if (year != static_cast<int32_t>(year) || !IsMonth(month) || !IsDay(day))
        return false;

    output[YEAR] = year;
    output[MONTH] = month - 1; // 0-based
    output[DAY] = day;
    return true;
}

bool DateParser::TimeComposer::Write(double* output)
{
    // All time slots default to 0
    while (index_ < kSize) {
        comp_[index_++] = 0;
    }

    int& hour = comp_[0];
    int& minute = comp_[1];
    int& second = comp_[2];
    int& millisecond = comp_[3];

    if (hour_offset_ != kNone) {
        if (!IsHour12(hour))
            return false;
        hour %= 12;
        hour += hour_offset_;
    }

    if (!IsHour(hour) || !IsMinute(minute) || !IsSecond(second) || !IsMillisecond(millisecond)) {
        // A 24th hour is allowed if minutes, seconds, and milliseconds are 0
        if (hour != 24 || minute != 0 || second != 0 || millisecond != 0) {
            return false;
        }
    }

    output[HOUR] = hour;
    output[MINUTE] = minute;
    output[SECOND] = second;
    output[MILLISECOND] = millisecond;
    return true;
}

bool DateParser::TimeZoneComposer::Write(double* output)
{
    if (sign_ != kNone) {
        if (hour_ == kNone)
            hour_ = 0;
        if (minute_ == kNone)
            minute_ = 0;
        // Avoid signed integer overflow (undefined behavior) by doing unsigned
        // arithmetic.
        unsigned total_seconds_unsigned = hour_ * 3600U + minute_ * 60U;
        if (total_seconds_unsigned > kMaxValue)
            return false;
        int total_seconds = static_cast<int>(total_seconds_unsigned);
        if (sign_ < 0) {
            total_seconds = -total_seconds;
        }
        // DCHECK(Smi::IsValid(total_seconds));
        output[UTC_OFFSET] = total_seconds;
    } else {
        output[UTC_OFFSET] = std::numeric_limits<double>::quiet_NaN();
    }
    return true;
}

const int8_t
    DateParser::KeywordTable::array[][DateParser::KeywordTable::kEntrySize]
    = {
          { 'j', 'a', 'n', DateParser::MONTH_NAME, 1 },
          { 'f', 'e', 'b', DateParser::MONTH_NAME, 2 },
          { 'm', 'a', 'r', DateParser::MONTH_NAME, 3 },
          { 'a', 'p', 'r', DateParser::MONTH_NAME, 4 },
          { 'm', 'a', 'y', DateParser::MONTH_NAME, 5 },
          { 'j', 'u', 'n', DateParser::MONTH_NAME, 6 },
          { 'j', 'u', 'l', DateParser::MONTH_NAME, 7 },
          { 'a', 'u', 'g', DateParser::MONTH_NAME, 8 },
          { 's', 'e', 'p', DateParser::MONTH_NAME, 9 },
          { 'o', 'c', 't', DateParser::MONTH_NAME, 10 },
          { 'n', 'o', 'v', DateParser::MONTH_NAME, 11 },
          { 'd', 'e', 'c', DateParser::MONTH_NAME, 12 },
          { 'a', 'm', '\0', DateParser::AM_PM, 0 },
          { 'p', 'm', '\0', DateParser::AM_PM, 12 },
          { 'u', 't', '\0', DateParser::TIME_ZONE_NAME, 0 },
          { 'u', 't', 'c', DateParser::TIME_ZONE_NAME, 0 },
          { 'z', '\0', '\0', DateParser::TIME_ZONE_NAME, 0 },
          { 'g', 'm', 't', DateParser::TIME_ZONE_NAME, 0 },
          { 'c', 'd', 't', DateParser::TIME_ZONE_NAME, -5 },
          { 'c', 's', 't', DateParser::TIME_ZONE_NAME, -6 },
          { 'e', 'd', 't', DateParser::TIME_ZONE_NAME, -4 },
          { 'e', 's', 't', DateParser::TIME_ZONE_NAME, -5 },
          { 'm', 'd', 't', DateParser::TIME_ZONE_NAME, -6 },
          { 'm', 's', 't', DateParser::TIME_ZONE_NAME, -7 },
          { 'p', 'd', 't', DateParser::TIME_ZONE_NAME, -7 },
          { 'p', 's', 't', DateParser::TIME_ZONE_NAME, -8 },
          { 't', '\0', '\0', DateParser::TIME_SEPARATOR, 0 },
          { '\0', '\0', '\0', DateParser::INVALID, 0 },
      };

// We could use perfect hashing here, but this is not a bottleneck.
int DateParser::KeywordTable::Lookup(const uint32_t* pre, int len)
{
    int i;
    for (i = 0; array[i][kTypeOffset] != INVALID; i++) {
        int j = 0;
        while (j < kPrefixLength && pre[j] == static_cast<uint32_t>(array[i][j])) {
            j++;
        }
        // Check if we have a match and the length is legal.
        // Word longer than keyword is only allowed for month names.
        if (j == kPrefixLength && (len <= kPrefixLength || array[i][kTypeOffset] == MONTH_NAME)) {
            return i;
        }
    }
    return i;
}

int DateParser::ReadMilliseconds(DateToken token)
{
    // Read first three significant digits of the original numeral,
    // as inferred from the value and the number of digits.
    // I.e., use the number of digits to see if there were
    // leading zeros.
    int number = token.number();
    int length = token.length();
    if (length < 3) {
        // Less than three digits. Multiply to put most significant digit
        // in hundreds position.
        if (length == 1) {
            number *= 100;
        } else if (length == 2) {
            number *= 10;
        }
    } else if (length > 3) {
        if (length > kMaxSignificantDigits)
            length = kMaxSignificantDigits;
        // More than three digits. Divide by 10^(length - 3) to get three
        // most significant digits.
        int factor = 1;
        do {
            // DCHECK_LE(factor, 100000000); // factor won't overflow.
            factor *= 10;
            length--;
        } while (length > 3);
        number /= factor;
    }
    return number;
}

/***** src/date/dataparser-inl.h *****/

// template<typename Char>
bool DateParser::Parse(void* isolate, const char* str, size_t size, double* out)
{
    InputReader in(str, size);
    DateStringTokenizer scanner(&in);
    TimeZoneComposer tz;
    TimeComposer time;
    DayComposer day;

    // Specification:
    // Accept ES5 ISO 8601 date-time-strings or legacy dates compatible
    // with Safari.
    // ES5 ISO 8601 dates:
    //   [('-'|'+')yy]yyyy[-MM[-DD]][THH:mm[:ss[.sss]][Z|(+|-)hh:mm]]
    //   where yyyy is in the range 0000..9999 and
    //         +/-yyyyyy is in the range -999999..+999999 -
    //           but -000000 is invalid (year zero must be positive),
    //         MM is in the range 01..12,
    //         DD is in the range 01..31,
    //         MM and DD defaults to 01 if missing,,
    //         HH is generally in the range 00..23, but can be 24 if mm, ss
    //           and sss are zero (or missing), representing midnight at the
    //           end of a day,
    //         mm and ss are in the range 00..59,
    //         sss is in the range 000..999,
    //         hh is in the range 00..23,
    //         mm, ss, and sss default to 00 if missing, and
    //         timezone defaults to Z if missing
    //           (following Safari, ISO actually demands local time).
    //  Extensions:
    //   We also allow sss to have more or less than three digits (but at
    //   least one).
    //   We allow hh:mm to be specified as hhmm.
    // Legacy dates:
    //  Any unrecognized word before the first number is ignored.
    //  Parenthesized text is ignored.
    //  An unsigned number followed by ':' is a time value, and is
    //  added to the TimeComposer. A number followed by '::' adds a second
    //  zero as well. A number followed by '.' is also a time and must be
    //  followed by milliseconds.
    //  Any other number is a date component and is added to DayComposer.
    //  A month name (or really: any word having the same first three letters
    //  as a month name) is recorded as a named month in the Day composer.
    //  A word recognizable as a time-zone is recorded as such, as is
    //  '(+|-)(hhmm|hh:)'.
    //  Legacy dates don't allow extra signs ('+' or '-') or umatched ')'
    //  after a number has been read (before the first number, any garbage
    //  is allowed).
    // Intersection of the two:
    //  A string that matches both formats (e.g. 1970-01-01) will be
    //  parsed as an ES5 date-time string - which means it will default
    //  to UTC time-zone. That's unavoidable if following the ES5
    //  specification.
    //  After a valid "T" has been read while scanning an ES5 datetime string,
    //  the input can no longer be a valid legacy date, since the "T" is a
    //  garbage string after a number has been read.

    // First try getting as far as possible with as ES5 Date Time String.
    DateToken next_unhandled_token = ParseES5DateTime(&scanner, &day, &time, &tz);
    if (next_unhandled_token.IsInvalid())
        return false;
    bool has_read_number = !day.IsEmpty();
    // If there's anything left, continue with the legacy parser.
    bool legacy_parser = false;
    for (DateToken token = next_unhandled_token; !token.IsEndOfInput();
         token = scanner.Next()) {
        if (token.IsNumber()) {
            legacy_parser = true;
            has_read_number = true;
            int n = token.number();
            if (scanner.SkipSymbol(':')) {
                if (scanner.SkipSymbol(':')) {
                    // n + "::"
                    if (!time.IsEmpty())
                        return false;
                    time.Add(n);
                    time.Add(0);
                } else {
                    // n + ":"
                    if (!time.Add(n))
                        return false;
                    if (scanner.Peek().IsSymbol('.'))
                        scanner.Next();
                }
            } else if (scanner.SkipSymbol('.') && time.IsExpecting(n)) {
                time.Add(n);
                if (!scanner.Peek().IsNumber())
                    return false;
                int ms = ReadMilliseconds(scanner.Next());
                if (ms < 0)
                    return false;
                time.AddFinal(ms);
            } else if (tz.IsExpecting(n)) {
                tz.SetAbsoluteMinute(n);
            } else if (time.IsExpecting(n)) {
                time.AddFinal(n);
                // Require end, white space, "Z", "+" or "-" immediately after
                // finalizing time.
                DateToken peek = scanner.Peek();
                if (!peek.IsEndOfInput() && !peek.IsWhiteSpace() && !peek.IsKeywordZ() && !peek.IsAsciiSign())
                    return false;
            } else {
                if (!day.Add(n))
                    return false;
                scanner.SkipSymbol('-');
            }
        } else if (token.IsKeyword()) {
            legacy_parser = true;
            // Parse a "word" (sequence of chars. >= 'A').
            KeywordType type = token.keyword_type();
            int value = token.keyword_value();
            if (type == AM_PM && !time.IsEmpty()) {
                time.SetHourOffset(value);
            } else if (type == MONTH_NAME) {
                day.SetNamedMonth(value);
                scanner.SkipSymbol('-');
            } else if (type == TIME_ZONE_NAME && has_read_number) {
                tz.Set(value);
            } else {
                // Garbage words are illegal if a number has been read.
                if (has_read_number)
                    return false;
                // The first number has to be separated from garbage words by
                // whitespace or other separators.
                if (scanner.Peek().IsNumber())
                    return false;
            }
        } else if (token.IsAsciiSign() && (tz.IsUTC() || !time.IsEmpty())) {
            legacy_parser = true;
            // Parse UTC offset (only after UTC or time).
            tz.SetSign(token.ascii_sign());
            // The following number may be empty.
            int n = 0;
            int length = 0;
            if (scanner.Peek().IsNumber()) {
                DateToken next_token = scanner.Next();
                length = next_token.length();
                n = next_token.number();
            }
            has_read_number = true;

            if (scanner.Peek().IsSymbol(':')) {
                tz.SetAbsoluteHour(n);
                // TODO(littledan): Use minutes as part of timezone?
                tz.SetAbsoluteMinute(kNone);
            } else if (length == 2 || length == 1) {
                // Handle time zones like GMT-8
                tz.SetAbsoluteHour(n);
                tz.SetAbsoluteMinute(0);
            } else if (length == 4 || length == 3) {
                // Looks like the hhmm format
                tz.SetAbsoluteHour(n / 100);
                tz.SetAbsoluteMinute(n % 100);
            } else {
                // No need to accept time zones like GMT-12345
                return false;
            }
        } else if ((token.IsAsciiSign() || token.IsSymbol(')')) && has_read_number) {
            // Extra sign or ')' is illegal if a number has been read.
            return false;
        } else {
            // Ignore other characters and whitespace.
        }
    }

    bool success = day.Write(out) && time.Write(out) && tz.Write(out);

    // if (legacy_parser && success) {
    //     isolate->CountUsage(v8::Isolate::kLegacyDateParser);
    // }

    return success;
}

// template<typename CharType>
DateParser::DateToken DateParser::DateStringTokenizer::Scan()
{
    int pre_pos = in_->position();
    if (in_->IsEnd())
        return DateToken::EndOfInput();
    if (in_->IsAsciiDigit()) {
        int n = in_->ReadUnsignedNumeral();
        int length = in_->position() - pre_pos;
        return DateToken::Number(n, length);
    }
    if (in_->Skip(':'))
        return DateToken::Symbol(':');
    if (in_->Skip('-'))
        return DateToken::Symbol('-');
    if (in_->Skip('+'))
        return DateToken::Symbol('+');
    if (in_->Skip('.'))
        return DateToken::Symbol('.');
    if (in_->Skip(')'))
        return DateToken::Symbol(')');
    if (in_->IsAsciiAlphaOrAbove() && !in_->IsWhiteSpaceChar()) {
        // DCHECK_EQ(KeywordTable::kPrefixLength, 3);
        uint32_t buffer[3] = { 0, 0, 0 };
        int length = in_->ReadWord(buffer, 3);
        int index = KeywordTable::Lookup(buffer, length);
        return DateToken::Keyword(KeywordTable::GetType(index),
            KeywordTable::GetValue(index), length);
    }
    if (in_->SkipWhiteSpace()) {
        return DateToken::WhiteSpace(in_->position() - pre_pos);
    }
    if (in_->SkipParentheses()) {
        return DateToken::Unknown();
    }
    in_->Next();
    return DateToken::Unknown();
}

// template<typename Char>
bool DateParser::InputReader::SkipWhiteSpace()
{
    if (isASCIIWhitespace(ch_) || IsLineTerminator(ch_)) {
        Next();
        return true;
    }
    return false;
}

// template<typename Char>
bool DateParser::InputReader::SkipParentheses()
{
    if (ch_ != '(')
        return false;
    int balance = 0;
    do {
        if (ch_ == ')')
            --balance;
        else if (ch_ == '(')
            ++balance;
        Next();
    } while (balance > 0 && ch_);
    return true;
}

// template<typename Char>
DateParser::DateToken DateParser::ParseES5DateTime(
    DateStringTokenizer* scanner, DayComposer* day, TimeComposer* time,
    TimeZoneComposer* tz)
{
    // DCHECK(day->IsEmpty());
    // DCHECK(time->IsEmpty());
    // DCHECK(tz->IsEmpty());

    // Parse mandatory date string: [('-'|'+')yy]yyyy[':'MM[':'DD]]
    if (scanner->Peek().IsAsciiSign()) {
        // Keep the sign token, so we can pass it back to the legacy
        // parser if we don't use it.
        DateToken sign_token = scanner->Next();
        if (!scanner->Peek().IsFixedLengthNumber(6))
            return sign_token;
        int sign = sign_token.ascii_sign();
        int year = scanner->Next().number();
        if (sign < 0 && year == 0)
            return sign_token;
        day->Add(sign * year);
    } else if (scanner->Peek().IsFixedLengthNumber(4)) {
        day->Add(scanner->Next().number());
    } else {
        return scanner->Next();
    }
    if (scanner->SkipSymbol('-')) {
        if (!scanner->Peek().IsFixedLengthNumber(2) || !DayComposer::IsMonth(scanner->Peek().number()))
            return scanner->Next();
        day->Add(scanner->Next().number());
        if (scanner->SkipSymbol('-')) {
            if (!scanner->Peek().IsFixedLengthNumber(2) || !DayComposer::IsDay(scanner->Peek().number()))
                return scanner->Next();
            day->Add(scanner->Next().number());
        }
    }
    // Check for optional time string: 'T'HH':'mm[':'ss['.'sss]]Z
    if (!scanner->Peek().IsKeywordType(TIME_SEPARATOR)) {
        if (!scanner->Peek().IsEndOfInput())
            return scanner->Next();
    } else {
        // ES5 Date Time String time part is present.
        scanner->Next();
        if (!scanner->Peek().IsFixedLengthNumber(2) || !Between(scanner->Peek().number(), 0, 24)) {
            return DateToken::Invalid();
        }
        // Allow 24:00[:00[.000]], but no other time starting with 24.
        bool hour_is_24 = (scanner->Peek().number() == 24);
        time->Add(scanner->Next().number());
        if (!scanner->SkipSymbol(':'))
            return DateToken::Invalid();
        if (!scanner->Peek().IsFixedLengthNumber(2) || !TimeComposer::IsMinute(scanner->Peek().number()) || (hour_is_24 && scanner->Peek().number() > 0)) {
            return DateToken::Invalid();
        }
        time->Add(scanner->Next().number());
        if (scanner->SkipSymbol(':')) {
            if (!scanner->Peek().IsFixedLengthNumber(2) || !TimeComposer::IsSecond(scanner->Peek().number()) || (hour_is_24 && scanner->Peek().number() > 0)) {
                return DateToken::Invalid();
            }
            time->Add(scanner->Next().number());
            if (scanner->SkipSymbol('.')) {
                if (!scanner->Peek().IsNumber() || (hour_is_24 && scanner->Peek().number() > 0)) {
                    return DateToken::Invalid();
                }
                // Allow more or less than the mandated three digits.
                time->Add(ReadMilliseconds(scanner->Next()));
            }
        }
        // Check for optional timezone designation: 'Z' | ('+'|'-')hh':'mm
        if (scanner->Peek().IsKeywordZ()) {
            scanner->Next();
            tz->Set(0);
        } else if (scanner->Peek().IsSymbol('+') || scanner->Peek().IsSymbol('-')) {
            tz->SetSign(scanner->Next().symbol() == '+' ? 1 : -1);
            if (scanner->Peek().IsFixedLengthNumber(4)) {
                // hhmm extension syntax.
                int hourmin = scanner->Next().number();
                int hour = hourmin / 100;
                int min = hourmin % 100;
                if (!TimeComposer::IsHour(hour) || !TimeComposer::IsMinute(min)) {
                    return DateToken::Invalid();
                }
                tz->SetAbsoluteHour(hour);
                tz->SetAbsoluteMinute(min);
            } else {
                // hh:mm standard syntax.
                if (!scanner->Peek().IsFixedLengthNumber(2) || !TimeComposer::IsHour(scanner->Peek().number())) {
                    return DateToken::Invalid();
                }
                tz->SetAbsoluteHour(scanner->Next().number());
                if (!scanner->SkipSymbol(':'))
                    return DateToken::Invalid();
                if (!scanner->Peek().IsFixedLengthNumber(2) || !TimeComposer::IsMinute(scanner->Peek().number())) {
                    return DateToken::Invalid();
                }
                tz->SetAbsoluteMinute(scanner->Next().number());
            }
        }
        if (!scanner->Peek().IsEndOfInput())
            return DateToken::Invalid();
    }
    // Successfully parsed ES5 Date Time String.
    // ES#sec-date-time-string-format Date Time String Format
    // "When the time zone offset is absent, date-only forms are interpreted
    //  as a UTC time and date-time forms are interpreted as a local time."
    if (tz->IsEmpty() && time->IsEmpty()) {
        tz->Set(0);
    }
    day->set_iso_date();
    return DateToken::EndOfInput();
}

inline double ymdhmsToMilliseconds(int year, long mon, long day, long hour, long minute, long second, double milliseconds)
{
    int mday = WTF::firstDayOfMonth[isLeapYear(year)][mon - 1];
    double ydays = daysFrom1970ToYear(year);

    double dateMilliseconds = milliseconds + second * msPerSecond + minute * (secondsPerMinute * msPerSecond) + hour * (WTF::secondsPerHour * msPerSecond) + (mday + day - 1 + ydays) * (secondsPerDay * msPerSecond);

    // Clamp to EcmaScript standard (ecma262/#sec-time-values-and-time-range) of
    //  +/- 100,000,000 days from 01 January, 1970.
    if (dateMilliseconds < -8640000000000000.0 || dateMilliseconds > 8640000000000000.0)
        return std::numeric_limits<double>::quiet_NaN();

    return dateMilliseconds;
}

namespace JSDateMathInternal {
static constexpr bool verbose = false;
}

#if PLATFORM(COCOA) || USE(BUN_JSC_ADDITIONS)
std::atomic<uint64_t> lastTimeZoneID { 1 };
#endif

#if HAVE(ICU_C_TIMEZONE_API)
class OpaqueICUTimeZone {
    WTF_MAKE_FAST_ALLOCATED(OpaqueICUTimeZone);

public:
    std::unique_ptr<UCalendar, ICUDeleter<ucal_close>> m_calendar;
    String m_canonicalTimeZoneID;
};
#else
static icu::TimeZone* toICUTimeZone(OpaqueICUTimeZone* timeZone)
{
    return bitwise_cast<icu::TimeZone*>(timeZone);
}
static OpaqueICUTimeZone* toOpaqueICUTimeZone(icu::TimeZone* timeZone)
{
    return bitwise_cast<OpaqueICUTimeZone*>(timeZone);
}
#endif

void OpaqueICUTimeZoneDeleter::operator()(OpaqueICUTimeZone* timeZone)
{
    if (timeZone) {
#if HAVE(ICU_C_TIMEZONE_API)
        delete timeZone;
#else
        delete toICUTimeZone(timeZone);
#endif
    }
}

// Get the combined UTC + DST offset for the time passed in.
//
// NOTE: The implementation relies on the fact that no time zones have
// more than one daylight savings offset change per month.
// If this function is called with NaN it returns random value.
LocalTimeOffset DateCache::calculateLocalTimeOffset(double millisecondsFromEpoch, WTF::TimeType inputTimeType)
{
    int32_t rawOffset = 0;
    int32_t dstOffset = 0;
    UErrorCode status = U_ZERO_ERROR;

    // This function can fail input date is invalid: NaN etc.
    // We can return any values in this case since later we fail when computing non timezone offset part anyway.
    constexpr LocalTimeOffset failed { false, 0 };

#if HAVE(ICU_C_TIMEZONE_API)
    auto& timeZoneCache = *this->timeZoneCache();
    ucal_setMillis(timeZoneCache.m_calendar.get(), millisecondsFromEpoch, &status);
    if (U_FAILURE(status))
        return failed;

    if (inputTimeType != WTF::LocalTime) {
        rawOffset = ucal_get(timeZoneCache.m_calendar.get(), UCAL_ZONE_OFFSET, &status);
        if (U_FAILURE(status))
            return failed;
        dstOffset = ucal_get(timeZoneCache.m_calendar.get(), UCAL_DST_OFFSET, &status);
        if (U_FAILURE(status))
            return failed;
    } else {
        ucal_getTimeZoneOffsetFromLocal(timeZoneCache.m_calendar.get(), UCAL_TZ_LOCAL_FORMER, UCAL_TZ_LOCAL_FORMER, &rawOffset, &dstOffset, &status);
        if (U_FAILURE(status))
            return failed;
    }
#else
    auto& timeZoneCache = *toICUTimeZone(this->timeZoneCache());
    if (inputTimeType != WTF::LocalTime) {
        constexpr bool isLocalTime = false;
        timeZoneCache.getOffset(millisecondsFromEpoch, isLocalTime, rawOffset, dstOffset, status);
        if (U_FAILURE(status))
            return failed;
    } else {
        // icu::TimeZone is a timezone instance which inherits icu::BasicTimeZone.
        // https://unicode-org.atlassian.net/browse/ICU-13705 will move getOffsetFromLocal to icu::TimeZone.
        static_cast<const icu::BasicTimeZone&>(timeZoneCache).getOffsetFromLocal(millisecondsFromEpoch, icu::BasicTimeZone::kFormer, icu::BasicTimeZone::kFormer, rawOffset, dstOffset, status);
        if (U_FAILURE(status))
            return failed;
    }
#endif

    return { !!dstOffset, rawOffset + dstOffset };
}

LocalTimeOffsetCache* DateCache::DSTCache::leastRecentlyUsed(LocalTimeOffsetCache* exclude)
{
    LocalTimeOffsetCache* result = nullptr;
    for (auto& cache : m_entries) {
        if (&cache == exclude)
            continue;
        if (!result) {
            result = &cache;
            continue;
        }
        if (result->epoch > cache.epoch)
            result = &cache;
    }
    *result = LocalTimeOffsetCache {};
    return result;
}

std::tuple<LocalTimeOffsetCache*, LocalTimeOffsetCache*> DateCache::DSTCache::probe(int64_t millisecondsFromEpoch)
{
    LocalTimeOffsetCache* before = nullptr;
    LocalTimeOffsetCache* after = nullptr;
    for (auto& cache : m_entries) {
        if (cache.start <= millisecondsFromEpoch) {
            if (!before || before->start < cache.start)
                before = &cache;
        } else if (millisecondsFromEpoch < cache.end) {
            if (!after || after->end > cache.end)
                after = &cache;
        }
    }

    if (!before) {
        if (m_before->isEmpty())
            before = m_before;
        else
            before = leastRecentlyUsed(after);
    }
    if (!after) {
        if (m_after->isEmpty() && before != m_after)
            after = m_after;
        else
            after = leastRecentlyUsed(before);
    }

    m_before = before;
    m_after = after;
    return std::tuple { before, after };
}

void DateCache::DSTCache::extendTheAfterCache(int64_t millisecondsFromEpoch, LocalTimeOffset offset)
{
    if (m_after->offset == offset && m_after->start - defaultDSTDeltaInMilliseconds <= millisecondsFromEpoch && millisecondsFromEpoch <= m_after->end) {
        // Extend the m_after cache.
        m_after->start = millisecondsFromEpoch;
        dataLogLnIf(JSDateMathInternal::verbose, "Cache extended1 from ", millisecondsFromEpoch, " to ", m_after->end, " ", offset.offset, " ", offset.isDST);
    } else {
        // The m_after cache is either invalid or starts too late.
        if (!m_after->isEmpty()) {
            // If the m_after cache is valid, replace it with a new cache.
            m_after = leastRecentlyUsed(m_before);
        }
        m_after->start = millisecondsFromEpoch;
        m_after->end = millisecondsFromEpoch;
        m_after->offset = offset;
        m_after->epoch = bumpEpoch();
        dataLogLnIf(JSDateMathInternal::verbose, "Cache miss, recompute2 ", millisecondsFromEpoch, " ", offset.offset, " ", offset.isDST);
    }
}

LocalTimeOffset DateCache::DSTCache::localTimeOffset(DateCache& dateCache, int64_t millisecondsFromEpoch, WTF::TimeType inputTimeType)
{
    if (millisecondsFromEpoch >= WTF::Int64Milliseconds::minECMAScriptTime && millisecondsFromEpoch <= WTF::Int64Milliseconds::maxECMAScriptTime) {
        // Do nothing. Use millisecondsFromEpoch directly
    } else {
        // Adjust to equivalent time.
        int64_t newTime = WTF::equivalentTime(millisecondsFromEpoch);
        dataLogLnIf(JSDateMathInternal::verbose, "Equivalent time conversion from ", millisecondsFromEpoch, " to ", newTime);
        millisecondsFromEpoch = newTime;
    }

    if (UNLIKELY(m_epoch > UINT32_MAX)) {
        dataLogLnIf(JSDateMathInternal::verbose, "reset DSTCache");
        reset();
    }

    // If the time fits in the cached interval in the last cache hit, return the cached offset.
    if (m_before->start <= millisecondsFromEpoch && millisecondsFromEpoch <= m_before->end) {
        dataLogLnIf(JSDateMathInternal::verbose, "Previous Cache Hit");
        m_before->epoch = bumpEpoch();
        return m_before->offset;
    }

    probe(millisecondsFromEpoch);

    ASSERT(m_before->isEmpty() || m_before->start <= millisecondsFromEpoch);
    ASSERT(m_after->isEmpty() || millisecondsFromEpoch < m_after->start);

    if (m_before->isEmpty()) {
        // Cache miss!
        // Compute the DST offset for the time and shrink the cache interval
        // to only contain the time. This allows fast repeated DST offset
        // computations for the same time.
        LocalTimeOffset offset = dateCache.calculateLocalTimeOffset(millisecondsFromEpoch, inputTimeType);
        m_before->offset = offset;
        m_before->start = millisecondsFromEpoch;
        m_before->end = millisecondsFromEpoch;
        m_before->epoch = bumpEpoch();
        dataLogLnIf(JSDateMathInternal::verbose, "Cache miss, recompute1 ", millisecondsFromEpoch, " ", offset.offset, " ", offset.isDST);
        return offset;
    }

    // Cache hit!
    // If the time fits in the cached interval, return the cached offset.
    if (millisecondsFromEpoch <= m_before->end) {
        dataLogLnIf(JSDateMathInternal::verbose, "Cache hit, simple ", millisecondsFromEpoch, " ", m_before->offset.offset, " ", m_before->offset.isDST);
        m_before->epoch = bumpEpoch();
        return m_before->offset;
    }

    if ((millisecondsFromEpoch - defaultDSTDeltaInMilliseconds) > m_before->end) {
        LocalTimeOffset offset = dateCache.calculateLocalTimeOffset(millisecondsFromEpoch, inputTimeType);
        extendTheAfterCache(millisecondsFromEpoch, offset);
        std::swap(m_before, m_after);
        dataLogLnIf(JSDateMathInternal::verbose, "Cache hit, extend ", millisecondsFromEpoch, " ", offset.offset, " ", offset.isDST);
        return offset;
    }

    m_before->epoch = bumpEpoch();

    // Check if m_after is invalid or starts too late.
    // Note that start of invalid caches is maxECMAScriptTime.
    int64_t newAfterStart = m_before->end < WTF::Int64Milliseconds::maxECMAScriptTime - defaultDSTDeltaInMilliseconds ? m_before->end + defaultDSTDeltaInMilliseconds : WTF::Int64Milliseconds::maxECMAScriptTime;
    if (newAfterStart <= m_after->start) {
        LocalTimeOffset offset = dateCache.calculateLocalTimeOffset(newAfterStart, inputTimeType);
        extendTheAfterCache(newAfterStart, offset);
    } else {
        // Update the usage counter of m_after since it is going to be used.
        ASSERT(!m_after->isEmpty());
        m_after->epoch = bumpEpoch();
    }

    // Now the millisecondsFromEpoch is between m_before->end and m_after->start.
    // Only one daylight savings offset change can occur in this interval.

    if (m_before->offset == m_after->offset) {
        // Merge two caches if they have the same offset.
        m_before->end = m_after->end;
        *m_after = LocalTimeOffsetCache {};
        return m_before->offset;
    }

    // Binary search for daylight savings offset change point,
    // but give up if we don't find it in five iterations.
    for (int i = 4; i >= 0; --i) {
        int64_t delta = m_after->start - m_before->end;
        int64_t middle = !i ? millisecondsFromEpoch : m_before->end + delta / 2;
        LocalTimeOffset offset = dateCache.calculateLocalTimeOffset(middle, inputTimeType);
        if (m_before->offset == offset) {
            m_before->end = middle;
            dataLogLnIf(JSDateMathInternal::verbose, "Cache extended2 from ", m_before->start, " to ", m_before->end, " ", offset.offset, " ", offset.isDST);
            if (millisecondsFromEpoch <= m_before->end)
                return offset;
        } else {
            ASSERT(m_after->offset == offset);
            m_after->start = middle;
            dataLogLnIf(JSDateMathInternal::verbose, "Cache extended3 from ", m_after->start, " to ", m_after->end, " ", offset.offset, " ", offset.isDST);
            if (millisecondsFromEpoch >= m_after->start) {
                // This swap helps the optimistic fast check in subsequent invocations.
                std::swap(m_before, m_after);
                return offset;
            }
        }
    }

    return {};
}

double DateCache::gregorianDateTimeToMS(const GregorianDateTime& t, double milliseconds, WTF::TimeType inputTimeType)
{
    double day = dateToDaysFrom1970(t.year(), t.month(), t.monthDay());
    double ms = timeToMS(t.hour(), t.minute(), t.second(), milliseconds);
    double localTimeResult = (day * WTF::msPerDay) + ms;

    if (inputTimeType == WTF::LocalTime && std::isfinite(localTimeResult))
        return localTimeResult - localTimeOffset(static_cast<int64_t>(localTimeResult), inputTimeType).offset;
    return localTimeResult;
}

double DateCache::localTimeToMS(double milliseconds, WTF::TimeType inputTimeType)
{
    if (inputTimeType == WTF::LocalTime && std::isfinite(milliseconds))
        return milliseconds - localTimeOffset(static_cast<int64_t>(milliseconds), inputTimeType).offset;
    return milliseconds;
}

std::tuple<int32_t, int32_t, int32_t> DateCache::yearMonthDayFromDaysWithCache(int32_t days)
{
    if (m_yearMonthDayCache) {
        // Check conservatively if the given 'days' has
        // the same year and month as the cached 'days'.
        int32_t cachedDays = m_yearMonthDayCache->m_days;
        int32_t newDay = m_yearMonthDayCache->m_day + (days - cachedDays);
        if (newDay >= 1 && newDay <= 28) {
            int32_t year = m_yearMonthDayCache->m_year;
            int32_t month = m_yearMonthDayCache->m_month;
            m_yearMonthDayCache = { days, year, month, newDay };
            return std::tuple { year, month, newDay };
        }
    }
    auto [year, month, day] = WTF::yearMonthDayFromDays(days);
    m_yearMonthDayCache = { days, year, month, day };
    return std::tuple { year, month, day };
}

// input is UTC
void DateCache::msToGregorianDateTime(double millisecondsFromEpoch, WTF::TimeType outputTimeType, GregorianDateTime& tm)
{
    LocalTimeOffset localTime;
    if (outputTimeType == WTF::LocalTime && std::isfinite(millisecondsFromEpoch)) {
        localTime = localTimeOffset(static_cast<int64_t>(millisecondsFromEpoch));
        millisecondsFromEpoch += localTime.offset;
    }
    if (std::isfinite(millisecondsFromEpoch)) {
        WTF::Int64Milliseconds timeClipped(static_cast<int64_t>(millisecondsFromEpoch));
        int32_t days = WTF::msToDays(timeClipped);
        int32_t timeInDayMS = WTF::timeInDay(timeClipped, days);
        auto [year, month, day] = yearMonthDayFromDaysWithCache(days);
        int32_t hour = timeInDayMS / (60 * 60 * 1000);
        int32_t minute = (timeInDayMS / (60 * 1000)) % 60;
        int32_t second = (timeInDayMS / 1000) % 60;
        tm = GregorianDateTime(year, month, dayInYear(year, month, day), day, WTF::weekDay(days), hour, minute, second, localTime.offset / WTF::Int64Milliseconds::msPerMinute, localTime.isDST);
    } else
        tm = GregorianDateTime(millisecondsFromEpoch, localTime);
}

double DateCache::parseDate(JSGlobalObject* globalObject, VM& vm, const String& date)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (date == m_cachedDateString)
        return m_cachedDateStringValue;

    // After ICU 72, CLDR generates narrowNoBreakSpace for date time format. Thus, `new Date().toLocaleString('en-US')` starts generating
    // a string including narrowNoBreakSpaces instead of simple spaces. However since code in the wild assumes `new Date(new Date().toLocaleString('en-US'))`
    // works, we need to maintain the ability to parse string including narrowNoBreakSpaces. Rough consensus among implementaters is replacing narrowNoBreakSpaces
    // with simple spaces before parsing.
    String updatedString = makeStringByReplacingAll(date, narrowNoBreakSpace, space);

    auto expectedString = updatedString.tryGetUTF8();
    if (!expectedString) {
        if (expectedString.error() == UTF8ConversionError::OutOfMemory)
            throwOutOfMemoryError(globalObject, scope);
        // https://tc39.github.io/ecma262/#sec-date-objects section 20.3.3.2 states that:
        // "Unrecognizable Strings or dates containing illegal element values in the
        // format String shall cause Date.parse to return NaN."
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto parseDateImpl = [this](const char* dateString, size_t size) {
        double value = 0.0f;

        if (Options::useV8DateParser()) {
            double out[DateParser::OUTPUT_SIZE];
            if (!DateParser::Parse(nullptr, dateString, size, out)) {
                value = std::numeric_limits<double>::quiet_NaN();
            } else {
                value = ymdhmsToMilliseconds(out[0], out[1], out[2], out[3], out[4], out[5], out[7]);
            }
        } else {
            bool isLocalTime;
            value = WTF::parseES5DateFromNullTerminatedCharacters(dateString, isLocalTime);
            if (std::isnan(value)) {
                value = WTF::parseDateFromNullTerminatedCharacters(dateString, isLocalTime);
            }

            if (isLocalTime && std::isfinite(value))
                value -= localTimeOffset(static_cast<int64_t>(value), WTF::LocalTime).offset;
        }

        return value;
    };

    auto dateUTF8 = expectedString.value();
    double value = parseDateImpl(dateUTF8.data(), dateUTF8.length());
    m_cachedDateString = date;
    m_cachedDateStringValue = value;
    return value;
}

// https://tc39.es/ecma402/#sec-defaulttimezone
String DateCache::defaultTimeZone()
{
#if HAVE(ICU_C_TIMEZONE_API)
    return timeZoneCache()->m_canonicalTimeZoneID;
#else
    icu::UnicodeString timeZoneID;
    icu::UnicodeString canonicalTimeZoneID;
    auto& timeZone = *toICUTimeZone(timeZoneCache());
    timeZone.getID(timeZoneID);

    UErrorCode status = U_ZERO_ERROR;
    UBool isSystem = false;
    icu::TimeZone::getCanonicalID(timeZoneID, canonicalTimeZoneID, isSystem, status);
    if (U_FAILURE(status))
        return "UTC"_s;

    String canonical = String(canonicalTimeZoneID.getBuffer(), canonicalTimeZoneID.length());
    if (isUTCEquivalent(canonical))
        return "UTC"_s;

    return canonical;
#endif
}

String DateCache::timeZoneDisplayName(bool isDST)
{
    if (m_timeZoneStandardDisplayNameCache.isNull()) {
#if HAVE(ICU_C_TIMEZONE_API)
        auto& timeZoneCache = *this->timeZoneCache();
        CString language = defaultLanguage().utf8();
        {
            Vector<UChar, 32> standardDisplayNameBuffer;
            auto status = callBufferProducingFunction(ucal_getTimeZoneDisplayName, timeZoneCache.m_calendar.get(), UCAL_STANDARD, language.data(), standardDisplayNameBuffer);
            if (U_SUCCESS(status))
                m_timeZoneStandardDisplayNameCache = String::adopt(WTFMove(standardDisplayNameBuffer));
        }
        {
            Vector<UChar, 32> dstDisplayNameBuffer;
            auto status = callBufferProducingFunction(ucal_getTimeZoneDisplayName, timeZoneCache.m_calendar.get(), UCAL_DST, language.data(), dstDisplayNameBuffer);
            if (U_SUCCESS(status))
                m_timeZoneDSTDisplayNameCache = String::adopt(WTFMove(dstDisplayNameBuffer));
        }
#else
        auto& timeZoneCache = *toICUTimeZone(this->timeZoneCache());
        String language = defaultLanguage();
        icu::Locale locale(language.utf8().data());
        {
            icu::UnicodeString standardDisplayName;
            timeZoneCache.getDisplayName(false /* inDaylight */, icu::TimeZone::LONG, locale, standardDisplayName);
            m_timeZoneStandardDisplayNameCache = String(standardDisplayName.getBuffer(), standardDisplayName.length());
        }
        {
            icu::UnicodeString dstDisplayName;
            timeZoneCache.getDisplayName(true /* inDaylight */, icu::TimeZone::LONG, locale, dstDisplayName);
            m_timeZoneDSTDisplayNameCache = String(dstDisplayName.getBuffer(), dstDisplayName.length());
        }
#endif
    }
    if (isDST)
        return m_timeZoneDSTDisplayNameCache;
    return m_timeZoneStandardDisplayNameCache;
}

#if PLATFORM(COCOA)
static void timeZoneChangeNotification(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT(isMainThread());
    ++lastTimeZoneID;
}
#endif

// To confine icu::TimeZone destructor invocation in this file.
DateCache::DateCache()
{
#if PLATFORM(COCOA)
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), nullptr, timeZoneChangeNotification, kCFTimeZoneSystemTimeZoneDidChangeNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    });
#endif
}

DateCache::~DateCache() = default;

Ref<DateInstanceData> DateCache::cachedDateInstanceData(double millisecondsFromEpoch)
{
    return *m_dateInstanceCache.add(millisecondsFromEpoch);
}

void DateCache::timeZoneCacheSlow()
{
    ASSERT(!m_timeZoneCache);

    Vector<UChar, 32> timeZoneID;
    getTimeZoneOverride(timeZoneID);
#if HAVE(ICU_C_TIMEZONE_API)
    auto* cache = new OpaqueICUTimeZone;

    String canonical;
    UErrorCode status = U_ZERO_ERROR;
    if (timeZoneID.isEmpty()) {
        status = callBufferProducingFunction(ucal_getHostTimeZone, timeZoneID);
        ASSERT_UNUSED(status, U_SUCCESS(status));
    }
    if (U_SUCCESS(status)) {
        Vector<UChar, 32> canonicalBuffer;
        auto status = callBufferProducingFunction(ucal_getCanonicalTimeZoneID, timeZoneID.data(), timeZoneID.size(), canonicalBuffer, nullptr);
        if (U_SUCCESS(status))
            canonical = String(canonicalBuffer);
    }
    if (canonical.isNull() || isUTCEquivalent(canonical))
        canonical = "UTC"_s;
    cache->m_canonicalTimeZoneID = WTFMove(canonical);

    status = U_ZERO_ERROR;
    cache->m_calendar = std::unique_ptr<UCalendar, ICUDeleter<ucal_close>>(ucal_open(timeZoneID.data(), timeZoneID.size(), "", UCAL_DEFAULT, &status));
    ASSERT_UNUSED(status, U_SUCCESS(status));
    ucal_setGregorianChange(cache->m_calendar.get(), minECMAScriptTime, &status); // Ignore "unsupported" error.
    m_timeZoneCache = std::unique_ptr<OpaqueICUTimeZone, OpaqueICUTimeZoneDeleter>(cache);
#else
    if (!timeZoneID.isEmpty()) {
        m_timeZoneCache = std::unique_ptr<OpaqueICUTimeZone, OpaqueICUTimeZoneDeleter>(toOpaqueICUTimeZone(icu::TimeZone::createTimeZone(icu::UnicodeString(timeZoneID.data(), timeZoneID.size()))));
        return;
    }
    // Do not use icu::TimeZone::createDefault. ICU internally has a cache for timezone and createDefault returns this cached value.
    m_timeZoneCache = std::unique_ptr<OpaqueICUTimeZone, OpaqueICUTimeZoneDeleter>(toOpaqueICUTimeZone(icu::TimeZone::detectHostTimeZone()));
#endif
}

void DateCache::resetIfNecessarySlow()
{
    // FIXME: We should clear it only when we know the timezone has been changed on Non-Cocoa platforms.
    // https://bugs.webkit.org/show_bug.cgi?id=218365
    m_timeZoneCache.reset();
    for (auto& cache : m_caches)
        cache.reset();
    m_yearMonthDayCache = std::nullopt;
    m_cachedDateString = String();
    m_cachedDateStringValue = std::numeric_limits<double>::quiet_NaN();
    m_dateInstanceCache.reset();
    m_timeZoneStandardDisplayNameCache = String();
    m_timeZoneDSTDisplayNameCache = String();
}

} // namespace JSC
