/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#pragma once

#include "HTTPHeaderNames.h"
#include "ReferrerPolicy.h"
#include "StoredCredentialsPolicy.h"
#include <wtf/Forward.h>
#include <wtf/HashSet.h>

namespace WebCore {

class CachedResourceRequest;
class Document;
class HTTPHeaderMap;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SecurityOrigin;

struct ResourceLoaderOptions;

WEBCORE_EXPORT bool isSimpleCrossOriginAccessRequest(const String& method, const HTTPHeaderMap&);
bool isOnAccessControlSimpleRequestMethodWhitelist(const String&);

void updateRequestReferrer(ResourceRequest&, ReferrerPolicy, const String&);
    
WEBCORE_EXPORT void updateRequestForAccessControl(ResourceRequest&, SecurityOrigin&, StoredCredentialsPolicy);

WEBCORE_EXPORT ResourceRequest createAccessControlPreflightRequest(const ResourceRequest&, SecurityOrigin&, const String&);
CachedResourceRequest createPotentialAccessControlRequest(ResourceRequest&&, Document&, const String& crossOriginAttribute, ResourceLoaderOptions&&);

bool isValidCrossOriginRedirectionURL(const URL&);

using HTTPHeaderNameSet = HashSet<HTTPHeaderName, WTF::IntHash<HTTPHeaderName>, WTF::StrongEnumHashTraits<HTTPHeaderName>>;
HTTPHeaderNameSet httpHeadersToKeepFromCleaning(const HTTPHeaderMap&);
WEBCORE_EXPORT void cleanHTTPRequestHeadersForAccessControl(ResourceRequest&, const HTTPHeaderNameSet& = { });

WEBCORE_EXPORT bool passesAccessControlCheck(const ResourceResponse&, StoredCredentialsPolicy, SecurityOrigin&, String& errorDescription);
WEBCORE_EXPORT bool validatePreflightResponse(const ResourceRequest&, const ResourceResponse&, StoredCredentialsPolicy, SecurityOrigin&, String& errorDescription);

WEBCORE_EXPORT Optional<ResourceError> validateCrossOriginResourcePolicy(const SecurityOrigin&, const URL&, const ResourceResponse&);

} // namespace WebCore
