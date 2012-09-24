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

// Canonicalizer functions for working with and resolving relative URLs.

#include "config.h"
#include "URLCanon.h"

#include "URLCanonInternal.h"
#include "URLFile.h"
#include "URLParseInternal.h"
#include "URLUtilInternal.h"

#if USE(WTFURL)

namespace WTF {

namespace URLCanonicalizer {

namespace {

// Firefox does a case-sensitive compare (which is probably wrong--Mozilla bug
// 379034), whereas IE is case-insensetive.
//
// We choose to be more permissive like IE. We don't need to worry about
// unescaping or anything here: neither IE or Firefox allow this. We also
// don't have to worry about invalid scheme characters since we are comparing
// against the canonical scheme of the base.
//
// The base URL should always be canonical, therefore is ASCII.
template<typename CHAR>
bool AreSchemesEqual(const char* base,
                     const URLComponent& baseScheme,
                     const CHAR* cmp,
                     const URLComponent& cmpScheme)
{
    if (baseScheme.length() != cmpScheme.length())
        return false;
    for (int i = 0; i < baseScheme.length(); i++) {
        // We assume the base is already canonical, so we don't have to
        // canonicalize it.
        if (canonicalSchemeChar(cmp[cmpScheme.begin() + i]) !=
            base[baseScheme.begin() + i])
            return false;
    }
    return true;
}

#if OS(WINDOWS)
// Here, we also allow Windows paths to be represented as "/C:/" so we can be
// consistent about URL paths beginning with slashes. This function is like
// DoesBeginWindowsDrivePath except that it also requires a slash at the
// beginning.
template<typename CHAR>
bool doesBeginSlashWindowsDriveSpec(const CHAR* spec, int startOffset, int specLength)
{
    if (startOffset >= specLength)
        return false;
    return URLParser::isURLSlash(spec[startOffset]) && URLParser::doesBeginWindowsDriveSpec(spec, startOffset + 1, specLength);
}

#endif // OS(WINDOWS)

// See isRelativeURL in the header file for usage.
template<typename CharacterType>
bool doIsRelativeURL(const char* base, const URLSegments& baseParsed,
                     const CharacterType* url, int urlLength,
                     bool isBaseHierarchical,
                     bool& isRelative, URLComponent& relativeComponent)
{
    isRelative = false; // So we can default later to not relative.

    // Trim whitespace and construct a new range for the substring.
    int begin = 0;
    URLParser::trimURL(url, begin, urlLength);
    if (begin >= urlLength) {
        // Empty URLs are relative, but do nothing.
        relativeComponent = URLComponent(begin, 0);
        isRelative = true;
        return true;
    }

#if OS(WINDOWS)
    // We special case paths like "C:\foo" so they can link directly to the
    // file on Windows (IE compatability). The security domain stuff should
    // prevent a link like this from actually being followed if its on a
    // web page.
    //
    // We treat "C:/foo" as an absolute URL. We can go ahead and treat "/c:/"
    // as relative, as this will just replace the path when the base scheme
    // is a file and the answer will still be correct.
    //
    // We require strict backslashes when detecting UNC since two forward
    // shashes should be treated a a relative URL with a hostname.
    if (URLParser::doesBeginWindowsDriveSpec(url, begin, urlLength) || URLParser::doesBeginUNCPath(url, begin, urlLength, true))
        return true;
#endif // OS(WINDOWS)

    // See if we've got a scheme, if not, we know this is a relative URL.
    // BUT: Just because we have a scheme, doesn't make it absolute.
    // "http:foo.html" is a relative URL with path "foo.html". If the scheme is
    // empty, we treat it as relative (":foo") like IE does.
    URLComponent scheme;
    if (!URLParser::ExtractScheme(url, urlLength, &scheme) || !scheme.length()) {
        // Don't allow relative URLs if the base scheme doesn't support it.
        if (!isBaseHierarchical)
            return false;

        relativeComponent = URLComponent::fromRange(begin, urlLength);
        isRelative = true;
        return true;
    }

    // If the scheme isn't valid, then it's relative.
    int schemeEnd = scheme.end();
    for (int i = scheme.begin(); i < schemeEnd; i++) {
        if (!canonicalSchemeChar(url[i])) {
            relativeComponent = URLComponent::fromRange(begin, urlLength);
            isRelative = true;
            return true;
        }
    }

    // If the scheme is not the same, then we can't count it as relative.
    if (!AreSchemesEqual(base, baseParsed.scheme, url, scheme))
        return true;

    // When the scheme that they both share is not hierarchical, treat the
    // incoming scheme as absolute (this way with the base of "data:foo",
    // "data:bar" will be reported as absolute.
    if (!isBaseHierarchical)
        return true;

    int colonOffset = scheme.end();

    // If it's a filesystem URL, the only valid way to make it relative is not to
    // supply a scheme. There's no equivalent to e.g. http:index.html.
    if (URLUtilities::CompareSchemeComponent(url, scheme, "filesystem"))
        return true;

    // ExtractScheme guarantees that the colon immediately follows what it
    // considers to be the scheme. countConsecutiveSlashes will handle the
    // case where the begin offset is the end of the input.
    int numSlashes = URLParser::countConsecutiveSlashes(url, colonOffset + 1, urlLength);

    if (!numSlashes || numSlashes == 1) {
        // No slashes means it's a relative path like "http:foo.html". One slash
        // is an absolute path. "http:/home/foo.html"
        isRelative = true;
        relativeComponent = URLComponent::fromRange(colonOffset + 1, urlLength);
        return true;
    }

    // Two or more slashes after the scheme we treat as absolute.
    return true;
}

// Copies all characters in the range [begin, end) of |spec| to the output,
// up until and including the last slash. There should be a slash in the
// range, if not, nothing will be copied.
//
// The input is assumed to be canonical, so we search only for exact slashes
// and not backslashes as well. We also know that it's ASCII.
void CopyToLastSlash(const char* spec, int begin, int end, URLBuffer<char>& output)
{
    // Find the last slash.
    int lastSlash = -1;
    for (int i = end - 1; i >= begin; --i) {
        if (spec[i] == '/') {
            lastSlash = i;
            break;
        }
    }
    if (lastSlash < 0)
        return; // No slash.

    // Copy.
    for (int i = begin; i <= lastSlash; ++i)
        output.append(spec[i]);
}

// Copies a single component from the source to the output. This is used
// when resolving relative URLs and a given component is unchanged. Since the
// source should already be canonical, we don't have to do anything special,
// and the input is ASCII.
void CopyOneComponent(const char* source,
                      const URLComponent& sourceComponent,
                      URLBuffer<char>& output,
                      URLComponent* outputComponent)
{
    if (sourceComponent.length() < 0) {
        // This component is not present.
        *outputComponent = URLComponent();
        return;
    }

    outputComponent->setBegin(output.length());
    int sourceEnd = sourceComponent.end();
    for (int i = sourceComponent.begin(); i < sourceEnd; i++)
        output.append(source[i]);
    outputComponent->setLength(output.length() - outputComponent->begin());
}

#if OS(WINDOWS)

// Called on Windows when the base URL is a file URL, this will copy the "C:"
// to the output, if there is a drive letter and if that drive letter is not
// being overridden by the relative URL. Otherwise, do nothing.
//
// It will return the index of the beginning of the next character in the
// base to be processed: if there is a "C:", the slash after it, or if
// there is no drive letter, the slash at the beginning of the path, or
// the end of the base. This can be used as the starting offset for further
// path processing.
template<typename CHAR>
int CopyBaseDriveSpecIfNecessary(const char* baseURL,
                                 int basePathBegin,
                                 int basePathEnd,
                                 const CHAR* relativeURL,
                                 int pathStart,
                                 int relativeUrlLength,
                                 URLBuffer<char>& output)
{
    if (basePathBegin >= basePathEnd)
        return basePathBegin; // No path.

    // If the relative begins with a drive spec, don't do anything. The existing
    // drive spec in the base will be replaced.
    if (URLParser::doesBeginWindowsDriveSpec(relativeURL,
                                             pathStart, relativeUrlLength)) {
        return basePathBegin; // Relative URL path is "C:/foo"
    }

    // The path should begin with a slash (as all canonical paths do). We check
    // if it is followed by a drive letter and copy it.
    if (doesBeginSlashWindowsDriveSpec(baseURL, basePathBegin, basePathEnd)) {
        // Copy the two-character drive spec to the output. It will now look like
        // "file:///C:" so the rest of it can be treated like a standard path.
        output.append('/');
        output.append(baseURL[basePathBegin + 1]);
        output.append(baseURL[basePathBegin + 2]);
        return basePathBegin + 3;
    }

    return basePathBegin;
}

#endif // OS(WINDOWS)

// A subroutine of doResolveRelativeURL, this resolves the URL knowning that
// the input is a relative path or less (qyuery or ref).
template<typename CHAR>
bool doResolveRelativePath(const char* baseURL,
                           const URLSegments& baseParsed,
                           bool /* baseIsFile */,
                           const CHAR* relativeURL,
                           const URLComponent& relativeComponent,
                           CharsetConverter* queryConverter,
                           URLBuffer<char>& output,
                           URLSegments* outputParsed)
{
    bool success = true;

    // We know the authority section didn't change, copy it to the output. We
    // also know we have a path so can copy up to there.
    URLComponent path, query, ref;
    URLParser::parsePathInternal(relativeURL,
                                 relativeComponent,
                                 &path,
                                 &query,
                                 &ref);
    // Canonical URLs always have a path, so we can use that offset.
    output.append(baseURL, baseParsed.path.begin());

    if (path.length() > 0) {
        // The path is replaced or modified.
        int truePathBegin = output.length();

        // For file: URLs on Windows, we don't want to treat the drive letter and
        // colon as part of the path for relative file resolution when the
        // incoming URL does not provide a drive spec. We save the true path
        // beginning so we can fix it up after we are done.
        int basePathBegin = baseParsed.path.begin();
#if OS(WINDOWS)
        if (baseIsFile) {
            basePathBegin = CopyBaseDriveSpecIfNecessary(baseURL, baseParsed.path.begin(), baseParsed.path.end(),
                                                         relativeURL, relativeComponent.begin(), relativeComponent.end(),
                                                         output);
            // Now the output looks like either "file://" or "file:///C:"
            // and we can start appending the rest of the path. |basePathBegin|
            // points to the character in the base that comes next.
        }
#endif // OS(WINDOWS)

        if (URLParser::isURLSlash(relativeURL[path.begin()])) {
            // Easy case: the path is an absolute path on the server, so we can
            // just replace everything from the path on with the new versions.
            // Since the input should be canonical hierarchical URL, we should
            // always have a path.
            success &= CanonicalizePath(relativeURL, path,
                                        output, &outputParsed->path);
        } else {
            // Relative path, replace the query, and reference. We take the
            // original path with the file part stripped, and append the new path.
            // The canonicalizer will take care of resolving ".." and "."
            int pathBegin = output.length();
            CopyToLastSlash(baseURL, basePathBegin, baseParsed.path.end(),
                            output);
            success &= CanonicalizePartialPath(relativeURL, path, pathBegin,
                                               output);
            outputParsed->path = URLComponent::fromRange(pathBegin, output.length());

            // Copy the rest of the stuff after the path from the relative path.
        }

        // Finish with the query and reference part (these can't fail).
        CanonicalizeQuery(relativeURL, query, queryConverter, output, &outputParsed->query);
        canonicalizeFragment(relativeURL, ref, output, outputParsed->fragment);

        // Fix the path beginning to add back the "C:" we may have written above.
        outputParsed->path = URLComponent::fromRange(truePathBegin,
                                                     outputParsed->path.end());
        return success;
    }

    // If we get here, the path is unchanged: copy to output.
    CopyOneComponent(baseURL, baseParsed.path, output, &outputParsed->path);

    if (query.isValid()) {
        // Just the query specified, replace the query and reference (ignore
        // failures for refs)
        CanonicalizeQuery(relativeURL, query, queryConverter,
                          output, &outputParsed->query);
        canonicalizeFragment(relativeURL, ref, output, outputParsed->fragment);
        return success;
    }

    // If we get here, the query is unchanged: copy to output. Note that the
    // range of the query parameter doesn't include the question mark, so we
    // have to add it manually if there is a component.
    if (baseParsed.query.isValid())
        output.append('?');
    CopyOneComponent(baseURL, baseParsed.query, output, &outputParsed->query);

    if (ref.isValid()) {
        // Just the reference specified: replace it (ignoring failures).
        canonicalizeFragment(relativeURL, ref, output, outputParsed->fragment);
        return success;
    }

    // We should always have something to do in this function, the caller checks
    // that some component is being replaced.
    ASSERT_NOT_REACHED();
    return success;
}

// Resolves a relative URL that contains a host. Typically, these will
// be of the form "//www.apple.com/foo/bar?baz#fragment" and the only thing which
// should be kept from the original URL is the scheme.
template<typename CHAR>
bool doResolveRelativeHost(const char* baseURL,
                           const URLSegments& baseParsed,
                           const CHAR* relativeURL,
                           const URLComponent& relativeComponent,
                           CharsetConverter* queryConverter,
                           URLBuffer<char>& output,
                           URLSegments* outputParsed)
{
    // Parse the relative URL, just like we would for anything following a
    // scheme.
    URLSegments relativeParsed; // Everything but the scheme is valid.
    URLParser::parseAfterScheme(&relativeURL[relativeComponent.begin()],
                                relativeComponent.length(), relativeComponent.begin(),
                                relativeParsed);

    // Now we can just use the replacement function to replace all the necessary
    // parts of the old URL with the new one.
    Replacements<CHAR> replacements;
    replacements.SetUsername(relativeURL, relativeParsed.username);
    replacements.SetPassword(relativeURL, relativeParsed.password);
    replacements.SetHost(relativeURL, relativeParsed.host);
    replacements.SetPort(relativeURL, relativeParsed.port);
    replacements.SetPath(relativeURL, relativeParsed.path);
    replacements.SetQuery(relativeURL, relativeParsed.query);
    replacements.SetRef(relativeURL, relativeParsed.fragment);

    return ReplaceStandardURL(baseURL, baseParsed, replacements,
                              queryConverter, output, outputParsed);
}

// Resolves a relative URL that happens to be an absolute file path. Examples
// include: "//hostname/path", "/c:/foo", and "//hostname/c:/foo".
template<typename CharacterType>
bool doResolveAbsoluteFile(const CharacterType* relativeURL,
                           const URLComponent& relativeComponent,
                           CharsetConverter* queryConverter,
                           URLBuffer<char>& output,
                           URLSegments& outputParsed)
{
    // Parse the file URL. The file URl parsing function uses the same logic
    // as we do for determining if the file is absolute, in which case it will
    // not bother to look for a scheme.
    URLSegments relativeParsed;
    URLParser::ParseFileURL(&relativeURL[relativeComponent.begin()],
                            relativeComponent.length(), &relativeParsed);

    return CanonicalizeFileURL(&relativeURL[relativeComponent.begin()],
                               relativeComponent.length(), relativeParsed,
                               queryConverter, output, &outputParsed);
}

// TODO(brettw) treat two slashes as root like Mozilla for FTP?
template<typename CHAR>
bool doResolveRelativeURL(const char* baseURL,
                          const URLSegments& baseParsed,
                          bool baseIsFile,
                          const CHAR* relativeURL,
                          const URLComponent& relativeComponent,
                          CharsetConverter* queryConverter,
                          URLBuffer<char>& output,
                          URLSegments* outputParsed)
{
    // Starting point for our output parsed. We'll fix what we change.
    *outputParsed = baseParsed;

    // Sanity check: the input should have a host or we'll break badly below.
    // We can only resolve relative URLs with base URLs that have hosts and
    // paths (even the default path of "/" is OK).
    //
    // We allow hosts with no length so we can handle file URLs, for example.
    if (baseParsed.path.length() <= 0) {
        // On error, return the input (resolving a relative URL on a non-relative
        // base = the base).
        int baseLength = baseParsed.length();
        for (int i = 0; i < baseLength; i++)
            output.append(baseURL[i]);
        return false;
    }

    if (relativeComponent.length() <= 0) {
        // Empty relative URL, leave unchanged, only removing the ref component.
        int baseLength = baseParsed.length();
        baseLength -= baseParsed.fragment.length() + 1;
        outputParsed->fragment.reset();
        output.append(baseURL, baseLength);
        return true;
    }

    int numSlashes = URLParser::countConsecutiveSlashes(relativeURL, relativeComponent.begin(), relativeComponent.end());

#if OS(WINDOWS)
    // On Windows, two slashes for a file path (regardless of which direction
    // they are) means that it's UNC. Two backslashes on any base scheme mean
    // that it's an absolute UNC path (we use the baseIsFile flag to control
    // how strict the UNC finder is).
    //
    // We also allow Windows absolute drive specs on any scheme (for example
    // "c:\foo") like IE does. There must be no preceeding slashes in this
    // case (we reject anything like "/c:/foo") because that should be treated
    // as a path. For file URLs, we allow any number of slashes since that would
    // be setting the path.
    //
    // This assumes the absolute path resolver handles absolute URLs like this
    // properly. URLUtilities::DoCanonicalize does this.
    int afterSlashes = relativeComponent.begin + numSlashes;
    if (URLParser::doesBeginUNCPath(relativeURL, relativeComponent.begin(), relativeComponent.end(), !baseIsFile)
        || ((!numSlashes || baseIsFile) && URLParser::doesBeginWindowsDriveSpec(relativeURL, afterSlashes, relativeComponent.end()))) {
             return doResolveAbsoluteFile(relativeURL, relativeComponent,
                                          queryConverter, output, *outputParsed);
         }
#else
    // Other platforms need explicit handling for file: URLs with multiple
    // slashes because the generic scheme parsing always extracts a host, but a
    // file: URL only has a host if it has exactly 2 slashes. This also
    // handles the special case where the URL is only slashes, since that
    // doesn't have a host part either.
    if (baseIsFile && (numSlashes > 2 || numSlashes == relativeComponent.length())) {
        return doResolveAbsoluteFile(relativeURL, relativeComponent,
                                     queryConverter, output, *outputParsed);
    }
#endif

    // Any other double-slashes mean that this is relative to the scheme.
    if (numSlashes >= 2) {
        return doResolveRelativeHost(baseURL, baseParsed,
                                     relativeURL, relativeComponent,
                                     queryConverter, output, outputParsed);
    }

    // When we get here, we know that the relative URL is on the same host.
    return doResolveRelativePath(baseURL, baseParsed, baseIsFile,
                                 relativeURL, relativeComponent,
                                 queryConverter, output, outputParsed);
}

} // namespace

bool isRelativeURL(const char* base, const URLSegments& baseParsed,
                   const char* fragment, int fragmentLength,
                   bool isBaseHierarchical,
                   bool& isRelative, URLComponent& relativeComponent)
{
    return doIsRelativeURL<char>(base, baseParsed, fragment, fragmentLength, isBaseHierarchical, isRelative, relativeComponent);
}

bool isRelativeURL(const char* base, const URLSegments& baseParsed,
                   const UChar* fragment, int fragmentLength,
                   bool isBaseHierarchical,
                   bool& isRelative, URLComponent& relativeComponent)
{
    return doIsRelativeURL<UChar>(base, baseParsed, fragment, fragmentLength, isBaseHierarchical, isRelative, relativeComponent);
}

bool resolveRelativeURL(const char* baseURL,
                        const URLSegments& baseParsed,
                        bool baseIsFile,
                        const char* relativeURL,
                        const URLComponent& relativeComponent,
                        CharsetConverter* queryConverter,
                        URLBuffer<char>& output,
                        URLSegments* outputParsed)
{
    return doResolveRelativeURL<char>(baseURL, baseParsed, baseIsFile, relativeURL,
                                      relativeComponent, queryConverter, output, outputParsed);
}

bool resolveRelativeURL(const char* baseURL,
                        const URLSegments& baseParsed,
                        bool baseIsFile,
                        const UChar* relativeURL,
                        const URLComponent& relativeComponent,
                        CharsetConverter* queryConverter,
                        URLBuffer<char>& output,
                        URLSegments* outputParsed)
{
    return doResolveRelativeURL<UChar>(baseURL, baseParsed, baseIsFile, relativeURL,
                                       relativeComponent, queryConverter, output, outputParsed);
}

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)
