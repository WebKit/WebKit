/*
 * Copyright 2009 Google Inc. All rights reserved.
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

#include "config.h"

#include "URLCanonInternal.h"
#include "URLCharacterTypes.h"
#include <limits>
#include <stdint.h>
#include <stdlib.h>

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

// Converts one of the character types that represent a numerical base to the
// corresponding base.
static inline int baseForType(URLCharacterTypes::CharacterTypes type)
{
    switch (type) {
    case URLCharacterTypes::HexadecimalCharacter:
        return 16;
    case URLCharacterTypes::DecimalCharacter:
        return 10;
    case URLCharacterTypes::OctalCharacter:
        return 8;
    default:
        return 0;
    }
}

// Converts an IPv4 component to a 32-bit number, while checking for overflow.
//
// Possible return values:
// - IPV4    - The number was valid, and did not overflow.
// - BROKEN  - The input was numeric, but too large for a 32-bit field.
// - NEUTRAL - Input was not numeric.
//
// The input is assumed to be ASCII. FindIPv4Components should have stripped
// out any input that is greater than 7 bits. The components are assumed
// to be non-empty.
template<typename CharacterType>
CanonHostInfo::Family IPv4ComponentToNumber(const CharacterType* spec, const URLComponent& component, uint32_t* number)
{
    // Figure out the base
    URLCharacterTypes::CharacterTypes base;
    int basePrefixLength = 0; // Size of the prefix for this base.
    if (spec[component.begin()] == '0') {
        // Either hex or dec, or a standalone zero.
        if (component.length() == 1) {
            base = URLCharacterTypes::DecimalCharacter;
        } else if (spec[component.begin() + 1] == 'X' || spec[component.begin() + 1] == 'x') {
            base = URLCharacterTypes::HexadecimalCharacter;
            basePrefixLength = 2;
        } else {
            base = URLCharacterTypes::OctalCharacter;
            basePrefixLength = 1;
        }
    } else
        base = URLCharacterTypes::DecimalCharacter;

    // Extend the prefix to consume all leading zeros.
    while (basePrefixLength < component.length() && spec[component.begin() + basePrefixLength] == '0')
        ++basePrefixLength;

    // Put the component, minus any base prefix, into a zero-terminated buffer so
    // we can call the standard library. Because leading zeros have already been
    // discarded, filling the entire buffer is guaranteed to trigger the 32-bit
    // overflow check.
    const int kMaxComponentLen = 16;
    char buf[kMaxComponentLen + 1]; // digits + '\0'
    int destI = 0;
    for (int i = component.begin() + basePrefixLength; i < component.end(); i++) {
        // We know the input is 7-bit, so convert to narrow (if this is the wide
        // version of the template) by casting.
        char input = static_cast<char>(spec[i]);

        // Validate that this character is OK for the given base.
        if (!URLCharacterTypes::isCharacterOfType(input, base))
            return CanonHostInfo::NEUTRAL;

        // Fill the buffer, if there's space remaining. This check allows us to
        // verify that all characters are numeric, even those that don't fit.
        if (destI < kMaxComponentLen)
            buf[destI++] = input;
    }

    buf[destI] = '\0';

    // Use the 64-bit strtoi so we get a big number (no hex, decimal, or octal
    // number can overflow a 64-bit number in <= 16 characters).
    uint64_t num = _strtoui64(buf, 0, baseForType(base));

    // Check for 32-bit overflow.
    if (num > std::numeric_limits<uint32_t>::max())
        return CanonHostInfo::BROKEN;

    // No overflow. Success!
    *number = static_cast<uint32_t>(num);
    return CanonHostInfo::IPV4;
}

template<typename CharacterType, typename UCHAR>
bool doFindIPv4Components(const CharacterType* spec, const URLComponent& host, URLComponent components[4])
{
    if (!host.isNonEmpty())
        return false;

    int currentComponent = 0; // Index of the component we're working on.
    int currentComponentBegin = host.begin(); // Start of the current component.
    int end = host.end();
    for (int i = host.begin(); /* nothing */; ++i) {
        if (i >= end || spec[i] == '.') {
            // Found the end of the current component.
            int componentLength = i - currentComponentBegin;
            components[currentComponent] = URLComponent(currentComponentBegin, componentLength);

            // The next component starts after the dot.
            currentComponentBegin = i + 1;
            currentComponent++;

            // Don't allow empty components (two dots in a row), except we may
            // allow an empty component at the end (this would indicate that the
            // input ends in a dot). We also want to error if the component is
            // empty and it's the only component (currentComponent == 1).
            if (!componentLength && (i < end || currentComponent == 1))
                return false;

            if (i >= end)
                break; // End of the input.

            if (currentComponent == 4) {
                // Anything else after the 4th component is an error unless it is a
                // dot that would otherwise be treated as the end of input.
                if (spec[i] == '.' && i + 1 == end)
                    break;
                return false;
            }
        } else if (static_cast<UCHAR>(spec[i]) >= 0x80 || !URLCharacterTypes::isIPv4Char(static_cast<unsigned char>(spec[i]))) {
            // Invalid character for an IPv4 address.
            return false;
        }
    }

    // Fill in any unused components.
    while (currentComponent < 4)
        components[currentComponent++] = URLComponent();
    return true;
}

