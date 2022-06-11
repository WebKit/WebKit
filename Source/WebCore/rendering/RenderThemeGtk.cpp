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
    static MainThreadNeverDestroyed<RenderThemeGtk> theme;
    return theme;
}

Seconds RenderThemeGtk::caretBlinkInterval() const
{
    gboolean shouldBlink;
    gint time;
    g_object_get(gtk_settings_get_default(), "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, nullptr);
    return shouldBlink ? 500_us * time : 0_s;
}

} // namespace WebCore
