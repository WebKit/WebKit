/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebColorPickerEfl.h"

#if ENABLE(INPUT_TYPE_COLOR)
#include "NotImplemented.h"
#include "ewk_color_picker_private.h"

namespace WebKit {

WebColorPickerEfl::WebColorPickerEfl(WebViewEfl* webView, WebColorPicker::Client* client, const WebCore::Color&)
    : WebColorPicker(client)
    , m_webView(webView)
{
    ASSERT(m_webView);
    m_colorPickerResultListener = WebColorPickerResultListenerProxy::create(this);
}

void WebColorPickerEfl::endPicker()
{
    m_webView->colorPickerClient().endPicker(m_webView);
}

void WebColorPickerEfl::showColorPicker(const WebCore::Color& color)
{
    m_webView->colorPickerClient().showColorPicker(m_webView, color.serialized(), toAPI(m_colorPickerResultListener.get()));
}

void WebColorPickerEfl::setSelectedColor(const WebCore::Color&)
{
    notImplemented();
}

void WebColorPickerEfl::didChooseColor(const WebCore::Color& color)
{
    if (!m_client)
        return;

    m_client->didChooseColor(color);
    m_client->didEndColorPicker();
}

} // namespace WebKit
#endif
