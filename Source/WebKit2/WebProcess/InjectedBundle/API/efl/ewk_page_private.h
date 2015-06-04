/*
 * Copyright (C) 2015 Ryuan Choi <ryuan.choi@gmail.com>.  All rights reserved.
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

#ifndef ewk_page_private_h
#define ewk_page_private_h

typedef struct EwkPageClient Ewk_Page_Client;

namespace WebKit {
class WebPage;
}

class EwkPage {
public:
    explicit EwkPage(WebKit::WebPage*);

    WebKit::WebPage* page() const { return m_page; }

    void append(const Ewk_Page_Client*);
    void remove(const Ewk_Page_Client*);

private:
    static void didStartProvisionalLoadForFrame(WKBundlePageRef, WKBundleFrameRef, WKTypeRef*, const void *);
    static void didClearWindowObjectForFrame(WKBundlePageRef, WKBundleFrameRef, WKBundleScriptWorldRef, const void *);
    static void didFinishDocumentLoadForFrame(WKBundlePageRef, WKBundleFrameRef, WKTypeRef*, const void *);

    WebKit::WebPage* m_page;
    Vector<const Ewk_Page_Client*> m_clients;
};

#endif // ewk_page_private_h
