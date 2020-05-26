/*
 * Copyright (C) 2010 Igalia S.L.
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

#include "config.h"
#include "GdkCairoUtilities.h"

#include "CairoUniquePtr.h"
#include "CairoUtilities.h"
#include "IntSize.h"
#include <cairo.h>
#include <gtk/gtk.h>
#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

class SystemFontOptions {
public:
    static SystemFontOptions& singleton()
    {
        static LazyNeverDestroyed<SystemFontOptions> fontOptions;
        static std::once_flag flag;
        std::call_once(flag, [&] {
            fontOptions.construct();
        });
        return fontOptions;
    }

    SystemFontOptions()
        : m_settings(gtk_settings_get_default())
    {
        if (m_settings) {
            auto fontOptionsChangedCallback = +[](GtkSettings*, GParamSpec*, SystemFontOptions* systemFontOptions) {
                systemFontOptions->updateFontOptions();
            };
            g_signal_connect(m_settings, "notify::gtk-xft-antialias", G_CALLBACK(fontOptionsChangedCallback), this);
            g_signal_connect(m_settings, "notify::gtk-xft-hinting", G_CALLBACK(fontOptionsChangedCallback), this);
            g_signal_connect(m_settings, "notify::gtk-xft-hintstyle", G_CALLBACK(fontOptionsChangedCallback), this);
            g_signal_connect(m_settings, "notify::gtk-xft-rgba", G_CALLBACK(fontOptionsChangedCallback), this);
            updateFontOptions();
        } else
            m_fontOptions.reset(cairo_font_options_create());
    }

    const cairo_font_options_t* fontOptions() const
    {
        return m_fontOptions.get();
    }

private:
    void updateFontOptions()
    {
        m_fontOptions.reset(cairo_font_options_create());

        gint antialias, hinting;
        GUniqueOutPtr<char> hintStyleString;
        GUniqueOutPtr<char> rgbaString;
        g_object_get(m_settings, "gtk-xft-antialias", &antialias, "gtk-xft-hinting", &hinting, "gtk-xft-hintstyle",
            &hintStyleString.outPtr(), "gtk-xft-rgba", &rgbaString.outPtr(), nullptr);

        cairo_font_options_set_hint_metrics(m_fontOptions.get(), CAIRO_HINT_METRICS_ON);

        cairo_hint_style_t hintStyle = CAIRO_HINT_STYLE_DEFAULT;
        switch (hinting) {
        case 0:
            hintStyle = CAIRO_HINT_STYLE_NONE;
            break;
        case 1:
            if (hintStyleString) {
                if (!strcmp(hintStyleString.get(), "hintnone"))
                    hintStyle = CAIRO_HINT_STYLE_NONE;
                else if (!strcmp(hintStyleString.get(), "hintslight"))
                    hintStyle = CAIRO_HINT_STYLE_SLIGHT;
                else if (!strcmp(hintStyleString.get(), "hintmedium"))
                    hintStyle = CAIRO_HINT_STYLE_MEDIUM;
                else if (!strcmp(hintStyleString.get(), "hintfull"))
                    hintStyle = CAIRO_HINT_STYLE_FULL;
            }
            break;
        }
        cairo_font_options_set_hint_style(m_fontOptions.get(), hintStyle);

        cairo_subpixel_order_t subpixelOrder = CAIRO_SUBPIXEL_ORDER_DEFAULT;
        if (rgbaString) {
            if (!strcmp(rgbaString.get(), "rgb"))
                subpixelOrder = CAIRO_SUBPIXEL_ORDER_RGB;
            else if (!strcmp(rgbaString.get(), "bgr"))
                subpixelOrder = CAIRO_SUBPIXEL_ORDER_BGR;
            else if (!strcmp(rgbaString.get(), "vrgb"))
                subpixelOrder = CAIRO_SUBPIXEL_ORDER_VRGB;
            else if (!strcmp(rgbaString.get(), "vbgr"))
                subpixelOrder = CAIRO_SUBPIXEL_ORDER_VBGR;
        }
        cairo_font_options_set_subpixel_order(m_fontOptions.get(), subpixelOrder);

        cairo_antialias_t antialiasMode = CAIRO_ANTIALIAS_DEFAULT;
        switch (antialias) {
        case 0:
            antialiasMode = CAIRO_ANTIALIAS_NONE;
            break;
        case 1:
            if (subpixelOrder != CAIRO_SUBPIXEL_ORDER_DEFAULT)
                antialiasMode = CAIRO_ANTIALIAS_SUBPIXEL;
            else
                antialiasMode = CAIRO_ANTIALIAS_GRAY;
            break;
        }
        cairo_font_options_set_antialias(m_fontOptions.get(), antialiasMode);
    }

    GtkSettings* m_settings;
    CairoUniquePtr<cairo_font_options_t> m_fontOptions;
};

const cairo_font_options_t* getDefaultCairoFontOptions()
{
    return SystemFontOptions::singleton().fontOptions();
}

GdkPixbuf* cairoSurfaceToGdkPixbuf(cairo_surface_t* surface)
{
    IntSize size = cairoSurfaceSize(surface);
    return gdk_pixbuf_get_from_surface(surface, 0, 0, size.width(), size.height());
}

} // namespace WebCore
