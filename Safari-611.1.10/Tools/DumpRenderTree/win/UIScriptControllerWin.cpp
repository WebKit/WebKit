/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "UIScriptControllerWin.h"

#include "DumpRenderTreeWin.h"
#include "UIScriptContext.h"
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKit.h>

namespace WTR {

Ref<UIScriptController> UIScriptController::create(UIScriptContext& context)
{
    return adoptRef(*new UIScriptControllerWin(context));
}

void UIScriptControllerWin::doAsyncTask(JSValueRef callback)
{
    unsigned callbackID = m_context->prepareForAsyncTask(callback, CallbackTypeNonPersistent);

    callOnMainThread([this, protectedThis = makeRef(*this), callbackID ] {
        if (!m_context)
            return;
        m_context->asyncTaskComplete(callbackID);
    });
}

void UIScriptControllerWin::zoomToScale(double scale, JSValueRef callback)
{
    doAsyncTask(callback);

    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return;

    COMPtr<IWebViewPrivate2> webView2(Query, webView);
    if (!webView2)
        return;

    webView2->scaleWebView(scale, { 0, 0 });
}

double UIScriptControllerWin::zoomScale() const
{
    // No API to get the page scale factor
    return 1;
}

}
