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

#include "APIObject.h"
#include "APIPageConfiguration.h"
#include "MessageReceiver.h"
#include "WebExtensionContext.h"
#include "WebExtensionContextIdentifier.h"
#include "WebExtensionControllerIdentifier.h"
#include "WebExtensionURLSchemeHandler.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/Forward.h>
#include <wtf/URLHash.h>
#include <wtf/WeakHashSet.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSError;
OBJC_CLASS _WKWebExtensionController;
#endif

namespace WebKit {

class WebExtensionContext;
class WebPageProxy;
class WebProcessPool;
struct WebExtensionControllerParameters;

class WebExtensionController : public API::ObjectImpl<API::Object::Type::WebExtensionController>, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebExtensionController);

public:
    static Ref<WebExtensionController> create() { return adoptRef(*new WebExtensionController); }
    static WebExtensionController* get(WebExtensionControllerIdentifier);

    explicit WebExtensionController();
    ~WebExtensionController();

    using WebExtensionContextSet = HashSet<Ref<WebExtensionContext>>;
    using WebExtensionSet = HashSet<Ref<WebExtension>>;
    using WebExtensionContextBaseURLMap = HashMap<URL, Ref<WebExtensionContext>>;

    WebExtensionControllerIdentifier identifier() const { return m_identifier; }
    WebExtensionControllerParameters parameters() const;

    bool operator==(const WebExtensionController& other) const { return (this == &other); }
    bool operator!=(const WebExtensionController& other) const { return !(this == &other); }

#if PLATFORM(COCOA)
    bool load(WebExtensionContext&, NSError ** = nullptr);
    bool unload(WebExtensionContext&, NSError ** = nullptr);

    void unloadAll();

    void addPage(WebPageProxy&);
    void removePage(WebPageProxy&);

    RefPtr<WebExtensionContext> extensionContext(const WebExtension&) const;
    RefPtr<WebExtensionContext> extensionContext(const URL&) const;

    const WebExtensionContextSet& extensionContexts() const { return m_extensionContexts; }
    WebExtensionSet extensions() const;

    template<typename T, typename U>
    void sendToAllProcesses(const T& message, ObjectIdentifier<U> destinationID);

#ifdef __OBJC__
    _WKWebExtensionController *wrapper() const { return (_WKWebExtensionController *)API::ObjectImpl<API::Object::Type::WebExtensionController>::wrapper(); }
#endif
#endif

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    WebExtensionControllerIdentifier m_identifier;

#if PLATFORM(COCOA)
    WebExtensionContextSet m_extensionContexts;
    WebExtensionContextBaseURLMap m_extensionContextBaseURLMap;
    WeakHashSet<WebPageProxy> m_pages;
    WeakHashSet<WebProcessPool> m_processPools;
    HashMap<String, Ref<WebExtensionURLSchemeHandler>> m_registeredSchemeHandlers;
#endif
};

template<typename T, typename U>
void WebExtensionController::sendToAllProcesses(const T& message, ObjectIdentifier<U> destinationID)
{
    HashSet<WebProcessProxy*> seenProcesses;
    seenProcesses.reserveInitialCapacity(m_pages.capacity());

    for (auto& page : m_pages) {
        auto& process = page.process();
        if (seenProcesses.contains(&process))
            continue;

        seenProcesses.add(&process);

        if (process.canSendMessage())
            process.send(T(message), destinationID);
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
