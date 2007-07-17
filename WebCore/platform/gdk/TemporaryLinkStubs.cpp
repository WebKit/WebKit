/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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

#include "AXObjectCache.h"
#include "CString.h"
#include "CachedPage.h"
#include "CachedResource.h"
#include "ChromeClientGdk.h"
#include "Clipboard.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "CookieJar.h"
#include "Cursor.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "Editor.h"
#include "FileChooser.h"
#include "Font.h"
#include "FrameGdk.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGdk.h"
#include "FrameView.h"
#include "FTPDirectoryDocument.h"
#include "GlobalHistory.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "Icon.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "LocalizedStrings.h"
#include "MainResourceLoader.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "PlatformMouseEvent.h"
#include "PlugInInfoStore.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "SearchPopupMenu.h"
#include "TextBoundaries.h"
#include "TextBreakIteratorInternalICU.h"
#include "loader.h"
#include <gtk/gtk.h>
#include <stdio.h>

using namespace WebCore;

namespace WebCore {
    class Page;
}

void FrameView::updateBorder() { notImplemented(); }

int WebCore::findNextWordFromIndex(UChar const*, int, int, bool) { notImplemented(); return 0; }
void WebCore::findWordBoundary(UChar const* str, int len, int position, int* start, int* end) {*start = position; *end = position; }
const char* WebCore::currentTextBreakLocaleID() { notImplemented(); return "en_us"; }

void ChromeClientGdk::chromeDestroyed() { notImplemented(); }
FloatRect ChromeClientGdk::windowRect() { notImplemented(); return FloatRect(); }
void ChromeClientGdk::setWindowRect(const FloatRect& r) {notImplemented(); }
FloatRect ChromeClientGdk::pageRect() { notImplemented(); return FloatRect(); }
float ChromeClientGdk::scaleFactor() { notImplemented(); return 1.0; }
void ChromeClientGdk::focus() { notImplemented(); }
void ChromeClientGdk::unfocus() { notImplemented(); }
WebCore::Page* ChromeClientGdk::createWindow(Frame*, const FrameLoadRequest&) { notImplemented(); return 0; }
WebCore::Page* ChromeClientGdk::createModalDialog(Frame*, const FrameLoadRequest&) { notImplemented(); return 0; }
void ChromeClientGdk::show() { notImplemented(); }
bool ChromeClientGdk::canRunModal() { notImplemented(); return false; }
void ChromeClientGdk::runModal() { notImplemented(); }
void ChromeClientGdk::setToolbarsVisible(bool) { notImplemented(); }
bool ChromeClientGdk::toolbarsVisible() { notImplemented(); return false; }
void ChromeClientGdk::setStatusbarVisible(bool) { notImplemented(); }
bool ChromeClientGdk::statusbarVisible() { notImplemented(); return false; }
void ChromeClientGdk::setScrollbarsVisible(bool) { notImplemented(); }
bool ChromeClientGdk::scrollbarsVisible() { notImplemented(); return false; }
void ChromeClientGdk::setMenubarVisible(bool) { notImplemented(); }
bool ChromeClientGdk::menubarVisible() { notImplemented(); return false; }
void ChromeClientGdk::setResizable(bool) { notImplemented(); }
void ChromeClientGdk::closeWindowSoon() { notImplemented(); }
bool ChromeClientGdk::canTakeFocus(FocusDirection) { notImplemented(); return true; }
void ChromeClientGdk::takeFocus(FocusDirection) { notImplemented(); }
bool ChromeClientGdk::canRunBeforeUnloadConfirmPanel() { notImplemented(); return false; }
void ChromeClientGdk::addMessageToConsole(const WebCore::String&, unsigned int, const WebCore::String&) { notImplemented(); }
bool ChromeClientGdk::runBeforeUnloadConfirmPanel(const WebCore::String&, WebCore::Frame*) { notImplemented(); return false; }
void ChromeClientGdk::runJavaScriptAlert(Frame*, const String&)
{
    notImplemented();
}
bool ChromeClientGdk::runJavaScriptConfirm(Frame*, const String&)
{
    notImplemented();
    return false;
}
bool ChromeClientGdk::runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result)
{
    notImplemented();
    return false;
}
void ChromeClientGdk::setStatusbarText(const String&)
{
    notImplemented();
}

bool ChromeClientGdk::shouldInterruptJavaScript()
{
    notImplemented();
    return false;
}

bool ChromeClientGdk::tabsToLinks() const
{
    notImplemented();
    return false;
}

IntRect ChromeClientGdk::windowResizerRect() const
{
    notImplemented();
    return IntRect();
}

