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

#ifndef WebPageNamespace_h
#define WebPageNamespace_h

#include "APIObject.h"
#include "WebContext.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

struct WKContextStatistics;

namespace WebKit {

class WebContext;

class WebPageNamespace : public APIObject {
public:
    static const Type APIType = TypePageNamespace;

    static PassRefPtr<WebPageNamespace> create(WebContext* context)
    {
        return adoptRef(new WebPageNamespace(context));
    }

    ~WebPageNamespace();

    WebPageProxy* createWebPage();    

    WebContext* context() const { return m_context.get(); }
    WebProcessProxy* process() const { return m_context->process(); }

    void preferencesDidChange();

    void getStatistics(WKContextStatistics*);

private:
    WebPageNamespace(WebContext*);

    virtual Type type() const { return APIType; }

    RefPtr<WebContext> m_context;
};

} // namespace WebKit

#endif // WebPageNamespace_h
