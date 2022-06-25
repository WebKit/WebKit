/*
 * Copyright (C) 2020 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "SystemFontDatabase.h"

#include "PlatformScreen.h"
#include "WebKitFontFamilyNames.h"
#include <gtk/gtk.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

SystemFontDatabase& SystemFontDatabase::singleton()
{
    static NeverDestroyed<SystemFontDatabase> database = SystemFontDatabase();
    return database.get();
}

auto SystemFontDatabase::platformSystemFontShorthandInfo(FontShorthand) -> SystemFontShorthandInfo
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return { WebKitFontFamilyNames::standardFamily, 16, normalWeightValue() };

    // This will be a font selection string like "Sans 10" so we cannot use it as the family name.
    GUniqueOutPtr<gchar> fontName;
    g_object_get(settings, "gtk-font-name", &fontName.outPtr(), nullptr);
    if (!fontName || !fontName.get()[0])
        return { WebKitFontFamilyNames::standardFamily, 16, normalWeightValue() };

    PangoFontDescription* pangoDescription = pango_font_description_from_string(fontName.get());
    if (!pangoDescription)
        return { WebKitFontFamilyNames::standardFamily, 16, normalWeightValue() };

    int size = pango_font_description_get_size(pangoDescription) / PANGO_SCALE;
    // If the size of the font is in points, we need to convert it to pixels.
    if (!pango_font_description_get_size_is_absolute(pangoDescription))
        size = size * (screenDPI() / 72.0);

    SystemFontShorthandInfo result { AtomString::fromLatin1(pango_font_description_get_family(pangoDescription)), static_cast<float>(size), normalWeightValue() };
    pango_font_description_free(pangoDescription);
    return result;
}

void SystemFontDatabase::platformInvalidate()
{
}

} // namespace WebCore
