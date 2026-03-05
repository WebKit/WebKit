/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "ValidationBubbleGtk.h"

#include "GtkVersioning.h"
#include "WebKitWebViewBasePrivate.h"
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CStringView.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

ValidationBubbleGtk::ValidationBubbleGtk(GtkWidget* webView, String&& message, const WebCore::ValidationBubble::Settings& settings)
    : m_webView(webView)
{
    m_message = WTF::move(message);
    static constexpr double minFontSize = 11;
    m_fontSize = std::max(settings.minimumFontSize, minFontSize);

    GtkWidget* label = gtk_label_new(nullptr);

    // https://docs.gtk.org/Pango/pango_markup.html
    auto messageUTF8 = m_message.utf8();
    GUniquePtr<char> escapedMessage(g_markup_escape_text(messageUTF8.data(), messageUTF8.length()));
    String markup = makeString("<span font='"_s, m_fontSize, "'>"_s, CStringView::unsafeFromUTF8(escapedMessage.get()).span(), "</span>"_s);
    gtk_label_set_markup(GTK_LABEL(label), markup.utf8().data());

    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);

    static constexpr int margin = 5;
    gtk_widget_set_margin_top(label, margin);
    gtk_widget_set_margin_bottom(label, margin);
    gtk_widget_set_margin_start(label, margin);
    gtk_widget_set_margin_end(label, margin);

    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    static constexpr int labelLines = 4;
    gtk_label_set_lines(GTK_LABEL(label), labelLines);
    static constexpr int maxLabelWidthChars = 40;
    gtk_label_set_max_width_chars(GTK_LABEL(label), maxLabelWidthChars);

#if USE(GTK4)
    m_popover = gtk_popover_new();
    gtk_popover_set_autohide(GTK_POPOVER(m_popover), FALSE);
    gtk_popover_set_child(GTK_POPOVER(m_popover), label);

    gtk_widget_set_parent(m_popover, m_webView.get());
#else
    m_popover = gtk_popover_new(m_webView.get());
    gtk_popover_set_modal(GTK_POPOVER(m_popover), FALSE);
    gtk_popover_set_constrain_to(GTK_POPOVER(m_popover), GTK_POPOVER_CONSTRAINT_NONE);

    gtk_container_add(GTK_CONTAINER(m_popover), label);
    gtk_widget_show(label);
#endif
    gtk_popover_set_position(GTK_POPOVER(m_popover), GTK_POS_TOP);

    g_signal_connect_swapped(m_popover, "closed", G_CALLBACK(+[](ValidationBubbleGtk* validationBubble) {
        validationBubble->invalidate();
    }), this);
}

ValidationBubbleGtk::~ValidationBubbleGtk()
{
    invalidate();
}

void ValidationBubbleGtk::invalidate()
{
    if (!m_popover)
        return;

    g_signal_handlers_disconnect_by_data(m_popover, this);

#if USE(GTK4)
    g_clear_pointer(&m_popover, gtk_widget_unparent);
#else
    g_clear_pointer(&m_popover, gtk_widget_destroy);
#endif

    if (m_webView)
        webkitWebViewBaseSetShouldNotifyFocusEvents(WEBKIT_WEB_VIEW_BASE(m_webView.get()), true);
}

void ValidationBubbleGtk::showRelativeTo(const WebCore::IntRect& anchorRect)
{
    if (!m_webView)
        return;

    webkitWebViewBaseSetShouldNotifyFocusEvents(WEBKIT_WEB_VIEW_BASE(m_webView.get()), false);

    GdkRectangle rect(anchorRect);
    gtk_popover_set_pointing_to(GTK_POPOVER(m_popover), &rect);
    gtk_popover_popup(GTK_POPOVER(m_popover));
}

} // namespace WebKit
