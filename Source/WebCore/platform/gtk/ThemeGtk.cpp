/*
 * Copyright (C) 2020 Igalia S.L.
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

#if !USE(GTK4)

#include "ThemeGtk.h"

#include "GRefPtrGtk.h"
#include <gtk/gtk.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

Theme& Theme::singleton()
{
    static NeverDestroyed<ThemeGtk> theme;
    return theme;
}

void ThemeGtk::ensurePlatformColors() const
{
    if (m_activeSelectionBackgroundColor.isValid())
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN

    GRefPtr<GtkWidgetPath> entryPath = adoptGRef(gtk_widget_path_new());
    gtk_widget_path_append_type(entryPath.get(), G_TYPE_NONE);
    gtk_widget_path_iter_set_object_name(entryPath.get(), -1, "entry");
    GRefPtr<GtkStyleContext> entryContext = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(entryContext.get(), entryPath.get());

    GRefPtr<GtkWidgetPath> selectionPath = adoptGRef(gtk_widget_path_copy(gtk_style_context_get_path(entryContext.get())));
    gtk_widget_path_append_type(selectionPath.get(), G_TYPE_NONE);
    gtk_widget_path_iter_set_object_name(selectionPath.get(), -1, "selection");
    GRefPtr<GtkStyleContext> selectionContext = adoptGRef(gtk_style_context_new());
    gtk_style_context_set_path(selectionContext.get(), selectionPath.get());
    gtk_style_context_set_parent(selectionContext.get(), entryContext.get());

    gtk_style_context_set_state(selectionContext.get(), static_cast<GtkStateFlags>(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED));

    GdkRGBA color;
    gtk_style_context_get_background_color(selectionContext.get(), gtk_style_context_get_state(selectionContext.get()), &color);
    m_activeSelectionBackgroundColor = color;

    gtk_style_context_get_color(selectionContext.get(), gtk_style_context_get_state(selectionContext.get()), &color);
    m_ativeSelectionForegroundColor = color;

    gtk_style_context_set_state(selectionContext.get(), GTK_STATE_FLAG_SELECTED);

    gtk_style_context_get_background_color(selectionContext.get(), gtk_style_context_get_state(selectionContext.get()), &color);
    m_inactiveSelectionBackgroundColor = color;

    gtk_style_context_get_color(selectionContext.get(), gtk_style_context_get_state(selectionContext.get()), &color);
    m_inactiveSelectionForegroundColor = color;

    ALLOW_DEPRECATED_DECLARATIONS_END
}

void ThemeGtk::platformColorsDidChange()
{
    m_activeSelectionBackgroundColor = { };
}

Color ThemeGtk::activeSelectionForegroundColor() const
{
    ensurePlatformColors();
    return m_ativeSelectionForegroundColor;
}

Color ThemeGtk::activeSelectionBackgroundColor() const
{
    ensurePlatformColors();
    return m_activeSelectionBackgroundColor;
}

Color ThemeGtk::inactiveSelectionForegroundColor() const
{
    ensurePlatformColors();
    return m_inactiveSelectionForegroundColor;
}

Color ThemeGtk::inactiveSelectionBackgroundColor() const
{
    ensurePlatformColors();
    return m_inactiveSelectionBackgroundColor;
}

} // namespace WebCore

#endif // !USE(GTK4)
