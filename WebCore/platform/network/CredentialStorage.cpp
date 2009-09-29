/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CredentialStorage.h"

#include "CString.h"
#include "Credential.h"
#include "KURL.h"
#include "ProtectionSpaceHash.h"
#include "StringHash.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {

typedef HashMap<ProtectionSpace, Credential> ProtectionSpaceToCredentialMap;
static ProtectionSpaceToCredentialMap& protectionSpaceToCredentialMap()
{
    DEFINE_STATIC_LOCAL(ProtectionSpaceToCredentialMap, map, ());
    return map;
}

typedef HashMap<String, HashMap<String, Credential> > OriginToDefaultBasicCredentialMap;
static OriginToDefaultBasicCredentialMap& originToDefaultBasicCredentialMap()
{
    DEFINE_STATIC_LOCAL(OriginToDefaultBasicCredentialMap, map, ());
    return map;
}
    
static String originStringFromURL(const KURL& url)
{
    if (url.port())
        return url.protocol() + "://" + url.host() + String::format(":%i/", url.port());
    
    return url.protocol() + "://" + url.host() + "/";
}

void CredentialStorage::set(const Credential& credential, const ProtectionSpace& protectionSpace, const KURL& url)
{
    ASSERT(url.protocolInHTTPFamily());
    ASSERT(url.isValid());

    protectionSpaceToCredentialMap().set(protectionSpace, credential);
    
    ProtectionSpaceAuthenticationScheme scheme = protectionSpace.authenticationScheme();
    if (url.protocolInHTTPFamily() && (scheme == ProtectionSpaceAuthenticationSchemeHTTPBasic || scheme == ProtectionSpaceAuthenticationSchemeDefault)) {
        String origin = originStringFromURL(url);
        
        HashMap<String, Credential> pathToCredentialMap;
        pair<HashMap<String, HashMap<String, Credential> >::iterator, bool> result = originToDefaultBasicCredentialMap().add(origin, pathToCredentialMap);
        
        // Remove the last path component that is not a directory to determine the subpath for which this credential applies.
        // We keep a leading slash, but remove a trailing one.
        String path = url.path();
        ASSERT(path.length() > 0);
        ASSERT(path[0] == '/');
        if (path.length() > 1) {
            int index = path.reverseFind('/');
            path = path.substring(0, index ? index : 1);
        }
        ASSERT(path.length() == 1 || path[path.length() - 1] != '/');
        
        result.first->second.set(path, credential);
    }
}

Credential CredentialStorage::get(const ProtectionSpace& protectionSpace)
{
    return protectionSpaceToCredentialMap().get(protectionSpace);
}

Credential CredentialStorage::getDefaultAuthenticationCredential(const KURL& url)
{
    ASSERT(url.protocolInHTTPFamily());
    String origin = originStringFromURL(url);
    const HashMap<String, Credential>& pathToCredentialMap(originToDefaultBasicCredentialMap().get(origin));
    if (pathToCredentialMap.isEmpty())
        return Credential();
    
    // Check to see if there is a stored credential for the subpath ancestry of this url.
    String path = url.path();
    Credential credential = pathToCredentialMap.get(path);
    while (credential.isEmpty() && !path.isNull()) {
        int index = path.reverseFind('/');
        if (index == 0) {
            credential = pathToCredentialMap.get("/");
            break;
        } else if (index == -1) {
            // This case should never happen, as all HTTP URL paths should start with a leading /
            ASSERT_NOT_REACHED();
            credential = pathToCredentialMap.get(path);
            break;
        } else {
            path = path.substring(0, index);
            credential = pathToCredentialMap.get(path);
        }
    }
    return credential;
}

} // namespace WebCore
