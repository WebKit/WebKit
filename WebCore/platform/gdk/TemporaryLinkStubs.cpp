/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "CachedResource.h"
#include "ChromeClientGdk.h"
#include "CookieJar.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "Clipboard.h"
#include "CString.h"
#include "Cursor.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "Editor.h"
#include "FileChooser.h"
#include "Font.h"
#include "FrameGdk.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGdk.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "History.h"
#include "Icon.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "loader.h"
#include "LocalizedStrings.h"
#include "MainResourceLoader.h"
#include "Node.h"
#include "NotImplementedGdk.h"
#include "PageCache.h"
#include "Pasteboard.h"
#include "PlatformMouseEvent.h"
#include "PlatformScrollBar.h"
#include "PlugInInfoStore.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "SearchPopupMenu.h"
#include "TextBoundaries.h"
#include "Widget.h"
#include <gtk/gtk.h>
#include <stdio.h>

using namespace WebCore;

namespace WebCore {
class Page;
}

void FrameView::updateBorder() { notImplementedGdk(); }

void Widget::setEnabled(bool) { notImplementedGdk(); }
bool Widget::isEnabled() const { notImplementedGdk(); return false; }
Widget::FocusPolicy Widget::focusPolicy() const { notImplementedGdk(); return NoFocus; }
void Widget::enableFlushDrawing() { notImplementedGdk(); }
void Widget::disableFlushDrawing() { notImplementedGdk(); }
GraphicsContext* Widget::lockDrawingFocus() { notImplementedGdk(); return 0; }
void Widget::unlockDrawingFocus(GraphicsContext*) { notImplementedGdk(); }
void Widget::removeFromParent() { notImplementedGdk(); }
void Widget::paint(GraphicsContext*, IntRect const&) { notImplementedGdk(); }
void Widget::setIsSelected(bool) { notImplementedGdk(); }
void Widget::invalidate() { notImplementedGdk(); }
void Widget::invalidateRect(const IntRect&) { notImplementedGdk(); }

int WebCore::findNextSentenceFromIndex(UChar const*, int, int, bool) { notImplementedGdk(); return 0; }
void WebCore::findSentenceBoundary(UChar const*, int, int, int*, int*) { notImplementedGdk(); }
int WebCore::findNextWordFromIndex(UChar const*, int, int, bool) { notImplementedGdk(); return 0; }
void WebCore::findWordBoundary(UChar const* str, int len, int position, int* start, int* end) {*start = position; *end = position; }

void ChromeClientGdk::chromeDestroyed() { notImplementedGdk(); }
FloatRect ChromeClientGdk::windowRect() { notImplementedGdk(); return FloatRect(); }
void ChromeClientGdk::setWindowRect(const FloatRect& r) {notImplementedGdk(); }
FloatRect ChromeClientGdk::pageRect() { notImplementedGdk(); return FloatRect(); }
float ChromeClientGdk::scaleFactor() { notImplementedGdk(); return 1.0; }
void ChromeClientGdk::focus() { notImplementedGdk(); }
void ChromeClientGdk::unfocus() { notImplementedGdk(); }
WebCore::Page* ChromeClientGdk::createWindow(const FrameLoadRequest&) { notImplementedGdk(); return 0; }
WebCore::Page* ChromeClientGdk::createModalDialog(const FrameLoadRequest&) { notImplementedGdk(); return 0;}
void ChromeClientGdk::show() { notImplementedGdk(); }
bool ChromeClientGdk::canRunModal() { notImplementedGdk(); return false; }
void ChromeClientGdk::runModal() { notImplementedGdk(); }
void ChromeClientGdk::setToolbarsVisible(bool) { notImplementedGdk(); }
bool ChromeClientGdk::toolbarsVisible() { notImplementedGdk(); return false; }
void ChromeClientGdk::setStatusbarVisible(bool) { notImplementedGdk(); }
bool ChromeClientGdk::statusbarVisible() { notImplementedGdk(); return false; }
void ChromeClientGdk::setScrollbarsVisible(bool) { notImplementedGdk(); }
bool ChromeClientGdk::scrollbarsVisible() { notImplementedGdk(); return false; }
void ChromeClientGdk::setMenubarVisible(bool) { notImplementedGdk(); }
bool ChromeClientGdk::menubarVisible() { notImplementedGdk(); return false; }
void ChromeClientGdk::setResizable(bool) { notImplementedGdk(); }
void ChromeClientGdk::closeWindowSoon() { notImplementedGdk(); }
bool ChromeClientGdk::canTakeFocus(FocusDirection) { notImplementedGdk(); return true; }
void ChromeClientGdk::takeFocus(FocusDirection) { notImplementedGdk(); }
bool ChromeClientGdk::canRunBeforeUnloadConfirmPanel() { notImplementedGdk(); return false; }
void ChromeClientGdk::addMessageToConsole(const WebCore::String&, unsigned int, const WebCore::String&) { notImplementedGdk(); }
bool ChromeClientGdk::runBeforeUnloadConfirmPanel(const WebCore::String&, WebCore::Frame*) { notImplementedGdk(); return false; }
void ChromeClientGdk::runJavaScriptAlert(Frame*, const String&)
{
    notImplementedGdk();
}
bool ChromeClientGdk::runJavaScriptConfirm(Frame*, const String&)
{
    notImplementedGdk();
    return false;
}
bool ChromeClientGdk::runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result)
{
    notImplementedGdk();
    return false;
}
void ChromeClientGdk::setStatusbarText(const String&)
{
    notImplementedGdk();
}

