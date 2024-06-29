// Copyright 2013 the V8 project authors. All rights reserved.
// Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "JSDateMath-v8.h"

namespace v8 {

constexpr int kMaxInt = 0x7FFFFFFF;

constexpr intptr_t kIntptrAllBitsSet = intptr_t { -1 };
constexpr uintptr_t kUintptrAllBitsSet = static_cast<uintptr_t>(kIntptrAllBitsSet);
#if USE(JSVALUE32_64)
static constexpr int kSmiValueSize = 31;
#else
static constexpr int kSmiValueSize = 32;
#endif

static constexpr intptr_t kSmiMinValue = static_cast<intptr_t>(kUintptrAllBitsSet << (kSmiValueSize - 1));
static constexpr intptr_t kSmiMaxValue = -(kSmiMinValue + 1);
static constexpr int kMaxValue = static_cast<int>(kSmiMaxValue);

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/base/bounds.h#L17
template<typename T, typename U>
inline constexpr bool IsInRange(T value, U lower_limit, U higher_limit)
{
    ASSERT(lower_limit <= higher_limit);
    static_assert(sizeof(U) <= sizeof(T));
    using unsigned_T = typename std::make_unsigned<T>::type;
    // Use static_cast to support enum classes.
    return static_cast<unsigned_T>(static_cast<unsigned_T>(value) - static_cast<unsigned_T>(lower_limit)) <= static_cast<unsigned_T>(static_cast<unsigned_T>(higher_limit) - static_cast<unsigned_T>(lower_limit));
}

inline constexpr int AsciiAlphaToLower(uint32_t c) { return c | 0x20; }

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/strings/char-predicates-inl.h#L32
inline constexpr bool IsDecimalDigit(uint32_t c)
{
    // ECMA-262, 3rd, 7.8.3 (p 16)
    return IsInRange(c, '0', '9');
}

bool IsLineTerminator(unsigned int c)
{
    return c == 0x000A || c == 0x000D || c == 0x2028 || c == 0x2029;
}

namespace Smi {
bool IsValid(intptr_t value)
{
#if USE(JSVALUE32_64)
    return (static_cast<uintptr_t>(value) - static_cast<uintptr_t>(kSmiMinValue)) <= (static_cast<uintptr_t>(kSmiMaxValue) - static_cast<uintptr_t>(kSmiMinValue));
#else
    return value == static_cast<int32_t>(value);
#endif
}
}

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/dateparser.h#L15
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
    static bool Parse(void* isolate, const unsigned char* str, size_t size, double* output);

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
        explicit InputReader(const unsigned char* s, size_t size)
            : index_(0)
            , buffer_(s)
            , size_(size)
        {
            Next();
        }

        int position() { return index_; }

