/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef APIPageConfiguration_h
#define APIPageConfiguration_h

#include "APIObject.h"
#include <wtf/GetPtr.h>

namespace WebKit {
struct WebPageConfiguration;
class WebUserContentControllerProxy;
class WebProcessPool;
class WebPageGroup;
class WebPreferences;
class WebPageProxy;
}

namespace API {

class PageConfiguration : public ObjectImpl<Object::Type::PageConfiguration> {
public:
    static Ref<PageConfiguration> create()
    {
        return adoptRef(*new PageConfiguration);
    }
    virtual ~PageConfiguration();

    // FIXME: The configuration properties should return their default values
    // rather than nullptr.
    
    WebKit::WebProcessPool* processPool();
    void setProcessPool(WebKit::WebProcessPool*);

    WebKit::WebUserContentControllerProxy* userContentController();
    void setUserContentController(WebKit::WebUserContentControllerProxy*);

    WebKit::WebPageGroup* pageGroup();
    void setPageGroup(WebKit::WebPageGroup*);

    WebKit::WebPreferences* preferences();
    void setPreferences(WebKit::WebPreferences*);

    WebKit::WebPageProxy* relatedPage();
    void setRelatedPage(WebKit::WebPageProxy*);

    WebKit::WebPageConfiguration webPageConfiguration();

private:
    PageConfiguration();

    RefPtr<WebKit::WebProcessPool> m_processPool;
    RefPtr<WebKit::WebUserContentControllerProxy> m_userContentController;
    RefPtr<WebKit::WebPageGroup> m_pageGroup;
    RefPtr<WebKit::WebPreferences> m_preferences;
    RefPtr<WebKit::WebPageProxy> m_relatedPage;
};

} // namespace API


#endif // APIPageConfiguration_h
