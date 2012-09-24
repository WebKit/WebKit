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
#include "URLCanonInternal.h"

#include <cstdio>
#include <errno.h>
#include <stdlib.h>
#include <string>

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

template<typename CharacterType, typename UCHAR>
void doAppendStringOfType(const CharacterType* source, int length, URLCharacterTypes::CharacterTypes type, URLBuffer<char>& output)
{
    for (int i = 0; i < length; ++i) {
        if (static_cast<UCHAR>(source[i]) >= 0x80) {
            // ReadChar will fill the code point with kUnicodeReplacementCharacter
            // when the input is invalid, which is what we want.
            unsigned codePoint;
            readUTFChar(source, &i, length, &codePoint);
            AppendUTF8EscapedValue(codePoint, output);
        } else {
            // Just append the 7-bit character, possibly escaping it.
            unsigned char uch = static_cast<unsigned char>(source[i]);
            if (!URLCharacterTypes::isCharacterOfType(uch, type))
                appendURLEscapedCharacter(uch, output);
            else
                output.append(uch);
        }
    }
}

// This function assumes the input values are all contained in 8-bit,
// although it allows any type. Returns true if input is valid, false if not.
template<typename CharacterType, typename UCHAR>
void doAppendInvalidNarrowString(const CharacterType* spec, int begin, int end, URLBuffer<char>& output)
{
    for (int i = begin; i < end; ++i) {
        UCHAR uch = static_cast<UCHAR>(spec[i]);
        if (uch >= 0x80) {
            // Handle UTF-8/16 encodings. This call will correctly handle the error
            // case by appending the invalid character.
            AppendUTF8EscapedChar(spec, &i, end, output);
        } else if (uch <= ' ' || uch == 0x7f) {
            // This function is for error handling, so we escape all control
            // characters and spaces, but not anything else since we lack
            // context to do something more specific.
            appendURLEscapedCharacter(static_cast<unsigned char>(uch), output);
        } else
            output.append(static_cast<char>(uch));
    }
}

// Overrides one component, see the URLCanonicalizer::Replacements structure for
// what the various combionations of source pointer and component mean.
void doOverrideComponent(const char* overrideSource, const URLComponent& overrideComponent, const char*& destination, URLComponent& destinationComponent)
{
    if (overrideSource) {
        destination = overrideSource;
        destinationComponent = overrideComponent;
    }
}

// Similar to doOverrideComponent except that it takes a UTF-16 input and does
// not actually set the output character pointer.
//
// The input is converted to UTF-8 at the end of the given buffer as a temporary
// holding place. The component indentifying the portion of the buffer used in
// the |utf8Buffer| will be specified in |*destinationComponent|.
//
// This will not actually set any |dest| pointer like doOverrideComponent
// does because all of the pointers will point into the |utf8Buffer|, which
// may get resized while we're overriding a subsequent component. Instead, the
// caller should use the beginning of the |utf8Buffer| as the string pointer
// for all components once all overrides have been prepared.
bool PrepareUTF16OverrideComponent(const UChar* overrideSource,
                                   const URLComponent& overrideComponent,
                                   URLBuffer<char>& utf8Buffer,
                                   URLComponent* destinationComponent)
{
    bool success = true;
    if (overrideSource) {
        if (!overrideComponent.isValid()) {
            // Non-"valid" component (means delete), so we need to preserve that.
            *destinationComponent = URLComponent();
        } else {
            // Convert to UTF-8.
            destinationComponent->setBegin(utf8Buffer.length());
            success = ConvertUTF16ToUTF8(&overrideSource[overrideComponent.begin()],
                                         overrideComponent.length(), utf8Buffer);
            destinationComponent->setLength(utf8Buffer.length() - destinationComponent->begin());
        }
    }
    return success;
}

} // namespace

const char kCharToHexLookup[8] = {
    0, // 0x00 - 0x1f
    '0', // 0x20 - 0x3f: digits 0 - 9 are 0x30 - 0x39
    'A' - 10, // 0x40 - 0x5f: letters A - F are 0x41 - 0x46
    'a' - 10, // 0x60 - 0x7f: letters a - f are 0x61 - 0x66
    0, // 0x80 - 0x9F
    0, // 0xA0 - 0xBF
    0, // 0xC0 - 0xDF
    0, // 0xE0 - 0xFF
};

const UChar kUnicodeReplacementCharacter = 0xfffd;

void appendStringOfType(const char* source, int length, URLCharacterTypes::CharacterTypes urlComponentType, URLBuffer<char>& output)
{
    doAppendStringOfType<char, unsigned char>(source, length, urlComponentType, output);
}

void appendStringOfType(const UChar* source, int length, URLCharacterTypes::CharacterTypes urlComponentType, URLBuffer<char>& output)
{
    doAppendStringOfType<UChar, UChar>(source, length, urlComponentType, output);
}

void AppendInvalidNarrowString(const char* spec, int begin, int end, URLBuffer<char>& output)
{
    doAppendInvalidNarrowString<char, unsigned char>(spec, begin, end, output);
}

void AppendInvalidNarrowString(const UChar* spec, int begin, int end, URLBuffer<char>& output)
{
    doAppendInvalidNarrowString<UChar, UChar>(spec, begin, end, output);
}

