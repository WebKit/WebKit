/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitColorChooser.h"

#include "APIViewClient.h"
#include "WPEWebView.h"
#include "WebKitColorChooserRequestPrivate.h"
#include "WebKitWebViewClient.h"
#include "WebPageProxy.h"

namespace WebKit {
using namespace WebCore;

Ref<WebKitColorChooser> WebKitColorChooser::create(WKWPE::View& view, WebPageProxy& page, std::optional<WebCore::FrameIdentifier> frameID)
{
    ASSERT(view.client().isGLibBasedAPI());
    return adoptRef(*new WebKitColorChooser(view, page, frameID));
}

WebKitColorChooser::WebKitColorChooser(WKWPE::View& view, WebPageProxy& page, std::optional<WebCore::FrameIdentifier> frameID)
    : WebColorPicker(&page.colorPickerClient(), frameID)
    , m_view(view)
{
}

WebKitColorChooser::~WebKitColorChooser()
{
    endPicker();
}

void WebKitColorChooser::endPicker()
{
    if (m_request) {
        GRefPtr request = m_request;
        webkit_color_chooser_request_finish(request.get());
        return;
    }

    WebColorPicker::endPicker();
}

void WebKitColorChooser::colorChooserRequestFinished(WebKitColorChooserRequest*, WebKitColorChooser* colorChooser)
{
    colorChooser->m_request = nullptr;
    protect(colorChooser)->WebColorPicker::endPicker();
}

void WebKitColorChooser::showColorPicker(const Color& color, const IntRect& rect)
{
    GRefPtr<WebKitColorChooserRequest> request = adoptGRef(webkitColorChooserRequestCreate(*this, color, rect));
    g_signal_connect(request.get(), "finished", G_CALLBACK(WebKitColorChooser::colorChooserRequestFinished), this);
    m_request = request;

    if (static_cast<WebKitWebViewClient&>(m_view.client()).runColorChooser(request.get()))
        return;

    // There is no default color chooser in WPE, so the request is finished right away keeping the initial color.
    webkit_color_chooser_request_finish(request.get());
}

} // namespace WebKit
