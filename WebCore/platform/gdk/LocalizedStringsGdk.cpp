/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Holger Hans Peter Freyther
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

#include "LocalizedStrings.h"
#include "PlatformString.h"
#include "TextEncoding.h"

#include <glib/gi18n.h>


namespace WebCore {

static String fromUtf8(const gchar* string)
{
    return UTF8Encoding().decode(string, strlen(string));
}

String submitButtonDefaultLabel()
{
    return fromUtf8(_("Submit"));
}

String inputElementAltText()
{ 
    return fromUtf8(_("Submit"));
}

String resetButtonDefaultLabel()
{ 
    return fromUtf8(_("Reset"));
}

String searchableIndexIntroduction()
{
    return fromUtf8(_("Searchable Index"));
}

String fileButtonChooseFileLabel()
{ 
    return fromUtf8(_("Choose File"));
}

String fileButtonNoFileSelectedLabel()
{ 
    return fromUtf8(_("No file selected"));
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    return fromUtf8(_("Open Link in New Window"));
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return fromUtf8(_("Download Linked File"));
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return fromUtf8(_("Copy Link"));
}

String contextMenuItemTagOpenImageInNewWindow()
{ 
    return fromUtf8(_("Open Image in New Window"));
}

String contextMenuItemTagDownloadImageToDisk()
{
    return fromUtf8(_("Download Image"));
}

String contextMenuItemTagCopyImageToClipboard()
{
    return fromUtf8(_("Copy Image"));
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return fromUtf8(_("Open Frame in New Window"));
}

String contextMenuItemTagCopy()
{
    return fromUtf8(_("Copy"));
}

String contextMenuItemTagGoBack()
{
    return fromUtf8(_("Back"));
}

String contextMenuItemTagGoForward()
{
    return fromUtf8(_("Forward"));
}

String contextMenuItemTagStop()
{
    return fromUtf8(_("Stop"));
}

String contextMenuItemTagReload()
{
    return fromUtf8(_("Reload"));
}

String contextMenuItemTagCut()
{
    return fromUtf8(_("Cut"));
}

String contextMenuItemTagPaste()
{
    return fromUtf8(_("Paste"));
}

String contextMenuItemTagNoGuessesFound()
{
    return fromUtf8(_("No Guesses Found"));
}

String contextMenuItemTagIgnoreSpelling()
{
    return fromUtf8(_("Ignore Spelling"));
}

String contextMenuItemTagLearnSpelling()
{
    return fromUtf8(_("Learn Spelling"));
}

String contextMenuItemTagSearchWeb()
{
    return fromUtf8(_("Search with MSN"));
}

String contextMenuItemTagLookUpInDictionary()
{
    return fromUtf8(_("Look Up in Dictionary"));
}

String contextMenuItemTagOpenLink()
{
    return fromUtf8(_("Open Link"));
}

String contextMenuItemTagIgnoreGrammar()
{
    return fromUtf8(_("Ignore Grammar"));
}

String contextMenuItemTagSpellingMenu()
{
    return fromUtf8(_("Spelling and Grammar"));
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return fromUtf8(show ? _("Show Spelling and Grammar") : _("Hide Spelling and Grammar"));
}

String contextMenuItemTagCheckSpelling()
{
    return fromUtf8(_("Check Document Now"));
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return fromUtf8(_("Check Spelling While Typing"));
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return fromUtf8(_("Check Grammar With Spelling"));
}

String contextMenuItemTagFontMenu()
{
    return fromUtf8(_("Font"));
}

String contextMenuItemTagBold()
{
    return fromUtf8(_("Bold"));
}

String contextMenuItemTagItalic()
{
    return fromUtf8(_("Italic"));
}

String contextMenuItemTagUnderline()
{
    return fromUtf8(_("Underline"));
}

String contextMenuItemTagOutline()
{
    return fromUtf8(_("Outline"));
}

String contextMenuItemTagWritingDirectionMenu()
{
    return fromUtf8(_("Write Direction"));
}

String contextMenuItemTagDefaultDirection()
{
    return fromUtf8(_("Default"));
}

String contextMenuItemTagLeftToRight()
{
    return fromUtf8(_("Left to Right"));
}

String contextMenuItemTagRightToLeft()
{
    return fromUtf8(_("Right to Left"));
}

String contextMenuItemTagInspectElement()
{
    return fromUtf8(_("Inspect Element"));
}

String searchMenuNoRecentSearchesText()
{
    return fromUtf8(_("No recent searches"));
}

String searchMenuRecentSearchesText()
{
    return fromUtf8(_("Recent searches"));
}

String searchMenuClearRecentSearchesText()
{
    return fromUtf8(_("Clear recent searches"));
}

String unknownFileSizeText()
{
    return fromUtf8(_("Unknown"));
}

}
