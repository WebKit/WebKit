/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebInspector_h
#define WebInspector_h

#if ENABLE(INSPECTOR)

#include "APIObject.h"
#include "Connection.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebKit {

class WebPage;
struct WebPageCreationParameters;

class WebInspector : public APIObject {
public:
    static const Type APIType = TypeBundleInspector;

    static PassRefPtr<WebInspector> create(WebPage*);

    WebPage* page() const { return m_page; }
    WebPage* inspectorPage() const { return m_inspectorPage; }

    // Implemented in generated WebInspectorMessageReceiver.cpp
    void didReceiveWebInspectorMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    // Called by WebInspector messages
    void show();
    void close();

    void evaluateScriptForTest(long callID, const String& script);

    void startPageProfiling();
    void stopPageProfiling();
    
    bool canAttachWindow() const;
    void requestAttachWindow();

private:
    friend class WebInspectorClient;
    friend class WebInspectorFrontendClient;

    explicit WebInspector(WebPage*);

    virtual Type type() const { return APIType; }

    // Called from WebInspectorClient
    WebPage* createInspectorPage();

    // Called from WebInspectorFrontendClient
    void didLoadInspectorPage();
    void didClose();
    void bringToFront();
    void inspectedURLChanged(const String&);

    void attach();
    void detach();

    void setAttachedWindowHeight(unsigned);

    // Implemented in platform WebInspector file
    String localizedStringsURL() const;

    void showConsole();

    void startJavaScriptDebugging();
    void stopJavaScriptDebugging();

    void startJavaScriptProfiling();
    void stopJavaScriptProfiling();

    WebPage* m_page;
    WebPage* m_inspectorPage;
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR)

#endif // WebInspector_h
