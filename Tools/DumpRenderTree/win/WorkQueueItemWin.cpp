/*
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkQueueItem.h"

#include <comutil.h>
#include "DumpRenderTree.h"
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <JavaScriptCore/JSStringRefCF.h>
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKit.h>
#include <WebKitLegacy/WebKitCOMAPI.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <string>

using std::wstring;

bool LoadItem::invoke() const
{
    _bstr_t targetBSTR(JSStringCopyBSTR(m_target.get()), false);

    COMPtr<IWebFrame> targetFrame;
    if (!targetBSTR.length())
        targetFrame = frame;
    else if (FAILED(frame->findFrameNamed(targetBSTR, &targetFrame)))
        return false;

    COMPtr<IWebURLRequest> request;
    if (FAILED(WebKitCreateInstance(CLSID_WebURLRequest, 0, IID_IWebURLRequest, (void**)&request)))
        return false;

    _bstr_t urlBSTR(JSStringCopyBSTR(m_url.get()), false);
    if (FAILED(request->initWithURL(urlBSTR, WebURLRequestUseProtocolCachePolicy, 60)))
        return false;

    targetFrame->loadRequest(request.get());
    return true;
}

bool LoadHTMLStringItem::invoke() const
{
    _bstr_t contentBSTR(JSStringCopyBSTR(m_content.get()), false);
    _bstr_t baseURLBSTR(JSStringCopyBSTR(m_baseURL.get()), false);

    if (m_unreachableURL) {
        _bstr_t unreachableURLBSTR(JSStringCopyBSTR(m_unreachableURL.get()), false);
        frame->loadAlternateHTMLString(contentBSTR, baseURLBSTR, unreachableURLBSTR);
        return true;
    }

    frame->loadHTMLString(contentBSTR, baseURLBSTR);

    return true;
}

bool ReloadItem::invoke() const
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return false;

    COMPtr<IWebIBActions> webActions;
    if (FAILED(webView->QueryInterface(&webActions)))
        return false;

    webActions->reload(0);
    return true;
}

bool ScriptItem::invoke() const
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return false;

    _bstr_t scriptBSTR(JSStringCopyBSTR(m_script.get()), false);

    _bstr_t result;
    webView->stringByEvaluatingJavaScriptFromString(scriptBSTR, &result.GetBSTR());

    return true;
}

bool BackForwardItem::invoke() const
{
    COMPtr<IWebView> webView;
    if (FAILED(frame->webView(&webView)))
        return false;

    BOOL result;
    if (m_howFar == 1) {
        webView->goForward(&result);
        return true;
    }

    if (m_howFar == -1) {
        webView->goBack(&result);
        return true;
    }
    
    COMPtr<IWebBackForwardList> bfList;
    if (FAILED(webView->backForwardList(&bfList)))
        return false;

    COMPtr<IWebHistoryItem> item;
    if (FAILED(bfList->itemAtIndex(m_howFar, &item)))
        return false;

    webView->goToBackForwardItem(item.get(), &result);
    return true;
}
