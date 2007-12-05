/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Pasteboard.h"

#include "CString.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "Image.h"
#include "RenderImage.h"
#include "KURL.h"
#include "markup.h"

#include <gtk/gtk.h>

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

Pasteboard::Pasteboard()
{
    notImplemented();
}

Pasteboard::~Pasteboard()
{
    notImplemented();
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    String text = frame->selectedText();
    gtk_clipboard_set_text(clipboard, text.utf8().data(), text.utf8().length());
}

void Pasteboard::writeURL(const KURL&, const String&, Frame*)
{
    notImplemented();
}

void Pasteboard::writeImage(Node* node, const KURL&, const String&)
{
    // TODO: Enable this when Image gets GdkPixbuf support

    /*
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    ASSERT(node && node->renderer() && node->renderer()->isImage());
    RenderImage* renderer = static_cast<RenderImage*>(node->renderer());
    CachedImage* cachedImage = static_cast<CachedImage*>(renderer->cachedImage());
    ASSERT(cachedImage);
    Image* image = cachedImage->image();
    ASSERT(image);

    gtk_clipboard_set_image(clipboard, image->pixbuf());
    */

    notImplemented();
}

void Pasteboard::clear()
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    gtk_clipboard_clear(clipboard);
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
#if GLIB_CHECK_VERSION(2,10,0)
    GdkAtom textHtml = gdk_atom_intern_static_string("text/html");
#else
    GdkAtom textHtml = gdk_atom_intern("text/html", false);
#endif
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
    chosePlainText = false;

    if (GtkSelectionData* data = gtk_clipboard_wait_for_contents(clipboard, textHtml)) {
        ASSERT(data->data);
        String html = String::fromUTF8(reinterpret_cast<gchar*>(data->data), data->length * data->format / 8);
        gtk_selection_data_free(data);

        if (!html.isEmpty()) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), html, "");
            if (fragment)
                return fragment.release();
        }
    }

    if (!allowPlainText)
        return 0;

    if (gchar* utf8 = gtk_clipboard_wait_for_text(clipboard)) {
        String text = String::fromUTF8(utf8);
        g_free(utf8);

        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), text);
        if (fragment)
            return fragment.release();
    }

    return 0;
}

String Pasteboard::plainText(Frame*)
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    gchar* utf8 = gtk_clipboard_wait_for_text(clipboard);

    if (!utf8)
        return String();

    String text = String::fromUTF8(utf8);
    g_free(utf8);

    return text;
}
}
