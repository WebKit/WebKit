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

// Functions for canonicalizing "file:" URLs.

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

#if OS(WINDOWS)

// Given a pointer into the spec, this copies and canonicalizes the drive
// letter and colon to the output, if one is found. If there is not a drive
// spec, it won't do anything. The index of the next character in the input
// spec is returned (after the colon when a drive spec is found, the begin
// offset if one is not).
template<typename CHAR>
int FileDoDriveSpec(const CHAR* spec, int begin, int end, URLBuffer<char>& output)
{
    // The path could be one of several things: /foo/bar, c:/foo/bar, /c:/foo,
    // (with backslashes instead of slashes as well).
    int numSlashes = URLParser::countConsecutiveSlashes(spec, begin, end);
    int afterSlashes = begin + numSlashes;

    if (!URLParser::doesBeginWindowsDriveSpec(spec, afterSlashes, end))
        return begin; // Haven't consumed any characters

    // A drive spec is the start of a path, so we need to add a slash for the
    // authority terminator (typically the third slash).
    output.append('/');

    // doesBeginWindowsDriveSpec will ensure that the drive letter is valid
    // and that it is followed by a colon/pipe.

    // Normalize Windows drive letters to uppercase
    if (spec[afterSlashes] >= 'a' && spec[afterSlashes] <= 'z')
        output.append(spec[afterSlashes] - 'a' + 'A');
    else
        output.append(static_cast<char>(spec[afterSlashes]));

    // Normalize the character following it to a colon rather than pipe.
    output.append(':');
    return afterSlashes + 2;
}

#endif // OS(WINDOWS)

template<typename CharacterType, typename UCHAR>
bool doFileCanonicalizePath(const CharacterType* spec,
                            const URLComponent& path,
                            URLBuffer<char>& output,
                            URLComponent& outputPath)
{
    // Copies and normalizes the "c:" at the beginning, if present.
    outputPath.setBegin(output.length());
    int afterDrive;
#if OS(WINDOWS)
    afterDrive = FileDoDriveSpec(spec, path.begin, path.end(), output);
#else
    afterDrive = path.begin();
#endif

    // Copies the rest of the path, starting from the slash following the
    // drive colon (if any, Windows only), or the first slash of the path.
    bool success = true;
    if (afterDrive < path.end()) {
        // Use the regular path canonicalizer to canonicalize the rest of the
        // path. Give it a fake output component to write into. DoCanonicalizeFile
        // will compute the full path component.
        URLComponent subPath = URLComponent::fromRange(afterDrive, path.end());
        URLComponent fakeOutputPath;
        success = CanonicalizePath(spec, subPath, output, &fakeOutputPath);
    } else {
        // No input path, canonicalize to a slash.
        output.append('/');
    }

    outputPath.setLength(output.length() - outputPath.begin());
    return success;
}

template<typename CharacterType, typename UCHAR>
bool doCanonicalizeFileURL(const URLComponentSource<CharacterType>& source,
                           const URLSegments& parsed,
                           CharsetConverter* queryConverter,
                           URLBuffer<char>& output,
                           URLSegments& outputParsed)
{
    // Things we don't set in file: URLs.
    outputParsed.username = URLComponent();
    outputParsed.password = URLComponent();
    outputParsed.port = URLComponent();

    // Scheme (known, so we don't bother running it through the more
    // complicated scheme canonicalizer).
    outputParsed.scheme.setBegin(output.length());
    output.append("file://", 7);
    outputParsed.scheme.setLength(4);

    // Append the host. For many file URLs, this will be empty. For UNC, this
    // will be present.
    // TODO(brettw) This doesn't do any checking for host name validity. We
    // should probably handle validity checking of UNC hosts differently than
    // for regular IP hosts.
    bool success = canonicalizeHost(source.host, parsed.host, output, outputParsed.host);
    success &= doFileCanonicalizePath<CharacterType, UCHAR>(source.path, parsed.path, output, outputParsed.path);
    CanonicalizeQuery(source.query, parsed.query, queryConverter, output, &outputParsed.query);

    // Ignore failure for refs since the URL can probably still be loaded.
    canonicalizeFragment(source.ref, parsed.fragment, output, outputParsed.fragment);

    return success;
}

} // namespace

bool CanonicalizeFileURL(const char* spec,
                         int /* specLength */,
                         const URLSegments& parsed,
                         CharsetConverter* queryConverter,
                         URLBuffer<char>& output,
                         URLSegments* outputParsed)
{
    return doCanonicalizeFileURL<char, unsigned char>(URLComponentSource<char>(spec), parsed, queryConverter, output, *outputParsed);
}

bool CanonicalizeFileURL(const UChar* spec,
                         int /* specLength */,
                         const URLSegments& parsed,
                         CharsetConverter* queryConverter,
                         URLBuffer<char>& output,
                         URLSegments* outputParsed)
{
    return doCanonicalizeFileURL<UChar, UChar>(URLComponentSource<UChar>(spec), parsed, queryConverter, output, *outputParsed);
}

bool FileCanonicalizePath(const char* spec,
                          const URLComponent& path,
                          URLBuffer<char>& output,
                          URLComponent* outputPath)
{
    return doFileCanonicalizePath<char, unsigned char>(spec, path, output, *outputPath);
}

bool FileCanonicalizePath(const UChar* spec,
                          const URLComponent& path,
                          URLBuffer<char>& output,
                          URLComponent* outputPath)
{
    return doFileCanonicalizePath<UChar, UChar>(spec, path, output, *outputPath);
}

bool ReplaceFileURL(const char* base,
                    const URLSegments& baseParsed,
                    const Replacements<char>& replacements,
                    CharsetConverter* queryConverter,
                    URLBuffer<char>& output,
                    URLSegments* outputParsed)
{
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupOverrideComponents(base, replacements, &source, &parsed);
    return doCanonicalizeFileURL<char, unsigned char>(source, parsed, queryConverter, output, *outputParsed);
}

bool ReplaceFileURL(const char* base,
                    const URLSegments& baseParsed,
                    const Replacements<UChar>& replacements,
                    CharsetConverter* queryConverter,
                    URLBuffer<char>& output,
                    URLSegments* outputParsed)
{
    RawURLBuffer<char> utf8;
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupUTF16OverrideComponents(base, replacements, utf8, &source, &parsed);
    return doCanonicalizeFileURL<char, unsigned char>(source, parsed, queryConverter, output, *outputParsed);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
