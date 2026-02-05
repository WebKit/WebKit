/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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

#include "ColorControlSupportsAlpha.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/ColorChooserClient.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebColorChooser);

WebColorChooser::WebColorChooser(WebPage* page, ColorChooserClient* client, const Color& initialColor)
    : m_colorChooserClient(client)
    , m_page(page)
{
    page->setActiveColorChooser(this);
    auto supportsAlpha = client->supportsAlpha() ? ColorControlSupportsAlpha::Yes : ColorControlSupportsAlpha::No;

    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPageProxy::ShowColorPicker(initialColor, client->elementRectRelativeToRootView(), supportsAlpha, client->suggestedColors(), client->rootFrameID()), page->identifier());
}

WebColorChooser::~WebColorChooser()
{
    if (RefPtr page = m_page.get())
        page->setActiveColorChooser(nullptr);
}

void WebColorChooser::didChooseColor(const Color& color)
{
    if (RefPtr colorChooserClient = m_colorChooserClient.get())
        colorChooserClient->didChooseColor(color);
}

void WebColorChooser::didEndChooser()
{
    if (RefPtr colorChooserClient = m_colorChooserClient.get())
        colorChooserClient->didEndChooser();
}

void WebColorChooser::disconnectFromPage()
{
    m_page = nullptr;
}

void WebColorChooser::reattachColorChooser(const Color& color)
{
    Ref page = *m_page;
    page->setActiveColorChooser(this);

    Ref colorChooserClient = *m_colorChooserClient;
    auto supportsAlpha = colorChooserClient->supportsAlpha() ? ColorControlSupportsAlpha::Yes : ColorControlSupportsAlpha::No;
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPageProxy::ShowColorPicker(color, colorChooserClient->elementRectRelativeToRootView(), supportsAlpha, colorChooserClient->suggestedColors(), colorChooserClient->rootFrameID()), page->identifier());
}

void WebColorChooser::setSelectedColor(const Color& color)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    
    if (page->activeColorChooser() != this)
        return;

    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPageProxy::SetColorPickerColor(color), page->identifier());
}

void WebColorChooser::endChooser()
{
    if (!m_page)
        return;

    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebPageProxy::EndColorPicker(), m_page->identifier());
}

} // namespace WebKit
