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

// Functions to canonicalize "standard" URLs, which are ones that have an
// authority section including a host name.

#include "config.h"
#include "URLCanon.h"

#include "RawURLBuffer.h"
#include "URLCanonInternal.h"

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

template<typename CHAR, typename UCHAR>
bool doCanonicalizeStandardURL(const URLComponentSource<CHAR>& source,
                               const URLSegments& parsed,
                               URLQueryCharsetConverter* queryConverter,
                               URLBuffer<char>& output,
                               URLSegments& outputParsed)
{
    // Scheme: this will append the colon.
    bool success = canonicalizeScheme(source.scheme, parsed.scheme, output, outputParsed.scheme);

    // Authority (username, password, host, port)
    bool haveAuthority;
    if (parsed.username.isNonEmpty() || parsed.password.isValid() || parsed.host.isNonEmpty() || parsed.port.isValid()) {
        haveAuthority = true;

        // Only write the authority separators when we have a scheme.
        if (parsed.scheme.isValid())
            output.append("//", 2);

        // User info: the canonicalizer will handle the : and @.
        success &= canonicalizeUserInfo(source.username, parsed.username, source.password, parsed.password,
                                        output, outputParsed.username, outputParsed.password);
        success &= canonicalizeHost(source.host, parsed.host, output, outputParsed.host);

        // Host must not be empty for standard URLs.
        if (!parsed.host.isNonEmpty())
            success = false;

        // Port: the port canonicalizer will handle the colon.
        int defaultPort = defaultPortForScheme(&output.data()[outputParsed.scheme.begin()], outputParsed.scheme.length());
        success &= canonicalizePort(source.port, parsed.port, defaultPort, output, outputParsed.port);
    } else {
        // No authority, clear the components.
        haveAuthority = false;
        outputParsed.host.reset();
        outputParsed.username.reset();
        outputParsed.password.reset();
        outputParsed.port.reset();
        success = false; // Standard URLs must have an authority.
    }

    // Path
    if (parsed.path.isNonEmpty())
        success &= CanonicalizePath(source.path, parsed.path, output, &outputParsed.path);
    else if (haveAuthority || parsed.query.isNonEmpty() || parsed.fragment.isNonEmpty()) {
        // When we have an empty path, make up a path when we have an authority
        // or something following the path. The only time we allow an empty
        // output path is when there is nothing else.
        outputParsed.path = URLComponent(output.length(), 1);
        output.append('/');
    } else
        outputParsed.path.reset(); // No path at all

    // Query
    CanonicalizeQuery(source.query, parsed.query, queryConverter, output, &outputParsed.query);

    // Ref: ignore failure for this, since the page can probably still be loaded.
    canonicalizeFragment(source.ref, parsed.fragment, output, outputParsed.fragment);

    return success;
}

} // namespace


// Returns the default port for the given canonical scheme, or PORT_UNSPECIFIED
// if the scheme is unknown.
int defaultPortForScheme(const char* scheme, int schemeLength)
{
    int defaultPort = URLParser::PORT_UNSPECIFIED;
    switch (schemeLength) {
    case 4:
        if (!strncmp(scheme, "http", schemeLength))
            defaultPort = 80;
        break;
    case 5:
        if (!strncmp(scheme, "https", schemeLength))
            defaultPort = 443;
        break;
    case 3:
        if (!strncmp(scheme, "ftp", schemeLength))
            defaultPort = 21;
        else if (!strncmp(scheme, "wss", schemeLength))
            defaultPort = 443;
        break;
    case 6:
        if (!strncmp(scheme, "gopher", schemeLength))
            defaultPort = 70;
        break;
    case 2:
        if (!strncmp(scheme, "ws", schemeLength))
            defaultPort = 80;
        break;
    }
    return defaultPort;
}

bool CanonicalizeStandardURL(const char* spec,
                             int /* specLength */,
                             const URLSegments& parsed,
                             URLQueryCharsetConverter* queryConverter,
                             URLBuffer<char>& output,
                             URLSegments* outputParsed)
{
    return doCanonicalizeStandardURL<char, unsigned char>(URLComponentSource<char>(spec), parsed, queryConverter,
                                                          output, *outputParsed);
}

bool CanonicalizeStandardURL(const UChar* spec,
                             int /* specLength */,
                             const URLSegments& parsed,
                             URLQueryCharsetConverter* queryConverter,
                             URLBuffer<char>& output,
                             URLSegments* outputParsed)
{
    return doCanonicalizeStandardURL<UChar, UChar>(URLComponentSource<UChar>(spec), parsed, queryConverter,
                                                   output, *outputParsed);
}

// It might be nice in the future to optimize this so unchanged components don't
// need to be recanonicalized. This is especially true since the common case for
// ReplaceComponents is removing things we don't want, like reference fragments
// and usernames. These cases can become more efficient if we can assume the
// rest of the URL is OK with these removed (or only the modified parts
// recanonicalized). This would be much more complex to implement, however.
//
// You would also need to update DoReplaceComponents in URLUtilities.cc which
// relies on this re-checking everything (see the comment there for why).
bool ReplaceStandardURL(const char* base,
                        const URLSegments& baseParsed,
                        const Replacements<char>& replacements,
                        URLQueryCharsetConverter* queryConverter,
                        URLBuffer<char>& output,
                        URLSegments* outputParsed)
{
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupOverrideComponents(base, replacements, &source, &parsed);
    return doCanonicalizeStandardURL<char, unsigned char>(source, parsed, queryConverter, output, *outputParsed);
}

// For 16-bit replacements, we turn all the replacements into UTF-8 so the
// regular codepath can be used.
bool ReplaceStandardURL(const char* base,
                        const URLSegments& baseParsed,
                        const Replacements<UChar>& replacements,
                        URLQueryCharsetConverter* queryConverter,
                        URLBuffer<char>& output,
                        URLSegments* outputParsed)
{
    RawURLBuffer<char> utf8;
    URLComponentSource<char> source(base);
    URLSegments parsed(baseParsed);
    SetupUTF16OverrideComponents(base, replacements, utf8, &source, &parsed);
    return doCanonicalizeStandardURL<char, unsigned char>(source, parsed, queryConverter, output, *outputParsed);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
