/*
 * Copyright 2008 Google Inc. All rights reserved.
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

// Functions for canonicalizing "mailto:" URLs.

#include "config.h"
#include "URLCanon.h"

#include "RawURLBuffer.h"
#include "URLCanonInternal.h"
#include "URLFile.h"
#include "URLParseInternal.h"

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {


template<typename CharacterType, typename UCHAR>
bool doCanonicalizeMailtoURL(const URLComponentSource<CharacterType>& source, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& outputParsed)
{
    // mailto: only uses {scheme, path, query} -- clear the rest.
    outputParsed.username = URLComponent();
    outputParsed.password = URLComponent();
    outputParsed.host = URLComponent();
    outputParsed.port = URLComponent();
    outputParsed.fragment = URLComponent();

    // Scheme (known, so we don't bother running it through the more
    // complicated scheme canonicalizer).
    outputParsed.scheme.setBegin(output.length());
    output.append("mailto:", 7);
    outputParsed.scheme.setLength(6);

    bool success = true;

    // Path
    if (parsed.path.isValid()) {
        outputParsed.path.setBegin(output.length());

        // Copy the path using path URL's more lax escaping rules.
        // We convert to UTF-8 and escape non-ASCII, but leave all
        // ASCII characters alone.
        int end = parsed.path.end();
        for (int i = parsed.path.begin(); i < end; ++i) {
            UCHAR uch = static_cast<UCHAR>(source.path[i]);
            if (uch < 0x20 || uch >= 0x80)
                success &= AppendUTF8EscapedChar(source.path, &i, end, output);
            else
                output.append(static_cast<char>(uch));
        }

        outputParsed.path.setLength(output.length() - outputParsed.path.begin());
    } else {
        // No path at all
        outputParsed.path.reset();
    }

    // Query -- always use the default utf8 charset converter.
    CanonicalizeQuery(source.query, parsed.query, 0, output, &outputParsed.query);

    return success;
}

} // namespace

bool canonicalizeMailtoURL(const char* spec, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& outputParsed)
{
    return doCanonicalizeMailtoURL<char, unsigned char>(URLComponentSource<char>(spec), parsed, output, outputParsed);
}

bool canonicalizeMailtoURL(const UChar* spec, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& outputParsed)
{
    return doCanonicalizeMailtoURL<UChar, UChar>(URLComponentSource<UChar>(spec), parsed, output, outputParsed);
}

bool replaceMailtoURL(const char* base, const URLSegments& baseParsed,
                      const Replacements<char>& replacements,
                      URLBuffer<char>& output, URLSegments& outputParsed)
{
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupOverrideComponents(base, replacements, &source, &parsed);
    return doCanonicalizeMailtoURL<char, unsigned char>(source, parsed, output, outputParsed);
}

bool replaceMailtoURL(const char* base, const URLSegments& baseParsed,
                      const Replacements<UChar>& replacements,
                      URLBuffer<char>& output, URLSegments& outputParsed)
{
    RawURLBuffer<char> utf8;
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupUTF16OverrideComponents(base, replacements, utf8, &source, &parsed);
    return doCanonicalizeMailtoURL<char, unsigned char>(source, parsed, output, outputParsed);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
