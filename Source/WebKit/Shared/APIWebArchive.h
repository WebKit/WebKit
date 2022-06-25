/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(COCOA)

#include "APIObject.h"
#include <wtf/RefPtr.h>

namespace API {
class Array;
class Data;
}

namespace WebCore {
class LegacyWebArchive;
struct SimpleRange;
}

namespace API {

class WebArchiveResource;

class WebArchive : public API::ObjectImpl<API::Object::Type::WebArchive> {
public:
    virtual ~WebArchive();

    static Ref<WebArchive> create(WebArchiveResource* mainResource, RefPtr<API::Array>&& subresources, RefPtr<API::Array>&& subframeArchives);
    static Ref<WebArchive> create(API::Data*);
    static Ref<WebArchive> create(RefPtr<WebCore::LegacyWebArchive>&&);
    static Ref<WebArchive> create(const WebCore::SimpleRange&);

    WebArchiveResource* mainResource();
    API::Array* subresources();
    API::Array* subframeArchives();

    Ref<API::Data> data();

    WebCore::LegacyWebArchive* coreLegacyWebArchive();

private:
    WebArchive(WebArchiveResource* mainResource, RefPtr<API::Array>&& subresources, RefPtr<API::Array>&& subframeArchives);
    WebArchive(API::Data*);
    WebArchive(RefPtr<WebCore::LegacyWebArchive>&&);

    RefPtr<WebCore::LegacyWebArchive> m_legacyWebArchive;
    RefPtr<WebArchiveResource> m_cachedMainResource;
    RefPtr<API::Array> m_cachedSubresources;
    RefPtr<API::Array> m_cachedSubframeArchives;
};

} // namespace API

#endif // PLATFORM(COCOA)
