/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
#include "MediaFragmentURIParser.h"

#if ENABLE(VIDEO)

#include "HTMLElement.h"
#include "MediaPlayer.h"
#include "ProcessingInstruction.h"
#include "SegmentedString.h"
#include "Text.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

constexpr int secondsPerHour = 3600;
constexpr int secondsPerMinute = 60;
constexpr unsigned nptIdentifierLength = 4; // "npt:"

static String collectDigits(const LChar* input, unsigned length, unsigned& position)
{
    StringBuilder digits;

    // http://www.ietf.org/rfc/rfc2326.txt
    // DIGIT ; any positive number
    while (position < length && isASCIIDigit(input[position]))
        digits.append(input[position++]);
    return digits.toString();
}

static String collectFraction(const LChar* input, unsigned length, unsigned& position)
{
    StringBuilder digits;

    // http://www.ietf.org/rfc/rfc2326.txt
    // [ "." *DIGIT ]
    if (input[position] != '.')
        return String();

    digits.append(input[position++]);
    while (position < length && isASCIIDigit(input[position]))
        digits.append(input[position++]);
    return digits.toString();
}

MediaFragmentURIParser::MediaFragmentURIParser(const URL& url)
    : m_url(url)
    , m_timeFormat(None)
    , m_startTime(MediaTime::invalidTime())
    , m_endTime(MediaTime::invalidTime())
{
}

MediaTime MediaFragmentURIParser::startTime()
{
    if (!m_url.isValid())
        return MediaTime::invalidTime();
    if (m_timeFormat == None)
        parseTimeFragment();
    return m_startTime;
}

MediaTime MediaFragmentURIParser::endTime()
{
    if (!m_url.isValid())
        return MediaTime::invalidTime();
    if (m_timeFormat == None)
        parseTimeFragment();
    return m_endTime;
}

void MediaFragmentURIParser::parseFragments()
{
    auto fragmentString = m_url.fragmentIdentifier();
    if (fragmentString.isEmpty())
        return;

    unsigned offset = 0;
    unsigned end = fragmentString.length();
    while (offset < end) {
        // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#processing-name-value-components
        // 1. Parse the octet string according to the namevalues syntax, yielding a list of 
        //    name-value pairs, where name and value are both octet string. In accordance 
        //    with RFC 3986, the name and value components must be parsed and separated before
        //    percent-encoded octets are decoded.
        size_t parameterStart = offset;
        size_t parameterEnd = fragmentString.find('&', offset);
        if (parameterEnd == notFound)
            parameterEnd = end;

        size_t equalOffset = fragmentString.find('=', offset);
        if (equalOffset == notFound || equalOffset > parameterEnd) {
            offset = parameterEnd + 1;
            continue;
        }

        // 2. For each name-value pair:
        //  a. Decode percent-encoded octets in name and value as defined by RFC 3986. If either
        //     name or value are not valid percent-encoded strings, then remove the name-value pair
        //     from the list.
        String name = decodeURLEscapeSequences(fragmentString.substring(parameterStart, equalOffset - parameterStart));
        String value;
        if (equalOffset != parameterEnd)
            value = decodeURLEscapeSequences(fragmentString.substring(equalOffset + 1, parameterEnd - equalOffset - 1));
        
        //  b. Convert name and value to Unicode strings by interpreting them as UTF-8. If either
        //     name or value are not valid UTF-8 strings, then remove the name-value pair from the list.
        bool validUTF8 = false;
        if (!name.isEmpty() && !value.isEmpty()) {
            name = name.utf8(StrictConversion).data();
            validUTF8 = !name.isEmpty();

            if (validUTF8) {
                value = value.utf8(StrictConversion).data();
                validUTF8 = !value.isEmpty();
            }
        }
        
        if (validUTF8)
            m_fragments.append(std::make_pair(name, value));

        offset = parameterEnd + 1;
    }
}

void MediaFragmentURIParser::parseTimeFragment()
{
    ASSERT(m_timeFormat == None);

    if (m_fragments.isEmpty())
        parseFragments();

    m_timeFormat = Invalid;

    for (auto& fragment : m_fragments) {
        ASSERT(fragment.first.is8Bit());
        ASSERT(fragment.second.is8Bit());

        // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#naming-time
        // Temporal clipping is denoted by the name t, and specified as an interval with a begin 
        // time and an end time
        if (fragment.first != "t")
            continue;

        // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#npt-time
        // Temporal clipping can be specified either as Normal Play Time (npt) RFC 2326, as SMPTE timecodes,
        // SMPTE, or as real-world clock time (clock) RFC 2326. Begin and end times are always specified
        // in the same format. The format is specified by name, followed by a colon (:), with npt: being
        // the default.
        
        MediaTime start = MediaTime::invalidTime();
        MediaTime end = MediaTime::invalidTime();
        if (parseNPTFragment(fragment.second.characters8(), fragment.second.length(), start, end)) {
            m_startTime = start;
            m_endTime = end;
            m_timeFormat = NormalPlayTime;

            // Although we have a valid fragment, don't return yet because when a fragment dimensions
            // occurs multiple times, only the last occurrence of that dimension is used:
            // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#error-uri-general
            // Multiple occurrences of the same dimension: only the last valid occurrence of a dimension
            // (e.g., t=10 in #t=2&t=10) is interpreted, all previous occurrences (valid or invalid) 
            // SHOULD be ignored by the UA. 
        }
    }
    m_fragments.clear();
}