bool ChromeClientGdk::shouldInterruptJavaScript()
{
    notImplementedGdk();
    return false;
}

bool ChromeClientGdk::tabsToLinks() const
{
    notImplementedGdk();
    return false;
}

IntRect ChromeClientGdk::windowResizerRect() const
{
    notImplementedGdk();
    return IntRect();
}

void ChromeClientGdk::addToDirtyRegion(const IntRect&)
{
    notImplementedGdk();
}

void ChromeClientGdk::scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect)
{
    notImplementedGdk();
}

void ChromeClientGdk::updateBackingStore()
{
    notImplementedGdk();
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
String WebCore::searchMenuNoRecentSearchesText() { return String("No recent searches"); }
String WebCore::searchMenuRecentSearchesText() { return String("Recent searches"); }
String WebCore::searchMenuClearRecentSearchesText() { return String("Clear recent searches"); }

PluginInfo* PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplementedGdk(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplementedGdk(); return 0; }
bool WebCore::PlugInInfoStore::supportsMIMEType(const WebCore::String&) { notImplementedGdk(); return false; }
void WebCore::refreshPlugins(bool) { notImplementedGdk(); }

void SearchPopupMenu::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems) { notImplementedGdk(); }
void SearchPopupMenu::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems) { notImplementedGdk(); }
SearchPopupMenu::SearchPopupMenu(PopupMenuClient* client) : PopupMenu(client) { notImplementedGdk(); }
bool SearchPopupMenu::enabled() { notImplementedGdk(); return true; }

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize) : Scrollbar(client, orientation, controlSize) { notImplementedGdk(); }
PlatformScrollbar::~PlatformScrollbar() { notImplementedGdk(); }
int PlatformScrollbar::width() const { return 15; }
int PlatformScrollbar::height() const { return 15; }
void PlatformScrollbar::setEnabled(bool) { notImplementedGdk(); }
void PlatformScrollbar::paint(GraphicsContext*, const IntRect& damageRect) { notImplementedGdk(); }
void PlatformScrollbar::updateThumbPosition() { notImplementedGdk(); }
void PlatformScrollbar::updateThumbProportion() { notImplementedGdk(); }
void PlatformScrollbar::setRect(const IntRect&) { notImplementedGdk(); }

FileChooser::FileChooser(FileChooserClient*, const String&) { notImplementedGdk(); }
FileChooser::~FileChooser() { notImplementedGdk(); }
void FileChooser::openFileChooser(Document*) { notImplementedGdk(); }
String FileChooser::basenameForWidth(const Font&, int width) const { notImplementedGdk(); return String(); }

Color WebCore::focusRingColor() { return 0xFF0000FF; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { }

void Frame::setNeedsReapplyStyles() { }

bool ResourceHandle::willLoadFromCache(ResourceRequest&) { notImplementedGdk(); return false; }
bool ResourceHandle::loadsBlocked() { notImplementedGdk(); return false; }
void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& data) { notImplementedGdk(); }

Icon::Icon() { notImplementedGdk(); }
Icon::~Icon() { notImplementedGdk(); }
PassRefPtr<Icon> Icon::newIconForFile(const String& filename) { notImplementedGdk(); return PassRefPtr<Icon>(new Icon()); }
void Icon::paint(GraphicsContext*, const IntRect&) { notImplementedGdk(); }

