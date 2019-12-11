/*
 * Copyright (C) 2015 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
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
#include "WebColorPickerGtk.h"

#if ENABLE(INPUT_TYPE_COLOR)

#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

namespace WebKit {
using namespace WebCore;

Ref<WebColorPickerGtk> WebColorPickerGtk::create(WebPageProxy& page, const Color& initialColor, const IntRect& rect)
{
    return adoptRef(*new WebColorPickerGtk(page, initialColor, rect));
}

WebColorPickerGtk::WebColorPickerGtk(WebPageProxy& page, const Color& initialColor, const IntRect&)
    : WebColorPicker(&page)
    , m_initialColor(initialColor)
    , m_webView(page.viewWidget())
    , m_colorChooser(nullptr)
{
}

WebColorPickerGtk::~WebColorPickerGtk()
{
    endPicker();
}

void WebColorPickerGtk::cancel()
{
    setSelectedColor(m_initialColor);
}

void WebColorPickerGtk::endPicker()
{
    if (!m_colorChooser)
        return;

    gtk_widget_destroy(m_colorChooser);
    m_colorChooser = nullptr;
}

void WebColorPickerGtk::didChooseColor(const Color& color)
{
    if (!m_client)
        return;

    m_client->didChooseColor(color);
}

void WebColorPickerGtk::colorChooserDialogRGBAChangedCallback(GtkColorChooser* colorChooser, GParamSpec*, WebColorPickerGtk* colorPicker)
{
    GdkRGBA rgba;
    gtk_color_chooser_get_rgba(colorChooser, &rgba);
    colorPicker->didChooseColor(rgba);
}

void WebColorPickerGtk::colorChooserDialogResponseCallback(GtkColorChooser*, int responseID, WebColorPickerGtk* colorPicker)
{
    if (responseID != GTK_RESPONSE_OK)
        colorPicker->cancel();
    colorPicker->endPicker();
}

void WebColorPickerGtk::showColorPicker(const Color& color)
{
    if (!m_client)
        return;

    m_initialColor = color;

    if (!m_colorChooser) {
        GtkWidget* toplevel = gtk_widget_get_toplevel(m_webView);
        m_colorChooser = gtk_color_chooser_dialog_new(_("Select Color"), WebCore::widgetIsOnscreenToplevelWindow(toplevel) ? GTK_WINDOW(toplevel) : nullptr);
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(m_colorChooser), &m_initialColor);
        g_signal_connect(m_colorChooser, "notify::rgba", G_CALLBACK(WebColorPickerGtk::colorChooserDialogRGBAChangedCallback), this);
        g_signal_connect(m_colorChooser, "response", G_CALLBACK(WebColorPickerGtk::colorChooserDialogResponseCallback), this);
    } else
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(m_colorChooser), &m_initialColor);

    gtk_widget_show(m_colorChooser);
}

} // namespace WebKit

#endif // ENABLE(INPUT_TYPE_COLOR)
