/*
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * All rights reserved.
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

#include "PlatformString.h"
#include "LocalizedStrings.h"
#include "NotImplemented.h"

#include <QCoreApplication>

namespace WebCore {

String submitButtonDefaultLabel()
{
    return QCoreApplication::translate("QWebPage", "Submit", "default label for Submit buttons in forms on web pages");
}

String inputElementAltText()
{
    return QCoreApplication::translate("QWebPage", "Submit", "Submit (input element) alt text for <input> elements with no alt, title, or value");
}

String resetButtonDefaultLabel()
{
    return QCoreApplication::translate("QWebPage", "Reset", "default label for Reset buttons in forms on web pages");
}

String defaultLanguage()
{
    return "en";
}

String searchableIndexIntroduction()
{
    return QCoreApplication::translate("QWebPage", "Searchable Index", "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'");
}
    
String fileButtonChooseFileLabel()
{
    return QCoreApplication::translate("QWebPage", "Choose File", "title for file button used in HTML forms");
}

String fileButtonNoFileSelectedLabel()
{
    return QCoreApplication::translate("QWebPage", "No file selected", "text to display in file button used in HTML forms when no file is selected");
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    return QCoreApplication::translate("QWebPage", "Open in New Window", "Open in New Window context menu item");
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return QCoreApplication::translate("QWebPage", "Save Link...", "Download Linked File context menu item");
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return QCoreApplication::translate("QWebPage", "Copy Link", "Copy Link context menu item");
}

String contextMenuItemTagOpenImageInNewWindow()
{
    return QCoreApplication::translate("QWebPage", "Open Image", "Open Image in New Window context menu item");
}

String contextMenuItemTagDownloadImageToDisk()
{
    return QCoreApplication::translate("QWebPage", "Save Image", "Download Image context menu item");
}

String contextMenuItemTagCopyImageToClipboard()
{
    return QCoreApplication::translate("QWebPage", "Copy Image", "Copy Link context menu item");
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return QCoreApplication::translate("QWebPage", "Open Frame", "Open Frame in New Window context menu item");
}

String contextMenuItemTagCopy()
{
    return QCoreApplication::translate("QWebPage", "Copy", "Copy context menu item");
}

String contextMenuItemTagGoBack()
{
    return QCoreApplication::translate("QWebPage", "Go Back", "Back context menu item");
}

String contextMenuItemTagGoForward()
{
    return QCoreApplication::translate("QWebPage", "Go Forward", "Forward context menu item");
}

String contextMenuItemTagStop()
{
    return QCoreApplication::translate("QWebPage", "Stop", "Stop context menu item");
}

String contextMenuItemTagReload()
{
    return QCoreApplication::translate("QWebPage", "Reload", "Reload context menu item");
}

String contextMenuItemTagCut()
{
    return QCoreApplication::translate("QWebPage", "Cut", "Cut context menu item");
}

String contextMenuItemTagPaste()
{
    return QCoreApplication::translate("QWebPage", "Paste", "Paste context menu item");
}

String contextMenuItemTagNoGuessesFound()
{
    return QCoreApplication::translate("QWebPage", "No Guesses Found", "No Guesses Found context menu item");
}

String contextMenuItemTagIgnoreSpelling()
{
    return QCoreApplication::translate("QWebPage", "Ignore", "Ignore Spelling context menu item");
}

String contextMenuItemTagLearnSpelling()
{
    return QCoreApplication::translate("QWebPage", "Add To Dictionary", "Learn Spelling context menu item");
}

String contextMenuItemTagSearchWeb()
{
    return QCoreApplication::translate("QWebPage", "Search The Web", "Search The Web context menu item");
}

String contextMenuItemTagLookUpInDictionary()
{
    return QCoreApplication::translate("QWebPage", "Look Up In Dictionary", "Look Up in Dictionary context menu item");
}

String contextMenuItemTagOpenLink()
{
    return QCoreApplication::translate("QWebPage", "Open Link", "Open Link context menu item");
}

String contextMenuItemTagIgnoreGrammar()
{
    return QCoreApplication::translate("QWebPage", "Ignore", "Ignore Grammar context menu item");
}

String contextMenuItemTagSpellingMenu()
{
    return QCoreApplication::translate("QWebPage", "Spelling", "Spelling and Grammar context sub-menu item");
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return show ? QCoreApplication::translate("QWebPage", "Show Spelling and Grammar", "menu item title") : 
                  QCoreApplication::translate("QWebPage", "Hide Spelling and Grammar", "menu item title");
}

String contextMenuItemTagCheckSpelling()
{
    return QCoreApplication::translate("QWebPage", "Check Spelling", "Check spelling context menu item");
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return QCoreApplication::translate("QWebPage", "Check Spelling While Typing", "Check spelling while typing context menu item");
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return QCoreApplication::translate("QWebPage", "Check Grammar With Spelling", "Check grammar with spelling context menu item");
}

String contextMenuItemTagFontMenu()
{
    return QCoreApplication::translate("QWebPage", "Fonts", "Font context sub-menu item");
}

String contextMenuItemTagBold()
{
    return QCoreApplication::translate("QWebPage", "Bold", "Bold context menu item");
}

String contextMenuItemTagItalic()
{
    return QCoreApplication::translate("QWebPage", "Italic", "Italic context menu item");
}

String contextMenuItemTagUnderline()
{
    return QCoreApplication::translate("QWebPage", "Underline", "Underline context menu item");
}

String contextMenuItemTagOutline()
{
    return QCoreApplication::translate("QWebPage", "Outline", "Outline context menu item");
}

String contextMenuItemTagWritingDirectionMenu()
{
    return QCoreApplication::translate("QWebPage", "Direction", "Writing direction context sub-menu item");
}

String contextMenuItemTagDefaultDirection()
{
    return QCoreApplication::translate("QWebPage", "Default", "Default writing direction context menu item");
}

String contextMenuItemTagLeftToRight()
{
    return QCoreApplication::translate("QWebPage", "LTR", "Left to Right context menu item");
}

String contextMenuItemTagRightToLeft()
{
    return QCoreApplication::translate("QWebPage", "RTL", "Right to Left context menu item");
}

String contextMenuItemTagInspectElement()
{
    return QCoreApplication::translate("QWebPage", "Inspect", "Inspect Element context menu item");
}

String searchMenuNoRecentSearchesText()
{
    return QCoreApplication::translate("QWebPage", "No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed");
}

String searchMenuRecentSearchesText()
{
    return QCoreApplication::translate("QWebPage", "Recent searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title");
}

String searchMenuClearRecentSearchesText()
{
    return QCoreApplication::translate("QWebPage", "Clear recent searches", "menu item in Recent Searches menu that empties menu's contents");
}

String AXWebAreaText()
{
    return String();
}

String AXLinkText()
{
    return String();
}

String AXListMarkerText()
{
    return String();
}

String AXImageMapText()
{
    return String();
}

String AXHeadingText()
{
    return String();
}

String unknownFileSizeText()
{
    return QCoreApplication::translate("QWebPage", "Unknown", "Unknown filesize FTP directory listing item");
}

}
// vim: ts=4 sw=4 et
