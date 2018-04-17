/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <WebCore/ResourceRequest.h>
#include <wtf/Ref.h>

namespace WebKit {
class WebNavigationState;
}

namespace API {

class Navigation : public ObjectImpl<Object::Type::Navigation> {
public:
    static Ref<Navigation> create(WebKit::WebNavigationState& state)
    {
        return adoptRef(*new Navigation(state));
    }

    static Ref<Navigation> create(WebKit::WebNavigationState& state, WebCore::ResourceRequest&& request)
    {
        return adoptRef(*new Navigation(state, WTFMove(request)));
    }

    virtual ~Navigation();

    uint64_t navigationID() const { return m_navigationID; }

    const WebCore::ResourceRequest& request() const { return m_request; }

    void setShouldForceDownload(bool value) { m_shouldForceDownload = value; }
    bool shouldForceDownload() const { return m_shouldForceDownload; }

private:
    explicit Navigation(WebKit::WebNavigationState&);
    explicit Navigation(WebKit::WebNavigationState&, WebCore::ResourceRequest&&);

    uint64_t m_navigationID;
    WebCore::ResourceRequest m_request;
    bool m_shouldForceDownload { false };
};

} // namespace API