void ChromeClientGdk::addToDirtyRegion(const IntRect&)
{
    notImplemented();
}

void ChromeClientGdk::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    notImplemented();
}

void ChromeClientGdk::updateBackingStore()
{
    notImplemented();
}

void ChromeClientGdk::mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags)
{
    notImplemented();
}

void ChromeClientGdk::setToolTip(const String&)
{
    notImplemented();
}

void ChromeClientGdk::print(Frame*)
{
    notImplemented();
}

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
bool AXObjectCache::gAccessibilityEnabled = false;

bool WebCore::historyContains(DeprecatedString const&) { return false; }

// LocalizedStrings
String WebCore::submitButtonDefaultLabel() { return "Submit"; }
String WebCore::inputElementAltText() { return String(); }
String WebCore::resetButtonDefaultLabel() { return "Reset"; }
String WebCore::defaultLanguage() { return "en"; }
String WebCore::searchableIndexIntroduction() { return "Searchable Index"; }
String WebCore::fileButtonChooseFileLabel() { return "Choose File"; }
String WebCore::fileButtonNoFileSelectedLabel() { return "No file selected"; }
String WebCore::contextMenuItemTagOpenLinkInNewWindow() { return String(); }
String WebCore::contextMenuItemTagDownloadLinkToDisk() { return String(); }
String WebCore::contextMenuItemTagCopyLinkToClipboard() { return String(); }
String WebCore::contextMenuItemTagOpenImageInNewWindow() { return String(); }
String WebCore::contextMenuItemTagDownloadImageToDisk() { return String(); }
String WebCore::contextMenuItemTagCopyImageToClipboard() { return String(); }
String WebCore::contextMenuItemTagOpenFrameInNewWindow() { return String(); }
String WebCore::contextMenuItemTagCopy() { return String(); }
String WebCore::contextMenuItemTagGoBack() { return String(); }
String WebCore::contextMenuItemTagGoForward() { return String(); }
String WebCore::contextMenuItemTagStop() { return String(); }
String WebCore::contextMenuItemTagReload() { return String(); }
String WebCore::contextMenuItemTagCut() { return String(); }
String WebCore::contextMenuItemTagPaste() { return String(); }
String WebCore::contextMenuItemTagNoGuessesFound() { return String(); }
String WebCore::contextMenuItemTagIgnoreSpelling() { return String(); }
String WebCore::contextMenuItemTagLearnSpelling() { return String(); }
String WebCore::contextMenuItemTagSearchWeb() { return String(); }
String WebCore::contextMenuItemTagLookUpInDictionary() { return String(); }
String WebCore::contextMenuItemTagOpenLink() { return String(); }
String WebCore::contextMenuItemTagIgnoreGrammar() { return String(); }
String WebCore::contextMenuItemTagSpellingMenu() { return String(); }
String WebCore::contextMenuItemTagShowSpellingPanel(bool show) { return String(); }
String WebCore::contextMenuItemTagCheckSpelling() { return String(); }
String WebCore::contextMenuItemTagCheckSpellingWhileTyping() { return String(); }
String WebCore::contextMenuItemTagCheckGrammarWithSpelling() { return String(); }
String WebCore::contextMenuItemTagFontMenu() { return String(); }
String WebCore::contextMenuItemTagBold() { return String(); }
String WebCore::contextMenuItemTagItalic() { return String(); }
String WebCore::contextMenuItemTagUnderline() { return String(); }
String WebCore::contextMenuItemTagOutline() { return String(); }
String WebCore::contextMenuItemTagWritingDirectionMenu() { return String(); }
String WebCore::contextMenuItemTagDefaultDirection() { return String(); }
String WebCore::contextMenuItemTagLeftToRight() { return String(); }
String WebCore::contextMenuItemTagRightToLeft() { return String(); }
String WebCore::contextMenuItemTagInspectElement() { return String(); }
String WebCore::searchMenuNoRecentSearchesText() { return String("No recent searches"); }
String WebCore::searchMenuRecentSearchesText() { return String("Recent searches"); }
String WebCore::searchMenuClearRecentSearchesText() { return String("Clear recent searches"); }
String WebCore::unknownFileSizeText() { return "Unknown"; }

PluginInfo* PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplemented(); return 0; }
bool WebCore::PlugInInfoStore::supportsMIMEType(const WebCore::String&) { notImplemented(); return false; }
void WebCore::refreshPlugins(bool) { notImplemented(); }

void SearchPopupMenu::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems) { notImplemented(); }
void SearchPopupMenu::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems) { notImplemented(); }
SearchPopupMenu::SearchPopupMenu(PopupMenuClient* client) : PopupMenu(client) { notImplemented(); }
bool SearchPopupMenu::enabled() { notImplemented(); return true; }

