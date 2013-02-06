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
#ifndef URLCanon_h
#define URLCanon_h

#include "URLParse.h"
#include <stdlib.h>
#include <wtf/unicode/Unicode.h>
#include <wtf/url/api/URLBuffer.h>

#if USE(WTFURL)

namespace WTF {

class URLQueryCharsetConverter;

namespace URLCanonicalizer {

// Whitespace -----------------------------------------------------------------

// Searches for whitespace that should be removed from the middle of URLs, and
// removes it. Removed whitespace are tabs and newlines, but NOT spaces. Spaces
// are preserved, which is what most browsers do. A pointer to the output will
// be returned, and the length of that output will be in |outputLength|.
//
// This should be called before parsing if whitespace removal is desired (which
// it normally is when you are canonicalizing).
//
// If no whitespace is removed, this function will not use the buffer and will
// return a pointer to the input, to avoid the extra copy. If modification is
// required, the given |buffer| will be used and the returned pointer will
// point to the beginning of the buffer.
//
// Therefore, callers should not use the buffer, since it may actuall be empty,
// use the computed pointer and |outputLength| instead.
const char* removeURLWhitespace(const char* input, int inputLength, URLBuffer<char>&, int& outputLength);
const UChar* removeURLWhitespace(const UChar* input, int inputLength, URLBuffer<UChar>&, int& outputLength);

// IDN ------------------------------------------------------------------------

// Converts the Unicode input representing a hostname to ASCII using IDN rules.
// The output must fall in the ASCII range, but will be encoded in UTF-16.
//
// On success, the output will be filled with the ASCII host name and it will
// return true. Unlike most other canonicalization functions, this assumes that
// the output is empty. The beginning of the host will be at offset 0, and
// the length of the output will be set to the length of the new host name.
//
// On error, returns false. The output in this case is undefined.
bool IDNToASCII(const UChar* src, int sourceLength, URLBuffer<UChar>& output);

// Piece-by-piece canonicalizers ----------------------------------------------
//
// These individual canonicalizers append the canonicalized versions of the
// corresponding URL component to the given std::string. The spec and the
// previously-identified range of that component are the input. The range of
// the canonicalized component will be written to the output component.
//
// These functions all append to the output so they can be chained. Make sure
// the output is empty when you start.
//
// These functions returns boolean values indicating success. On failure, they
// will attempt to write something reasonable to the output so that, if
// displayed to the user, they will recognise it as something that's messed up.
// Nothing more should ever be done with these invalid URLs, however.

// Scheme: Appends the scheme and colon to the URL. The output component will
// indicate the range of characters up to but not including the colon.
//
// Canonical URLs always have a scheme. If the scheme is not present in the
// input, this will just write the colon to indicate an empty scheme. Does not
// append slashes which will be needed before any authority components for most
// URLs.
//
// The 8-bit version requires UTF-8 encoding.
bool canonicalizeScheme(const char* spec, const URLComponent& scheme, URLBuffer<char>&, URLComponent& ouputScheme);
bool canonicalizeScheme(const UChar* spec, const URLComponent& scheme, URLBuffer<char>&, URLComponent& ouputScheme);

// User info: username/password. If present, this will add the delimiters so
// the output will be "<username>:<password>@" or "<username>@". Empty
// username/password pairs, or empty passwords, will get converted to
// nonexistant in the canonical version.
//
// The components for the username and password refer to ranges in the
// respective source strings. Usually, these will be the same string, which
// is legal as long as the two components don't overlap.
//
// The 8-bit version requires UTF-8 encoding.
bool canonicalizeUserInfo(const char* usernameSource, const URLComponent& username, const char* passwordSource, const URLComponent& password,
                          URLBuffer<char>&, URLComponent& outputUsername, URLComponent& outputPassword);
bool canonicalizeUserInfo(const UChar* usernameSource, const URLComponent& username, const UChar* passwordSource, const URLComponent& password,
                          URLBuffer<char>&, URLComponent& outputUsername, URLComponent& outputPassword);


// This structure holds detailed state exported from the IP/Host canonicalizers.
// Additional fields may be added as callers require them.
struct CanonHostInfo {
    CanonHostInfo()
        : family(NEUTRAL)
        , ipv4ComponentsCount(0)
        , ouputHost()
    {
    }

