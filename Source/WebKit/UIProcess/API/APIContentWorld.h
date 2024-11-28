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
#include "ContentWorldData.h"
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebUserContentControllerProxy;
}

namespace API {

class ContentWorld final : public API::ObjectImpl<API::Object::Type::ContentWorld>, public CanMakeWeakPtr<ContentWorld> {
public:
    static ContentWorld* worldForIdentifier(WebKit::ContentWorldIdentifier);
    static Ref<ContentWorld> sharedWorldWithName(const WTF::String&, OptionSet<WebKit::ContentWorldOption> options = { });
    static ContentWorld& pageContentWorld();
    static ContentWorld& defaultClientWorld();

    virtual ~ContentWorld();

    WebKit::ContentWorldIdentifier identifier() const { return m_identifier; }
    const WTF::String& name() const { return m_name; }
    WebKit::ContentWorldData worldData() const { return { m_identifier, m_name, m_options }; }

    bool allowAccessToClosedShadowRoots() const { return m_options.contains(WebKit::ContentWorldOption::AllowAccessToClosedShadowRoots); }
    void setAllowAccessToClosedShadowRoots(bool value) { m_options.add(WebKit::ContentWorldOption::AllowAccessToClosedShadowRoots); }

    bool allowAutofill() const { return m_options.contains(WebKit::ContentWorldOption::AllowAutofill); }
    void setAllowAutofill(bool value) { m_options.add(WebKit::ContentWorldOption::AllowAutofill); }

    bool allowElementUserInfo() const { return m_options.contains(WebKit::ContentWorldOption::AllowElementUserInfo); }
    void setAllowElementUserInfo(bool value) { m_options.add(WebKit::ContentWorldOption::AllowElementUserInfo); }

    bool disableLegacyBuiltinOverrides() const { return m_options.contains(WebKit::ContentWorldOption::DisableLegacyBuiltinOverrides); }
    void setDisableLegacyBuiltinOverrides(bool value) { m_options.add(WebKit::ContentWorldOption::DisableLegacyBuiltinOverrides); }

    void addAssociatedUserContentControllerProxy(WebKit::WebUserContentControllerProxy&);
    void userContentControllerProxyDestroyed(WebKit::WebUserContentControllerProxy&);

private:
    explicit ContentWorld(const WTF::String&, OptionSet<WebKit::ContentWorldOption>);
    explicit ContentWorld(WebKit::ContentWorldIdentifier);

    WebKit::ContentWorldIdentifier m_identifier;
    WTF::String m_name;
    OptionSet<WebKit::ContentWorldOption> m_options;
    WeakHashSet<WebKit::WebUserContentControllerProxy> m_associatedContentControllerProxies;
};

} // namespace API
