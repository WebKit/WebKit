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

#ifndef URLParse_h
#define URLParse_h

#include "URLComponent.h"
#include "URLSegments.h"
#include <wtf/unicode/Unicode.h>

#if USE(WTFURL)

namespace WTF {

namespace URLParser {

// Initialization functions ---------------------------------------------------
//
// These functions parse the given URL, filling in all of the structure's
// components. These functions can not fail, they will always do their best
// at interpreting the input given.
//
// The string length of the URL MUST be specified, we do not check for NULLs
// at any point in the process, and will actually handle embedded NULLs.
//
// IMPORTANT: These functions do NOT hang on to the given pointer or copy it
// in any way. See the comment above the struct.
//
// The 8-bit versions require UTF-8 encoding.

// StandardURL is for when the scheme is known to be one that has an
// authority (host) like "http". This function will not handle weird ones
// like "about:" and "javascript:", or do the right thing for "file:" URLs.
void ParseStandardURL(const char* url, int urlLength, URLSegments* parsed);
void ParseStandardURL(const UChar* url, int urlLength, URLSegments* parsed);

// PathURL is for when the scheme is known not to have an authority (host)
// section but that aren't file URLs either. The scheme is parsed, and
// everything after the scheme is considered as the path. This is used for
// things like "about:" and "javascript:"
void ParsePathURL(const char* url, int urlLength, URLSegments* parsed);
void ParsePathURL(const UChar* url, int urlLength, URLSegments* parsed);

// FileURL is for file URLs. There are some special rules for interpreting
// these.
void ParseFileURL(const char* url, int urlLength, URLSegments* parsed);
void ParseFileURL(const UChar* url, int urlLength, URLSegments* parsed);

// Filesystem URLs are structured differently than other URLs.
void ParseFileSystemURL(const char* url, int urlLength, URLSegments* parsed);
void ParseFileSystemURL(const UChar* url, int urlLength, URLSegments* parsed);

// MailtoURL is for mailto: urls. They are made up scheme,path,query
void ParseMailtoURL(const char* url, int urlLength, URLSegments* parsed);
void ParseMailtoURL(const UChar* url, int urlLength, URLSegments* parsed);

// Helper functions -----------------------------------------------------------

// Locates the scheme according to the URL  parser's rules. This function is
// designed so the caller can find the scheme and call the correct Init*
// function according to their known scheme types.
//
// It also does not perform any validation on the scheme.
//
// This function will return true if the scheme is found and will put the
// scheme's range into *scheme. False means no scheme could be found. Note
// that a URL beginning with a colon has a scheme, but it is empty, so this
// function will return true but *scheme will = (0,0).
//
// The scheme is found by skipping spaces and control characters at the
// beginning, and taking everything from there to the first colon to be the
// scheme. The character at scheme.end() will be the colon (we may enhance
// this to handle full width colons or something, so don't count on the
// actual character value). The character at scheme.end()+1 will be the
// beginning of the rest of the URL, be it the authority or the path (or the
// end of the string).
//
// The 8-bit version requires UTF-8 encoding.
bool ExtractScheme(const char* url, int urlLength, URLComponent* scheme);
bool ExtractScheme(const UChar* url, int urlLength, URLComponent* scheme);

// Returns true if ch is a character that terminates the authority segment of a URL.
bool IsAuthorityTerminator(UChar);

// Does a best effort parse of input |spec|, in range |auth|. If a particular
// component is not found, it will be set to invalid.
void ParseAuthority(const char* spec, const URLComponent& auth,
                    URLComponent* username, URLComponent* password, URLComponent* hostname, URLComponent* portNumber);
void ParseAuthority(const UChar* spec, const URLComponent& auth,
                    URLComponent* username, URLComponent* password, URLComponent* hostname, URLComponent* portNumber);

// Computes the integer port value from the given port component. The port
// component should have been identified by one of the init functions on
// |Parsed| for the given input url.
//
// The return value will be a positive integer between 0 and 64K, or one of
// the two special values below.
enum SpecialPort { PORT_UNSPECIFIED = -1, PORT_INVALID = -2 };
int ParsePort(const char* url, const URLComponent& port);
int ParsePort(const UChar* url, const URLComponent& port);

// Extracts the range of the file name in the given url. The path must
// already have been computed by the parse function, and the matching URL
// and extracted path are provided to this function. The filename is
// defined as being everything from the last slash/backslash of the path
// to the end of the path.
//
// The file name will be empty if the path is empty or there is nothing
// following the last slash.
//
// The 8-bit version requires UTF-8 encoding.
void ExtractFileName(const char* url, const URLComponent& path, URLComponent* fileName);
void ExtractFileName(const UChar* url, const URLComponent& path, URLComponent* fileName);

// Extract the first key/value from the range defined by |*query|. Updates
// |*query| to start at the end of the extracted key/value pair. This is
// designed for use in a loop: you can keep calling it with the same query
// object and it will iterate over all items in the query.
//
// Some key/value pairs may have the key, the value, or both be empty (for
// example, the query string "?&"). These will be returned. Note that an empty
// last parameter "foo.com?" or foo.com?a&" will not be returned, this case
// is the same as "done."
//
// The initial query component should not include the '?' (this is the default
// for parsed URLs).
//
// If no key/value are found |*key| and |*value| will be unchanged and it will
// return false.
bool ExtractQueryKeyValue(const char* url, URLComponent* query, URLComponent* key, URLComponent* value);
bool ExtractQueryKeyValue(const UChar* url, URLComponent* query, URLComponent* key, URLComponent* value);

} // namespace URLParser

} // namespace WTF

#endif // USE(WTFURL)

#endif // URLParse_h
