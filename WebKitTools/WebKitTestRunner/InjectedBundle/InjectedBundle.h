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

#ifndef InjectedBundle_h
#define InjectedBundle_h

#include "EventSendingController.h"
#include "GCController.h"
#include "LayoutTestController.h"
#include <WebKit2/WKBase.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

#include <sstream>

namespace WTR {

class InjectedBundlePage;

class InjectedBundle {
public:
    static InjectedBundle& shared();

    // Initialize the InjectedBundle.
    void initialize(WKBundleRef);

    WKBundleRef bundle() const { return m_bundle; }

    LayoutTestController* layoutTestController() { return m_layoutTestController.get(); }
    GCController* gcController() { return m_gcController.get(); }
    EventSendingController* eventSendingController() { return m_eventSendingController.get(); }

    InjectedBundlePage* page() { return m_mainPage.get(); }
    size_t pageCount() { return !!m_mainPage + m_otherPages.size(); }
    void closeOtherPages();

    void done();
    std::ostringstream& os() { return m_outputStream; }

    bool isTestRunning() { return m_state == Testing; }

private:
    InjectedBundle();
    ~InjectedBundle();

    static void _didCreatePage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo);
    static void _willDestroyPage(WKBundleRef bundle, WKBundlePageRef page, const void* clientInfo);
    static void _didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody, const void *clientInfo);

    void didCreatePage(WKBundlePageRef page);
    void willDestroyPage(WKBundlePageRef page);
    void didReceiveMessage(WKStringRef messageName, WKTypeRef messageBody);

    void beginTesting();

    WKBundleRef m_bundle;
    HashMap<WKBundlePageRef, InjectedBundlePage*> m_otherPages;
    OwnPtr<InjectedBundlePage> m_mainPage;

    RefPtr<LayoutTestController> m_layoutTestController;
    RefPtr<GCController> m_gcController;
    RefPtr<EventSendingController> m_eventSendingController;

    std::ostringstream m_outputStream;
    
    enum State {
        Idle,
        Testing
    };
    State m_state;
};

} // namespace WTR

#endif // InjectedBundle_h
