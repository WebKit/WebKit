/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "WebColorChooser.h"

#if ENABLE(INPUT_TYPE_COLOR)

#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/ColorChooserClient.h>

namespace WebKit {
using namespace WebCore;

WebColorChooser::WebColorChooser(WebPage* page, ColorChooserClient* client, const Color& initialColor)
    : m_colorChooserClient(client)
    , m_page(page)
{
    m_page->setActiveColorChooser(this);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::ShowColorPicker(initialColor, client->elementRectRelativeToRootView(), client->suggestedColors()), m_page->pageID());
}

WebColorChooser::~WebColorChooser()
{
    if (!m_page)
        return;

    m_page->setActiveColorChooser(0);
}

void WebColorChooser::didChooseColor(const Color& color)
{
    m_colorChooserClient->didChooseColor(color);
}

void WebColorChooser::didEndChooser()
{
    m_colorChooserClient->didEndChooser();
}

void WebColorChooser::disconnectFromPage()
{
    m_page = 0;
}

void WebColorChooser::reattachColorChooser(const Color& color)
{
    ASSERT(m_page);
    m_page->setActiveColorChooser(this);

    ASSERT(m_colorChooserClient);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::ShowColorPicker(color, m_colorChooserClient->elementRectRelativeToRootView(), m_colorChooserClient->suggestedColors()), m_page->pageID());
}

void WebColorChooser::setSelectedColor(const Color& color)
{
    if (!m_page)
        return;
    
    if (m_page->activeColorChooser() != this)
        return;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::SetColorPickerColor(color), m_page->pageID());
}

void WebColorChooser::endChooser()
{
    if (!m_page)
        return;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::EndColorPicker(), m_page->pageID());
}

} // namespace WebKit

#endif // ENABLE(INPUT_TYPE_COLOR)