bool ConvertUTF16ToUTF8(const UChar* input, int inputLength, URLBuffer<char>& output)
{
    bool success = true;
    for (int i = 0; i < inputLength; ++i) {
        unsigned codePoint;
        success &= readUTFChar(input, &i, inputLength, &codePoint);
        AppendUTF8Value(codePoint, output);
    }
    return success;
}

bool ConvertUTF8ToUTF16(const char* input, int inputLength, URLBuffer<UChar>& output)
{
    bool success = true;
    for (int i = 0; i < inputLength; i++) {
        unsigned codePoint;
        success &= readUTFChar(input, &i, inputLength, &codePoint);
        AppendUTF16Value(codePoint, output);
    }
    return success;
}

void SetupOverrideComponents(const char* /* base */,
                             const Replacements<char>& repl,
                             URLComponentSource<char>* source,
                             URLSegments* parsed)
{
    // Get the source and parsed structures of the things we are replacing.
    const URLComponentSource<char>& replSource = repl.sources();
    const URLSegments& replParsed = repl.components();

    doOverrideComponent(replSource.scheme, replParsed.scheme, source->scheme, parsed->scheme);
    doOverrideComponent(replSource.username, replParsed.username, source->username, parsed->username);
    doOverrideComponent(replSource.password, replParsed.password, source->password, parsed->password);

    // Our host should be empty if not present, so override the default setup.
    doOverrideComponent(replSource.host, replParsed.host, source->host, parsed->host);
    if (parsed->host.length() == -1)
        parsed->host.setLength(0);

    doOverrideComponent(replSource.port, replParsed.port, source->port, parsed->port);
    doOverrideComponent(replSource.path, replParsed.path, source->path, parsed->path);
    doOverrideComponent(replSource.query, replParsed.query, source->query, parsed->query);
    doOverrideComponent(replSource.ref, replParsed.fragment, source->ref, parsed->fragment);
}

bool SetupUTF16OverrideComponents(const char* /* base */,
                                  const Replacements<UChar>& repl,
                                  URLBuffer<char>& utf8Buffer,
                                  URLComponentSource<char>* source,
                                  URLSegments* parsed)
    {
        bool success = true;

        // Get the source and parsed structures of the things we are replacing.
        const URLComponentSource<UChar>& replSource = repl.sources();
        const URLSegments& replParsed = repl.components();

        success &= PrepareUTF16OverrideComponent(replSource.scheme, replParsed.scheme,
                                                 utf8Buffer, &parsed->scheme);
        success &= PrepareUTF16OverrideComponent(replSource.username, replParsed.username,
                                                 utf8Buffer, &parsed->username);
        success &= PrepareUTF16OverrideComponent(replSource.password, replParsed.password,
                                                 utf8Buffer, &parsed->password);
        success &= PrepareUTF16OverrideComponent(replSource.host, replParsed.host,
                                                 utf8Buffer, &parsed->host);
        success &= PrepareUTF16OverrideComponent(replSource.port, replParsed.port,
                                                 utf8Buffer, &parsed->port);
        success &= PrepareUTF16OverrideComponent(replSource.path, replParsed.path,
                                                 utf8Buffer, &parsed->path);
        success &= PrepareUTF16OverrideComponent(replSource.query, replParsed.query,
                                                 utf8Buffer, &parsed->query);
        success &= PrepareUTF16OverrideComponent(replSource.ref, replParsed.fragment,
                                                 utf8Buffer, &parsed->fragment);

        // PrepareUTF16OverrideComponent will not have set the data pointer since the
        // buffer could be resized, invalidating the pointers. We set the data
        // pointers for affected components now that the buffer is finalized.
        if (replSource.scheme)
            source->scheme = utf8Buffer.data();
        if (replSource.username)
            source->username = utf8Buffer.data();
        if (replSource.password)
            source->password = utf8Buffer.data();
        if (replSource.host)
            source->host = utf8Buffer.data();
        if (replSource.port)
            source->port = utf8Buffer.data();
        if (replSource.path)
            source->path = utf8Buffer.data();
        if (replSource.query)
            source->query = utf8Buffer.data();
        if (replSource.ref)
            source->ref = utf8Buffer.data();

        return success;
}

#if !OS(WINDOWS)
int _itoa_s(int value, char* buffer, size_t sizeInChars, int radix)
{
    int written;
    if (radix == 10)
        written = snprintf(buffer, sizeInChars, "%d", value);
    else if (radix == 16)
        written = snprintf(buffer, sizeInChars, "%x", value);
    else
        return EINVAL;

    if (static_cast<size_t>(written) >= sizeInChars) {
        // Output was truncated, or written was negative.
        return EINVAL;
    }
    return 0;
}

int _itow_s(int value, UChar* buffer, size_t sizeInChars, int radix)
{
    if (radix != 10)
        return EINVAL;

    // No more than 12 characters will be required for a 32-bit integer.
    // Add an extra byte for the terminating null.
    char temp[13];
    int written = snprintf(temp, sizeof(temp), "%d", value);
    if (static_cast<size_t>(written) >= sizeInChars) {
        // Output was truncated, or written was negative.
        return EINVAL;
    }

    for (int i = 0; i < written; ++i)
        buffer[i] = static_cast<UChar>(temp[i]);
    buffer[written] = '\0';
    return 0;
}

#endif // !OS(WINDOWS)

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
