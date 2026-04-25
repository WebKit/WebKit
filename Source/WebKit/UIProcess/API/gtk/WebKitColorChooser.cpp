/*
 * Copyright (C) 2015 Igalia S.L.
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

#include "GtkUtilities.h"
#include "WebKitColorChooserRequestPrivate.h"
#include "WebKitWebViewPrivate.h"
#include <WebCore/Color.h>
#include <WebCore/IntRect.h>

namespace WebKit {
using namespace WebCore;

Ref<WebKitColorChooser> WebKitColorChooser::create(WebPageProxy& page, const WebCore::Color& initialColor, const WebCore::IntRect& rect, std::optional<WebCore::FrameIdentifier> frameID)
{
    return adoptRef(*new WebKitColorChooser(page, initialColor, rect, frameID));
}

WebKitColorChooser::WebKitColorChooser(WebPageProxy& page, const Color& initialColor, const IntRect& rect, std::optional<WebCore::FrameIdentifier> frameID)
    : WebColorPickerGtk(page, initialColor, rect, frameID)
    , m_elementRect(rect)
{
}

WebKitColorChooser::~WebKitColorChooser()
{
    endPicker();
}

void WebKitColorChooser::endPicker()
{
    if (!m_request) {
        WebColorPickerGtk::endPicker();
        return;
    }

    webkit_color_chooser_request_finish(m_request.get());
}

void WebKitColorChooser::colorChooserRequestFinished(WebKitColorChooserRequest*, WebKitColorChooser* colorChooser)
{
    colorChooser->m_request = nullptr;
}

void WebKitColorChooser::colorChooserRequestRGBAChanged(WebKitColorChooserRequest* request, GParamSpec*, WebKitColorChooser* colorChooser)
{
    GdkRGBA rgba;
    webkit_color_chooser_request_get_rgba(request, &rgba);
    colorChooser->didChooseColor(gdkRGBAToColor(rgba));
}

void WebKitColorChooser::showColorPicker(const Color& color, const IntRect& rect)
{
    m_initialColor = colorToGdkRGBA(color);
    GRefPtr<WebKitColorChooserRequest> request = adoptGRef(webkitColorChooserRequestCreate(this));
    g_signal_connect(request.get(), "notify::rgba", G_CALLBACK(WebKitColorChooser::colorChooserRequestRGBAChanged), this);
    g_signal_connect(request.get(), "finished", G_CALLBACK(WebKitColorChooser::colorChooserRequestFinished), this);

    if (webkitWebViewEmitRunColorChooser(WEBKIT_WEB_VIEW(m_webView), request.get()))
        m_request = request;
    else
        WebColorPickerGtk::showColorPicker(color, rect);
}

} // namespace WebKit