FloatRect Font::selectionRectForComplexText(const TextRun&, const TextStyle&, const IntPoint&, int, int, int) const { return FloatRect(); notImplementedGdk(); }
void Font::drawComplexText(GraphicsContext*, const TextRun&, const TextStyle&, const FloatPoint&, int from, int to) const { notImplementedGdk(); }
float Font::floatWidthForComplexText(const TextRun&, const TextStyle&) const { notImplementedGdk(); return 0; }
int Font::offsetForPositionForComplexText(const TextRun&, const TextStyle&, int, bool) const { notImplementedGdk(); return 0; }

void PageCache::close() { notImplementedGdk(); }

void Editor::ignoreSpelling() { notImplementedGdk(); }
void Editor::learnSpelling() { notImplementedGdk(); }
bool Editor::isSelectionUngrammatical() { notImplementedGdk(); return false; }
bool Editor::isSelectionMisspelled() { notImplementedGdk(); return false; }
Vector<String> Editor::guessesForMisspelledSelection() { notImplementedGdk(); return Vector<String>(); }
Vector<String> Editor::guessesForUngrammaticalSelection() { notImplementedGdk(); return Vector<String>(); }
void Editor::markMisspellingsAfterTypingToPosition(const VisiblePosition&) { notImplementedGdk(); }
PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy) { notImplementedGdk(); return 0; }
void Editor::markMisspellings(const Selection&) { notImplementedGdk(); }

Pasteboard* Pasteboard::generalPasteboard() { notImplementedGdk(); return 0; }
void Pasteboard::writeSelection(Range*, bool, Frame*) { notImplementedGdk(); }
void Pasteboard::writeURL(const KURL&, const String&, Frame*) { notImplementedGdk(); }
void Pasteboard::writeImage(const HitTestResult&) { notImplementedGdk(); }
void Pasteboard::clear() { notImplementedGdk(); }
bool Pasteboard::canSmartReplace() { notImplementedGdk(); return false; }
PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame*, PassRefPtr<Range>, bool allowPlainText, bool& chosePlainText) { notImplementedGdk(); return 0; }
String Pasteboard::plainText(Frame* frame) { notImplementedGdk(); return String(); }
Pasteboard::Pasteboard() { notImplementedGdk(); }
Pasteboard::~Pasteboard() { notImplementedGdk(); }

ContextMenu::ContextMenu(const HitTestResult& result) : m_hitTestResult(result) { notImplementedGdk(); }
ContextMenu::~ContextMenu() { notImplementedGdk(); }
void ContextMenu::appendItem(ContextMenuItem&) { notImplementedGdk(); }
void ContextMenu::setPlatformDescription(PlatformMenuDescription menu) { m_platformDescription = menu; }
PlatformMenuDescription ContextMenu::platformDescription() const  { return m_platformDescription; }

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription) { notImplementedGdk(); }
ContextMenuItem::ContextMenuItem(ContextMenu*) { notImplementedGdk(); }
ContextMenuItem::ContextMenuItem(ContextMenuItemType type, ContextMenuAction action, const String& title, ContextMenu* subMenu) { notImplementedGdk(); }
ContextMenuItem::~ContextMenuItem() { notImplementedGdk(); }
PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription() { notImplementedGdk(); return m_platformDescription; }
ContextMenuItemType ContextMenuItem::type() const { notImplementedGdk(); return ActionType; }
void ContextMenuItem::setType(ContextMenuItemType) { notImplementedGdk(); }
ContextMenuAction ContextMenuItem::action() const { notImplementedGdk(); return ContextMenuItemTagNoAction; }
void ContextMenuItem::setAction(ContextMenuAction) { notImplementedGdk(); }
String ContextMenuItem::title() const { notImplementedGdk(); return String(); }
void ContextMenuItem::setTitle(const String&) { notImplementedGdk(); }
PlatformMenuDescription ContextMenuItem::platformSubMenu() const { notImplementedGdk(); return 0; }
void ContextMenuItem::setSubMenu(ContextMenu*) { notImplementedGdk(); }
void ContextMenuItem::setChecked(bool) { notImplementedGdk(); }
void ContextMenuItem::setEnabled(bool) { notImplementedGdk(); }

namespace WebCore {
Vector<String> supportedKeySizes() { notImplementedGdk(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }
void systemBeep() { notImplementedGdk(); }
float userIdleTime() { notImplementedGdk(); return 0.0; }
}