    // Convenience function to test if family is an IP address.
    bool IsIPAddress() const { return family == IPV4 || family == IPV6; }

    // This field summarizes how the input was classified by the canonicalizer.
    enum Family {
        // - Doesn't resemble an IP address. As far as the IP
        // canonicalizer is concerned, it should be treated as a
        //   hostname.
        NEUTRAL,

        // - Almost an IP, but was not canonicalized. This could be an
        // IPv4 address where truncation occurred, or something
        // containing the special characters :[] which did not parse
        // as an IPv6 address. Never attempt to connect to this
        // address, because it might actually succeed!
        BROKEN,

        IPV4, // - Successfully canonicalized as an IPv4 address.
        IPV6, // - Successfully canonicalized as an IPv6 address.
    };
    Family family;

    // If |family| is IPV4, then this is the number of nonempty dot-separated
    // components in the input text, from 1 to 4. If |family| is not IPV4,
    // this value is undefined.
    int ipv4ComponentsCount;

    // Location of host within the canonicalized output.
    // canonicalizeIPAddress() only sets this field if |family| is IPV4 or IPV6.
    URLComponent ouputHost;

    // |address| contains the parsed IP Address (if any) in its first
    // AddressLength() bytes, in network order. If IsIPAddress() is false
    // AddressLength() will return zero and the content of |address| is undefined.
    unsigned char address[16];

