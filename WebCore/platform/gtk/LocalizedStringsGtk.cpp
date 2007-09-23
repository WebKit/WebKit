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

#include <glib/gi18n.h>


namespace WebCore {

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
    return String::fromUTF8(_("Searchable Index"));
}

String fileButtonChooseFileLabel()
{ 
    return String::fromUTF8(_("Choose File"));
}

String fileButtonNoFileSelectedLabel()
{ 
    return String::fromUTF8(_("No file selected"));
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    return String::fromUTF8(_("Open Link in New Window"));
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return String::fromUTF8(_("Download Linked File"));
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return String::fromUTF8(_("Copy Link"));
}

String contextMenuItemTagOpenImageInNewWindow()
{ 
    return String::fromUTF8(_("Open Image in New Window"));
}

String contextMenuItemTagDownloadImageToDisk()
{
    return String::fromUTF8(_("Download Image"));
}

String contextMenuItemTagCopyImageToClipboard()
{
    return String::fromUTF8(_("Copy Image"));
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return String::fromUTF8(_("Open Frame in New Window"));
}

String contextMenuItemTagCopy()
{
    return String::fromUTF8(_("Copy"));
}

String contextMenuItemTagGoBack()
{
    return String::fromUTF8(_("Back"));
}

String contextMenuItemTagGoForward()
{
    return String::fromUTF8(_("Forward"));
}

String contextMenuItemTagStop()
{
    return String::fromUTF8(_("Stop"));
}

String contextMenuItemTagReload()
{
    return String::fromUTF8(_("Reload"));
}

String contextMenuItemTagCut()
{
    return String::fromUTF8(_("Cut"));
}

String contextMenuItemTagPaste()
{
    return String::fromUTF8(_("Paste"));
}

String contextMenuItemTagNoGuessesFound()
{
    return String::fromUTF8(_("No Guesses Found"));
}

String contextMenuItemTagIgnoreSpelling()
{
    return String::fromUTF8(_("Ignore Spelling"));
}

String contextMenuItemTagLearnSpelling()
{
    return String::fromUTF8(_("Learn Spelling"));
}

String contextMenuItemTagSearchWeb()
{
    return String::fromUTF8(_("Search with MSN"));
}

String contextMenuItemTagLookUpInDictionary()
{
    return String::fromUTF8(_("Look Up in Dictionary"));
}

String contextMenuItemTagOpenLink()
{
    return String::fromUTF8(_("Open Link"));
}

String contextMenuItemTagIgnoreGrammar()
{
    return String::fromUTF8(_("Ignore Grammar"));
}

String contextMenuItemTagSpellingMenu()
{
    return String::fromUTF8(_("Spelling and Grammar"));
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return String::fromUTF8(show ? _("Show Spelling and Grammar") : _("Hide Spelling and Grammar"));
}

String contextMenuItemTagCheckSpelling()
{
    return String::fromUTF8(_("Check Document Now"));
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return String::fromUTF8(_("Check Spelling While Typing"));
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return String::fromUTF8(_("Check Grammar With Spelling"));
}

String contextMenuItemTagFontMenu()
{
    return String::fromUTF8(_("Font"));
}

String contextMenuItemTagBold()
{
    return String::fromUTF8(_("Bold"));
}

String contextMenuItemTagItalic()
{
    return String::fromUTF8(_("Italic"));
}

String contextMenuItemTagUnderline()
{
    return String::fromUTF8(_("Underline"));
}

String contextMenuItemTagOutline()
{
    return String::fromUTF8(_("Outline"));
}

String contextMenuItemTagWritingDirectionMenu()
{
    return String::fromUTF8(_("Write Direction"));
}

String contextMenuItemTagDefaultDirection()
{
    return String::fromUTF8(_("Default"));
}

String contextMenuItemTagLeftToRight()
{
    return String::fromUTF8(_("Left to Right"));
}

String contextMenuItemTagRightToLeft()
{
    return String::fromUTF8(_("Right to Left"));
}

String contextMenuItemTagInspectElement()
{
    return String::fromUTF8(_("Inspect Element"));
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
    return String::fromUTF8(_("Clear recent searches"));
}

String unknownFileSizeText()
{
    return String::fromUTF8(_("Unknown"));
}

}
