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
#include "URLUtil.h"

#include "RawURLBuffer.h"
#include "URLCanonInternal.h"
#include "URLFile.h"
#include "URLUtilInternal.h"
#include <wtf/ASCIICType.h>

#if USE(WTFURL)

namespace WTF {

namespace URLUtilities {

const char kFileScheme[] = "file";
const char kFileSystemScheme[] = "filesystem";
const char kMailtoScheme[] = "mailto";

namespace {

template<typename Iter>
static bool lowerCaseEqualsASCII(Iter aBegin, Iter aEnd, const char* b)
{
    for (Iter it = aBegin; it != aEnd; ++it, ++b) {
        if (!*b || toASCIILower(*it) != *b)
            return false;
    }
    return !(*b);
}

const int kNumStandardURLSchemes = 8;
const char* kStandardURLSchemes[kNumStandardURLSchemes] = {
    "http",
    "https",
    kFileScheme, // Yes, file urls can have a hostname!
    "ftp",
    "gopher",
    "ws", // WebSocket.
    "wss", // WebSocket secure.
    kFileSystemScheme
};

// Given a string and a range inside the string, compares it to the given
// lower-case |compareTo| buffer.
template<typename CharacterType>
inline bool doCompareSchemeComponent(const CharacterType* spec, const URLComponent& component, const char* compareTo)
{
    if (!component.isNonEmpty())
        return !compareTo[0]; // When component is empty, match empty scheme.
    return lowerCaseEqualsASCII(&spec[component.begin()], &spec[component.end()], compareTo);
}

// Returns true if the given scheme identified by |scheme| within |spec| is one
// of the registered "standard" schemes.
template<typename CharacterType>
bool doIsStandard(const CharacterType* spec, const URLComponent& scheme)
{
    if (!scheme.isNonEmpty())
        return false; // Empty or invalid schemes are non-standard.

    for (int i = 0; i < kNumStandardURLSchemes; ++i) {
        if (lowerCaseEqualsASCII(&spec[scheme.begin()], &spec[scheme.end()], kStandardURLSchemes[i]))
            return true;
    }
    return false;
}

template<typename CharacterType>
bool doFindAndCompareScheme(const CharacterType* str, int strLength, const char* compare, URLComponent* foundScheme)
{
    // Before extracting scheme, canonicalize the URL to remove any whitespace.
    // This matches the canonicalization done in doCanonicalize function.
    RawURLBuffer<CharacterType> whitespaceBuffer;
    int specLength;
    const CharacterType* spec = URLCanonicalizer::removeURLWhitespace(str, strLength, whitespaceBuffer, specLength);

    URLComponent ourScheme;
    if (!URLParser::ExtractScheme(spec, specLength, &ourScheme)) {
        // No scheme.
        if (foundScheme)
            *foundScheme = URLComponent();
        return false;
    }
    if (foundScheme)
        *foundScheme = ourScheme;
    return doCompareSchemeComponent(spec, ourScheme, compare);
}

template<typename CharacterType>
bool doCanonicalize(const CharacterType* inSpec, int inSpecLength,
                    URLQueryCharsetConverter* charsetConverter,
                    URLBuffer<char>& output, URLSegments& ouputParsed)
{
    // Remove any whitespace from the middle of the relative URL, possibly
    // copying to the new buffer.
    RawURLBuffer<CharacterType> whitespaceBuffer;
    int specLength;
    const CharacterType* spec = URLCanonicalizer::removeURLWhitespace(inSpec, inSpecLength, whitespaceBuffer, specLength);

    URLSegments parsedInput;
#if OS(WINDOWS)
    // For Windows, we allow things that look like absolute Windows paths to be
    // fixed up magically to file URLs. This is done for IE compatability. For
    // example, this will change "c:/foo" into a file URL rather than treating
    // it as a URL with the protocol "c". It also works for UNC ("\\foo\bar.txt").
    // There is similar logic in URLCanonicalizer_relative.cc for
    //
    // For Max & Unix, we don't do this (the equivalent would be "/foo/bar" which
    // has no meaning as an absolute path name. This is because browsers on Mac
    // & Unix don't generally do this, so there is no compatibility reason for
    // doing so.
    if (URLParser::doesBeginUNCPath(spec, 0, specLength, false)
        || URLParser::doesBeginWindowsDriveSpec(spec, 0, specLength)) {
        URLParser::ParseFileURL(spec, specLength, &parsedInput);
        return URLCanonicalizer::CanonicalizeFileURL(spec, specLength, parsedInput,
                                                     charsetConverter,
                                                     output, *ouputParsed);
    }
#endif

    URLComponent scheme;
    if (!URLParser::ExtractScheme(spec, specLength, &scheme))
        return false;

    // This is the parsed version of the input URL, we have to canonicalize it
    // before storing it in our object.
    bool success;
    if (doCompareSchemeComponent(spec, scheme, kFileScheme)) {
        // File URLs are special.
        URLParser::ParseFileURL(spec, specLength, &parsedInput);
        success = URLCanonicalizer::CanonicalizeFileURL(spec, specLength, parsedInput,
                                                        charsetConverter, output,
                                                        &ouputParsed);
    } else if (doCompareSchemeComponent(spec, scheme, kFileSystemScheme)) {
        // Filesystem URLs are special.
        URLParser::ParseFileSystemURL(spec, specLength, &parsedInput);
        success = URLCanonicalizer::canonicalizeFileSystemURL(spec, parsedInput,
                                                              charsetConverter,
                                                              output, ouputParsed);

    } else if (doIsStandard(spec, scheme)) {
        // All "normal" URLs.
        URLParser::ParseStandardURL(spec, specLength, &parsedInput);
        success = URLCanonicalizer::CanonicalizeStandardURL(spec, specLength, parsedInput,
                                                            charsetConverter,
                                                            output, &ouputParsed);

    } else if (doCompareSchemeComponent(spec, scheme, kMailtoScheme)) {
        // Mailto are treated like a standard url with only a scheme, path, query
        URLParser::ParseMailtoURL(spec, specLength, &parsedInput);
        success = URLCanonicalizer::canonicalizeMailtoURL(spec, parsedInput, output, ouputParsed);

    } else {
        // "Weird" URLs like data: and javascript:
        URLParser::ParsePathURL(spec, specLength, &parsedInput);
        success = URLCanonicalizer::canonicalizePathURL(spec, parsedInput, output, ouputParsed);
    }
    return success;
}

template<typename CharacterType>
bool doResolveRelative(const char* baseSpec, const URLSegments& baseParsed,
                       const CharacterType* inRelative, int inRelativeLength,
                       URLQueryCharsetConverter* charsetConverter,
                       URLBuffer<char>& output, URLSegments* ouputParsed)
{
    // Remove any whitespace from the middle of the relative URL, possibly
    // copying to the new buffer.
    RawURLBuffer<CharacterType> whitespaceBuffer;
    int relativeLength;
    const CharacterType* relative = URLCanonicalizer::removeURLWhitespace(inRelative, inRelativeLength, whitespaceBuffer, relativeLength);

    // See if our base URL should be treated as "standard".
    bool standardBaseScheme = baseParsed.scheme.isNonEmpty() && doIsStandard(baseSpec, baseParsed.scheme);

    bool isRelative;
    URLComponent relativeComponent;
    if (!URLCanonicalizer::isRelativeURL(baseSpec, baseParsed,
                                         relative, relativeLength,
                                         standardBaseScheme,
                                         isRelative, relativeComponent))
        return false; // Error resolving.

    if (isRelative) {
        // Relative, resolve and canonicalize.
        bool fileBaseScheme = baseParsed.scheme.isNonEmpty() && doCompareSchemeComponent(baseSpec, baseParsed.scheme, kFileScheme);
        return URLCanonicalizer::resolveRelativeURL(baseSpec, baseParsed,
                                                    fileBaseScheme, relative,
                                                    relativeComponent, charsetConverter,
                                                    output, ouputParsed);
    }

    // Not relative, canonicalize the input.
    return doCanonicalize(relative, relativeLength, charsetConverter, output, *ouputParsed);
}

template<typename CharacterType>
bool doReplaceComponents(const char* spec,
                         int specLength,
                         const URLSegments& parsed,
                         const URLCanonicalizer::Replacements<CharacterType>& replacements,
                         URLQueryCharsetConverter* charsetConverter,
                         URLBuffer<char>& output,
                         URLSegments& outputParsed)
{
    // If the scheme is overridden, just do a simple string substitution and
    // reparse the whole thing. There are lots of edge cases that we really don't
    // want to deal with. Like what happens if I replace "http://e:8080/foo"
    // with a file. Does it become "file:///E:/8080/foo" where the port number
    // becomes part of the path? Parsing that string as a file URL says "yes"
    // but almost no sane rule for dealing with the components individually would
    // come up with that.
    //
    // Why allow these crazy cases at all? Programatically, there is almost no
    // case for replacing the scheme. The most common case for hitting this is
    // in JS when building up a URL using the location object. In this case, the
    // JS code expects the string substitution behavior:
    //   http://www.w3.org/TR/2008/WD-html5-20080610/structured.html#common3
    if (replacements.IsSchemeOverridden()) {
        // Canonicalize the new scheme so it is 8-bit and can be concatenated with
        // the existing spec.
        RawURLBuffer<char, 128> scheme_replaced;
        URLComponent schemeReplacedParsed;
        URLCanonicalizer::canonicalizeScheme(replacements.sources().scheme,
                                             replacements.components().scheme,
                                             scheme_replaced, schemeReplacedParsed);

        // We can assume that the input is canonicalized, which means it always has
        // a colon after the scheme (or where the scheme would be).
        int specAfterColon = parsed.scheme.isValid() ? parsed.scheme.end() + 1
        : 1;
        if (specLength - specAfterColon > 0) {
            scheme_replaced.append(&spec[specAfterColon],
                                   specLength - specAfterColon);
        }

        // We now need to completely re-parse the resulting string since its meaning
        // may have changed with the different scheme.
        RawURLBuffer<char, 128> recanonicalized;
        URLSegments recanonicalizedParsed;
        doCanonicalize(scheme_replaced.data(), scheme_replaced.length(),
                       charsetConverter,
                       recanonicalized, recanonicalizedParsed);

        // Recurse using the version with the scheme already replaced. This will now
        // use the replacement rules for the new scheme.
        //
        // Warning: this code assumes that ReplaceComponents will re-check all
        // components for validity. This is because we can't fail if DoCanonicalize
        // failed above since theoretically the thing making it fail could be
        // getting replaced here. If ReplaceComponents didn't re-check everything,
        // we wouldn't know if something *not* getting replaced is a problem.
        // If the scheme-specific replacers are made more intelligent so they don't
        // re-check everything, we should instead recanonicalize the whole thing
        // after this call to check validity (this assumes replacing the scheme is
        // much much less common than other types of replacements, like clearing the
        // ref).
        URLCanonicalizer::Replacements<CharacterType> replacementsNoScheme = replacements;
        replacementsNoScheme.SetScheme(0, URLComponent());
        return doReplaceComponents(recanonicalized.data(), recanonicalized.length(),
                                   recanonicalizedParsed, replacementsNoScheme,
                                   charsetConverter, output, outputParsed);
    }

    // If we get here, then we know the scheme doesn't need to be replaced, so can
    // just key off the scheme in the spec to know how to do the replacements.
    if (doCompareSchemeComponent(spec, parsed.scheme, kFileScheme)) {
        return URLCanonicalizer::ReplaceFileURL(spec, parsed, replacements,
                                                charsetConverter, output, &outputParsed);
    }
    if (doCompareSchemeComponent(spec, parsed.scheme, kFileSystemScheme)) {
        return URLCanonicalizer::ReplaceFileSystemURL(spec, parsed, replacements,
                                                      charsetConverter, output,
                                                      &outputParsed);
    }
    if (doIsStandard(spec, parsed.scheme)) {
        return URLCanonicalizer::ReplaceStandardURL(spec, parsed, replacements,
                                                    charsetConverter, output, &outputParsed);
    }
    if (doCompareSchemeComponent(spec, parsed.scheme, kMailtoScheme))
        return URLCanonicalizer::replaceMailtoURL(spec, parsed, replacements, output, outputParsed);

    // Default is a path URL.
    return URLCanonicalizer::ReplacePathURL(spec, parsed, replacements, output, &outputParsed);
}

} // namespace

bool isStandard(const LChar* spec, const URLComponent& scheme)
{
    return doIsStandard(spec, scheme);
}

bool isStandard(const UChar* spec, const URLComponent& scheme)
{
    return doIsStandard(spec, scheme);
}

bool FindAndCompareScheme(const char* str, int strLength, const char* compare, URLComponent* foundScheme)
{
    return doFindAndCompareScheme(str, strLength, compare, foundScheme);
}

bool FindAndCompareScheme(const UChar* str, int strLength, const char* compare, URLComponent* foundScheme)
{
    return doFindAndCompareScheme(str, strLength, compare, foundScheme);
}

bool canonicalize(const char* spec, int specLength,
                  URLQueryCharsetConverter* charsetConverter,
                  URLBuffer<char>& output, URLSegments& ouputParsed)
{
    return doCanonicalize(spec, specLength, charsetConverter, output, ouputParsed);
}

bool canonicalize(const UChar* spec, int specLength,
                  URLQueryCharsetConverter* charsetConverter,
                  URLBuffer<char>& output, URLSegments& ouputParsed)
{
    return doCanonicalize(spec, specLength, charsetConverter, output, ouputParsed);
}

bool resolveRelative(const char* baseSpec, const URLSegments& baseParsed,
                     const char* relative, int relativeLength,
                     URLQueryCharsetConverter* charsetConverter,
                     URLBuffer<char>& output, URLSegments* ouputParsed)
{
    return doResolveRelative(baseSpec, baseParsed,
                             relative, relativeLength,
                             charsetConverter, output, ouputParsed);
}

bool resolveRelative(const char* baseSpec, const URLSegments& baseParsed,
                     const UChar* relative, int relativeLength,
                     URLQueryCharsetConverter* charsetConverter,
                     URLBuffer<char>& output, URLSegments* ouputParsed)
{
    return doResolveRelative(baseSpec, baseParsed,
                             relative, relativeLength,
                             charsetConverter, output, ouputParsed);
}

bool ReplaceComponents(const char* spec,
                       int specLength,
                       const URLSegments& parsed,
                       const URLCanonicalizer::Replacements<char>& replacements,
                       URLQueryCharsetConverter* charsetConverter,
                       URLBuffer<char>& output,
                       URLSegments* outputParsed)
{
    return doReplaceComponents(spec, specLength, parsed, replacements,
                               charsetConverter, output, *outputParsed);
}

bool ReplaceComponents(const char* spec,
                       int specLength,
                       const URLSegments& parsed,
                       const URLCanonicalizer::Replacements<UChar>& replacements,
                       URLQueryCharsetConverter* charsetConverter,
                       URLBuffer<char>& output,
                       URLSegments* outputParsed)
{
    return doReplaceComponents(spec, specLength, parsed, replacements,
                               charsetConverter, output, *outputParsed);
}

void DecodeURLEscapeSequences(const char* input, int length, URLBuffer<UChar>& output)
{
    RawURLBuffer<char> unescapedChars;
    for (int i = 0; i < length; ++i) {
        if (input[i] == '%') {
            unsigned char ch;
            if (URLCanonicalizer::DecodeEscaped(input, &i, length, &ch))
                unescapedChars.append(ch);
            else {
                // Invalid escape sequence, copy the percent literal.
                unescapedChars.append('%');
            }
        } else {
            // Regular non-escaped 8-bit character.
            unescapedChars.append(input[i]);
        }
    }

    // Convert that 8-bit to UTF-16. It's not clear IE does this at all to
    // JavaScript URLs, but Firefox and Safari do.
    for (int i = 0; i < unescapedChars.length(); i++) {
        unsigned char uch = static_cast<unsigned char>(unescapedChars.at(i));
        if (uch < 0x80) {
            // Non-UTF-8, just append directly
            output.append(uch);
        } else {
            // next_ch will point to the last character of the decoded
            // character.
            int nextCharacter = i;
            unsigned codePoint;
            if (URLCanonicalizer::readUTFChar(unescapedChars.data(), &nextCharacter,
                                              unescapedChars.length(), &codePoint)) {
                // Valid UTF-8 character, convert to UTF-16.
                URLCanonicalizer::AppendUTF16Value(codePoint, output);
                i = nextCharacter;
            } else {
                // If there are any sequences that are not valid UTF-8, we keep
                // invalid code points and promote to UTF-16. We copy all characters
                // from the current position to the end of the identified sequence.
                while (i < nextCharacter) {
                    output.append(static_cast<unsigned char>(unescapedChars.at(i)));
                    i++;
                }
                output.append(static_cast<unsigned char>(unescapedChars.at(i)));
            }
        }
    }
}

void EncodeURIComponent(const char* input, int length, URLBuffer<char>& output)
{
    for (int i = 0; i < length; ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (URLCharacterTypes::isComponentChar(c))
            output.append(c);
        else
            URLCanonicalizer::appendURLEscapedCharacter(c, output);
    }
}

bool CompareSchemeComponent(const char* spec, const URLComponent& component, const char* compareTo)
{
    return doCompareSchemeComponent(spec, component, compareTo);
}

bool CompareSchemeComponent(const UChar* spec, const URLComponent& component, const char* compareTo)
{
    return doCompareSchemeComponent(spec, component, compareTo);
}

} // namespace URLUtilities

} // namespace WTF

#endif // USE(WTFURL)