        // Advance to the next character of the string.
        void Next()
        {
            ch_ = (index_ < size_) ? buffer_[index_] : 0;
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
        const unsigned char* buffer_;
        size_t size_;
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
            ASSERT(IsNumber());
            return value_;
        }
        KeywordType keyword_type()
        {
            ASSERT(IsKeyword());
            return static_cast<KeywordType>(tag_);
        }
        int keyword_value()
        {
            ASSERT(IsKeyword());
            return value_;
        }
        char symbol()
        {
            ASSERT(IsSymbol());
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
            ASSERT(IsAsciiSign());
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

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/dateparser.cc#L13
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

    if (!Smi::IsValid(year) || !IsMonth(month) || !IsDay(day))
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
        ASSERT(Smi::IsValid(total_seconds));
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
            ASSERT(factor <= 100000000); // factor won't overflow.
            factor *= 10;
            length--;
        } while (length > 3);
        number /= factor;
    }
    return number;
}

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/dateparser-inl.h#L16
// template<typename Char>
bool DateParser::Parse(void*, const unsigned char* str, size_t size, double* out)
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
    // bool legacy_parser = false;
    for (DateToken token = next_unhandled_token; !token.IsEndOfInput();
         token = scanner.Next()) {
        if (token.IsNumber()) {
            // legacy_parser = true;
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
            // legacy_parser = true;
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
            // legacy_parser = true;
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

    // DIFF: leaving this commented because we don't have isolate and don't need to count usage
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
        ASSERT(KeywordTable::kPrefixLength == 3);
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
    ASSERT(day->IsEmpty());
    ASSERT(time->IsEmpty());
    ASSERT(tz->IsEmpty());

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

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/date.cc#L462
// ES6 section 20.3.1.1 Time Values and Time Range
const double kMinYear = -1000000.0;
const double kMaxYear = -kMinYear;
const double kMinMonth = -10000000.0;
const double kMaxMonth = -kMinMonth;

const double kMsPerDay = 86400000.0;

const double kMsPerSecond = 1000.0;
const double kMsPerMinute = 60000.0;
const double kMsPerHour = 3600000.0;
static const int64_t kMsPerMonth = kMsPerDay * 30;

// The largest time that can be stored in JSDate.
static const int64_t kMaxTimeInMs = static_cast<int64_t>(864000000) * 10000000;

// Conservative upper bound on time that can be stored in JSDate
// before UTC conversion.
static const int64_t kMaxTimeBeforeUTCInMs = kMaxTimeInMs + kMsPerMonth;

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/numbers/conversions-inl.h#L83
// #sec-tointegerorinfinity
inline double DoubleToInteger(double x)
{
    // ToIntegerOrInfinity normalizes -0 to +0. Special case 0 for performance.
    if (std::isnan(x) || x == 0.0)
        return 0;
    if (!std::isfinite(x))
        return x;
    // Add 0.0 in the truncation case to ensure this doesn't return -0.
    return ((x > 0) ? std::floor(x) : std::ceil(x)) + 0.0;
}

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/date.cc#L68
// ECMA 262 - ES#sec-timeclip TimeClip (time)
double TimeClip(double time)
{
    if (-kMaxTimeInMs <= time && time <= kMaxTimeInMs) {
        return DoubleToInteger(time);
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/date.cc#L482
double MakeDay(double year, double month, double date)
{
    if ((kMinYear <= year && year <= kMaxYear) && (kMinMonth <= month && month <= kMaxMonth) && std::isfinite(date)) {
        int y = static_cast<int>(year);
        int m = static_cast<int>(month);
        y += m / 12;
        m %= 12;
        if (m < 0) {
            m += 12;
            y -= 1;
        }
        ASSERT(0 <= m);
        ASSERT(m < 12);

        // kYearDelta is an arbitrary number such that:
        // a) kYearDelta = -1 (mod 400)
        // b) year + kYearDelta > 0 for years in the range defined by
        //    ECMA 262 - 15.9.1.1, i.e. upto 100,000,000 days on either side of
        //    Jan 1 1970. This is required so that we don't run into integer
        //    division of negative numbers.
        // c) there shouldn't be an overflow for 32-bit integers in the following
        //    operations.
        static const int kYearDelta = 399999;
        static const int kBaseDay = 365 * (1970 + kYearDelta) + (1970 + kYearDelta) / 4 - (1970 + kYearDelta) / 100 + (1970 + kYearDelta) / 400;
        int day_from_year = 365 * (y + kYearDelta) + (y + kYearDelta) / 4 - (y + kYearDelta) / 100 + (y + kYearDelta) / 400 - kBaseDay;
        if ((y % 4 != 0) || (y % 100 == 0 && y % 400 != 0)) {
            static const int kDayFromMonth[] = { 0, 31, 59, 90, 120, 151,
                181, 212, 243, 273, 304, 334 };
            day_from_year += kDayFromMonth[m];
        } else {
            static const int kDayFromMonth[] = { 0, 31, 60, 91, 121, 152,
                182, 213, 244, 274, 305, 335 };
            day_from_year += kDayFromMonth[m];
        }
        return static_cast<double>(day_from_year - 1) + DoubleToInteger(date);
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/date.cc#L525
double MakeTime(double hour, double min, double sec, double ms)
{
    if (std::isfinite(hour) && std::isfinite(min) && std::isfinite(sec) && std::isfinite(ms)) {
        double const h = DoubleToInteger(hour);
        double const m = DoubleToInteger(min);
        double const s = DoubleToInteger(sec);
        double const milli = DoubleToInteger(ms);
        return h * kMsPerHour + m * kMsPerMinute + s * kMsPerSecond + milli;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/date/date.cc#L475
double MakeDate(double day, double time)
{
    if (std::isfinite(day) && std::isfinite(time)) {
        return time + day * kMsPerDay;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

// https://github.com/v8/v8/blob/c45b7804109ece574f71fd45417b4ad498a99e6f/src/builtins/builtins-date.cc#L30
double ParseDateTimeString(const unsigned char* str, size_t size, bool& local)
{
    double out[v8::DateParser::OUTPUT_SIZE];

    if (!DateParser::Parse(nullptr, str, size, out))
        return std::numeric_limits<double>::quiet_NaN();

    double const day = MakeDay(out[DateParser::YEAR], out[DateParser::MONTH], out[DateParser::DAY]);
    double const time = MakeTime(out[DateParser::HOUR], out[DateParser::MINUTE], out[DateParser::SECOND], out[DateParser::MILLISECOND]);

    double date = MakeDate(day, time);

    if (std::isnan(out[DateParser::UTC_OFFSET])) {
        if (date >= -kMaxTimeBeforeUTCInMs && date <= kMaxTimeBeforeUTCInMs) {
            // DIFF: Use JSC DateCache::DSTCache instead of v8's DateCache. Mark as local and handle
            // the offset in DateCache::ParseDate.
            local = true;
        } else {
            return std::numeric_limits<double>::quiet_NaN();
        }
    } else {
        date -= out[DateParser::UTC_OFFSET] * 1000.0;
    }

    return date;
}

} // namespace v8
