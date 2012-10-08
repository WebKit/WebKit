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

// Canonicalizers for random bits that aren't big enough for their own files.

#include "config.h"
#include "URLCanon.h"

#include "URLCanonInternal.h"
#include <wtf/ASCIICType.h>

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

// Returns true if the given character should be removed from the middle of a
// URL.
inline bool isRemovableURLWhitespace(int character)
{
    return character == '\r' || character == '\n' || character == '\t';
}

// Backend for removeURLWhitespace (see declaration in URLCanon.h).
// It sucks that we have to do this, since this takes about 13% of the total URL
// canonicalization time.
template<typename CharacterType>
const CharacterType* doRemoveURLWhitespace(const CharacterType* input, int inputLength, URLBuffer<CharacterType>& buffer, int& outputLength)
{
    // Fast verification that there's nothing that needs removal. This is the 99%
    // case, so we want it to be fast and don't care about impacting the speed
    // when we do find whitespace.
    bool foundWhitespace = false;
    for (int i = 0; i < inputLength; ++i) {
        if (!isRemovableURLWhitespace(input[i]))
            continue;
        foundWhitespace = true;
        break;
    }

    if (!foundWhitespace) {
        // Didn't find any whitespace, we don't need to do anything. We can just
        // return the input as the output.
        outputLength = inputLength;
        return input;
    }

    // Remove the whitespace into the new buffer and return it.
    for (int i = 0; i < inputLength; i++) {
        if (!isRemovableURLWhitespace(input[i]))
            buffer.append(input[i]);
    }
    outputLength = buffer.length();
    return buffer.data();
}

