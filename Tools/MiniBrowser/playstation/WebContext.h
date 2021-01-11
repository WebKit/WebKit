/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include <WebKit/WKContext.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKRetainPtr.h>
#include <list>

class WebViewWindow;

class WebContext {
protected:
    WebContext();
public:
    ~WebContext();
    static std::shared_ptr<WebContext> singleton();

    WKContextRef context() { return m_context.get(); }
    WKPageGroupRef pageGroup() { return m_pageGroup.get(); }
    WKPreferencesRef preferences() { return m_preferencesMaster.get(); }
    WKWebsiteDataStoreRef websiteDataStore() { return m_websiteDataStore.get(); }

    void addWindow(WebViewWindow*);
    void removeWindow(WebViewWindow*);

private:
    static std::weak_ptr<WebContext> s_instance;
    WKRetainPtr<WKContextRef> m_context;
    WKRetainPtr<WKPageGroupRef> m_pageGroup;
    WKRetainPtr<WKPreferencesRef> m_preferencesMaster;
    WKRetainPtr<WKWebsiteDataStoreRef> m_websiteDataStore;
    std::list<WebViewWindow*> m_windows;
};
