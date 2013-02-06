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

#ifndef URLUtil_h
#define URLUtil_h

#include "URLCanon.h"
#include "URLParse.h"
#include <wtf/unicode/Unicode.h>
#include <wtf/url/api/URLBuffer.h>

#if USE(WTFURL)

namespace WTF {

class URLQueryCharsetConverter;

namespace URLUtilities {

// Locates the scheme in the given string and places it into |foundScheme|,
// which may be 0 to indicate the caller does not care about the range.
//
// Returns whether the given |compare| scheme matches the scheme found in the
// input (if any). The |compare| scheme must be a valid canonical scheme or
// the result of the comparison is undefined.
bool FindAndCompareScheme(const char* str, int strLength, const char* compare, URLComponent* foundScheme);
bool FindAndCompareScheme(const UChar* str, int strLength, const char* compare, URLComponent* foundScheme);

// Returns true if the given string represents a standard URL. This means that
// either the scheme is in the list of known standard schemes.
bool isStandard(const LChar* spec, const URLComponent& scheme);
bool isStandard(const UChar* spec, const URLComponent& scheme);
inline bool isStandard(const char* spec, const URLComponent& scheme) { return isStandard(reinterpret_cast<const LChar*>(spec), scheme); }

// URL library wrappers -------------------------------------------------------

// Parses the given spec according to the extracted scheme type. Normal users
// should use the URL object, although this may be useful if performance is
// critical and you don't want to do the heap allocation for the std::string.
//
// As with the URLCanonicalizer::Canonicalize* functions, the charset converter can
// be 0 to use UTF-8 (it will be faster in this case).
//
// Returns true if a valid URL was produced, false if not. On failure, the
// output and parsed structures will still be filled and will be consistent,
// but they will not represent a loadable URL.
bool canonicalize(const char* spec, int specLength, URLQueryCharsetConverter*,
                  URLBuffer<char>&, URLSegments& ouputParsed);
bool canonicalize(const UChar* spec, int specLength, URLQueryCharsetConverter*,
                  URLBuffer<char>&, URLSegments& ouputParsed);

// Resolves a potentially relative URL relative to the given parsed base URL.
// The base MUST be valid. The resulting canonical URL and parsed information
// will be placed in to the given out variables.
//
// The relative need not be relative. If we discover that it's absolute, this
// will produce a canonical version of that URL. See Canonicalize() for more
// about the charsetConverter.
//
// Returns true if the output is valid, false if the input could not produce
// a valid URL.
bool resolveRelative(const char* baseSpec, const URLSegments& baseParsed,
                     const char* relative, int relativeLength,
                     URLQueryCharsetConverter*,
                     URLBuffer<char>&, URLSegments* ouputParsed);
bool resolveRelative(const char* baseSpec, const URLSegments& baseParsed,
                     const UChar* relative, int relativeLength,
                     URLQueryCharsetConverter*,
                     URLBuffer<char>&, URLSegments* ouputParsed);

// Replaces components in the given VALID input url. The new canonical URL info
// is written to output and outputParsed.
//
// Returns true if the resulting URL is valid.
bool ReplaceComponents(const char* spec, int specLength, const URLSegments& parsed,
                       const URLCanonicalizer::Replacements<char>&,
                       URLQueryCharsetConverter*,
                       URLBuffer<char>&, URLSegments* outputParsed);
bool ReplaceComponents(const char* spec, int specLength, const URLSegments& parsed,
                       const URLCanonicalizer::Replacements<UChar>&,
                       URLQueryCharsetConverter*,
                       URLBuffer<char>&, URLSegments* outputParsed);

// Unescapes the given string using URL escaping rules.
void DecodeURLEscapeSequences(const char* input, int length, URLBuffer<UChar>&);

// Escapes the given string as defined by the JS method encodeURIComponent.
// See https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/encodeURIComponent
void EncodeURIComponent(const char* input, int length, URLBuffer<char>&);


} // namespace URLUtilities

} // namespace WTF

#endif // USE(WTFURL)

#endif // URLUtil_h
