/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebURLSchemeHandlerCocoa.h"

#import "WKFoundation.h"
#import "WKURLSchemeHandler.h"
#import "WKURLSchemeTaskInternal.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "WebURLSchemeTask.h"
#import <wtf/RunLoop.h>

namespace WebKit {

Ref<WebURLSchemeHandlerCocoa> WebURLSchemeHandlerCocoa::create(id<WKURLSchemeHandler> apiHandler)
{
    return adoptRef(*new WebURLSchemeHandlerCocoa(apiHandler));
}

WebURLSchemeHandlerCocoa::WebURLSchemeHandlerCocoa(id<WKURLSchemeHandler> apiHandler)
    : m_apiHandler(apiHandler)
{
}

void WebURLSchemeHandlerCocoa::platformStartTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    auto strongTask = retainPtr(wrapper(task));
    if (auto webView = page.cocoaView())
        [m_apiHandler.get() webView:webView.get() startURLSchemeTask:strongTask.get()];
}

void WebURLSchemeHandlerCocoa::platformStopTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    auto strongTask = retainPtr(wrapper(task));
    if (auto webView = page.cocoaView())
        [m_apiHandler.get() webView:webView.get() stopURLSchemeTask:strongTask.get()];
    else
        task.suppressTaskStoppedExceptions();
}

} // namespace WebKit
