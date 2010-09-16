/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Christian Dywan <christian@imendio.com>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2008 INdT Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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

#include "NotImplemented.h"
#include "PlatformString.h"

namespace WebCore {

String submitButtonDefaultLabel()
{
    return String::fromUTF8("Submit");
}

String inputElementAltText()
{
    return String::fromUTF8("Submit");
}

String resetButtonDefaultLabel()
{
    return String::fromUTF8("Reset");
}

String searchableIndexIntroduction()
{
    return String::fromUTF8("_Searchable Index");
}

String fileButtonChooseFileLabel()
{
    return String::fromUTF8("Choose File");
}

String fileButtonNoFileSelectedLabel()
{
    return String::fromUTF8("No file selected");
}

String contextMenuItemTagOpenLinkInNewWindow()
{
    return String::fromUTF8("Open Link in New Window");
}

String contextMenuItemTagDownloadLinkToDisk()
{
    return String::fromUTF8("Download Linked File");
}

String contextMenuItemTagCopyLinkToClipboard()
{
    return String::fromUTF8("Copy Link Location");
}

String contextMenuItemTagOpenImageInNewWindow()
{
    return String::fromUTF8("Open Image in New Window");
}

String contextMenuItemTagDownloadImageToDisk()
{
    return String::fromUTF8("Save Image As");
}

String contextMenuItemTagCopyImageToClipboard()
{
    return String::fromUTF8("Copy Image");
}

String contextMenuItemTagOpenMediaInNewWindow()
{
    return String::fromUTF8("Open Media in New Window");
}

String contextMenuItemTagCopyMediaLinkToClipboard()
{
    return String::fromUTF8("Copy Media Link Location");
}

String contextMenuItemTagToggleMediaControls()
{
    return String::fromUTF8("Toggle Media Controls");
}

String contextMenuItemTagToggleMediaLoop()
{
    return String::fromUTF8("Toggle Media Loop Playback");
}

String contextMenuItemTagEnterVideoFullscreen()
{
    return String::fromUTF8("Switch Video to Fullscreen");
}

String contextMenuItemTagMediaPlay()
{
    return String::fromUTF8("Play");
}

String contextMenuItemTagMediaPause()
{
    return String::fromUTF8("Pause");
}

String contextMenuItemTagMediaMute()
{
    return String::fromUTF8("Mute");
}

String contextMenuItemTagMediaUnMute()
{
    return String::fromUTF8("UnMute");
}

String contextMenuItemTagOpenFrameInNewWindow()
{
    return String::fromUTF8("Open Frame in New Window");
}

String contextMenuItemTagCopy()
{
    static String stockLabel = String::fromUTF8("Copy");
    return stockLabel;
}

String contextMenuItemTagDelete()
{
    static String stockLabel = String::fromUTF8("Delete");
    return stockLabel;
}

String contextMenuItemTagSelectAll()
{
    static String stockLabel = String::fromUTF8("Select All");
    return stockLabel;
}

String contextMenuItemTagUnicode()
{
    return String::fromUTF8("Insert Unicode Control Character");
}

String contextMenuItemTagInputMethods()
{
    return String::fromUTF8("Input Methods");
}

String contextMenuItemTagGoBack()
{
    static String stockLabel = String::fromUTF8("Go Back");
    return stockLabel;
}

String contextMenuItemTagGoForward()
{
    static String stockLabel = String::fromUTF8("Go Forward");
    return stockLabel;
}

String contextMenuItemTagStop()
{
    static String stockLabel = String::fromUTF8("Stop");
    return stockLabel;
}

String contextMenuItemTagReload()
{
    return String::fromUTF8("Reload");
}

String contextMenuItemTagCut()
{
    static String stockLabel = String::fromUTF8("Cut");
    return stockLabel;
}

String contextMenuItemTagPaste()
{
    static String stockLabel = String::fromUTF8("Paste");
    return stockLabel;
}

String contextMenuItemTagNoGuessesFound()
{
    return String::fromUTF8("No Guesses Found");
}

String contextMenuItemTagIgnoreSpelling()
{
    return String::fromUTF8("Ignore Spelling");
}

String contextMenuItemTagLearnSpelling()
{
    return String::fromUTF8("Learn Spelling");
}

String contextMenuItemTagSearchWeb()
{
    return String::fromUTF8("Search the Web");
}

String contextMenuItemTagLookUpInDictionary()
{
    return String::fromUTF8("Look Up in Dictionary");
}

String contextMenuItemTagOpenLink()
{
    return String::fromUTF8("Open Link");
}

String contextMenuItemTagIgnoreGrammar()
{
    return String::fromUTF8("Ignore Grammar");
}

String contextMenuItemTagSpellingMenu()
{
    return String::fromUTF8("Spelling and Grammar");
}

String contextMenuItemTagShowSpellingPanel(bool show)
{
    return String::fromUTF8(show ? "Show Spelling and Grammar" : "Hide Spelling and Grammar");
}

String contextMenuItemTagCheckSpelling()
{
    return String::fromUTF8("Check Document Now");
}

String contextMenuItemTagCheckSpellingWhileTyping()
{
    return String::fromUTF8("Check Spelling While _Typing");
}

String contextMenuItemTagCheckGrammarWithSpelling()
{
    return String::fromUTF8("Check Grammar With Spelling");
}

String contextMenuItemTagFontMenu()
{
    return String::fromUTF8("Font");
}

String contextMenuItemTagBold()
{
    static String stockLabel = String::fromUTF8("Bold");
    return stockLabel;
}

String contextMenuItemTagItalic()
{
    static String stockLabel = String::fromUTF8("Italic");
    return stockLabel;
}

String contextMenuItemTagUnderline()
{
    static String stockLabel = String::fromUTF8("Underline");
    return stockLabel;
}

String contextMenuItemTagOutline()
{
    return String::fromUTF8("Outline");
}

String contextMenuItemTagInspectElement()
{
    return String::fromUTF8("Inspect Element");
}

String contextMenuItemTagRightToLeft()
{
    return String();
}

String contextMenuItemTagLeftToRight()
{
    return String();
}

String contextMenuItemTagWritingDirectionMenu()
{
    return String();
}

String contextMenuItemTagTextDirectionMenu()
{
    return String();
}

String contextMenuItemTagDefaultDirection()
{
    return String();
}

String searchMenuNoRecentSearchesText()
{
    return String::fromUTF8("No recent searches");
}

String searchMenuRecentSearchesText()
{
    return String::fromUTF8("Recent searches");
}

String searchMenuClearRecentSearchesText()
{
    return String::fromUTF8("Clear recent searches");
}

String AXDefinitionListTermText()
{
    return String::fromUTF8("term");
}

String AXDefinitionListDefinitionText()
{
    return String::fromUTF8("definition");
}

String AXButtonActionVerb()
{
    return String::fromUTF8("press");
}

String AXRadioButtonActionVerb()
{
    return String::fromUTF8("select");
}

String AXTextFieldActionVerb()
{
    return String::fromUTF8("activate");
}

String AXCheckedCheckBoxActionVerb()
{
    return String::fromUTF8("uncheck");
}

String AXUncheckedCheckBoxActionVerb()
{
    return String::fromUTF8("check");
}

String AXLinkActionVerb()
{
    return String::fromUTF8("jump");
}

String unknownFileSizeText()
{
    return String::fromUTF8("Unknown");
}

String imageTitle(const String& filename, const IntSize& size)
{
    notImplemented();
    return String();
}

#if ENABLE(VIDEO)
String localizedMediaControlElementString(const String& name)
{
    notImplemented();
    return String();
}

String localizedMediaControlElementHelpText(const String& name)
{
    notImplemented();
    return String();
}

String localizedMediaTimeDescription(float time)
{
    notImplemented();
    return String();
}
#endif

String mediaElementLoadingStateText()
{
    return String::fromUTF8("Loading...");
}

String mediaElementLiveBroadcastStateText()
{
    return String::fromUTF8("Live Broadcast");
}

String validationMessagePatternMismatchText()
{
    return String::fromUTF8("pattern mismatch");
}

String validationMessageRangeOverflowText()
{
    return String::fromUTF8("range overflow");
}

String validationMessageRangeUnderflowText()
{
    return String::fromUTF8("range underflow");
}

String validationMessageStepMismatchText()
{
    return String::fromUTF8("step mismatch");
}

String validationMessageTooLongText()
{
    return String::fromUTF8("too long");
}

String validationMessageTypeMismatchText()
{
    return String::fromUTF8("type mismatch");
}

String validationMessageValueMissingText()
{
    return String::fromUTF8("value missing");
}

String missingPluginText()
{
    return String::fromUTF8("missing plugin");
}

String AXMenuListPopupActionVerb()
{
    return String();
}

String AXMenuListActionVerb()
{
    return String();
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    return String::number(numberOfFiles) + String::fromUTF8(" files");
}

String crashedPluginText()
{
    return String::fromUTF8("plugin crashed");
}

}