// Contains the canonical version of each possible input letter in the scheme
// (basically, lower-cased). The corresponding entry will be 0 if the letter
// is not allowed in a scheme.
const char kSchemeCanonical[0x80] = {
// 00-1f: all are invalid
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
//  ' '   !    "    #    $    %    &    '    (    )    *    +    ,    -    .    /
     0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  '+',  0,  '-', '.',  0,
//   0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ?
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
//   @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O
     0 , 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
//   P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',  0,   0 ,  0,   0 ,  0,
//   `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o
     0 , 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
//   p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',  0 ,  0 ,  0 ,  0 ,  0 };

// This could be a table lookup as well by setting the high bit for each
// valid character, but it's only called once per URL, and it makes the lookup
// table easier to read not having extra stuff in it.
inline bool isSchemeFirstChar(unsigned char character)
{
    return isASCIIAlpha(character);
}

template<typename CharacterType, typename UCHAR>
bool doScheme(const CharacterType* spec, const URLComponent& scheme, URLBuffer<char>& output, URLComponent& outputScheme)
{
    if (scheme.length() <= 0) {
        // Scheme is unspecified or empty, convert to empty by appending a colon.
        outputScheme = URLComponent(output.length(), 0);
        output.append(':');
        return true;
    }

    // The output scheme starts from the current position.
    outputScheme.moveBy(output.length());

    // Danger: it's important that this code does not strip any characters: it
    // only emits the canonical version (be it valid or escaped) of each of
    // the input characters. Stripping would put it out of sync with
    // URLUtilities::FindAndCompareScheme, which could cause some security checks on
    // schemes to be incorrect.
    bool success = true;
    int end = scheme.end();
    for (int i = scheme.begin(); i < end; ++i) {
        UCHAR character = static_cast<UCHAR>(spec[i]);
        char replacement = 0;
        if (character < 0x80) {
            if (i == scheme.begin()) {
                // Need to do a special check for the first letter of the scheme.
                if (isSchemeFirstChar(static_cast<unsigned char>(character)))
                    replacement = kSchemeCanonical[character];
            } else
                replacement = kSchemeCanonical[character];
        }

        if (replacement)
            output.append(replacement);
        else if (character == '%') {
            // Canonicalizing the scheme multiple times should lead to the same
            // result. Since invalid characters will be escaped, we need to preserve
            // the percent to avoid multiple escaping. The scheme will be invalid.
            success = false;
            output.append('%');
        } else {
            // Invalid character, store it but mark this scheme as invalid.
            success = false;

            // This will escape the output and also handle encoding issues.
            // Ignore the return value since we already failed.
            AppendUTF8EscapedChar(spec, &i, end, output);
        }
    }

    // The output scheme ends with the the current position, before appending
    // the colon.
    outputScheme.setLength(output.length() - outputScheme.begin());
    output.append(':');
    return success;
}

// The username and password components reference ranges in the corresponding
// *_spec strings. Typically, these specs will be the same (we're
// canonicalizing a single source string), but may be different when
// replacing components.
template<typename CharacterType, typename UCHAR>
bool doUserInfo(const CharacterType* usernameSpec,
                const URLComponent& username,
                const CharacterType* passwordSpec,
                const URLComponent& password,
                URLBuffer<char>& output,
                URLComponent& outputUsername,
                URLComponent& outputPassword)
{
    if (username.length() <= 0 && password.length() <= 0) {
        // Common case: no user info. We strip empty username/passwords.
        outputUsername = URLComponent();
        outputPassword = URLComponent();
        return true;
    }

    // Write the username.
    outputUsername.setBegin(output.length());
    if (username.length() > 0) {
        // This will escape characters not valid for the username.
        appendStringOfType(&usernameSpec[username.begin()], username.length(), URLCharacterTypes::UserInfoCharacter, output);
    }
    outputUsername.setLength(output.length() - outputUsername.begin());

    // When there is a password, we need the separator. Note that we strip
    // empty but specified passwords.
    if (password.length() > 0) {
        output.append(':');
        outputPassword.setBegin(output.length());
        appendStringOfType(&passwordSpec[password.begin()], password.length(), URLCharacterTypes::UserInfoCharacter, output);
        outputPassword.setLength(output.length() - outputPassword.begin());
    } else
        outputPassword = URLComponent();

    output.append('@');
    return true;
}

// Helper functions for converting port integers to strings.
inline void writePortInt(char* output, int outputLength, int port)
{
    _itoa_s(port, output, outputLength, 10);
}

// This function will prepend the colon if there will be a port.
template<typename CharacterType, typename UCHAR>
bool doPort(const CharacterType* spec,
            const URLComponent& port,
            int defaultPortForScheme,
            URLBuffer<char>& output,
            URLComponent& outputPortComponent)
{
    int portNumber = URLParser::ParsePort(spec, port);
    if (portNumber == URLParser::PORT_UNSPECIFIED || portNumber == defaultPortForScheme) {
        outputPortComponent = URLComponent();
        return true; // Leave port empty.
    }

    if (portNumber == URLParser::PORT_INVALID) {
        // Invalid port: We'll copy the text from the input so the user can see
        // what the error was, and mark the URL as invalid by returning false.
        output.append(':');
        outputPortComponent.setBegin(output.length());
        AppendInvalidNarrowString(spec, port.begin(), port.end(), output);
        outputPortComponent.setLength(output.length() - outputPortComponent.begin());
        return false;
    }

    // Convert port number back to an integer. Max port value is 5 digits, and
    // the Parsed::ExtractPort will have made sure the integer is in range.
    const int bufferSize = 6;
    char buffer[bufferSize];
    writePortInt(buffer, bufferSize, portNumber);

    // Append the port number to the output, preceeded by a colon.
    output.append(':');
    outputPortComponent.setBegin(output.length());
    for (int i = 0; i < bufferSize && buffer[i]; ++i)
        output.append(buffer[i]);

    outputPortComponent.setLength(output.length() - outputPortComponent.begin());
    return true;
}

template<typename CharacterType, typename UCHAR>
void doCanonicalizeFragment(const CharacterType* spec, const URLComponent& fragment, URLBuffer<char>& output, URLComponent& outputFragment)
{
    if (fragment.length() < 0) {
        // Common case of no fragment.
        outputFragment = URLComponent();
        return;
    }

    // Append the fragment separator. Note that we need to do this even when the fragment
    // is empty but present.
    output.append('#');
    outputFragment.setBegin(output.length());

    // Now iterate through all the characters, converting to UTF-8 and validating.
    int end = fragment.end();
    for (int i = fragment.begin(); i < end; ++i) {
        if (!spec[i]) {
            // IE just strips NULLs, so we do too.
            continue;
        }
        if (static_cast<UCHAR>(spec[i]) < 0x20) {
            // Unline IE seems to, we escape control characters. This will probably
            // make the reference fragment unusable on a web page, but people
            // shouldn't be using control characters in their anchor names.
            appendURLEscapedCharacter(static_cast<unsigned char>(spec[i]), output);
        } else if (static_cast<UCHAR>(spec[i]) < 0x80) {
            // Normal ASCII characters are just appended.
            output.append(static_cast<char>(spec[i]));
        } else {
            // Non-ASCII characters are appended unescaped, but only when they are
            // valid. Invalid Unicode characters are replaced with the "invalid
            // character" as IE seems to (readUTFChar puts the unicode replacement
            // character in the output on failure for us).
            unsigned codePoint;
            readUTFChar(spec, &i, end, &codePoint);
            AppendUTF8Value(codePoint, output);
        }
    }

    outputFragment.setLength(output.length() - outputFragment.begin());
}

} // namespace

