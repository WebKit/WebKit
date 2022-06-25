/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "ResourceLoadInfo.h"

namespace API {

class ResourceLoadInfo final : public ObjectImpl<Object::Type::ResourceLoadInfo> {
public:
    static Ref<ResourceLoadInfo> create(WebKit::ResourceLoadInfo&& info)
    {
        return adoptRef(*new ResourceLoadInfo(WTFMove(info)));
    }

    explicit ResourceLoadInfo(WebKit::ResourceLoadInfo&& info)
        : m_info(WTFMove(info)) { }

    WebKit::NetworkResourceLoadIdentifier resourceLoadID() const { return m_info.resourceLoadID; }
    std::optional<WebCore::FrameIdentifier> frameID() const { return m_info.frameID; }
    std::optional<WebCore::FrameIdentifier> parentFrameID() const { return m_info.parentFrameID; }
    const WTF::URL& originalURL() const { return m_info.originalURL; }
    const WTF::String& originalHTTPMethod() const { return m_info.originalHTTPMethod; }
    WallTime eventTimestamp() const { return m_info.eventTimestamp; }
    bool loadedFromCache() const { return m_info.loadedFromCache; }
    WebKit::ResourceLoadInfo::Type resourceLoadType() const { return m_info.type; }

private:
    const WebKit::ResourceLoadInfo m_info;
};

} // namespace API