static bool FindIPv4Components(const char* spec, const URLComponent& host, URLComponent components[4])
{
    return doFindIPv4Components<char, unsigned char>(spec, host, components);
}

static bool FindIPv4Components(const UChar* spec, const URLComponent& host, URLComponent components[4])
{
    return doFindIPv4Components<UChar, UChar>(spec, host, components);
}

template<typename CharacterType>
CanonHostInfo::Family doIPv4AddressToNumber(const CharacterType* spec, const URLComponent& host, unsigned char address[4], int& ipv4ComponentsCount)
{
    // The identified components. Not all may exist.
    URLComponent components[4];
    if (!FindIPv4Components(spec, host, components))
        return CanonHostInfo::NEUTRAL;

    // Convert existing components to digits. Values up to
    // |existingComponents| will be valid.
    uint32_t componentValues[4];
    int existingComponents = 0;

    // Set to true if one or more components are BROKEN. BROKEN is only
    // returned if all components are IPV4 or BROKEN, so, for example,
    // 12345678912345.de returns NEUTRAL rather than broken.
    bool broken = false;
    for (int i = 0; i < 4; i++) {
        if (components[i].length() <= 0)
            continue;
        CanonHostInfo::Family family = IPv4ComponentToNumber(spec, components[i], &componentValues[existingComponents]);

        if (family == CanonHostInfo::BROKEN)
            broken = true;
        else if (family != CanonHostInfo::IPV4) {
            // Stop if we hit a non-BROKEN invalid non-empty component.
            return family;
        }

        existingComponents++;
    }

    if (broken)
        return CanonHostInfo::BROKEN;

    // Use that sequence of numbers to fill out the 4-component IP address.

    // First, process all components but the last, while making sure each fits
    // within an 8-bit field.
    for (int i = 0; i < existingComponents - 1; i++) {
        if (componentValues[i] > std::numeric_limits<uint8_t>::max())
            return CanonHostInfo::BROKEN;
        address[i] = static_cast<unsigned char>(componentValues[i]);
    }

    // Next, consume the last component to fill in the remaining bytes.
    uint32_t lastValue = componentValues[existingComponents - 1];
    for (int i = 3; i >= existingComponents - 1; i--) {
        address[i] = static_cast<unsigned char>(lastValue);
        lastValue >>= 8;
    }

    // If the last component has residual bits, report overflow.
    if (lastValue)
        return CanonHostInfo::BROKEN;

    // Tell the caller how many components we saw.
    ipv4ComponentsCount = existingComponents;

    // Success!
    return CanonHostInfo::IPV4;
}

