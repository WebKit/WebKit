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

#include "APIContentWorld.h"
#include "APIObject.h"
#include "UserScriptIdentifier.h"
#include <WebCore/UserScript.h>
#include <wtf/Identified.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class WebPageProxy;
}

namespace API {

class UserScript final : public ObjectImpl<Object::Type::UserScript>, public Identified<WebKit::UserScriptIdentifier> {
public:
    static Ref<UserScript> create(WebCore::UserScript&& userScript, API::ContentWorld& world, WebKit::WebPageProxy* page = nullptr)
    {
        return adoptRef(*new UserScript(WTF::move(userScript), world, page));
    }

    UserScript(WebCore::UserScript, API::ContentWorld&, WebKit::WebPageProxy* = nullptr);

    WebCore::UserScript& userScript() LIFETIME_BOUND { return m_userScript; }
    const WebCore::UserScript& userScript() const LIFETIME_BOUND { return m_userScript; }

    ContentWorld& contentWorld() { return m_world; }
    const ContentWorld& contentWorld() const { return m_world; }

    WebKit::WebPageProxy* page() const;

    bool isPageScoped() const { return m_isPageScoped; }

    // Scripts that are page scoped but whose page has gone away are orphaned,
    // and should not message anywhere.
    bool isOrphaned() const { return m_isPageScoped && !page(); }

private:
    WebCore::UserScript m_userScript;
    const Ref<ContentWorld> m_world;
    WeakPtr<WebKit::WebPageProxy> m_page;
    bool m_isPageScoped { false };
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(UserScript);
