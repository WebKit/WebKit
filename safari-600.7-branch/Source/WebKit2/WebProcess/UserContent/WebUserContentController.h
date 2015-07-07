/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WebUserContentController_h
#define WebUserContentController_h

#include "MessageReceiver.h"
#include "WebScriptMessageHandler.h"
#include <WebCore/UserContentController.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebUserMessageHandlerDescriptorProxy;

class WebUserContentController final : public RefCounted<WebUserContentController>, private IPC::MessageReceiver  {
public:
    static PassRefPtr<WebUserContentController> getOrCreate(uint64_t identifier);
    virtual ~WebUserContentController();

    WebCore::UserContentController& userContentController() { return m_userContentController.get(); }

    uint64_t identifier() { return m_identifier; } 

private:
    explicit WebUserContentController(uint64_t identifier);

    // IPC::MessageReceiver.
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;

    void addUserScripts(const Vector<WebCore::UserScript>& userScripts);
    void removeAllUserScripts();

    void addUserStyleSheets(const Vector<WebCore::UserStyleSheet>& userStyleSheets);
    void removeAllUserStyleSheets();

    void addUserScriptMessageHandlers(const Vector<WebScriptMessageHandlerHandle>& scriptMessageHandlers);
    void removeUserScriptMessageHandler(uint64_t);

    uint64_t m_identifier;
    Ref<WebCore::UserContentController> m_userContentController;
#if ENABLE(USER_MESSAGE_HANDLERS)
    HashMap<uint64_t, RefPtr<WebUserMessageHandlerDescriptorProxy>> m_userMessageHandlerDescriptors;
#endif
};

} // namespace WebKit

#endif // WebUserContentController_h
