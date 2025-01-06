/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VTTScanner.h"

#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

VTTScanner::VTTScanner(const String& line)
    : m_source(line)
    , m_is8Bit(line.is8Bit())
{
    if (m_is8Bit)
        m_data.characters8 = line.span8();
    else
        m_data.characters16 = line.span16();
}

bool VTTScanner::scan(char c)
{
    if (!match(c))
        return false;
    advance();
    return true;
}

bool VTTScanner::scan(std::span<const LChar> characters)
{
    auto matchLength = m_is8Bit ? m_data.characters8.size() : m_data.characters16.size();
    if (matchLength < characters.size())
        return false;
    bool matched;
    if (m_is8Bit)
        matched = equal(m_data.characters8.first(characters.size()), characters);
    else
        matched = equal(m_data.characters16.first(characters.size()), characters);
    if (matched)
        advance(characters.size());
    return matched;
}

bool VTTScanner::scanRun(const Run& run, const String& toMatch)
{
    ASSERT(run.start() == position());
    ASSERT(run.start() <= end());
    ASSERT(run.end() >= run.start());
    ASSERT(run.end() <= end());
    size_t matchLength = run.length();
    if (toMatch.length() > matchLength)
        return false;
    bool matched;
    if (m_is8Bit)
        matched = equal(toMatch.impl(), m_data.characters8.first(matchLength));
    else
        matched = equal(toMatch.impl(), m_data.characters16.first(matchLength));
    if (matched)
        advance(run.length());
    return matched;
}

void VTTScanner::skipRun(const Run& run)
{
    ASSERT(run.start() <= end());
    ASSERT(run.end() >= run.start());
    ASSERT(run.end() <= end());
    seekTo(run.end());
}

String VTTScanner::extractString(const Run& run)
{
    ASSERT(run.start() == position());
    ASSERT(run.start() <= end());
    ASSERT(run.end() >= run.start());
    ASSERT(run.end() <= end());
    String s;
    if (m_is8Bit)
        s = run.span8();
    else
        s = run.span16();
    advance(run.length());
    return s;
}

String VTTScanner::restOfInputAsString()
{
    return extractString(m_is8Bit ? Run { m_data.characters8 } : Run { m_data.characters16 });
}

unsigned VTTScanner::scanDigits(unsigned& number)
{
    Run runOfDigits = collectWhile<isASCIIDigit>();
    if (runOfDigits.isEmpty()) {
        number = 0;
        return 0;
    }

    StringView string;
    unsigned numDigits = runOfDigits.length();
    if (m_is8Bit)
        string = m_data.characters8.first(numDigits);
    else
        string = m_data.characters16.first(numDigits);

    // Since these are ASCII digits, the only failure mode is overflow, so use the maximum unsigned value.
    number = parseInteger<unsigned>(string).value_or(std::numeric_limits<unsigned>::max());

    // Consume the digits.
    advance(runOfDigits.length());
    return numDigits;
}

bool VTTScanner::scanFloat(float& number, bool* isNegative)
{
    bool negative = scan('-');
    Run integerRun = collectWhile<isASCIIDigit>();

    advance(integerRun.length());
    Run decimalRun = createRun(position(), position());
    if (scan('.')) {
        decimalRun = collectWhile<isASCIIDigit>();
        advance(decimalRun.length());
    }

    // At least one digit required.
    if (integerRun.isEmpty() && decimalRun.isEmpty()) {
        // Restore to starting position.
        seekTo(integerRun.start());
        return false;
    }

    Run floatRun = createRun(integerRun.start(), position());
    bool validNumber;
    if (m_is8Bit)
        number = charactersToFloat(floatRun.span8(), &validNumber);
    else
        number = charactersToFloat(floatRun.span16(), &validNumber);

    if (!validNumber)
        number = std::numeric_limits<float>::max();
    else if (negative)
        number = -number;

    if (isNegative)
        *isNegative = negative;

    return true;
}

auto VTTScanner::createRun(Position start, Position end) const -> Run
{
    if (m_is8Bit) {
        auto span8 = m_source.span8();
        auto* start8 = static_cast<const LChar*>(start);
        auto* end8 = static_cast<const LChar*>(end);
        RELEASE_ASSERT(start8 >= span8.data());
        return Run { span8.subspan(start8 - span8.data(), end8 - start8) };
    }
    auto span16 = m_source.span16();
    auto* start16 = static_cast<const UChar*>(start);
    auto* end16 = static_cast<const UChar*>(end);
    RELEASE_ASSERT(start16 >= span16.data());
    return Run { span16.subspan(start16 - span16.data(), end16 - start16) };
}

}