const char* removeURLWhitespace(const char* input, int inputLength, URLBuffer<char>& buffer, int& outputLength)
{
    return doRemoveURLWhitespace(input, inputLength, buffer, outputLength);
}

const UChar* removeURLWhitespace(const UChar* input, int inputLength, URLBuffer<UChar>& buffer, int& outputLength)
{
    return doRemoveURLWhitespace(input, inputLength, buffer, outputLength);
}

char canonicalSchemeChar(UChar character)
{
    if (character >= 0x80)
        return 0; // Non-ASCII is not supported by schemes.
    return kSchemeCanonical[character];
}

bool canonicalizeScheme(const char* spec, const URLComponent& scheme, URLBuffer<char>& output, URLComponent& outputScheme)
{
    return doScheme<char, unsigned char>(spec, scheme, output, outputScheme);
}

bool canonicalizeScheme(const UChar* spec, const URLComponent& scheme, URLBuffer<char>& output, URLComponent& outputScheme)
{
    return doScheme<UChar, UChar>(spec, scheme, output, outputScheme);
}

bool canonicalizeUserInfo(const char* usernameSource, const URLComponent& username, const char* passwordSource, const URLComponent& password,
                          URLBuffer<char>& output, URLComponent& outputUsername, URLComponent& outputPassword)
{
    return doUserInfo<char, unsigned char>(usernameSource, username, passwordSource, password, output, outputUsername, outputPassword);
}

bool canonicalizeUserInfo(const UChar* usernameSource, const URLComponent& username, const UChar* passwordSource, const URLComponent& password,
                          URLBuffer<char>& output, URLComponent& outputUsername, URLComponent& outputPassword)
{
    return doUserInfo<UChar, UChar>(usernameSource, username, passwordSource, password, output, outputUsername, outputPassword);
}

bool canonicalizePort(const char* spec, const URLComponent& port, int defaultPortForScheme,
                      URLBuffer<char>& output, URLComponent& outputPortComponent)
{
    return doPort<char, unsigned char>(spec, port,
                                       defaultPortForScheme,
                                       output, outputPortComponent);
}

bool canonicalizePort(const UChar* spec, const URLComponent& port, int defaultPortForScheme,
                      URLBuffer<char>& output, URLComponent& outputPortComponent)
{
    return doPort<UChar, UChar>(spec, port, defaultPortForScheme,
                                output, outputPortComponent);
}

void canonicalizeFragment(const char* spec, const URLComponent& ref, URLBuffer<char>& output, URLComponent& outputFragment)
{
    doCanonicalizeFragment<char, unsigned char>(spec, ref, output, outputFragment);
}

void canonicalizeFragment(const UChar* spec, const URLComponent& ref, URLBuffer<char>& output, URLComponent& outputFragment)
{
    doCanonicalizeFragment<UChar, UChar>(spec, ref, output, outputFragment);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
