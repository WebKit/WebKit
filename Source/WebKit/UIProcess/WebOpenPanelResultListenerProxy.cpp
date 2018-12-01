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

#include "config.h"
#include "WebOpenPanelResultListenerProxy.h"

#include "APIArray.h"
#include "APIString.h"
#include "WebPageProxy.h"
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace WebKit {
using namespace WebCore;

WebOpenPanelResultListenerProxy::WebOpenPanelResultListenerProxy(WebPageProxy* page)
    : m_page(page)
{
}

WebOpenPanelResultListenerProxy::~WebOpenPanelResultListenerProxy()
{
}

#if PLATFORM(IOS_FAMILY)
void WebOpenPanelResultListenerProxy::chooseFiles(const Vector<WTF::String>& filenames, const String& displayString, const API::Data* iconImageData)
{
    if (!m_page)
        return;

    m_page->didChooseFilesForOpenPanelWithDisplayStringAndIcon(filenames, displayString, iconImageData);
}
#endif

void WebOpenPanelResultListenerProxy::chooseFiles(const Vector<String>& filenames)
{
    if (!m_page)
        return;

    m_page->didChooseFilesForOpenPanel(filenames);
}

void WebOpenPanelResultListenerProxy::cancel()
{
    if (!m_page)
        return;

    m_page->didCancelForOpenPanel();
}

void WebOpenPanelResultListenerProxy::invalidate()
{
    m_page = nullptr;
}

} // namespace WebKit
