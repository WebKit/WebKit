/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "WebLocalizableStrings.h"
#include <WebCore/IntSize.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/PlatformString.h>
#include <wtf/RetainPtr.h>

using namespace WebCore;

String WebCore::searchableIndexIntroduction() { return String(LPCTSTR_UI_STRING("This is a searchable index. Enter search keywords: ", "text that appears at the start of nearly-obsolete web pages in the form of a 'searchable index'")); }
String WebCore::submitButtonDefaultLabel() { return String(LPCTSTR_UI_STRING("Submit", "default label for Submit buttons in forms on web pages")); }
String WebCore::inputElementAltText() { return String(LPCTSTR_UI_STRING_KEY("Submit", "Submit (input element)", "alt text for <input> elements with no alt, title, or value")); }
String WebCore::resetButtonDefaultLabel() { return String(LPCTSTR_UI_STRING("Reset", "default label for Reset buttons in forms on web pages")); }
String WebCore::fileButtonChooseFileLabel() { return String(LPCTSTR_UI_STRING("Choose File", "title for file button used in HTML forms")); }
String WebCore::fileButtonNoFileSelectedLabel() { return String(LPCTSTR_UI_STRING("no file selected", "text to display in file button used in HTML forms when no file is selected")); }
String WebCore::contextMenuItemTagOpenLinkInNewWindow() { return String(LPCTSTR_UI_STRING("Open Link in New Window", "Open in New Window context menu item")); }
String WebCore::contextMenuItemTagDownloadLinkToDisk() { return String(LPCTSTR_UI_STRING("Download Linked File", "Download Linked File context menu item")); }
String WebCore::contextMenuItemTagCopyLinkToClipboard() { return String(LPCTSTR_UI_STRING("Copy Link", "Copy Link context menu item")); }
String WebCore::contextMenuItemTagOpenImageInNewWindow() { return String(LPCTSTR_UI_STRING("Open Image in New Window", "Open Image in New Window context menu item")); }
String WebCore::contextMenuItemTagDownloadImageToDisk() { return String(LPCTSTR_UI_STRING("Download Image", "Download Image context menu item")); }
String WebCore::contextMenuItemTagCopyImageToClipboard() { return String(LPCTSTR_UI_STRING("Copy Image", "Copy Image context menu item")); }
String WebCore::contextMenuItemTagOpenFrameInNewWindow() { return String(LPCTSTR_UI_STRING("Open Frame in New Window", "Open Frame in New Window context menu item")); }
String WebCore::contextMenuItemTagCopy() { return String(LPCTSTR_UI_STRING("Copy", "Copy context menu item")); }
String WebCore::contextMenuItemTagGoBack() { return String(LPCTSTR_UI_STRING("Back", "Back context menu item")); }
String WebCore::contextMenuItemTagGoForward() { return String(LPCTSTR_UI_STRING("Forward", "Forward context menu item")); }
String WebCore::contextMenuItemTagStop() { return String(LPCTSTR_UI_STRING("Stop", "Stop context menu item")); }
String WebCore::contextMenuItemTagReload() { return String(LPCTSTR_UI_STRING("Reload", "Reload context menu item")); }
String WebCore::contextMenuItemTagCut() { return String(LPCTSTR_UI_STRING("Cut", "Cut context menu item")); }
String WebCore::contextMenuItemTagPaste() { return String(LPCTSTR_UI_STRING("Paste", "Paste context menu item")); }
String WebCore::contextMenuItemTagNoGuessesFound() { return String(LPCTSTR_UI_STRING("No Guesses Found", "No Guesses Found context menu item")); }
String WebCore::contextMenuItemTagIgnoreSpelling() { return String(LPCTSTR_UI_STRING("Ignore Spelling", "Ignore Spelling context menu item")); }
String WebCore::contextMenuItemTagLearnSpelling() { return String(LPCTSTR_UI_STRING("Learn Spelling", "Learn Spelling context menu item")); }
String WebCore::contextMenuItemTagSearchWeb() { return String(LPCTSTR_UI_STRING("Search with Google", "Search in Google context menu item")); }
String WebCore::contextMenuItemTagLookUpInDictionary() { return String(LPCTSTR_UI_STRING("Look Up in Dictionary", "Look Up in Dictionary context menu item")); }
String WebCore::contextMenuItemTagOpenLink() { return String(LPCTSTR_UI_STRING("Open Link", "Open Link context menu item")); }
String WebCore::contextMenuItemTagIgnoreGrammar() { return String(LPCTSTR_UI_STRING("Ignore Grammar", "Ignore Grammar context menu item")); }
String WebCore::contextMenuItemTagSpellingMenu() { return String(LPCTSTR_UI_STRING("Spelling and Grammar", "Spelling and Grammar context sub-menu item")); }
String WebCore::contextMenuItemTagCheckSpelling() { return String(LPCTSTR_UI_STRING("Check Document Now", "Check spelling context menu item")); }
String WebCore::contextMenuItemTagCheckSpellingWhileTyping() { return String(LPCTSTR_UI_STRING("Check Spelling While Typing", "Check spelling while typing context menu item")); }
String WebCore::contextMenuItemTagCheckGrammarWithSpelling() { return String(LPCTSTR_UI_STRING("Check Grammar With Spelling", "Check grammar with spelling context menu item")); }
String WebCore::contextMenuItemTagFontMenu() { return String(LPCTSTR_UI_STRING("Font", "Font context sub-menu item")); }
String WebCore::contextMenuItemTagBold() { return String(LPCTSTR_UI_STRING("Bold", "Bold context menu item")); }
String WebCore::contextMenuItemTagItalic() { return String(LPCTSTR_UI_STRING("Italic", "Italic context menu item")); }
String WebCore::contextMenuItemTagUnderline() { return String(LPCTSTR_UI_STRING("Underline", "Underline context menu item")); }
String WebCore::contextMenuItemTagOutline() { return String(LPCTSTR_UI_STRING("Outline", "Outline context menu item")); }
String WebCore::contextMenuItemTagWritingDirectionMenu() { return String(LPCTSTR_UI_STRING("Paragraph Direction", "Paragraph direction context sub-menu item")); }
String WebCore::contextMenuItemTagTextDirectionMenu() { return String(LPCTSTR_UI_STRING("Selection Direction", "Selection direction context sub-menu item")); }
String WebCore::contextMenuItemTagDefaultDirection() { return String(LPCTSTR_UI_STRING("Default", "Default writing direction context menu item")); }
String WebCore::contextMenuItemTagLeftToRight() { return String(LPCTSTR_UI_STRING("Left to Right", "Left to Right context menu item")); }
String WebCore::contextMenuItemTagRightToLeft() { return String(LPCTSTR_UI_STRING("Right to Left", "Right to Left context menu item")); }
String WebCore::contextMenuItemTagShowSpellingPanel(bool show) { return String(show ? LPCTSTR_UI_STRING("Show Spelling and Grammar", "menu item title") : LPCTSTR_UI_STRING("Hide Spelling and Grammar", "menu item title")); }
String WebCore::contextMenuItemTagInspectElement() { return String(LPCTSTR_UI_STRING("Inspect Element", "Inspect Element context menu item")); }
String WebCore::searchMenuNoRecentSearchesText() { return String(LPCTSTR_UI_STRING("No recent searches", "Label for only item in menu that appears when clicking on the search field image, when no searches have been performed")); }
String WebCore::searchMenuRecentSearchesText() { return String(LPCTSTR_UI_STRING("Recent Searches", "label for first item in the menu that appears when clicking on the search field image, used as embedded menu title")); }
String WebCore::searchMenuClearRecentSearchesText() { return String(LPCTSTR_UI_STRING("Clear Recent Searches", "menu item in Recent Searches menu that empties menu's contents")); }
String WebCore::AXWebAreaText() { return String(LPCTSTR_UI_STRING("web area", "accessibility role description for web area")); }
String WebCore::AXLinkText() { return String(LPCTSTR_UI_STRING("link", "accessibility role description for link")); }
String WebCore::AXListMarkerText() { return String(LPCTSTR_UI_STRING("list marker", "accessibility role description for list marker")); }
String WebCore::AXImageMapText() { return String(LPCTSTR_UI_STRING("image map", "accessibility role description for image map")); }
String WebCore::AXHeadingText() { return String(LPCTSTR_UI_STRING("heading", "accessibility role description for headings")); }
String WebCore::AXDefinitionListTermText() { return String(LPCTSTR_UI_STRING("term", "term word of a definition")); }
String WebCore::AXDefinitionListDefinitionText() { return String(LPCTSTR_UI_STRING("definition", "definition phrase")); }
String WebCore::AXButtonActionVerb() { return String(LPCTSTR_UI_STRING("press", "Verb stating the action that will occur when a button is pressed, as used by accessibility")); }
String WebCore::AXRadioButtonActionVerb() { return String(LPCTSTR_UI_STRING("select", "Verb stating the action that will occur when a radio button is clicked, as used by accessibility")); }
String WebCore::AXTextFieldActionVerb() { return String(LPCTSTR_UI_STRING("activate", "Verb stating the action that will occur when a text field is selected, as used by accessibility")); }
String WebCore::AXCheckedCheckBoxActionVerb() { return String(LPCTSTR_UI_STRING("uncheck", "Verb stating the action that will occur when a checked checkbox is clicked, as used by accessibility")); }
String WebCore::AXUncheckedCheckBoxActionVerb() { return String(LPCTSTR_UI_STRING("check", "Verb stating the action that will occur when an unchecked checkbox is clicked, as used by accessibility")); }
String WebCore::AXLinkActionVerb() { return String(LPCTSTR_UI_STRING("jump", "Verb stating the action that will occur when a link is clicked, as used by accessibility")); }
String WebCore::unknownFileSizeText() { return String(LPCTSTR_UI_STRING("Unknown", "Unknown filesize FTP directory listing item")); }
String WebCore::uploadFileText() { return String(LPCTSTR_UI_STRING("Upload file", "(Windows) Form submit file upload dialog title")); }
String WebCore::allFilesText() { return String(LPCTSTR_UI_STRING("All Files", "(Windows) Form submit file upload all files pop-up")); }
String WebCore::mediaElementLoadingStateText() { return String(LPCTSTR_UI_STRING("Loading...", "Media controller status message when the media is loading")); }
String WebCore::mediaElementLiveBroadcastStateText() { return String(LPCTSTR_UI_STRING("Live Broadcast", "Media controller status message when watching a live broadcast")); }

String WebCore::imageTitle(const String& filename, const IntSize& size) 
{ 
    static RetainPtr<CFStringRef> format(AdoptCF, UI_STRING("%@ %d×%d pixels", "window title for a standalone image (uses multiplication symbol, not x)"));

    RetainPtr<CFStringRef> filenameCF(AdoptCF, filename.createCFString());
    RetainPtr<CFStringRef> result(AdoptCF, CFStringCreateWithFormat(0, 0, format.get(), filenameCF.get(), size.width(), size.height()));

    return result.get();
}

String multipleFileUploadText(unsigned numberOfFiles)
{
    static RetainPtr<CFStringRef> format(AdoptCF, UI_STRING("%d files", "Label to describe the number of files selected in a file upload control that allows multiple files"));
    RetainPtr<CFStringRef> result(AdoptCF, CFStringCreateWithFormat(0, 0, format.get(), numberOfFiles));

    return result.get();
}