FileChooser::FileChooser(FileChooserClient*, const String&) { notImplemented(); }
FileChooser::~FileChooser() { notImplemented(); }
void FileChooser::openFileChooser(Document*) { notImplemented(); }
String FileChooser::basenameForWidth(const Font&, int width) const { notImplemented(); return String(); }

Color WebCore::focusRingColor() { return 0xFF0000FF; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { }

void Frame::setNeedsReapplyStyles() { }

bool ResourceHandle::willLoadFromCache(ResourceRequest&) { notImplemented(); return false; }
bool ResourceHandle::loadsBlocked() { notImplemented(); return false; }
void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& data) { notImplemented(); }

Icon::Icon() { notImplemented(); }
Icon::~Icon() { notImplemented(); }
PassRefPtr<Icon> Icon::newIconForFile(const String& filename) { notImplemented(); return PassRefPtr<Icon>(new Icon()); }
void Icon::paint(GraphicsContext*, const IntRect&) { notImplemented(); }

FloatRect Font::selectionRectForComplexText(const TextRun&, const TextStyle&, const IntPoint&, int, int, int) const { return FloatRect(); notImplemented(); }
void Font::drawComplexText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&, int from, int to) const { notImplemented(); }
float Font::floatWidthForComplexText(const TextRun&, const TextStyle&) const { notImplemented(); return 0; }
int Font::offsetForPositionForComplexText(const TextRun&, const TextStyle&, int, bool) const { notImplemented(); return 0; }

void CachedPage::close() { notImplemented(); }

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy) { notImplemented(); return 0; }

Pasteboard* Pasteboard::generalPasteboard() { notImplemented(); return 0; }
void Pasteboard::writeSelection(Range*, bool, Frame*) { notImplemented(); }
void Pasteboard::writeURL(const KURL&, const String&, Frame*) { notImplemented(); }
void Pasteboard::writeImage(Node*, const KURL&, const String&) { notImplemented(); }
void Pasteboard::clear() { notImplemented(); }
bool Pasteboard::canSmartReplace() { notImplemented(); return false; }
PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame*, PassRefPtr<Range>, bool allowPlainText, bool& chosePlainText) { notImplemented(); return 0; }
String Pasteboard::plainText(Frame* frame) { notImplemented(); return String(); }
Pasteboard::Pasteboard() { notImplemented(); }
Pasteboard::~Pasteboard() { notImplemented(); }

ContextMenu::ContextMenu(const HitTestResult& result) : m_hitTestResult(result) { notImplemented(); }
ContextMenu::~ContextMenu() { notImplemented(); }
void ContextMenu::appendItem(ContextMenuItem&) { notImplemented(); }
void ContextMenu::setPlatformDescription(PlatformMenuDescription menu) { m_platformDescription = menu; }
PlatformMenuDescription ContextMenu::platformDescription() const  { return m_platformDescription; }

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription) { notImplemented(); }
ContextMenuItem::ContextMenuItem(ContextMenu*) { notImplemented(); }
ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu) { notImplemented(); }
ContextMenuItem::~ContextMenuItem() { notImplemented(); }
PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription() { notImplemented(); return m_platformDescription; }
ContextMenuItemType ContextMenuItem::type() const { notImplemented(); return ActionType; }
void ContextMenuItem::setType(ContextMenuItemType) { notImplemented(); }
ContextMenuAction ContextMenuItem::action() const { notImplemented(); return ContextMenuItemTagNoAction; }
void ContextMenuItem::setAction(ContextMenuAction) { notImplemented(); }
String ContextMenuItem::title() const { notImplemented(); return String(); }
void ContextMenuItem::setTitle(const String&) { notImplemented(); }
PlatformMenuDescription ContextMenuItem::platformSubMenu() const { notImplemented(); return 0; }
void ContextMenuItem::setSubMenu(ContextMenu*) { notImplemented(); }
void ContextMenuItem::setChecked(bool) { notImplemented(); }
void ContextMenuItem::setEnabled(bool) { notImplemented(); }

FTPDirectoryDocument::FTPDirectoryDocument(WebCore::DOMImplementation* i, WebCore::Frame* f) : HTMLDocument(i, f) { notImplemented(); }
Tokenizer* FTPDirectoryDocument::createTokenizer() { notImplemented(); return 0; }

namespace WebCore {
Vector<String> supportedKeySizes() { notImplemented(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }
float userIdleTime() { notImplemented(); return 0.0; }
}