static inline void appendIPv4Address(const unsigned char address[4], URLBuffer<char>& output)
{
    for (int i = 0; i < 4; ++i) {
        char buffer[16];
        _itoa_s(address[i], buffer, 10);

        for (int ch = 0; buffer[ch]; ch++)
            output.append(buffer[ch]);

        if (i != 3)
            output.append('.');
    }
}

// Return true if we've made a final IPV4/BROKEN decision, false if the result
// is NEUTRAL, and we could use a second opinion.
template<typename CharacterType, typename UCHAR>
bool doCanonicalizeIPv4Address(const CharacterType* spec, const URLComponent& host, URLBuffer<char>& output, CanonHostInfo& hostInfo)
{
    hostInfo.family = doIPv4AddressToNumber(spec, host, hostInfo.address, hostInfo.ipv4ComponentsCount);

    switch (hostInfo.family) {
    case CanonHostInfo::IPV4:
        // Definitely an IPv4 address.
        hostInfo.ouputHost.setBegin(output.length());
        appendIPv4Address(hostInfo.address, output);
        hostInfo.ouputHost.setLength(output.length() - hostInfo.ouputHost.begin());
        return true;
    case CanonHostInfo::BROKEN:
        // Definitely broken.
        return true;
    default:
        // Could be IPv6 or a hostname.
        return false;
    }
}

// Helper class that describes the main components of an IPv6 input string.
// See the following examples to understand how it breaks up an input string:
//
// [Example 1]: input = "[::aa:bb]"
//  ==> numHexComponents = 2
//  ==> hexComponents[0] = Component(3,2) "aa"
//  ==> hexComponents[1] = Component(6,2) "bb"
//  ==> indexOfContraction = 0
//  ==> ipv4Component = Component(0, -1)
//
// [Example 2]: input = "[1:2::3:4:5]"
//  ==> numHexComponents = 5
//  ==> hexComponents[0] = Component(1,1) "1"
//  ==> hexComponents[1] = Component(3,1) "2"
//  ==> hexComponents[2] = Component(6,1) "3"
//  ==> hexComponents[3] = Component(8,1) "4"
//  ==> hexComponents[4] = Component(10,1) "5"
//  ==> indexOfContraction = 2
//  ==> ipv4Component = Component(0, -1)
//
// [Example 3]: input = "[::ffff:192.168.0.1]"
//  ==> numHexComponents = 1
//  ==> hexComponents[0] = Component(3,4) "ffff"
//  ==> indexOfContraction = 0
//  ==> ipv4Component = Component(8, 11) "192.168.0.1"
//
// [Example 4]: input = "[1::]"
//  ==> numHexComponents = 1
//  ==> hexComponents[0] = Component(1,1) "1"
//  ==> indexOfContraction = 1
//  ==> ipv4Component = Component(0, -1)
//
// [Example 5]: input = "[::192.168.0.1]"
//  ==> numHexComponents = 0
//  ==> indexOfContraction = 0
//  ==> ipv4Component = Component(8, 11) "192.168.0.1"
//
struct IPv6Parsed {
    // Zero-out the parse information.
    void reset()
    {
        numHexComponents = 0;
        indexOfContraction = -1;
        ipv4Component.reset();
    }

    // There can be up to 8 hex components (colon separated) in the literal.
    URLComponent hexComponents[8];

    // The count of hex components present. Ranges from [0,8].
    int numHexComponents;

    // The index of the hex component that the "::" contraction precedes, or
    // -1 if there is no contraction.
    int indexOfContraction;

    // The range of characters which are an IPv4 literal.
    URLComponent ipv4Component;
};