bool MediaFragmentURIParser::parseNPTFragment(const LChar* timeString, unsigned length, MediaTime& startTime, MediaTime& endTime)
{
    unsigned offset = 0;
    if (length >= nptIdentifierLength && timeString[0] == 'n' && timeString[1] == 'p' && timeString[2] == 't' && timeString[3] == ':')
        offset += nptIdentifierLength;

    if (offset == length)
        return false;
    
    // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#naming-time
    // If a single number only is given, this corresponds to the begin time except if it is preceded
    // by a comma that would in this case indicate the end time.
    if (timeString[offset] == ',')
        startTime = MediaTime::zeroTime();
    else {
        if (!parseNPTTime(timeString, length, offset, startTime))
            return false;
    }

    if (offset == length)
        return true;

    if (timeString[offset] != ',')
        return false;
    if (++offset == length)
        return false;

    if (!parseNPTTime(timeString, length, offset, endTime))
        return false;

    if (offset != length)
        return false;
    
    if (startTime >= endTime)
        return false;

    return true;
}

bool MediaFragmentURIParser::parseNPTTime(const LChar* timeString, unsigned length, unsigned& offset, MediaTime& time)
{
    enum Mode { minutes, hours };
    Mode mode = minutes;

    if (offset >= length || !isASCIIDigit(timeString[offset]))
        return false;

    // http://www.w3.org/2008/WebVideo/Fragments/WD-media-fragments-spec/#npttimedef
    // Normal Play Time can either be specified as seconds, with an optional
    // fractional part to indicate milliseconds, or as colon-separated hours,
    // minutes and seconds (again with an optional fraction). Minutes and
    // seconds must be specified as exactly two digits, hours and fractional
    // seconds can be any number of digits. The hours, minutes and seconds
    // specification for NPT is a convenience only, it does not signal frame
    // accuracy. The specification of the "npt:" identifier is optional since
    // NPT is the default time scheme. This specification builds on the RTSP
    // specification of NPT RFC 2326.
    //
    // ; defined in RFC 2326
    // npt-sec       = 1*DIGIT [ "." *DIGIT ]                     ; definitions taken
    // npt-hhmmss    = npt-hh ":" npt-mm ":" npt-ss [ "." *DIGIT] ; from RFC 2326
    // npt-mmss      = npt-mm ":" npt-ss [ "." *DIGIT] 
    // npt-hh        =   1*DIGIT     ; any positive number
    // npt-mm        =   2DIGIT      ; 0-59
    // npt-ss        =   2DIGIT      ; 0-59

    String digits1 = collectDigits(timeString, length, offset);
    int value1 = parseInteger<int>(digits1).value_or(0);
    if (offset >= length || timeString[offset] == ',') {
        time = MediaTime::createWithDouble(value1);
        return true;
    }

    MediaTime fraction;
    if (timeString[offset] == '.') {
        if (offset == length)
            return true;
        String digits = collectFraction(timeString, length, offset);
        fraction = MediaTime::createWithDouble(digits.toDouble());
        time = MediaTime::createWithDouble(value1) + fraction;
        return true;
    }
    
    if (digits1.length() < 2)
        return false;
    if (digits1.length() > 2)
        mode = hours;

    // Collect the next sequence of 0-9 after ':'
    if (offset >= length || timeString[offset++] != ':')
        return false;
    if (offset >= length || !isASCIIDigit(timeString[(offset)]))
        return false;
    String digits2 = collectDigits(timeString, length, offset);
    if (digits2.length() != 2)
        return false;
    int value2 = parseInteger<int>(digits2).value();

    // Detect whether this timestamp includes hours.
    int value3;
    if (mode == hours || (offset < length && timeString[offset] == ':')) {
        if (offset >= length || timeString[offset++] != ':')
            return false;
        if (offset >= length || !isASCIIDigit(timeString[offset]))
            return false;
        String digits3 = collectDigits(timeString, length, offset);
        if (digits3.length() != 2)
            return false;
        value3 = parseInteger<int>(digits3).value();
    } else {
        value3 = value2;
        value2 = value1;
        value1 = 0;
    }

    if (offset < length && timeString[offset] == '.')
        fraction = MediaTime::createWithDouble(collectFraction(timeString, length, offset).toDouble());
    
    time = MediaTime::createWithDouble((value1 * secondsPerHour) + (value2 * secondsPerMinute) + value3) + fraction;
    return true;
}

}
#endif