    // Convenience function to calculate the length of an IP address corresponding
    // to the current IP version in |family|, if any. For use with |address|.
    int AddressLength() const
    {
        return family == IPV4 ? 4 : (family == IPV6 ? 16 : 0);
    }
};

// Host.
//
// The 8-bit version requires UTF-8 encoding. Use this version when you only
// need to know whether canonicalization succeeded.
bool canonicalizeHost(const char* spec, const URLComponent& host, URLBuffer<char>&, URLComponent& ouputHost);
bool canonicalizeHost(const UChar* spec, const URLComponent& host, URLBuffer<char>&, URLComponent& ouputHost);

// IP addresses.
//
// Tries to interpret the given host name as an IPv4 or IPv6 address. If it is
// an IP address, it will canonicalize it as such, appending it to |output|.
// Additional status information is returned via the |*hostInfo| parameter.
// See the definition of CanonHostInfo above for details.
//
// This is called AUTOMATICALLY from the host canonicalizer, which ensures that
// the input is unescaped and name-prepped, etc. It should not normally be
// necessary or wise to call this directly.
void canonicalizeIPAddress(const char* spec, const URLComponent& host, URLBuffer<char>&, CanonHostInfo&);
void canonicalizeIPAddress(const UChar* spec, const URLComponent& host, URLBuffer<char>&, CanonHostInfo&);

// Port: this function will add the colon for the port if a port is present.
// The caller can pass URLParser::PORT_UNSPECIFIED as the
// defaultPortForScheme argument if there is no default port.
//
// The 8-bit version requires UTF-8 encoding.
bool canonicalizePort(const char* spec, const URLComponent& port, int defaultPortForScheme, URLBuffer<char>&, URLComponent& ouputPort);
bool canonicalizePort(const UChar* spec, const URLComponent& port, int defaultPortForScheme, URLBuffer<char>&, URLComponent& ouputPort);

// Returns the default port for the given canonical scheme, or PORT_UNSPECIFIED
// if the scheme is unknown.
int defaultPortForScheme(const char* scheme, int schemeLength);

// Path. If the input does not begin in a slash (including if the input is
// empty), we'll prepend a slash to the path to make it canonical.
//
// The 8-bit version assumes UTF-8 encoding, but does not verify the validity
// of the UTF-8 (i.e., you can have invalid UTF-8 sequences, invalid
// characters, etc.). Normally, URLs will come in as UTF-16, so this isn't
// an issue. Somebody giving us an 8-bit path is responsible for generating
// the path that the server expects (we'll escape high-bit characters), so
// if something is invalid, it's their problem.
bool CanonicalizePath(const char* spec, const URLComponent& path, URLBuffer<char>&, URLComponent* outputPath);
bool CanonicalizePath(const UChar* spec, const URLComponent& path, URLBuffer<char>&, URLComponent* outputPath);

// Canonicalizes the input as a file path. This is like CanonicalizePath except
// that it also handles Windows drive specs. For example, the path can begin
// with "c|\" and it will get properly canonicalized to "C:/".
// The string will be appended to |output| and |outputPath| will be updated.
//
// The 8-bit version requires UTF-8 encoding.
bool FileCanonicalizePath(const char* spec, const URLComponent& path, URLBuffer<char>&, URLComponent* outputPath);
bool FileCanonicalizePath(const UChar* spec, const URLComponent& path, URLBuffer<char>&, URLComponent* outputPath);

// Query: Prepends the ? if needed.
//
// The 8-bit version requires the input to be UTF-8 encoding. Incorrectly
// encoded characters (in UTF-8 or UTF-16) will be replaced with the Unicode
// "invalid character." This function can not fail, we always just try to do
// our best for crazy input here since web pages can set it themselves.
//
// This will convert the given input into the output encoding that the given
// character set converter object provides. The converter will only be called
// if necessary, for ASCII input, no conversions are necessary.
//
// The converter can be null. In this case, the output encoding will be UTF-8.
void CanonicalizeQuery(const char* spec, const URLComponent& query, URLQueryCharsetConverter*, URLBuffer<char>&, URLComponent* outputQuery);
void CanonicalizeQuery(const UChar* spec, const URLComponent& query, URLQueryCharsetConverter*, URLBuffer<char>&, URLComponent* outputQuery);

// Ref: Prepends the # if needed. The output will be UTF-8 (this is the only
// canonicalizer that does not produce ASCII output). The output is
// guaranteed to be valid UTF-8.
//
// This function will not fail. If the input is invalid UTF-8/UTF-16, we'll use
// the "Unicode replacement character" for the confusing bits and copy the rest.
void canonicalizeFragment(const char* spec, const URLComponent& path, URLBuffer<char>&, URLComponent& outputFragment);
void canonicalizeFragment(const UChar* spec, const URLComponent& path, URLBuffer<char>&, URLComponent& outputFragment);

// Full canonicalizer ---------------------------------------------------------
//
// These functions replace any string contents, rather than append as above.
// See the above piece-by-piece functions for information specific to
// canonicalizing individual components.
//
// The output will be ASCII except the reference fragment, which may be UTF-8.
//
// The 8-bit versions require UTF-8 encoding.

// Use for standard URLs with authorities and paths.
bool CanonicalizeStandardURL(const char* spec, int specLength, const URLSegments& parsed, URLQueryCharsetConverter* queryConverter,
                             URLBuffer<char>&, URLSegments* outputParsed);
bool CanonicalizeStandardURL(const UChar* spec, int specLength, const URLSegments& parsed, URLQueryCharsetConverter* queryConverter,
                             URLBuffer<char>&, URLSegments* outputParsed);

// Use for file URLs.
bool CanonicalizeFileURL(const char* spec, int specLength, const URLSegments& parsed, URLQueryCharsetConverter* queryConverter,
                         URLBuffer<char>&, URLSegments* outputParsed);
bool CanonicalizeFileURL(const UChar* spec, int specLength, const URLSegments& parsed, URLQueryCharsetConverter* queryConverter,
                         URLBuffer<char>&, URLSegments* outputParsed);

// Use for filesystem URLs.
bool canonicalizeFileSystemURL(const char* spec, const URLSegments& parsed, URLQueryCharsetConverter* queryConverter,
                               URLBuffer<char>&, URLSegments& outputParsed);
bool canonicalizeFileSystemURL(const UChar* spec, const URLSegments& parsed, URLQueryCharsetConverter* queryConverter,
                               URLBuffer<char>&, URLSegments& outputParsed);

// Use for path URLs such as javascript. This does not modify the path in any
// way, for example, by escaping it.
bool canonicalizePathURL(const char* spec, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& ouputParsed);
bool canonicalizePathURL(const UChar* spec, const URLSegments& parsed, URLBuffer<char>& output, URLSegments& ouputParsed);

// Use for mailto URLs. This "canonicalizes" the url into a path and query
// component. It does not attempt to merge "to" fields. It uses UTF-8 for
// the query encoding if there is a query. This is because a mailto URL is
// really intended for an external mail program, and the encoding of a page,
// etc. which would influence a query encoding normally are irrelevant.
bool canonicalizeMailtoURL(const char* spec, const URLSegments& parsed, URLBuffer<char>&, URLSegments& outputParsed);
bool canonicalizeMailtoURL(const UChar* spec,  const URLSegments& parsed, URLBuffer<char>&, URLSegments& outputParsed);

// Part replacer --------------------------------------------------------------

// Internal structure used for storing separate strings for each component.
// The basic canonicalization functions use this structure internally so that
// component replacement (different strings for different components) can be
// treated on the same code path as regular canonicalization (the same string
// for each component).
//
// A URLSegments structure usually goes along with this. Those
// components identify offsets within these strings, so that they can all be
// in the same string, or spread arbitrarily across different ones.
//
// This structures does not own any data. It is the caller's responsibility to
// ensure that the data the pointers point to stays in scope and is not
// modified.
template<typename CharacterType>
struct URLComponentSource {
    // Constructor normally used by callers wishing to replace components. This
    // will make them all null, which is no replacement. The caller would then
    // override the components they want to replace.
    URLComponentSource()
        : scheme(0)
        , username(0)
        , password(0)
        , host(0)
        , port(0)
        , path(0)
        , query(0)
        , ref(0)
    {
    }