// Parse the IPv6 input string. If parsing succeeded returns true and fills
// |parsed| with the information. If parsing failed (because the input is
// invalid) returns false.
template<typename CharacterType, typename UCHAR>
bool doParseIPv6(const CharacterType* spec, const URLComponent& host, IPv6Parsed& parsed)
{
    // Zero-out the info.
    parsed.reset();

    if (!host.isNonEmpty())
        return false;

    // The index for start and end of address range (no brackets).
    int begin = host.begin();
    int end = host.end();

    int currentComponentBegin = begin; // Start of the current component.

    // Scan through the input, searching for hex components, "::" contractions,
    // and IPv4 components.
    for (int i = begin; /* i <= end */; i++) {
        bool isColon = spec[i] == ':';
        bool isContraction = isColon && i < end - 1 && spec[i + 1] == ':';

        // We reached the end of the current component if we encounter a colon
        // (separator between hex components, or start of a contraction), or end of
        // input.
        if (isColon || i == end) {
            int componentLength = i - currentComponentBegin;

            // A component should not have more than 4 hex digits.
            if (componentLength > 4)
                return false;

            // Don't allow empty components.
            if (!componentLength) {
                // The exception is when contractions appear at beginning of the
                // input or at the end of the input.
                if (!((isContraction && i == begin) || (i == end && parsed.indexOfContraction == parsed.numHexComponents)))
                    return false;
            }

            // Add the hex component we just found to running list.
            if (componentLength > 0) {
                // Can't have more than 8 components!
                if (parsed.numHexComponents >= 8)
                    return false;

                parsed.hexComponents[parsed.numHexComponents++] =
                URLComponent(currentComponentBegin, componentLength);
            }
        }

        if (i == end)
            break; // Reached the end of the input, DONE.

        // We found a "::" contraction.
        if (isContraction) {
            // There can be at most one contraction in the literal.
            if (parsed.indexOfContraction != -1)
                return false;
            parsed.indexOfContraction = parsed.numHexComponents;
            ++i; // Consume the colon we peeked.
        }

        if (isColon) {
            // Colons are separators between components, keep track of where the
            // current component started (after this colon).
            currentComponentBegin = i + 1;
        } else {
            if (static_cast<UCHAR>(spec[i]) >= 0x80)
                return false; // Not ASCII.

            if (!URLCharacterTypes::isHexChar(static_cast<unsigned char>(spec[i]))) {
                // Regular components are hex numbers. It is also possible for
                // a component to be an IPv4 address in dotted form.
                if (URLCharacterTypes::isIPv4Char(static_cast<unsigned char>(spec[i]))) {
                    // Since IPv4 address can only appear at the end, assume the rest
                    // of the string is an IPv4 address. (We will parse this separately
                    // later).
                    parsed.ipv4Component = URLComponent::fromRange(currentComponentBegin, end);
                    break;
                }
                // The character was neither a hex digit, nor an IPv4 character.
                return false;
            }
        }
    }

    return true;
}

// Verifies the parsed IPv6 information, checking that the various components
// add up to the right number of bits (hex components are 16 bits, while
// embedded IPv4 formats are 32 bits, and contractions are placeholdes for
// 16 or more bits). Returns true if sizes match up, false otherwise. On
// success writes the length of the contraction (if any) to
// |outNumBytesOfContraction|.
bool CheckIPv6ComponentsSize(const IPv6Parsed& parsed, int* outNumBytesOfContraction)
{
    // Each group of four hex digits contributes 16 bits.
    int numBytesWithoutContraction = parsed.numHexComponents * 2;

    // If an IPv4 address was embedded at the end, it contributes 32 bits.
    if (parsed.ipv4Component.isValid())
        numBytesWithoutContraction += 4;

    // If there was a "::" contraction, its size is going to be:
    // MAX([16bits], [128bits] - numBytesWithoutContraction).
    int numBytesOfContraction = 0;
    if (parsed.indexOfContraction != -1) {
        numBytesOfContraction = 16 - numBytesWithoutContraction;
        if (numBytesOfContraction < 2)
            numBytesOfContraction = 2;
    }

    // Check that the numbers add up.
    if (numBytesWithoutContraction + numBytesOfContraction != 16)
        return false;

    *outNumBytesOfContraction = numBytesOfContraction;
    return true;
}

