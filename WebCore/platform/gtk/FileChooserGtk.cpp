/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FileChooser.h"

#include "CString.h"
#include "Document.h"
#include "FrameView.h"
#include "Icon.h"
#include "LocalizedStrings.h"
#include "StringTruncator.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

namespace WebCore {

static bool stringByAdoptingFileSystemRepresentation(gchar* systemFilename, String& result)
{
    if (!systemFilename)
        return false;

    gchar* filename = g_filename_to_utf8(systemFilename, -1, 0, 0, 0);
    g_free(systemFilename);

    if (!filename)
        return false;

    result = String::fromUTF8(filename);
    g_free(filename);
    return true;
}

FileChooser::FileChooser(FileChooserClient* client, const String& filename)
    : m_client(client)
    , m_filename(filename)
    , m_icon(chooseIcon(filename))
{
}

FileChooser::~FileChooser()
{
}

void FileChooser::openFileChooser(Document* document)
{
    FrameView* view = document->view();
    if (!view)
        return;

    GtkWidget* dialog = gtk_file_chooser_dialog_new(_("Upload File"),
                                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view->containingWindow()))),
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                                    NULL);

    // We need this protector because otherwise we can be deleted if the file upload control is detached while
    // we're within the gtk_run_dialog call.
    RefPtr<FileChooser> protector(this);
    String result;

    const bool acceptedDialog = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT;
    if (acceptedDialog && stringByAdoptingFileSystemRepresentation(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)), result))
        chooseFile(result);
    gtk_widget_destroy(dialog);
}

String FileChooser::basenameForWidth(const Font& font, int width) const
{
    if (width <= 0)
        return String();

    String string = fileButtonNoFileSelectedLabel();

    if (!m_filename.isEmpty()) {
        gchar* systemFilename = g_filename_from_utf8(m_filename.utf8().data(), -1, 0, 0, 0);
        if (systemFilename) {
            gchar* systemBasename = g_path_get_basename(systemFilename);
            g_free(systemFilename);

            stringByAdoptingFileSystemRepresentation(systemBasename, string);
        }
    }

    return StringTruncator::centerTruncate(string, width, font, false);
}
}
