/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#include "WebContextMenuClient.h"

#include "WebView.h"
#include <WebCore/Editor.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/UserGestureIndicator.h>

WebContextMenuClient::WebContextMenuClient(WebView* webView)
    : m_webView(webView)
{
}

void WebContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

void WebContextMenuClient::downloadURL(const URL& url)
{
    m_webView->downloadURL(url);
}

void WebContextMenuClient::searchWithGoogle(const WebCore::Frame* frame)
{
    auto page = frame->page();
    if (!page)
        return;

    auto searchString = frame->editor().selectedText();
    searchString.stripWhiteSpace();
    searchString = encodeWithURLEscapeSequences(searchString);
    searchString.replace("%20"_s, "+"_s);
    auto searchURL = URL { { }, "https://www.google.com/search?q=" + searchString + "&ie=UTF-8&oe=UTF-8" };

    WebCore::UserGestureIndicator indicator { WebCore::ProcessingUserGesture };
    page->mainFrame().loader().changeLocation(searchURL, { }, nullptr, WebCore::ReferrerPolicy::EmptyString, WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow);
}

void WebContextMenuClient::lookUpInDictionary(WebCore::Frame*)
{
    notImplemented();
}

void WebContextMenuClient::speak(const String&)
{
    notImplemented();
}

void WebContextMenuClient::stopSpeaking()
{
    notImplemented();
}

bool WebContextMenuClient::isSpeaking()
{
    notImplemented();
    return false;
}