// Converts a hex comonent into a number. This cannot fail since the caller has
// already verified that each character in the string was a hex digit, and
// that there were no more than 4 characters.
template<typename CharacterType>
uint16_t IPv6HexComponentToNumber(const CharacterType* spec, const URLComponent& component)
{
    ASSERT(component.length() <= 4);

    // Copy the hex string into a C-string.
    char buf[5];
    for (int i = 0; i < component.length(); ++i)
        buf[i] = static_cast<char>(spec[component.begin() + i]);
    buf[component.length()] = '\0';

    // Convert it to a number (overflow is not possible, since with 4 hex
    // characters we can at most have a 16 bit number).
    return static_cast<uint16_t>(_strtoui64(buf, 0, 16));
}

// Converts an IPv6 address to a 128-bit number (network byte order), returning
// true on success. False means that the input was not a valid IPv6 address.
template<typename CharacterType, typename UCHAR>
bool doIPv6AddressToNumber(const CharacterType* spec,
                           const URLComponent& host,
                           unsigned char address[16])
{
    // Make sure the component is bounded by '[' and ']'.
    int end = host.end();
    if (!host.isNonEmpty() || spec[host.begin()] != '[' || spec[end - 1] != ']')
        return false;

    // Exclude the square brackets.
    URLComponent ipv6Component(host.begin() + 1, host.length() - 2);

    // Parse the IPv6 address -- identify where all the colon separated hex
    // components are, the "::" contraction, and the embedded IPv4 address.
    IPv6Parsed ipv6Parsed;
    if (!doParseIPv6<CharacterType, UCHAR>(spec, ipv6Component, ipv6Parsed))
        return false;

    // Do some basic size checks to make sure that the address doesn't
    // specify more than 128 bits or fewer than 128 bits. This also resolves
    // how may zero bytes the "::" contraction represents.
    int numBytesOfContraction;
    if (!CheckIPv6ComponentsSize(ipv6Parsed, &numBytesOfContraction))
        return false;

    int currentIndexInAddress = 0;

    // Loop through each hex components, and contraction in order.
    for (int i = 0; i <= ipv6Parsed.numHexComponents; ++i) {
        // Append the contraction if it appears before this component.
        if (i == ipv6Parsed.indexOfContraction) {
            for (int j = 0; j < numBytesOfContraction; ++j)
                address[currentIndexInAddress++] = 0;
        }
        // Append the hex component's value.
        if (i != ipv6Parsed.numHexComponents) {
            // Get the 16-bit value for this hex component.
            uint16_t number = IPv6HexComponentToNumber<CharacterType>(spec, ipv6Parsed.hexComponents[i]);
            // Append to |address|, in network byte order.
            address[currentIndexInAddress++] = (number & 0xFF00) >> 8;
            address[currentIndexInAddress++] = (number & 0x00FF);
        }
    }

    // If there was an IPv4 section, convert it into a 32-bit number and append
    // it to |address|.
    if (ipv6Parsed.ipv4Component.isValid()) {
        // Append the 32-bit number to |address|.
        int ignoredIPv4ComponentsCount;
        if (CanonHostInfo::IPV4 !=
            doIPv4AddressToNumber(spec, ipv6Parsed.ipv4Component, &address[currentIndexInAddress], ignoredIPv4ComponentsCount))
            return false;
    }

    return true;
}

