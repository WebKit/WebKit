/*
 * Copyright 2012 Google Inc. All rights reserved.
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

// Functions for canonicalizing "filesystem:file:" URLs.

#include "config.h"
#include "URLCanon.h"

#include "RawURLBuffer.h"
#include "URLCanonInternal.h"
#include "URLFile.h"
#include "URLParseInternal.h"
#include "URLUtil.h"
#include "URLUtilInternal.h"

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

// We use the URLComponentSource for the outer URL, as it can have replacements,
// whereas the inner_url can't, so it uses spec.
template<typename CharacterType, typename UCHAR>
bool doCanonicalizeFileSystemURL(const CharacterType* spec,
                                 const URLComponentSource<CharacterType>& source,
                                 const URLSegments& parsed,
                                 CharsetConverter* charsetConverter,
                                 URLBuffer<char>& output,
                                 URLSegments& outputParsed)
{
    // filesystem only uses {scheme, path, query, ref} -- clear the rest.
    outputParsed.username = URLComponent();
    outputParsed.password = URLComponent();
    outputParsed.host = URLComponent();
    outputParsed.port = URLComponent();

    const URLSegments* innerParsed = parsed.innerURLSegments();
    URLSegments newInnerParsed;

    // Scheme (known, so we don't bother running it through the more
    // complicated scheme canonicalizer).
    outputParsed.scheme.setBegin(output.length());
    output.append("filesystem:", 11);
    outputParsed.scheme.setLength(10);

    if (!parsed.innerURLSegments() || !parsed.innerURLSegments()->scheme.isValid())
        return false;

    bool success = true;
    if (URLUtilities::CompareSchemeComponent(spec, innerParsed->scheme, URLUtilities::kFileScheme)) {
        newInnerParsed.scheme.setBegin(output.length());
        output.append("file://", 7);
        newInnerParsed.scheme.setLength(4);
        success &= CanonicalizePath(spec, innerParsed->path, output,
                                    &newInnerParsed.path);
    } else if (URLUtilities::isStandard(spec, innerParsed->scheme)) {
        success =
        URLCanonicalizer::CanonicalizeStandardURL(spec,
                                                  parsed.innerURLSegments()->length(),
                                                  *parsed.innerURLSegments(),
                                                  charsetConverter, output,
                                                  &newInnerParsed);
    } else {
        // TODO(ericu): The URL is wrong, but should we try to output more of what
        // we were given? Echoing back filesystem:mailto etc. doesn't seem all that useful.
        return false;
    }
    // The filesystem type must be more than just a leading slash for validity.
    success &= parsed.innerURLSegments()->path.length() > 1;

    success &= CanonicalizePath(source.path, parsed.path, output, &outputParsed.path);

    // Ignore failures for query/ref since the URL can probably still be loaded.
    CanonicalizeQuery(source.query, parsed.query, charsetConverter, output, &outputParsed.query);
    canonicalizeFragment(source.ref, parsed.fragment, output, outputParsed.fragment);
    if (success)
        outputParsed.setInnerURLSegments(newInnerParsed);

    return success;
}

} // namespace

bool canonicalizeFileSystemURL(const char* spec,
                               const URLSegments& parsed,
                               CharsetConverter* charsetConverter,
                               URLBuffer<char>& output,
                               URLSegments& outputParsed)
{
    return doCanonicalizeFileSystemURL<char, unsigned char>(spec, URLComponentSource<char>(spec), parsed, charsetConverter, output, outputParsed);
}

bool canonicalizeFileSystemURL(const UChar* spec,
                               const URLSegments& parsed,
                               CharsetConverter* charsetConverter,
                               URLBuffer<char>& output,
                               URLSegments& outputParsed)
{
    return doCanonicalizeFileSystemURL<UChar, UChar>(spec, URLComponentSource<UChar>(spec), parsed, charsetConverter, output, outputParsed);
}

bool ReplaceFileSystemURL(const char* base,
                          const URLSegments& baseParsed,
                          const Replacements<char>& replacements,
                          CharsetConverter* charsetConverter,
                          URLBuffer<char>& output,
                          URLSegments* outputParsed)
{
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupOverrideComponents(base, replacements, &source, &parsed);
    return doCanonicalizeFileSystemURL<char, unsigned char>(base, source, parsed, charsetConverter, output, *outputParsed);
}

bool ReplaceFileSystemURL(const char* base,
                          const URLSegments& baseParsed,
                          const Replacements<UChar>& replacements,
                          CharsetConverter* charsetConverter,
                          URLBuffer<char>& output,
                          URLSegments* outputParsed)
{
    RawURLBuffer<char> utf8;
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupUTF16OverrideComponents(base, replacements, utf8, &source, &parsed);
    return doCanonicalizeFileSystemURL<char, unsigned char>(base, source, parsed, charsetConverter, output, *outputParsed);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
