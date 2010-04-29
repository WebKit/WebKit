/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebSecurityPolicy_h
#define WebSecurityPolicy_h

#include "WebCommon.h"

namespace WebKit {

class WebString;
class WebURL;

class WebSecurityPolicy {
public:
    // Registers a URL scheme to be treated as a local scheme (i.e., with the
    // same security rules as those applied to "file" URLs).  This means that
    // normal pages cannot link to or access URLs of this scheme.
    WEBKIT_API static void registerURLSchemeAsLocal(const WebString&);

    // Registers a URL scheme to be treated as a noAccess scheme.  This means
    // that pages loaded with this URL scheme cannot access pages loaded with
    // any other URL scheme.
    WEBKIT_API static void registerURLSchemeAsNoAccess(const WebString&);

    // Registers a URL scheme to not generate mixed content warnings when
    // included by an HTTPS page.
    WEBKIT_API static void registerURLSchemeAsSecure(const WebString&);

    // Support for whitelisting access to origins beyond the same-origin policy.
    WEBKIT_API static void addOriginAccessWhitelistEntry(
        const WebURL& sourceOrigin, const WebString& destinationProtocol,
        const WebString& destinationHost, bool allowDestinationSubdomains);
    WEBKIT_API static void removeOriginAccessWhitelistEntry(
        const WebURL& sourceOrigin, const WebString& destinationProtocol,
        const WebString& destinationHost, bool allowDestinationSubdomains);
    WEBKIT_API static void resetOriginAccessWhitelists();
    // DEPRECATED: Phase on of renaming to addOriginAccessWhitelistEntry.
    WEBKIT_API static void whiteListAccessFromOrigin(
        const WebURL& sourceOrigin, const WebString& destinationProtocol,
        const WebString& destinationHost, bool allowDestinationSubdomains);
    // DEPRECATED: Phase on of renaming to resetOriginAccessWhitelists..
    WEBKIT_API static void resetOriginAccessWhiteLists();
    
    // Returns whether the url should be allowed to see the referrer
    // based on their respective protocols.
    WEBKIT_API static bool shouldHideReferrer(const WebURL& url, const WebString& referrer);

private:
    WebSecurityPolicy();
};

} // namespace WebKit

#endif
