/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "ValidationBubble.h"

#if PLATFORM(GTK)

#include "GtkVersioning.h"
#include <glib.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static const float horizontalMargin = 5;
static const float verticalMargin = 5;
static const float maxLabelWidthChars = 40;
static const double minFontSize = 11;

ValidationBubble::ValidationBubble(GtkWidget* webView, const String& message, const Settings& settings, ShouldNotifyFocusEventsCallback&& callback)
    : m_view(webView)
    , m_message(message)
    , m_fontSize(std::max(settings.minimumFontSize, minFontSize))
    , m_shouldNotifyFocusEventsCallback(WTFMove(callback))
{
    GtkWidget* label = gtk_label_new(nullptr);

    // https://docs.gtk.org/Pango/pango_markup.html
    GUniquePtr<gchar> markup(g_markup_printf_escaped("<span font='%f'>%s</span>", m_fontSize, message.utf8().data()));
    gtk_label_set_markup(GTK_LABEL(label), markup.get());

    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);

    gtk_widget_set_margin_top(label, verticalMargin);
    gtk_widget_set_margin_bottom(label, verticalMargin);
    gtk_widget_set_margin_start(label, horizontalMargin);
    gtk_widget_set_margin_end(label, horizontalMargin);

    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_lines(GTK_LABEL(label), 4);
    gtk_label_set_max_width_chars(GTK_LABEL(label), maxLabelWidthChars);

#if USE(GTK4)
    m_popover = gtk_popover_new();
    gtk_popover_set_autohide(GTK_POPOVER(m_popover), FALSE);
    gtk_popover_set_child(GTK_POPOVER(m_popover), label);

    gtk_widget_set_parent(m_popover, webView);
#else
    m_popover = gtk_popover_new(webView);
    gtk_popover_set_modal(GTK_POPOVER(m_popover), FALSE);
    gtk_popover_set_constrain_to(GTK_POPOVER(m_popover), GTK_POPOVER_CONSTRAINT_NONE);

    gtk_container_add(GTK_CONTAINER(m_popover), label);
    gtk_widget_show(label);
#endif
    gtk_popover_set_position(GTK_POPOVER(m_popover), GTK_POS_TOP);

    g_signal_connect_swapped(m_popover, "closed", G_CALLBACK(+[](ValidationBubble* validationBubble) {
        validationBubble->invalidate();
    }), this);
}

ValidationBubble::~ValidationBubble()
{
    invalidate();
}

void ValidationBubble::invalidate()
{
    if (!m_popover)
        return;

    g_signal_handlers_disconnect_by_data(m_popover, this);

#if USE(GTK4)
    g_clear_pointer(&m_popover, gtk_widget_unparent);
#else
    gtk_widget_destroy(m_popover);
    m_popover = nullptr;
#endif

    m_shouldNotifyFocusEventsCallback(m_view, true);
}

void ValidationBubble::showRelativeTo(const IntRect& anchorRect)
{
    m_shouldNotifyFocusEventsCallback(m_view, false);

    GdkRectangle rect(anchorRect);
    gtk_popover_set_pointing_to(GTK_POPOVER(m_popover), &rect);

    gtk_popover_popup(GTK_POPOVER(m_popover));
}

} // namespace WebCore

#endif // PLATFORM(GTK)
