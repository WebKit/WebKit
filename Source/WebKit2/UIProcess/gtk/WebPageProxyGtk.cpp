/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include "WebPageProxy.h"

#include "NativeWebKeyboardEvent.h"
#include "NotImplemented.h"
#include "PageClientImpl.h"

namespace WebKit {

GtkWidget* WebPageProxy::viewWidget()
{
    return static_cast<PageClientImpl*>(m_pageClient)->viewWidget();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    // FIXME: This should not be hard coded.
    return "Mozilla/5.0 (X11; Linux i686) AppleWebKit/534.7 (KHTML, like Gecko) Version/5.0 Safari/534.7";
}

void WebPageProxy::getEditorCommandsForKeyEvent(const AtomicString& eventType, Vector<WTF::String>& commandsList)
{
    m_pageClient->getEditorCommandsForKeyEvent(m_keyEventQueue.first(), eventType, commandsList);
}

void WebPageProxy::bindAccessibilityTree(const String& plugID)
{
    m_accessibilityPlugID = plugID;
}

void WebPageProxy::saveRecentSearches(const String&, const Vector<String>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(const String&, Vector<String>&)
{
    notImplemented();
}

} // namespace WebKit