    // Constructor normally used internally to initialize all the components to
    // point to the same spec.
    explicit URLComponentSource(const CharacterType* defaultValue)
        : scheme(defaultValue)
        , username(defaultValue)
        , password(defaultValue)
        , host(defaultValue)
        , port(defaultValue)
        , path(defaultValue)
        , query(defaultValue)
        , ref(defaultValue)
    {
    }

    const CharacterType* scheme;
    const CharacterType* username;
    const CharacterType* password;
    const CharacterType* host;
    const CharacterType* port;
    const CharacterType* path;
    const CharacterType* query;
    const CharacterType* ref;
};

// This structure encapsulates information on modifying a URL. Each component
// may either be left unchanged, replaced, or deleted.
//
// By default, each component is unchanged. For those components that should be
// modified, call either Set* or Clear* to modify it.
//
// The string passed to Set* functions DOES NOT GET COPIED AND MUST BE KEPT
// IN SCOPE BY THE CALLER for as long as this object exists!
//
// Prefer the 8-bit replacement version if possible since it is more efficient.
template<typename CharacterType>
class Replacements {
public:
    Replacements()
    {
    }

    // Scheme
    void SetScheme(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.scheme = s;
        m_segments.scheme = comp;
    }
    // Note: we don't have a ClearScheme since this doesn't make any sense.
    bool IsSchemeOverridden() const { return !!m_sources.scheme; }

    // Username
    void SetUsername(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.username = s;
        m_segments.username = comp;
    }
    void ClearUsername()
    {
        m_sources.username = Placeholder();
        m_segments.username = URLComponent();
    }
    bool IsUsernameOverridden() const { return !!m_sources.username; }

