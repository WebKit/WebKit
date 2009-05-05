/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Nuanti Ltd.
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

#include "LocalizedStrings.h"
#include "CString.h"
#include "GOwnPtr.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include "PlatformString.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

namespace WebCore {

static const char* gtkStockLabel(const char* stockID)
{
    GtkStockItem item;
    if (!gtk_stock_lookup(stockID, &item))
        return stockID;
    return item.label;
}

String submitButtonDefaultLabel()
{
    return String::fromUTF8(_("Submit"));
}

String inputElementAltText()
{
    return String::fromUTF8(_("Submit"));
}

String resetButtonDefaultLabel()
{
    return String::fromUTF8(_("Reset"));
}

String searchableIndexIntroduction()
{
    return String::fromUTF8(_("_Searchable Index"));
}

String fileButtonChooseFileLabel()
{
    return String::fromUTF8(_("Choose File"));
}

String fileButtonNoFileSelectedLabel()
{
    return String::fromUTF8(_("(None)"));
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    return String::fromUTF8(_("Open Link in New _Window"));
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return String::fromUTF8(_("_Download Linked File"));
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return String::fromUTF8(_("Copy Link Loc_ation"));
}

String contextMenuItemTagOpenImageInNewWindow()
{
    return String::fromUTF8(_("Open _Image in New Window"));
}

String contextMenuItemTagDownloadImageToDisk()
{
    return String::fromUTF8(_("Sa_ve Image As"));
}

String contextMenuItemTagCopyImageToClipboard()
{
    return String::fromUTF8(_("Cop_y Image"));
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return String::fromUTF8(_("Open _Frame in New Window"));
}

String contextMenuItemTagCopy()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_COPY));
    return stockLabel;
}

String contextMenuItemTagDelete()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_DELETE));
    return stockLabel;
}

String contextMenuItemTagSelectAll()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_SELECT_ALL));
    return stockLabel;
}

String contextMenuItemTagUnicode()
{
    return String::fromUTF8(_("_Insert Unicode Control Character"));
}

String contextMenuItemTagInputMethods()
{
    return String::fromUTF8(_("Input _Methods"));
}

String contextMenuItemTagGoBack()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_GO_BACK));
    return stockLabel;
}

String contextMenuItemTagGoForward()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_GO_FORWARD));
    return stockLabel;
}

String contextMenuItemTagStop()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_STOP));
    return stockLabel;
}

String contextMenuItemTagReload()
{
    return String::fromUTF8(_("_Reload"));
}

String contextMenuItemTagCut()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_CUT));
    return stockLabel;
}

String contextMenuItemTagPaste()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_PASTE));
    return stockLabel;
}

String contextMenuItemTagNoGuessesFound()
{
    return String::fromUTF8(_("No Guesses Found"));
}

String contextMenuItemTagIgnoreSpelling()
{
    return String::fromUTF8(_("_Ignore Spelling"));
}

String contextMenuItemTagLearnSpelling()
{
    return String::fromUTF8(_("_Learn Spelling"));
}

String contextMenuItemTagSearchWeb()
{
    return String::fromUTF8(_("_Search the Web"));
}

String contextMenuItemTagLookUpInDictionary()
{
    return String::fromUTF8(_("_Look Up in Dictionary"));
}

String contextMenuItemTagOpenLink()
{
    return String::fromUTF8(_("_Open Link"));
}

String contextMenuItemTagIgnoreGrammar()
{
    return String::fromUTF8(_("Ignore _Grammar"));
}

String contextMenuItemTagSpellingMenu()
{
    return String::fromUTF8(_("Spelling and _Grammar"));
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return String::fromUTF8(show ? _("_Show Spelling and Grammar") : _("_Hide Spelling and Grammar"));
}

String contextMenuItemTagCheckSpelling()
{
    return String::fromUTF8(_("_Check Document Now"));
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return String::fromUTF8(_("Check Spelling While _Typing"));
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return String::fromUTF8(_("Check _Grammar With Spelling"));
}

String contextMenuItemTagFontMenu()
{
    return String::fromUTF8(_("_Font"));
}

String contextMenuItemTagBold()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_BOLD));
    return stockLabel;
}

String contextMenuItemTagItalic()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_ITALIC));
    return stockLabel;
}

String contextMenuItemTagUnderline()
{
    static String stockLabel = String::fromUTF8(gtkStockLabel(GTK_STOCK_UNDERLINE));
    return stockLabel;
}

String contextMenuItemTagOutline()
{
    return String::fromUTF8(_("_Outline"));
}

String contextMenuItemTagInspectElement()
{
    return String::fromUTF8(_("Inspect _Element"));
}

String searchMenuNoRecentSearchesText()
{
    return String::fromUTF8(_("No recent searches"));
}

String searchMenuRecentSearchesText()
{
    return String::fromUTF8(_("Recent searches"));
}

String searchMenuClearRecentSearchesText()
{
    return String::fromUTF8(_("_Clear recent searches"));
}

String AXDefinitionListTermText()
{
    return String::fromUTF8(_("term"));
}

String AXDefinitionListDefinitionText()
{
    return String::fromUTF8(_("definition"));
}

String AXButtonActionVerb()
{
    return String::fromUTF8(_("press"));
}

String AXRadioButtonActionVerb()
{
    return String::fromUTF8(_("select"));
}

String AXTextFieldActionVerb()
{
    return String::fromUTF8(_("activate"));
}

String AXCheckedCheckBoxActionVerb()
{
    return String::fromUTF8(_("uncheck"));
}

String AXUncheckedCheckBoxActionVerb()
{
    return String::fromUTF8(_("check"));
}

String AXLinkActionVerb()
{
    return String::fromUTF8(_("jump"));
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    // FIXME: If this file gets localized, this should really be localized as one string with a wildcard for the number.
    return String::number(numberOfFiles) + String::fromUTF8(_(" files"));
}

String unknownFileSizeText()
{
    return String::fromUTF8(_("Unknown"));
}

String imageTitle(const String& filename, const IntSize& size)
{
    GOwnPtr<gchar> string(g_strdup_printf(C_("Title string for images", "%s  (%dx%d pixels)"),
                                          filename.utf8().data(),
                                          size.width(), size.height()));

    return String::fromUTF8(string.get());
}

}