// Searches for the longest sequence of zeros in |address|, and writes the
// range into |contractionRange|. The run of zeros must be at least 16 bits,
// and if there is a tie the first is chosen.
void ChooseIPv6ContractionRange(const unsigned char address[16], URLComponent* contractionRange)
{
    // The longest run of zeros in |address| seen so far.
    URLComponent maxRange;

    // The current run of zeros in |address| being iterated over.
    URLComponent currentRange;

    for (int i = 0; i < 16; i += 2) {
        // Test for 16 bits worth of zero.
        bool isZero = (!address[i] && !address[i + 1]);

        if (isZero) {
            // Add the zero to the current range (or start a new one).
            if (!currentRange.isValid())
                currentRange = URLComponent(i, 0);
            currentRange.setLength(currentRange.length() + 2);
        }

        if (!isZero || i == 14) {
            // Just completed a run of zeros. If the run is greater than 16 bits,
            // it is a candidate for the contraction.
            if (currentRange.length() > 2 && currentRange.length() > maxRange.length())
                maxRange = currentRange;

            currentRange.reset();
        }
    }
    *contractionRange = maxRange;
}

static inline void appendIPv6Address(const unsigned char address[16], URLBuffer<char>& output)
{
    // We will output the address according to the rules in:
    // http://tools.ietf.org/html/draft-kawamura-ipv6-text-representation-01#section-4

    // Start by finding where to place the "::" contraction (if any).
    URLComponent contractionRange;
    ChooseIPv6ContractionRange(address, &contractionRange);

    for (int i = 0; i <= 14;) {
        // We check 2 bytes at a time, from bytes (0, 1) to (14, 15), inclusive.
        ASSERT(!(i % 2));
        if (i == contractionRange.begin() && contractionRange.length() > 0) {
            // Jump over the contraction.
            if (!i)
                output.append(':');
            output.append(':');
            i = contractionRange.end();
        } else {
            // Consume the next 16 bits from |address|.
            int x = address[i] << 8 | address[i + 1];

            i += 2;

            // Stringify the 16 bit number (at most requires 4 hex digits).
            char str[5];
            _itoa_s(x, str, 16);
            for (int ch = 0; str[ch]; ++ch)
                output.append(str[ch]);

            // Put a colon after each number, except the last.
            if (i < 16)
                output.append(':');
        }
    }
}

// Return true if we've made a final IPV6/BROKEN decision, false if the result
// is NEUTRAL, and we could use a second opinion.
template<typename CharacterType, typename UCHAR>
bool doCanonicalizeIPv6Address(const CharacterType* spec, const URLComponent& host, URLBuffer<char>& output, CanonHostInfo& hostInfo)
{
    // Turn the IP address into a 128 bit number.
    if (!doIPv6AddressToNumber<CharacterType, UCHAR>(spec, host, hostInfo.address)) {
        // If it's not an IPv6 address, scan for characters that should *only*
        // exist in an IPv6 address.
        for (int i = host.begin(); i < host.end(); i++) {
            switch (spec[i]) {
            case '[':
            case ']':
            case ':':
                hostInfo.family = CanonHostInfo::BROKEN;
                return true;
            }
        }

        // No invalid characters. Could still be IPv4 or a hostname.
        hostInfo.family = CanonHostInfo::NEUTRAL;
        return false;
    }

    hostInfo.ouputHost.setBegin(output.length());
    output.append('[');
    appendIPv6Address(hostInfo.address, output);
    output.append(']');
    hostInfo.ouputHost.setLength(output.length() - hostInfo.ouputHost.begin());

    hostInfo.family = CanonHostInfo::IPV6;
    return true;
}

} // namespace

void canonicalizeIPAddress(const char* spec, const URLComponent& host, URLBuffer<char>& output, CanonHostInfo& hostInfo)
{
    if (doCanonicalizeIPv4Address<char, unsigned char>(spec, host, output, hostInfo))
        return;
    if (doCanonicalizeIPv6Address<char, unsigned char>(spec, host, output, hostInfo))
        return;
}

void canonicalizeIPAddress(const UChar* spec, const URLComponent& host, URLBuffer<char>& output, CanonHostInfo& hostInfo)
{
    if (doCanonicalizeIPv4Address<UChar, UChar>(spec, host, output, hostInfo))
        return;
    if (doCanonicalizeIPv6Address<UChar, UChar>(spec, host, output, hostInfo))
        return;
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
