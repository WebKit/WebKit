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

// Functions for canonicalizing "path" URLs. Not to be confused with the path
// of a URL, these are URLs that have no authority section, only a path. For
// example, "javascript:" and "data:".

#include "config.h"
#include "URLCanon.h"

#include "RawURLBuffer.h"
#include "URLCanonInternal.h"

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

template<typename CharacterType, typename UCHAR>
bool doCanonicalizePathURL(const URLComponentSource<CharacterType>& source, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& ouputParsed)
{
    // Scheme: this will append the colon.
    bool success = canonicalizeScheme(source.scheme, parsed.scheme, output, ouputParsed.scheme);

    // We assume there's no authority for path URLs. Note that hosts should never
    // have -1 length.
    ouputParsed.username.reset();
    ouputParsed.password.reset();
    ouputParsed.host.reset();
    ouputParsed.port.reset();

    if (parsed.path.isValid()) {
        // Copy the path using path URL's more lax escaping rules (think for
        // javascript:). We convert to UTF-8 and escape non-ASCII, but leave all
        // ASCII characters alone. This helps readability of JavaStript.
        ouputParsed.path.setBegin(output.length());
        int end = parsed.path.end();
        for (int i = parsed.path.begin(); i < end; ++i) {
            UCHAR uch = static_cast<UCHAR>(source.path[i]);
            if (uch < 0x20 || uch >= 0x80)
                success &= AppendUTF8EscapedChar(source.path, &i, end, output);
            else
                output.append(static_cast<char>(uch));
        }
        ouputParsed.path.setLength(output.length() - ouputParsed.path.begin());
    } else {
        // Empty path.
        ouputParsed.path.reset();
    }

    // Assume there's no query or ref.
    ouputParsed.query.reset();
    ouputParsed.fragment.reset();

    return success;
}

} // namespace

bool canonicalizePathURL(const char* spec, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& ouputParsed)
{
    return doCanonicalizePathURL<char, unsigned char>(URLComponentSource<char>(spec), parsed, output, ouputParsed);
}

bool canonicalizePathURL(const UChar* spec, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& ouputParsed)
{
    return doCanonicalizePathURL<UChar, UChar>(URLComponentSource<UChar>(spec), parsed, output, ouputParsed);
}

bool ReplacePathURL(const char* base, const URLSegments& baseParsed, const Replacements<char>& replacements, URLBuffer<char>& output, URLSegments* ouputParsed)
{
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupOverrideComponents(base, replacements, &source, &parsed);
    return doCanonicalizePathURL<char, unsigned char>(source, parsed, output, *ouputParsed);
}

bool ReplacePathURL(const char* base, const URLSegments& baseParsed, const Replacements<UChar>& replacements, URLBuffer<char>& output, URLSegments* ouputParsed)
{
    RawURLBuffer<char> utf8;
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupUTF16OverrideComponents(base, replacements, utf8, &source, &parsed);
    return doCanonicalizePathURL<char, unsigned char>(source, parsed, output, *ouputParsed);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
