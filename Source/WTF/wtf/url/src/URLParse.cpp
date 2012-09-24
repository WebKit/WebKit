/* Based on nsURLParsers.cc from Mozilla
 * -------------------------------------
 * Copyright (C) 1998 Netscape Communications Corporation.
 * Copyright 2012, Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 */

#include "config.h"
#include "URLParse.h"

#include "URLParseInternal.h"
#include "URLUtil.h"
#include "URLUtilInternal.h"
#include <stdlib.h>
#include <wtf/ASCIICType.h>

#if USE(WTFURL)

namespace WTF {

namespace URLParser {

namespace {

// Returns the offset of the next authority terminator in the input starting
// from startOffset. If no terminator is found, the return value will be equal
// to specLength.
template<typename CharacterType>
int findNextAuthorityTerminator(const CharacterType* spec, int startOffset, int specLength)
{
    for (int i = startOffset; i < specLength; i++) {
        if (IsAuthorityTerminator(spec[i]))
            return i;
    }
    return specLength; // Not found.
}

template<typename CharacterType>
void parseUserInfo(const CharacterType* spec, const URLComponent& user, URLComponent& username, URLComponent& password)
{
    // Find the first colon in the user section, which separates the username and
    // password.
    int colonOffset = 0;
    while (colonOffset < user.length() && spec[user.begin() + colonOffset] != ':')
        ++colonOffset;

    if (colonOffset < user.length()) {
        // Found separator: <username>:<password>
        username = URLComponent(user.begin(), colonOffset);
        password = URLComponent::fromRange(user.begin() + colonOffset + 1, user.end());
    } else {
        // No separator, treat everything as the username
        username = user;
        password = URLComponent();
    }
}

template<typename CharacterType>
void parseServerInfo(const CharacterType* spec, const URLComponent& serverInfo, URLComponent& hostname, URLComponent& port)
{
    if (!serverInfo.length()) {
        // No server info, host name is empty.
        hostname = URLComponent();
        port = URLComponent();
        return;
    }

    // If the host starts with a left-bracket, assume the entire host is an
    // IPv6 literal. Otherwise, assume none of the host is an IPv6 literal.
    // This assumption will be overridden if we find a right-bracket.
    //
    // Our IPv6 address canonicalization code requires both brackets to exist,
    // but the ability to locate an incomplete address can still be useful.
    int ipv6Terminator = spec[serverInfo.begin()] == '[' ? serverInfo.end() : -1;
    int colon = -1;

    // Find the last right-bracket, and the last colon.
    for (int i = serverInfo.begin(); i < serverInfo.end(); ++i) {
        switch (spec[i]) {
        case ']':
            ipv6Terminator = i;
            break;
        case ':':
            colon = i;
            break;
        }
    }

    if (colon > ipv6Terminator) {
        // Found a port number: <hostname>:<port>
        hostname = URLComponent::fromRange(serverInfo.begin(), colon);
        if (!hostname.length())
            hostname.reset();
        port = URLComponent::fromRange(colon + 1, serverInfo.end());
    } else {
        // No port: <hostname>
        hostname = serverInfo;
        port = URLComponent();
    }
}

// Given an already-identified auth section, breaks it into its consituent
// parts. The port number will be parsed and the resulting integer will be
// filled into the given *port variable, or -1 if there is no port number or it
// is invalid.
template<typename CharacterType>
void doParseAuthority(const CharacterType* spec, const URLComponent& authority, URLComponent& username, URLComponent& password, URLComponent& hostname, URLComponent& port)
{
    ASSERT_WITH_MESSAGE(authority.isValid(), "We should always get an authority");
    if (!authority.length()) {
        username = URLComponent();
        password = URLComponent();
        hostname = URLComponent();
        port = URLComponent();
        return;
    }

    // Search backwards for @, which is the separator between the user info and
    // the server info.
    int i = authority.end() - 1;
    while (i > authority.begin() && spec[i] != '@')
        --i;

    if (spec[i] == '@') {
        // Found user info: <user-info>@<server-info>
        parseUserInfo(spec, URLComponent(authority.begin(), i - authority.begin()), username, password);
        parseServerInfo(spec, URLComponent::fromRange(i + 1, authority.end()), hostname, port);
    } else {
        // No user info, everything is server info.
        username = URLComponent();
        password = URLComponent();
        parseServerInfo(spec, authority, hostname, port);
    }
}

template<typename CharacterType>
void parsePath(const CharacterType* spec, const URLComponent& hierarchicalidentifiers, URLComponent& resourcePath, URLComponent& query, URLComponent& fragment)
{
    // path = [/]<segment1>/<segment2>/<...>/<segmentN>;<param>?<query>#<ref>

    // Special case when there is no path.
    if (hierarchicalidentifiers.length() == -1) {
        resourcePath = URLComponent();
        query = URLComponent();
        fragment = URLComponent();
        return;
    }
    ASSERT_WITH_MESSAGE(hierarchicalidentifiers.length() > 0, "We should never have 0 length paths");

    // Search for first occurrence of either ? or #.
    int pathEnd = hierarchicalidentifiers.end();

    int querySeparator = -1; // Index of the '?'
    int fragmentSeparator = -1; // Index of the '#'
    for (int i = hierarchicalidentifiers.begin(); i < pathEnd; ++i) {
        switch (spec[i]) {
        case '?':
            // Only match the query string if it precedes the reference fragment
            // and when we haven't found one already.
            if (fragmentSeparator < 0 && querySeparator < 0)
                querySeparator = i;
            break;
        case '#':
            // Record the first # sign only.
            if (fragmentSeparator < 0)
                fragmentSeparator = i;
            break;
        }
    }

    // Markers pointing to the character after each of these corresponding
    // components. The code below words from the end back to the beginning,
    // and will update these indices as it finds components that exist.
    int resourcePathEnd = -1;
    int queryEnd = -1;

    // Ref fragment: from the # to the end of the path.
    if (fragmentSeparator >= 0) {
        resourcePathEnd = queryEnd = fragmentSeparator;
        fragment = URLComponent::fromRange(fragmentSeparator + 1, pathEnd);
    } else {
        resourcePathEnd = queryEnd = pathEnd;
        fragment = URLComponent();
    }

    // Query fragment: everything from the ? to the next boundary (either the end
    // of the path or the ref fragment).
    if (querySeparator >= 0) {
        resourcePathEnd = querySeparator;
        query = URLComponent::fromRange(querySeparator + 1, queryEnd);
    } else
        query = URLComponent();

    // File path: treat an empty file path as no file path.
    if (resourcePathEnd != hierarchicalidentifiers.begin())
        resourcePath = URLComponent::fromRange(hierarchicalidentifiers.begin(), resourcePathEnd);
    else
        resourcePath = URLComponent();
}

template<typename CharacterType>
bool doExtractScheme(const CharacterType* url, int urlLength, URLComponent& scheme)
{
    // Skip leading whitespace and control characters.
    int begin = 0;
    while (begin < urlLength && shouldTrimFromURL(url[begin]))
        ++begin;

    if (begin == urlLength)
        return false; // Input is empty or all whitespace.

    // Find the first colon character.
    for (int i = begin; i < urlLength; ++i) {
        if (url[i] == ':') {
            scheme = URLComponent::fromRange(begin, i);
            return true;
        }
    }
    return false; // No colon found: no scheme
}

// Fills in all members of the Parsed structure except for the scheme.
//
// |spec| is the full spec being parsed, of length |specLength|.
// |afterScheme| is the character immediately following the scheme (after the
//   colon) where we'll begin parsing.
//
// Compatability data points. I list "host", "path" extracted:
// Input                IE6             Firefox                Us
// -----                --------------  --------------         --------------
// http://foo.com/      "foo.com", "/"  "foo.com", "/"         "foo.com", "/"
// http:foo.com/        "foo.com", "/"  "foo.com", "/"         "foo.com", "/"
// http:/foo.com/       fail(*)         "foo.com", "/"         "foo.com", "/"
// http:\foo.com/       fail(*)         "\foo.com", "/"(fail)  "foo.com", "/"
// http:////foo.com/    "foo.com", "/"  "foo.com", "/"         "foo.com", "/"
//
// (*) Interestingly, although IE fails to load these URLs, its history
// canonicalizer handles them, meaning if you've been to the corresponding
// "http://foo.com/" link, it will be colored.
template <typename CharacterType>
void doParseAfterScheme(const CharacterType* spec, int specLength, int afterScheme, URLSegments& parsed)
{
    int slashesCount = countConsecutiveSlashes(spec, afterScheme, specLength);
    int afterSlashes = afterScheme + slashesCount;

    // First split into two main parts, the authority (username, password, host,
    // and port) and the full path (path, query, and reference).
    URLComponent authority;
    URLComponent fullPath;

    // Found "//<some data>", looks like an authority section. Treat everything
    // from there to the next slash (or end of spec) to be the authority. Note
    // that we ignore the number of slashes and treat it as the authority.
    int endAuth = findNextAuthorityTerminator(spec, afterSlashes, specLength);
    authority = URLComponent::fromRange(afterSlashes, endAuth);

    if (endAuth == specLength) // No beginning of path found.
        fullPath = URLComponent();
    else // Everything starting from the slash to the end is the path.
        fullPath = URLComponent::fromRange(endAuth, specLength);

    // Now parse those two sub-parts.
    doParseAuthority(spec, authority, parsed.username, parsed.password, parsed.host, parsed.port);
    parsePath(spec, fullPath, parsed.path, parsed.query, parsed.fragment);
}

// The main parsing function for standard URLs. Standard URLs have a scheme,
// host, path, etc.
template<typename CharacterType>
void doParseStandardURL(const CharacterType* spec, int specLength, URLSegments& parsed)
{
    ASSERT(specLength >= 0);

    // Strip leading & trailing spaces and control characters.
    int begin = 0;
    trimURL(spec, begin, specLength);

    int afterScheme = -1;
    if (doExtractScheme(spec, specLength, parsed.scheme))
        afterScheme = parsed.scheme.end() + 1; // Skip past the colon.
    else {
        // Say there's no scheme when there is no colon. We could also say that
        // everything is the scheme. Both would produce an invalid URL, but this way
        // seems less wrong in more cases.
        parsed.scheme.reset();
        afterScheme = begin;
    }
    doParseAfterScheme(spec, specLength, afterScheme, parsed);
}

template<typename CharacterType>
void doParseFileSystemURL(const CharacterType* spec, int specLength, URLSegments& parsed)
{
    ASSERT(specLength >= 0);

    // Get the unused parts of the URL out of the way.
    parsed.username.reset();
    parsed.password.reset();
    parsed.host.reset();
    parsed.port.reset();
    parsed.path.reset(); // May use this; reset for convenience.
    parsed.fragment.reset(); // May use this; reset for convenience.
    parsed.query.reset(); // May use this; reset for convenience.
    parsed.clearInnerURLSegments(); // May use this; reset for convenience.

    // Strip leading & trailing spaces and control characters.
    int begin = 0;
    trimURL(spec, begin, specLength);

    // Handle empty specs or ones that contain only whitespace or control chars.
    if (begin == specLength) {
        parsed.scheme.reset();
        return;
    }

    int innerStart = -1;

    // Extract the scheme. We also handle the case where there is no scheme.
    if (doExtractScheme(&spec[begin], specLength - begin, parsed.scheme)) {
        // Offset the results since we gave ExtractScheme a substring.
        parsed.scheme.setBegin(parsed.scheme.begin() + begin);

        if (parsed.scheme.end() == specLength - 1)
            return;

        innerStart = parsed.scheme.end() + 1;
    } else {
        // No scheme found; that's not valid for filesystem URLs.
        parsed.scheme.reset();
        return;
    }

    URLComponent innerScheme;
    const CharacterType* innerSpec = &spec[innerStart];
    int innerSpecLength = specLength - innerStart;

    if (doExtractScheme(innerSpec, innerSpecLength, innerScheme)) {
        // Offset the results since we gave ExtractScheme a substring.
        innerScheme.setBegin(innerScheme.begin() + innerStart);

        if (innerScheme.end() == specLength - 1)
            return;
    } else {
        // No scheme found; that's not valid for filesystem URLs.
        // The best we can do is return "filesystem://".
        return;
    }

    URLSegments innerParsed;
    if (URLUtilities::CompareSchemeComponent(spec, innerScheme, URLUtilities::kFileScheme)) {
        // File URLs are special.
        ParseFileURL(innerSpec, innerSpecLength, &innerParsed);
    } else if (URLUtilities::CompareSchemeComponent(spec, innerScheme, URLUtilities::kFileSystemScheme)) {
        // Filesystem URLs don't nest.
        return;
    } else if (URLUtilities::isStandard(spec, innerScheme)) {
        // All "normal" URLs.
        doParseStandardURL(innerSpec, innerSpecLength, innerParsed);
    } else
        return;

    // All members of innerParsed need to be offset by innerStart.
    // If we had any scheme that supported nesting more than one level deep,
    // we'd have to recurse into the innerParsed's innerParsed when
    // adjusting by innerStart.
    innerParsed.scheme.setBegin(innerParsed.scheme.begin() + innerStart);
    innerParsed.username.setBegin(innerParsed.username.begin() + innerStart);
    innerParsed.password.setBegin(innerParsed.password.begin() + innerStart);
    innerParsed.host.setBegin(innerParsed.host.begin() + innerStart);
    innerParsed.port.setBegin(innerParsed.port.begin() + innerStart);
    innerParsed.query.setBegin(innerParsed.query.begin() + innerStart);
    innerParsed.fragment.setBegin(innerParsed.fragment.begin() + innerStart);
    innerParsed.path.setBegin(innerParsed.path.begin() + innerStart);

    // Query and ref move from innerParsed to parsed.
    parsed.query = innerParsed.query;
    innerParsed.query.reset();
    parsed.fragment = innerParsed.fragment;
    innerParsed.fragment.reset();

    parsed.setInnerURLSegments(innerParsed);
    if (!innerParsed.scheme.isValid() || !innerParsed.path.isValid() || innerParsed.innerURLSegments())
        return;

    // The path in innerParsed should start with a slash, then have a filesystem
    // type followed by a slash. From the first slash up to but excluding the
    // second should be what it keeps; the rest goes to parsed. If the path ends
    // before the second slash, it's still pretty clear what the user meant, so
    // we'll let that through.
    if (!isURLSlash(spec[innerParsed.path.begin()]))
        return;

    int innerPathEnd = innerParsed.path.begin() + 1; // skip the leading slash
    while (innerPathEnd < specLength && !isURLSlash(spec[innerPathEnd]))
        ++innerPathEnd;
    parsed.path.setBegin(innerPathEnd);
    int newInnerPathLength = innerPathEnd - innerParsed.path.begin();
    parsed.path.setLength(innerParsed.path.length() - newInnerPathLength);
    innerParsed.path.setLength(newInnerPathLength);
    parsed.setInnerURLSegments(innerParsed);
}

// Initializes a path URL which is merely a scheme followed by a path. Examples
// include "about:foo" and "javascript:alert('bar');"
template<typename CharacterType>
void doParsePathURL(const CharacterType* spec, int specLength, URLSegments& parsed)
{
    // Get the non-path and non-scheme parts of the URL out of the way, we never
    // use them.
    parsed.username.reset();
    parsed.password.reset();
    parsed.host.reset();
    parsed.port.reset();
    parsed.query.reset();
    parsed.fragment.reset();

    // Strip leading & trailing spaces and control characters.
    int begin = 0;
    trimURL(spec, begin, specLength);

    // Handle empty specs or ones that contain only whitespace or control chars.
    if (begin == specLength) {
        parsed.scheme.reset();
        parsed.path.reset();
        return;
    }

    // Extract the scheme, with the path being everything following. We also
    // handle the case where there is no scheme.
    if (ExtractScheme(&spec[begin], specLength - begin, &parsed.scheme)) {
        // Offset the results since we gave ExtractScheme a substring.
        parsed.scheme.setBegin(parsed.scheme.begin() + begin);

        // For compatability with the standard URL parser, we treat no path as
        // -1, rather than having a length of 0 (we normally wouldn't care so
        // much for these non-standard URLs).
        if (parsed.scheme.end() == specLength - 1)
            parsed.path.reset();
        else
            parsed.path = URLComponent::fromRange(parsed.scheme.end() + 1, specLength);
    } else {
        // No scheme found, just path.
        parsed.scheme.reset();
        parsed.path = URLComponent::fromRange(begin, specLength);
    }
}

template<typename CharacterType>
void doParseMailtoURL(const CharacterType* spec, int specLength, URLSegments& parsed)
{
    ASSERT(specLength >= 0);

    // Get the non-path and non-scheme parts of the URL out of the way, we never
    // use them.
    parsed.username.reset();
    parsed.password.reset();
    parsed.host.reset();
    parsed.port.reset();
    parsed.fragment.reset();
    parsed.query.reset(); // May use this; reset for convenience.

    // Strip leading & trailing spaces and control characters.
    int begin = 0;
    trimURL(spec, begin, specLength);

    // Handle empty specs or ones that contain only whitespace or control chars.
    if (begin == specLength) {
        parsed.scheme.reset();
        parsed.path.reset();
        return;
    }

    int pathBegin = -1;
    int pathEnd = -1;

    // Extract the scheme, with the path being everything following. We also
    // handle the case where there is no scheme.
    if (ExtractScheme(&spec[begin], specLength - begin, &parsed.scheme)) {
        // Offset the results since we gave ExtractScheme a substring.
        parsed.scheme.setBegin(parsed.scheme.begin() + begin);

        if (parsed.scheme.end() != specLength - 1) {
            pathBegin = parsed.scheme.end() + 1;
            pathEnd = specLength;
        }
    } else {
        // No scheme found, just path.
        parsed.scheme.reset();
        pathBegin = begin;
        pathEnd = specLength;
    }

    // Split [pathBegin, pathEnd) into a path + query.
    for (int i = pathBegin; i < pathEnd; ++i) {
        if (spec[i] == '?') {
            parsed.query = URLComponent::fromRange(i + 1, pathEnd);
            pathEnd = i;
            break;
        }
    }

    // For compatability with the standard URL parser, treat no path as
    // -1, rather than having a length of 0
    if (pathBegin == pathEnd)
        parsed.path.reset();
    else
        parsed.path = URLComponent::fromRange(pathBegin, pathEnd);
}

// Converts a port number in a string to an integer. We'd like to just call
// sscanf but our input is not null-terminated, which sscanf requires. Instead,
// we copy the digits to a small stack buffer (since we know the maximum number
// of digits in a valid port number) that we can null terminate.
template<typename CharacterType>
int doParsePort(const CharacterType* spec, const URLComponent& component)
{
    // Easy success case when there is no port.
    const int kMaxDigits = 5;
    if (!component.isNonEmpty())
        return PORT_UNSPECIFIED;

    // Skip over any leading 0s.
    URLComponent digitComponent(component.end(), 0);
    for (int i = 0; i < component.length(); i++) {
        if (spec[component.begin() + i] != '0') {
            digitComponent = URLComponent::fromRange(component.begin() + i, component.end());
            break;
        }
    }
    if (!digitComponent.length())
        return 0; // All digits were 0.

    // Verify we don't have too many digits (we'll be copying to our buffer so
    // we need to double-check).
    if (digitComponent.length() > kMaxDigits)
        return PORT_INVALID;

    // Copy valid digits to the buffer.
    char digits[kMaxDigits + 1]; // +1 for null terminator
    for (int i = 0; i < digitComponent.length(); i++) {
        CharacterType ch = spec[digitComponent.begin() + i];
        if (!isASCIIDigit(ch)) {
            // Invalid port digit, fail.
            return PORT_INVALID;
        }
        digits[i] = static_cast<char>(ch);
    }

    // Null-terminate the string and convert to integer. Since we guarantee
    // only digits, atoi's lack of error handling is OK.
    digits[digitComponent.length()] = 0;
    int port = atoi(digits);
    if (port > 65535)
        return PORT_INVALID; // Out of range.
    return port;
}

template<typename CharacterType>
void doExtractFileName(const CharacterType* spec, const URLComponent& path, URLComponent& fileName)
{
    // Handle empty paths: they have no file names.
    if (!path.isNonEmpty()) {
        fileName = URLComponent();
        return;
    }

    // Search backwards for a parameter, which is a normally unused field in a
    // URL delimited by a semicolon. We parse the parameter as part of the
    // path, but here, we don't want to count it. The last semicolon is the
    // parameter. The path should start with a slash, so we don't need to check
    // the first one.
    int fileEnd = path.end();
    for (int i = path.end() - 1; i > path.begin(); --i) {
        if (spec[i] == ';') {
            fileEnd = i;
            break;
        }
    }

    // Now search backwards from the filename end to the previous slash
    // to find the beginning of the filename.
    for (int i = fileEnd - 1; i >= path.begin(); --i) {
        if (isURLSlash(spec[i])) {
            // File name is everything following this character to the end
            fileName = URLComponent::fromRange(i + 1, fileEnd);
            return;
        }
    }

    // No slash found, this means the input was degenerate (generally paths
    // will start with a slash). Let's call everything the file name.
    fileName = URLComponent::fromRange(path.begin(), fileEnd);
    return;
}

template<typename CharacterType>
bool doExtractQueryKeyValue(const CharacterType* spec, URLComponent& query, URLComponent& key, URLComponent& value)
{
    if (!query.isNonEmpty())
        return false;

    int start = query.begin();
    int current = start;
    int end = query.end();

    // We assume the beginning of the input is the beginning of the "key" and we
    // skip to the end of it.
    key.setBegin(current);
    while (current < end && spec[current] != '&' && spec[current] != '=')
        ++current;
    key.setLength(current - key.begin());

    // Skip the separator after the key (if any).
    if (current < end && spec[current] == '=')
        ++current;

    // Find the value part.
    value.setBegin(current);
    while (current < end && spec[current] != '&')
        ++current;
    value.setLength(current - value.begin());

    // Finally skip the next separator if any
    if (current < end && spec[current] == '&')
        ++current;

    // Save the new query
    query = URLComponent::fromRange(current, end);
    return true;
}

} // namespace

bool ExtractScheme(const char* url, int urlLength, URLComponent* scheme)
{
    return doExtractScheme(url, urlLength, *scheme);
}

bool ExtractScheme(const UChar* url, int urlLength, URLComponent* scheme)
{
    return doExtractScheme(url, urlLength, *scheme);
}

// This handles everything that may be an authority terminator, including
// backslash. For special backslash handling see DoParseAfterScheme.
bool IsAuthorityTerminator(UChar character)
{
    return isURLSlash(character) || character == '?' || character == '#';
}

void ExtractFileName(const char* url, const URLComponent& path, URLComponent* fileName)
{
    doExtractFileName(url, path, *fileName);
}

void ExtractFileName(const UChar* url, const URLComponent& path, URLComponent* fileName)
{
    doExtractFileName(url, path, *fileName);
}

bool ExtractQueryKeyValue(const char* url, URLComponent* query, URLComponent* key, URLComponent* value)
{
    return doExtractQueryKeyValue(url, *query, *key, *value);
}

bool ExtractQueryKeyValue(const UChar* url, URLComponent* query, URLComponent* key,  URLComponent* value)
{
    return doExtractQueryKeyValue(url, *query, *key, *value);
}

void ParseAuthority(const char* spec, const URLComponent& auth, URLComponent* username, URLComponent* password, URLComponent* hostname, URLComponent* portNumber)
{
    doParseAuthority(spec, auth, *username, *password, *hostname, *portNumber);
}

void ParseAuthority(const UChar* spec, const URLComponent& auth, URLComponent* username, URLComponent* password, URLComponent* hostname, URLComponent* portNumber)
{
    doParseAuthority(spec, auth, *username, *password, *hostname, *portNumber);
}

int ParsePort(const char* url, const URLComponent& port)
{
    return doParsePort(url, port);
}

int ParsePort(const UChar* url, const URLComponent& port)
{
    return doParsePort(url, port);
}

void ParseStandardURL(const char* url, int urlLength, URLSegments* parsed)
{
    doParseStandardURL(url, urlLength, *parsed);
}

void ParseStandardURL(const UChar* url, int urlLength, URLSegments* parsed)
{
    doParseStandardURL(url, urlLength, *parsed);
}

void ParsePathURL(const char* url, int urlLength, URLSegments* parsed)
{
    doParsePathURL(url, urlLength, *parsed);
}

void ParsePathURL(const UChar* url, int urlLength, URLSegments* parsed)
{
    doParsePathURL(url, urlLength, *parsed);
}

void ParseFileSystemURL(const char* url, int urlLength, URLSegments* parsed)
{
    doParseFileSystemURL(url, urlLength, *parsed);
}

void ParseFileSystemURL(const UChar* url, int urlLength, URLSegments* parsed)
{
    doParseFileSystemURL(url, urlLength, *parsed);
}

void ParseMailtoURL(const char* url, int urlLength, URLSegments* parsed)
{
    doParseMailtoURL(url, urlLength, *parsed);
}

void ParseMailtoURL(const UChar* url, int urlLength, URLSegments* parsed)
{
    doParseMailtoURL(url, urlLength, *parsed);
}

void parsePathInternal(const char* spec, const URLComponent& path, URLComponent* filepath, URLComponent* query, URLComponent* fragment)
{
    parsePath(spec, path, *filepath, *query, *fragment);
}

void parsePathInternal(const UChar* spec, const URLComponent& path, URLComponent* filepath, URLComponent* query, URLComponent* fragment)
{
    parsePath(spec, path, *filepath, *query, *fragment);
}

void parseAfterScheme(const char* spec, int specLength, int afterScheme, URLSegments& parsed)
{
    doParseAfterScheme(spec, specLength, afterScheme, parsed);
}

void parseAfterScheme(const UChar* spec, int specLength, int afterScheme, URLSegments& parsed)
{
    doParseAfterScheme(spec, specLength, afterScheme, parsed);
}

} // namespace URLParser

} // namespace WTF

#endif // USE(WTFURL)
