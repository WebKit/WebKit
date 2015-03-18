/*
 * Copyright (C) 2014 Ryuan Choi <ryuan.choi@gmail.com>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ewk_extension_private_h
#define ewk_extension_private_h

#include "ewk_page_private.h"
#include <wtf/Vector.h>

namespace WebKit {
class InjectedBundle;
class WebPage;
}

class EwkPage;

class EwkExtension {
public:
    explicit EwkExtension(WebKit::InjectedBundle*);
    ~EwkExtension() { }

    void append(Ewk_Extension_Client* client);
    void remove(Ewk_Extension_Client* client);

    WebKit::InjectedBundle* bundle() const { return m_bundle; }

    static void didCreatePage(WKBundleRef, WKBundlePageRef, const void*);
    static void willDestroyPage(WKBundleRef, WKBundlePageRef, const void*);
    static void didReceiveMessage(WKBundleRef, WKStringRef, WKTypeRef, const void* clientInfo);
    static void didReceiveMessageToPage(WKBundleRef, WKBundlePageRef, WKStringRef, WKTypeRef, const void*);

private:
    WebKit::InjectedBundle* m_bundle;

    Vector<Ewk_Extension_Client*> m_clients;
    HashMap<WebKit::WebPage*, std::unique_ptr<EwkPage>> m_pageMap;
};

#endif // ewk_extension_private_h
