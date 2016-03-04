/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WebIconDatabaseProxy_h
#define WebIconDatabaseProxy_h

#include "APIObject.h"
#include "MessageReceiver.h"
#include <WebCore/IconDatabaseBase.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebKit {

class WebProcess;

class WebIconDatabaseProxy : public WebCore::IconDatabaseBase, private IPC::MessageReceiver {
public:
    explicit WebIconDatabaseProxy(WebProcess*);
    virtual ~WebIconDatabaseProxy();

    bool isEnabled() const override;
    void setEnabled(bool) override;

    void retainIconForPageURL(const String&) override;
    void releaseIconForPageURL(const String&) override;
    void setIconURLForPageURL(const String&, const String&) override;
    void setIconDataForIconURL(PassRefPtr<WebCore::SharedBuffer>, const String&) override;

    String synchronousIconURLForPageURL(const String&) override;
    bool synchronousIconDataKnownForIconURL(const String&) override;
    WebCore::IconLoadDecision synchronousLoadDecisionForIconURL(const String&, WebCore::DocumentLoader*) override;
    WebCore::Image* synchronousIconForPageURL(const String&, const WebCore::IntSize&) override;
    
    // Asynchronous calls we should use to replace the above when supported.
    bool supportsAsynchronousMode() override;
    void loadDecisionForIconURL(const String&, PassRefPtr<WebCore::IconLoadDecisionCallback>) override;
    void receivedIconLoadDecision(int decision, uint64_t callbackID);
    void iconDataForIconURL(const String&, PassRefPtr<WebCore::IconDataCallback>) override;

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    
    // Callbacks from the UIProcess
    void urlImportFinished();

    bool m_isEnabled;
    WebProcess* m_process;
    
    HashMap<uint64_t, RefPtr<WebCore::IconLoadDecisionCallback>> m_iconLoadDecisionCallbacks;
};

} // namespace WebKit

#endif // WebIconDatabaseProxy_h