    // Password
    void SetPassword(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.password = s;
        m_segments.password = comp;
    }
    void ClearPassword()
    {
        m_sources.password = Placeholder();
        m_segments.password = URLComponent();
    }
    bool IsPasswordOverridden() const { return !!m_sources.password; }

    // Host
    void SetHost(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.host = s;
        m_segments.host = comp;
    }
    void ClearHost()
    {
        m_sources.host = Placeholder();
        m_segments.host = URLComponent();
    }
    bool IsHostOverridden() const { return !!m_sources.host; }

    // Port
    void SetPort(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.port = s;
        m_segments.port = comp;
    }
    void ClearPort()
    {
        m_sources.port = Placeholder();
        m_segments.port = URLComponent();
    }
    bool IsPortOverridden() const { return !!m_sources.port; }

    // Path
    void SetPath(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.path = s;
        m_segments.path = comp;
    }
    void ClearPath()
    {
        m_sources.path = Placeholder();
        m_segments.path = URLComponent();
    }
    bool IsPathOverridden() const { return !m_sources.path; }

    // Query
    void SetQuery(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.query = s;
        m_segments.query = comp;
    }
    void ClearQuery()
    {
        m_sources.query = Placeholder();
        m_segments.query = URLComponent();
    }
    bool IsQueryOverridden() const { return !m_sources.query; }

    // Ref
    void SetRef(const CharacterType* s, const URLComponent& comp)
    {
        m_sources.ref = s;
        m_segments.fragment = comp;
    }
    void ClearRef()
    {
        m_sources.ref = Placeholder();
        m_segments.fragment = URLComponent();
    }
    bool IsRefOverridden() const { return !m_sources.ref; }

    // Getters for the itnernal data. See the variables below for how the
    // information is encoded.
    const URLComponentSource<CharacterType>& sources() const { return m_sources; }
    const URLSegments& components() const { return m_segments; }

private:
    // Returns a pointer to a static empty string that is used as a placeholder
    // to indicate a component should be deleted (see below).
    const CharacterType* Placeholder()
    {
        static const CharacterType emptyString = 0;
        return &emptyString;
    }

