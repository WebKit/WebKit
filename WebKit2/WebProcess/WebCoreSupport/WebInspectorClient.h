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

#ifndef WebInspectorClient_h
#define WebInspectorClient_h

#include <WebCore/InspectorClient.h>

namespace WebKit {

class WebPage;

class WebInspectorClient : public WebCore::InspectorClient {
public:
    WebInspectorClient(WebPage* page)
        : m_page(page)
    {
    }
    
private:
    virtual void inspectorDestroyed();
    
    virtual void openInspectorFrontend(WebCore::InspectorController*);

    virtual WebCore::Page* createPage();
    
    virtual String localizedStringsURL();
    
    virtual String hiddenPanels();
    
    virtual void showWindow();
    virtual void closeWindow();
    
    virtual void attachWindow();
    virtual void detachWindow();
    
    virtual void setAttachedWindowHeight(unsigned height);
    
    virtual void highlight(WebCore::Node*);
    virtual void hideHighlight();
    
    virtual void inspectedURLChanged(const String& newURL);

    virtual void populateSetting(const String& key, String* value);
    virtual void storeSetting(const String& key, const String& value);

    virtual bool sendMessageToFrontend(const String&);

    virtual void inspectorWindowObjectCleared();
    
    WebPage* m_page;
};

} // namespace WebKit

#endif // WebInspectorClient_h
