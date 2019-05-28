/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *
 */

#pragma once

#include "CachedResource.h"
#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "LinkLoaderClient.h"

#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class LinkPreloadResourceClient;

struct LinkRelAttribute;

class LinkLoader : private CachedResourceClient, public CanMakeWeakPtr<LinkLoader> {
public:
    explicit LinkLoader(LinkLoaderClient&);
    virtual ~LinkLoader();

    bool loadLink(const LinkRelAttribute&, const URL&, const String& as, const String& media, const String& type, const String& crossOrigin, Document&);
    static Optional<CachedResource::Type> resourceTypeFromAsAttribute(const String& as);

    enum class MediaAttributeCheck { MediaAttributeEmpty, MediaAttributeNotEmpty, SkipMediaAttributeCheck };
    static void loadLinksFromHeader(const String& headerValue, const URL& baseURL, Document&, MediaAttributeCheck);
    static bool isSupportedType(CachedResource::Type, const String& mimeType);

    void triggerEvents(const CachedResource&);
    void cancelLoad();

private:
    void notifyFinished(CachedResource&) override;
    static void preconnectIfNeeded(const LinkRelAttribute&, const URL& href, Document&, const String& crossOrigin);
    static std::unique_ptr<LinkPreloadResourceClient> preloadIfNeeded(const LinkRelAttribute&, const URL& href, Document&, const String& as, const String& media, const String& type, const String& crossOriginMode, LinkLoader*);
    void prefetchIfNeeded(const LinkRelAttribute&, const URL& href, Document&);

    LinkLoaderClient& m_client;
    CachedResourceHandle<CachedResource> m_cachedLinkResource;
    std::unique_ptr<LinkPreloadResourceClient> m_preloadResourceClient;
};

}
