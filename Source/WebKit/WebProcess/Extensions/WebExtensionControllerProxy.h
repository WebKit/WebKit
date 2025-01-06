/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "MessageReceiver.h"
#include "WebExtensionContextProxy.h"
#include "WebExtensionControllerParameters.h"
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URLHash.h>

namespace WebCore {
class DOMWrapperWorld;
}

namespace WebKit {

class WebFrame;
class WebPage;

class WebExtensionControllerProxy final : public RefCounted<WebExtensionControllerProxy>, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionControllerProxy);
    WTF_MAKE_NONCOPYABLE(WebExtensionControllerProxy);

public:
    static RefPtr<WebExtensionControllerProxy> get(WebExtensionControllerIdentifier);
    static Ref<WebExtensionControllerProxy> getOrCreate(const WebExtensionControllerParameters&, WebPage* = nullptr);

    ~WebExtensionControllerProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    using WebExtensionContextProxySet = HashSet<Ref<WebExtensionContextProxy>>;
    using WebExtensionContextProxyBaseURLMap = HashMap<String, Ref<WebExtensionContextProxy>>;

    WebExtensionControllerIdentifier identifier() { return m_identifier; }

    bool operator==(const WebExtensionControllerProxy& other) const { return (this == &other); }

    bool inTestingMode() { return m_testingMode; }

    void globalObjectIsAvailableForFrame(WebPage&, WebFrame&, WebCore::DOMWrapperWorld&);
    void serviceWorkerGlobalObjectIsAvailableForFrame(WebPage&, WebFrame&, WebCore::DOMWrapperWorld&);
    void addBindingsToWebPageFrameIfNecessary(WebFrame&, WebCore::DOMWrapperWorld&);

    void didStartProvisionalLoadForFrame(WebPage&, WebFrame&, const URL&);
    void didCommitLoadForFrame(WebPage&, WebFrame&, const URL&);
    void didFinishLoadForFrame(WebPage&, WebFrame&, const URL&);
    // FIXME: Include the error here.
    void didFailLoadForFrame(WebPage&, WebFrame&, const URL&);

    RefPtr<WebExtensionContextProxy> extensionContext(const String& uniqueIdentifier) const;
    RefPtr<WebExtensionContextProxy> extensionContext(const URL&) const;
    RefPtr<WebExtensionContextProxy> extensionContext(WebFrame&, WebCore::DOMWrapperWorld&) const;

    bool hasLoadedContexts() const { return !m_extensionContexts.isEmpty(); }
    const WebExtensionContextProxySet& extensionContexts() const { return m_extensionContexts; }

private:
    explicit WebExtensionControllerProxy(const WebExtensionControllerParameters&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void load(const WebExtensionContextParameters&);
    void unload(WebExtensionContextIdentifier);

    WebExtensionControllerIdentifier m_identifier;
    bool m_testingMode { false };

    WebExtensionContextProxySet m_extensionContexts;
    WebExtensionContextProxyBaseURLMap m_extensionContextBaseURLMap;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