    // We support three states:
    //
    // Action                 | Source                Component
    // -----------------------+--------------------------------------------------
    // Don't change component | null                  (unused)
    // Replace component      | (replacement string)  (replacement component)
    // Delete component       | (non-null)            (invalid component: (0,-1))
    //
    // We use a pointer to the empty string for the source when the component
    // should be deleted.
    URLComponentSource<CharacterType> m_sources;
    URLSegments m_segments;
};

// The base must be an 8-bit canonical URL.
bool ReplaceStandardURL(const char* base,
                        const URLSegments& baseParsed,
                        const Replacements<char>&,
                        URLQueryCharsetConverter* queryConverter,
                        URLBuffer<char>&,
                        URLSegments* outputParsed);
bool ReplaceStandardURL(const char* base,
                        const URLSegments& baseParsed,
                        const Replacements<UChar>&,
                        URLQueryCharsetConverter* queryConverter,
                        URLBuffer<char>&,
                        URLSegments* outputParsed);

// Filesystem URLs can only have the path, query, or ref replaced.
// All other components will be ignored.
bool ReplaceFileSystemURL(const char* base,
                          const URLSegments& baseParsed,
                          const Replacements<char>&,
                          URLQueryCharsetConverter* queryConverter,
                          URLBuffer<char>&,
                          URLSegments* outputParsed);
bool ReplaceFileSystemURL(const char* base,
                          const URLSegments& baseParsed,
                          const Replacements<UChar>&,
                          URLQueryCharsetConverter* queryConverter,
                          URLBuffer<char>&,
                          URLSegments* outputParsed);

// Replacing some parts of a file URL is not permitted. Everything except
// the host, path, query, and ref will be ignored.
bool ReplaceFileURL(const char* base,
                    const URLSegments& baseParsed,
                    const Replacements<char>&,
                    URLQueryCharsetConverter* queryConverter,
                    URLBuffer<char>&,
                    URLSegments* outputParsed);
bool ReplaceFileURL(const char* base,
                    const URLSegments& baseParsed,
                    const Replacements<UChar>&,
                    URLQueryCharsetConverter* queryConverter,
                    URLBuffer<char>&,
                    URLSegments* outputParsed);

// Path URLs can only have the scheme and path replaced. All other components
// will be ignored.
bool ReplacePathURL(const char* base,
                    const URLSegments& baseParsed,
                    const Replacements<char>&,
                    URLBuffer<char>&,
                    URLSegments* outputParsed);
bool ReplacePathURL(const char* base,
                    const URLSegments& baseParsed,
                    const Replacements<UChar>&,
                    URLBuffer<char>&,
                    URLSegments* outputParsed);

// Mailto URLs can only have the scheme, path, and query replaced.
// All other components will be ignored.
bool replaceMailtoURL(const char* base, const URLSegments& baseParsed,
                      const Replacements<char>&,
                      URLBuffer<char>& output, URLSegments& outputParsed);
bool replaceMailtoURL(const char* base, const URLSegments& baseParsed,
                      const Replacements<UChar>&,
                      URLBuffer<char>& output, URLSegments& outputParsed);

// Relative URL ---------------------------------------------------------------

// Given an input URL or URL fragment |fragment|, determines if it is a
// relative or absolute URL and places the result into |*isRelative|. If it is
// relative, the relevant portion of the URL will be placed into
// |*relativeComponent| (there may have been trimmed whitespace, for example).
// This value is passed to resolveRelativeURL. If the input is not relative,
// this value is UNDEFINED (it may be changed by the function).
//
// Returns true on success (we successfully determined the URL is relative or
// not). Failure means that the combination of URLs doesn't make any sense.
//
// The base URL should always be canonical, therefore is ASCII.
bool isRelativeURL(const char* base, const URLSegments& baseParsed,
                   const char* fragment, int fragmentLength,
                   bool isBaseHierarchical,
                   bool& isRelative, URLComponent& relativeComponent);
bool isRelativeURL(const char* base, const URLSegments& baseParsed,
                   const UChar* fragment, int fragmentLength,
                   bool isBaseHierarchical,
                   bool& isRelative, URLComponent& relativeComponent);

// Given a canonical parsed source URL, a URL fragment known to be relative,
// and the identified relevant portion of the relative URL (computed by
// isRelativeURL), this produces a new parsed canonical URL in |output| and
// |outputParsed|.
//
// It also requires a flag indicating whether the base URL is a file: URL
// which triggers additional logic.
//
// The base URL should be canonical and have a host (may be empty for file
// URLs) and a path. If it doesn't have these, we can't resolve relative
// URLs off of it and will return the base as the output with an error flag.
// Becausee it is canonical is should also be ASCII.
//
// The query charset converter follows the same rules as CanonicalizeQuery.
//
// Returns true on success. On failure, the output will be "something
// reasonable" that will be consistent and valid, just probably not what
// was intended by the web page author or caller.
bool resolveRelativeURL(const char* baseURL, const URLSegments& baseParsed, bool baseIsFile,
                        const char* relativeURL, const URLComponent& relativeComponent,
                        URLQueryCharsetConverter* queryConverter,
                        URLBuffer<char>&, URLSegments* outputParsed);
bool resolveRelativeURL(const char* baseURL, const URLSegments& baseParsed, bool baseIsFile,
                        const UChar* relativeURL, const URLComponent& relativeComponent,
                        URLQueryCharsetConverter* queryConverter,
                        URLBuffer<char>&, URLSegments* outputParsed);

} // namespace URLCanonicalizer

} // namespace WTF

#endif // USE(WTFURL)

#endif // URLCanon_h
