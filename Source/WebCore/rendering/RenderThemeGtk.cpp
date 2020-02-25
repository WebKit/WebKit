/*
 * Copyright (C) 2020 Igalia S.L.
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
 *
 */

#include "config.h"
#include "RenderThemeGtk.h"

#include "CSSValueKeywords.h"
#include "FontDescription.h"
#include "GRefPtrGtk.h"
#include "PlatformScreen.h"
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

RenderTheme& RenderTheme::singleton()
{
    static NeverDestroyed<RenderThemeGtk> theme;
    return theme;
}

void RenderThemeGtk::updateCachedSystemFontDescription(CSSValueID, FontCascadeDescription& fontDescription) const
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return;

    // This will be a font selection string like "Sans 10" so we cannot use it as the family name.
    GUniqueOutPtr<gchar> fontName;
    g_object_get(settings, "gtk-font-name", &fontName.outPtr(), nullptr);
    if (!fontName || !fontName.get()[0])
        return;

    PangoFontDescription* pangoDescription = pango_font_description_from_string(fontName.get());
    if (!pangoDescription)
        return;

    fontDescription.setOneFamily(pango_font_description_get_family(pangoDescription));

    int size = pango_font_description_get_size(pangoDescription) / PANGO_SCALE;
    // If the size of the font is in points, we need to convert it to pixels.
    if (!pango_font_description_get_size_is_absolute(pangoDescription))
        size = size * (screenDPI() / 72.0);

    fontDescription.setSpecifiedSize(size);
    fontDescription.setIsAbsoluteSize(true);
    fontDescription.setWeight(normalWeightValue());
    fontDescription.setItalic(FontSelectionValue());
    pango_font_description_free(pangoDescription);
}

Seconds RenderThemeGtk::caretBlinkInterval() const
{
    gboolean shouldBlink;
    gint time;
    g_object_get(gtk_settings_get_default(), "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, nullptr);
    return shouldBlink ? 500_us * time : 0_s;
}

} // namespace WebCore
