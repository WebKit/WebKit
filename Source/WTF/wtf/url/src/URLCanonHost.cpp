/*
 * Copyright 2007 Google Inc. All rights reserved.
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
#include "URLCanon.h"

#include "RawURLBuffer.h"
#include "URLCanonInternal.h"

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

// For reference, here's what IE supports:
// Key: 0 (disallowed: failure if present in the input)
//      + (allowed either escaped or unescaped, and unmodified)
//      U (allowed escaped or unescaped but always unescaped if present in
//         escaped form)
//      E (allowed escaped or unescaped but always escaped if present in
//         unescaped form)
//      % (only allowed escaped in the input, will be unmodified).
//      I left blank alpha numeric characters.
//
//    00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f
//    -----------------------------------------------
// 0   0  E  E  E  E  E  E  E  E  E  E  E  E  E  E  E
// 1   E  E  E  E  E  E  E  E  E  E  E  E  E  E  E  E
// 2   E  +  E  E  +  E  +  +  +  +  +  +  +  U  U  0
// 3                                 %  %  E  +  E  0  <-- Those are  : ; < = > ?
// 4   %
// 5                                    U  0  U  U  U  <-- Those are  [ \ ] ^ _
// 6   E                                               <-- That's  `
// 7                                    E  E  E  U  E  <-- Those are { | } ~ (UNPRINTABLE)
//
// NOTE: I didn't actually test all the control characters. Some may be
// disallowed in the input, but they are all accepted escaped except for 0.
// I also didn't test if characters affecting HTML parsing are allowed
// unescaped, eg. (") or (#), which would indicate the beginning of the path.
// Surprisingly, space is accepted in the input and always escaped.

// This table lists the canonical version of all characters we allow in the
// input, with 0 indicating it is disallowed. We use the magic kEscapedHostChar
// value to indicate that this character should be escaped. We are a little more
// restrictive than IE, but less restrictive than Firefox.
//
// Note that we disallow the % character. We will allow it when part of an
// escape sequence, of course, but this disallows "%25". Even though IE allows
// it, allowing it would put us in a funny state. If there was an invalid
// escape sequence like "%zz", we'll add "%25zz" to the output and fail.
// Allowing percents means we'll succeed a second time, so validity would change
// based on how many times you run the canonicalizer. We prefer to always report
// the same vailidity, so reject this.
const unsigned char kEsc = 0xff;
const unsigned char kHostCharLookup[0x80] = {
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
//  ' '   !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /
    kEsc, kEsc, kEsc, kEsc, kEsc,   0,  kEsc, kEsc, kEsc, kEsc, kEsc,  '+', kEsc,  '-',  '.',   0,
//   0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?
    '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ':',   0 , kEsc, kEsc, kEsc,   0 ,
//   @      A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
    kEsc,  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
//   P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _
    'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  '[',   0 ,  ']',   0 ,  '_',
//   `      a     b     c     d     e     f     g     h     i     j     k     l     m     n     o
    kEsc,  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
//   p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~
    'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z', kEsc, kEsc, kEsc,   0 ,   0 };

typedef RawURLBuffer<char> StackBuffer;
typedef RawURLBuffer<UChar> StackBufferW;

// Scans a host name and fills in the output flags according to what we find.
// |hasNonASCII| will be true if there are any non-7-bit characters, and
// |hasEscaped| will be true if there is a percent sign.
template<typename CharacterType, typename UCHAR>
void scanHostname(const CharacterType* spec, const URLComponent& host, bool& hasNonASCII, bool& hasEscaped)
{
    int end = host.end();
    hasNonASCII = false;
    hasEscaped = false;
    for (int i = host.begin(); i < end; ++i) {
        if (static_cast<UCHAR>(spec[i]) >= 0x80)
            hasNonASCII = true;
        else if (spec[i] == '%')
            hasEscaped = true;
    }
}

// Canonicalizes a host name that is entirely 8-bit characters (even though
// the type holding them may be 16 bits. Escaped characters will be unescaped.
// Non-7-bit characters (for example, UTF-8) will be passed unchanged.
//
// The |*hasNonASCII| flag will be true if there are non-7-bit characters in
// the output.
//
// This function is used in two situations:
//
//  * When the caller knows there is no non-ASCII or percent escaped
//    characters. This is what DoHost does. The result will be a completely
//    canonicalized host since we know nothing weird can happen (escaped
//    characters could be unescaped to non-7-bit, so they have to be treated
//    with suspicion at this point). It does not use the |hasNonASCII| flag.
//
//  * When the caller has an 8-bit string that may need unescaping.
//    doComplexHost calls us this situation to do unescaping and validation.
//    After this, it may do other IDN operations depending on the value of the
//    |*hasNonASCII| flag.
//
// The return value indicates if the output is a potentially valid host name.
template<typename INCHAR, typename OUTCHAR>
bool doSimpleHost(const INCHAR* host, int hostLength, URLBuffer<OUTCHAR>& output, bool& hasNonASCII)
{
    hasNonASCII = false;

    bool success = true;
    for (int i = 0; i < hostLength; ++i) {
        unsigned source = host[i];
        if (source == '%') {
            // Unescape first, if possible.
            // Source will be used only if decode operation was successful.
            if (!DecodeEscaped(host, &i, hostLength,
                               reinterpret_cast<unsigned char*>(&source))) {
                // Invalid escaped character. There is nothing that can make this
                // host valid. We append an escaped percent so the URL looks reasonable
                // and mark as failed.
                appendURLEscapedCharacter('%', output);
                success = false;
                continue;
            }
        }

        if (source < 0x80) {
            // We have ASCII input, we can use our lookup table.
            unsigned char replacement = kHostCharLookup[source];
            if (!replacement) {
                // Invalid character, add it as percent-escaped and mark as failed.
                appendURLEscapedCharacter(source, output);
                success = false;
            } else if (replacement == kEsc) {
                // This character is valid but should be escaped.
                appendURLEscapedCharacter(source, output);
            } else {
                // Common case, the given character is valid in a hostname, the lookup
                // table tells us the canonical representation of that character (lower
                // cased).
                output.append(replacement);
            }
        } else {
            // It's a non-ascii char. Just push it to the output.
            // In case where we have UChar input, and char output it's safe to
            // cast UChar->char only if input string was converted to ASCII.
            output.append(static_cast<OUTCHAR>(source));
            hasNonASCII = true;
        }
    }

    return success;
}

// Canonicalizes a host that requires IDN conversion. Returns true on success
bool doIDNHost(const UChar* src, int sourceLength, URLBuffer<char>& output)
{
    // We need to escape URL before doing IDN conversion, since punicode strings
    // cannot be escaped after they are created.
    RawURLBuffer<UChar> urlEscapedHost;
    bool hasNonASCII;
    doSimpleHost(src, sourceLength, urlEscapedHost, hasNonASCII);

    StackBufferW wideOutput;
    if (!IDNToASCII(urlEscapedHost.data(),
                    urlEscapedHost.length(),
                    wideOutput)) {
        // Some error, give up. This will write some reasonable looking
        // representation of the string to the output.
        AppendInvalidNarrowString(src, 0, sourceLength, output);
        return false;
    }

    // Now we check the ASCII output like a normal host. It will also handle
    // unescaping. Although we unescaped everything before this function call, if
    // somebody does %00 as fullwidth, ICU will convert this to ASCII.
    bool success = doSimpleHost(wideOutput.data(), wideOutput.length(), output, hasNonASCII);
    ASSERT(!hasNonASCII);
    return success;
}

// 8-bit convert host to its ASCII version: this converts the UTF-8 input to
// UTF-16. The hasEscaped flag should be set if the input string requires
// unescaping.
bool doComplexHost(const char* host, int hostLength, bool hasNonASCII, bool hasEscaped, URLBuffer<char>& output)
{
    // Save the current position in the output. We may write stuff and rewind it
    // below, so we need to know where to rewind to.
    int beginLength = output.length();

    // Points to the UTF-8 data we want to convert. This will either be the
    // input or the unescaped version written to |output| if necessary.
    const char* utf8Source;
    int utf8SourceLength;
    if (hasEscaped) {
        // Unescape before converting to UTF-16 for IDN. We write this into the
        // output because it most likely does not require IDNization, and we can
        // save another huge stack buffer. It will be replaced below if it requires
        // IDN. This will also update our non-ASCII flag so we know whether the
        // unescaped input requires IDN.
        if (!doSimpleHost(host, hostLength, output, hasNonASCII)) {
            // Error with some escape sequence. We'll call the current output
            // complete. doSimpleHost will have written some "reasonable" output.
            return false;
        }

        // Unescaping may have left us with ASCII input, in which case the
        // unescaped version we wrote to output is complete.
        if (!hasNonASCII)
            return true;

        // Save the pointer into the data was just converted (it may be appended to
        // other data in the output buffer).
        utf8Source = &output.data()[beginLength];
        utf8SourceLength = output.length() - beginLength;
    } else {
        // We don't need to unescape, use input for IDNization later. (We know the
        // input has non-ASCII, or the simple version would have been called
        // instead of us.)
        utf8Source = host;
        utf8SourceLength = hostLength;
    }

    // Non-ASCII input requires IDN, convert to UTF-16 and do the IDN conversion.
    // Above, we may have used the output to write the unescaped values to, so
    // we have to rewind it to where we started after we convert it to UTF-16.
    StackBufferW utf16;
    if (!ConvertUTF8ToUTF16(utf8Source, utf8SourceLength, utf16)) {
        // In this error case, the input may or may not be the output.
        StackBuffer utf8;
        for (int i = 0; i < utf8SourceLength; i++)
            utf8.append(utf8Source[i]);
        output.setLength(beginLength);
        AppendInvalidNarrowString(utf8.data(), 0, utf8.length(), output);
        return false;
    }
    output.setLength(beginLength);

    // This will call doSimpleHost which will do normal ASCII canonicalization
    // and also check for IP addresses in the outpt.
    return doIDNHost(utf16.data(), utf16.length(), output);
}

// UTF-16 convert host to its ASCII version. The set up is already ready for
// the backend, so we just pass through. The hasEscaped flag should be set if
// the input string requires unescaping.
bool doComplexHost(const UChar* host, int hostLength, bool hasNonASCII, bool hasEscaped, URLBuffer<char>& output)
{
    if (hasEscaped) {
        // Yikes, we have escaped characters with wide input. The escaped
        // characters should be interpreted as UTF-8. To solve this problem,
        // we convert to UTF-8, unescape, then convert back to UTF-16 for IDN.
        //
        // We don't bother to optimize the conversion in the ASCII case (which
        // *could* just be a copy) and use the UTF-8 path, because it should be
        // very rare that host names have escaped characters, and it is relatively
        // fast to do the conversion anyway.
        StackBuffer utf8;
        if (!ConvertUTF16ToUTF8(host, hostLength, utf8)) {
            AppendInvalidNarrowString(host, 0, hostLength, output);
            return false;
        }

        // Once we convert to UTF-8, we can use the 8-bit version of the complex
        // host handling code above.
        return doComplexHost(utf8.data(), utf8.length(), hasNonASCII,
                             hasEscaped, output);
    }

    // No unescaping necessary, we can safely pass the input to ICU. This
    // function will only get called if we either have escaped or non-ascii
    // input, so it's safe to just use ICU now. Even if the input is ASCII,
    // this function will do the right thing (just slower than we could).
    return doIDNHost(host, hostLength, output);
}

template<typename CharacterType, typename UCHAR>
void doHost(const CharacterType* spec, const URLComponent& host, URLBuffer<char>& output, CanonHostInfo& hostInfo)
{
    if (host.length() <= 0) {
        // Empty hosts don't need anything.
        hostInfo.family = CanonHostInfo::NEUTRAL;
        hostInfo.ouputHost = URLComponent();
        return;
    }

    bool hasNonASCII;
    bool hasEscaped;
    scanHostname<CharacterType, UCHAR>(spec, host, hasNonASCII, hasEscaped);

    // Keep track of output's initial length, so we can rewind later.
    const int outputBegin = output.length();

    bool success;
    if (!hasNonASCII && !hasEscaped) {
        success = doSimpleHost(&spec[host.begin()], host.length(), output, hasNonASCII);
        ASSERT(!hasNonASCII);
    } else
        success = doComplexHost(&spec[host.begin()], host.length(), hasNonASCII, hasEscaped, output);

    if (!success) {
        // Canonicalization failed. Set BROKEN to notify the caller.
        hostInfo.family = CanonHostInfo::BROKEN;
    } else {
        // After all the other canonicalization, check if we ended up with an IP
        // address. IP addresses are small, so writing into this temporary buffer
        // should not cause an allocation.
        RawURLBuffer<char, 64> canon_ip;
        canonicalizeIPAddress(output.data(), URLComponent::fromRange(outputBegin, output.length()), canon_ip, hostInfo);

        // If we got an IPv4/IPv6 address, copy the canonical form back to the
        // real buffer. Otherwise, it's a hostname or broken IP, in which case
        // we just leave it in place.
        if (hostInfo.IsIPAddress()) {
            output.setLength(outputBegin);
            output.append(canon_ip.data(), canon_ip.length());
        }
    }

    hostInfo.ouputHost = URLComponent::fromRange(outputBegin, output.length());
}

} // namespace

bool canonicalizeHost(const char* spec, const URLComponent& host, URLBuffer<char>& output, URLComponent& ouputHost)
{
    CanonHostInfo hostInfo;
    doHost<char, unsigned char>(spec, host, output, hostInfo);
    ouputHost = hostInfo.ouputHost;
    return (hostInfo.family != CanonHostInfo::BROKEN);
}

bool canonicalizeHost(const UChar* spec, const URLComponent& host, URLBuffer<char>& output, URLComponent& ouputHost)
{
    CanonHostInfo hostInfo;
    doHost<UChar, UChar>(spec, host, output, hostInfo);
    ouputHost = hostInfo.ouputHost;
    return (hostInfo.family != CanonHostInfo::BROKEN);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
