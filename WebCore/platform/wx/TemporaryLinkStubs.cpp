/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include "Node.h"
#include "Font.h"
#include "IntPoint.h"
#include "Widget.h"
#include "GraphicsContext.h"
#include "loader.h"
#include "FrameView.h"
#include "KURL.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "CookieJar.h"
#include "Screen.h"
#include "GlobalHistory.h"
#include "Language.h"
#include "LocalizedStrings.h"
#include "PlugInInfoStore.h"
#include "RenderTheme.h"
#include "TextBoundaries.h"
#include "SharedTimer.h"
#include "ScrollBar.h"
#include "PlatformScrollBar.h"
#include "IconLoader.h"
#include "IconDatabase.h"
//#include "ResourceLoaderClient.h"
#include "CachedResource.h"
//#include "Slider.h"
//#include "TextField.h"
//#include "ListBox.h"
#include "FileChooser.h"
#include "Cursor.h"
#include "PopupMenu.h"
#include "AXObjectCache.h"
#include "Icon.h"
//#include "LoaderFunctions.h"
#include "EditCommand.h"
#include "Pasteboard.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "PlatformMenuDescription.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "Clipboard.h"
#include "AffineTransform.h"
#include "FrameLoader.h"
#include "EventHandler.h"
#include "SearchPopupMenu.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "ResourceHandleInternal.h"
#include "DocumentLoader.h"
#include "PageCache.h"
#include "BitmapImage.h"
#include "DragController.h"
#include "NotImplemented.h"

using namespace WebCore;

Vector<char> loadResourceIntoArray(const char* resourceName)
{
    Vector<char> resource;
    //if(strcmp(resourceName,"missingImage") == 0 ) {
    //}
    return resource;
}

// FrameView functions
void FrameView::updateBorder() { notImplemented(); }

void Widget::enableFlushDrawing() { notImplemented(); }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void Widget::disableFlushDrawing() { notImplemented(); }
GraphicsContext* Widget::lockDrawingFocus() { notImplemented(); return 0; }
void Widget::unlockDrawingFocus(GraphicsContext*) { notImplemented(); }
void Widget::removeFromParent() { notImplemented(); }

int WebCore::findNextSentenceFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }
void WebCore::findSentenceBoundary(UChar const*,int,int,int*,int*) { notImplemented(); }
int WebCore::findNextWordFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }

void Frame::print() { notImplemented(); }
void Frame::clearPlatformScriptObjects() { notImplemented(); }
bool Frame::isCharacterSmartReplaceExempt(UChar, bool) { notImplemented(); return true; }
//void Frame::respondToChangedSelection(WebCore::Selection const&,bool) { }
void Frame::issueTransposeCommand() { notImplemented(); }
DragImageRef Frame::dragImageForSelection() { notImplemented(); return 0; }

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
    const IntSize& bottomLeft, const IntSize& bottomRight) { notImplemented(); }
void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness) { notImplemented(); }

// cookies (we'll need a place to store these
void WebCore::setCookies(const KURL& url, const KURL& policyURL, const String& value) { notImplemented(); }
String WebCore::cookies(const KURL& url) { notImplemented(); return String(); }
bool WebCore::cookiesEnabled() { notImplemented(); return false; }

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
static WebCore::Cursor localCursor;
const WebCore::Cursor& WebCore::moveCursor() { return localCursor; }

bool AXObjectCache::gAccessibilityEnabled = false;

bool WebCore::historyContains(DeprecatedString const&) { return false; }

void WebCore::findWordBoundary(UChar const* str,int len,int position,int* start, int* end) { notImplemented(); *start=position; *end=position; }

PluginInfo*PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplemented(); return 0; }
bool WebCore::PlugInInfoStore::supportsMIMEType(const WebCore::String&) { notImplemented(); return false; }
void WebCore::refreshPlugins(bool) { notImplemented(); }

void Widget::setIsSelected(bool) { notImplemented(); }

void GraphicsContext::setShadow(IntSize const&,int,Color const&) { notImplemented(); }
void GraphicsContext::clearShadow() { notImplemented(); }
void GraphicsContext::beginTransparencyLayer(float) { notImplemented(); }
void GraphicsContext::endTransparencyLayer() { notImplemented(); }
void GraphicsContext::clearRect(const FloatRect&) { notImplemented(); }
void GraphicsContext::strokeRect(const FloatRect&, float) { notImplemented(); }
void GraphicsContext::setLineCap(LineCap) { notImplemented(); }
void GraphicsContext::setLineJoin(LineJoin) { notImplemented(); }
void GraphicsContext::setMiterLimit(float) { notImplemented(); }
void GraphicsContext::setAlpha(float) { notImplemented(); }

Color WebCore::focusRingColor() { return 0xFF0000FF; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { }

void Frame::setNeedsReapplyStyles() { notImplemented(); }
void Image::drawPattern(GraphicsContext*, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, CompositeOperator, const FloatRect& destRect) { notImplemented(); } 

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize) : Scrollbar(client, orientation, controlSize) { notImplemented(); }
PlatformScrollbar::~PlatformScrollbar() { notImplemented(); }
int PlatformScrollbar::width() const { notImplemented(); return 0; }
int PlatformScrollbar::height() const { notImplemented(); return 0; }
void PlatformScrollbar::setEnabled(bool) { notImplemented(); }
void PlatformScrollbar::paint(GraphicsContext*, const IntRect& damageRect) { notImplemented(); }
void PlatformScrollbar::updateThumbPosition() { notImplemented(); }
void PlatformScrollbar::updateThumbProportion() { notImplemented(); }
void PlatformScrollbar::setRect(const IntRect&) { notImplemented(); }

//bool IconDatabase::isIconExpiredForIconURL(const String& url) { return false; }
//bool IconDatabase::hasEntryForIconURL(const String& url) { return false; }
//IconDatabase* IconDatabase::sharedIconDatabase() { return 0; }
//bool IconDatabase::setIconURLForPageURL(const String& iconURL, const String& pageURL) { return false; }


//const PlatformMouseEvent::CurrentEventTag PlatformMouseEvent::currentEvent = {};
//PlatformMouseEvent::PlatformMouseEvent(const CurrentEventTag&) { notImplemented(); }


FileChooser::FileChooser(FileChooserClient*, const String& initialFilename) { notImplemented(); }
//PassRefPtr<FileChooser> FileChooser::create(FileChooserClient*, const String& initialFilename) { notImplemented(); return PassRefPtr<FileChooser>(); }
FileChooser::~FileChooser() { notImplemented(); }
void FileChooser::openFileChooser(Document*) { notImplemented(); }
String FileChooser::basenameForWidth(const Font&, int width) const { notImplemented(); return String(); }

PopupMenu::PopupMenu(PopupMenuClient*) { notImplemented(); }

PopupMenu::~PopupMenu() { notImplemented(); }
void PopupMenu::show(const IntRect&, FrameView*, int index) { notImplemented(); }
void PopupMenu::hide() { notImplemented(); }
void PopupMenu::updateFromElement() { notImplemented(); }
bool PopupMenu::itemWritingDirectionIsNatural() { notImplemented(); return false; }

Icon::Icon() { notImplemented(); }
Icon::~Icon() { notImplemented(); }
PassRefPtr<Icon> Icon::newIconForFile(const String& filename) { notImplemented(); return PassRefPtr<Icon>(new Icon()); }
void Icon::paint(GraphicsContext*, const IntRect&) { notImplemented(); }

ContextMenu::ContextMenu(const HitTestResult& result) : m_hitTestResult(result) { notImplemented(); }
ContextMenu::~ContextMenu() { notImplemented(); }
void ContextMenu::appendItem(ContextMenuItem&) { notImplemented(); }
void ContextMenu::setPlatformDescription(PlatformMenuDescription) { notImplemented(); }

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
//PlatformMenuDescription ContextMenuItem::platformSubMenu() const { notImplemented(); return 0; }
void ContextMenuItem::setSubMenu(ContextMenu*) { notImplemented(); }
void ContextMenuItem::setChecked(bool) { notImplemented(); }
void ContextMenuItem::setEnabled(bool) { notImplemented(); }

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

void Editor::ignoreSpelling() { notImplemented(); }
void Editor::learnSpelling() { notImplemented(); }
bool Editor::isSelectionUngrammatical() { notImplemented(); return false; }
bool Editor::isSelectionMisspelled() { notImplemented(); return false; }
Vector<String> Editor::guessesForMisspelledSelection() { notImplemented(); return Vector<String>(); }
Vector<String> Editor::guessesForUngrammaticalSelection() { notImplemented(); return Vector<String>(); }
void Editor::markMisspellingsAfterTypingToPosition(const VisiblePosition&) { notImplemented(); }
void Editor::showColorPanel() { notImplemented(); }
void Editor::showFontPanel() { notImplemented(); }
void Editor::showStylesPanel() { notImplemented(); }
void Editor::showSpellingGuessPanel() { notImplemented(); }
void Editor::advanceToNextMisspelling(bool) { notImplemented(); }

String FrameLoader::overrideMediaType() const { notImplemented(); return String(); }

bool EventHandler::tabsToAllControls(KeyboardEvent* event) const { notImplemented(); return false; }
bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe) { notImplemented(); return false; }
bool EventHandler::passMouseDownEventToWidget(Widget*) { notImplemented(); return false; }
bool EventHandler::passWheelEventToWidget(Widget*) { notImplemented(); return false; }

// FIXME: All this IconDatabase stuff could go away if much of
// WebCore/loader/icon was linked in.  Unfortunately that requires SQLite,
// which isn't currently part of the build.
//Image* IconDatabase::iconForPageURL(const String&, const IntSize&, bool cache) { notImplemented(); return 0; }
//Image* IconDatabase::defaultIcon(const IntSize&) { notImplemented(); return 0; }
//void IconDatabase::retainIconForPageURL(const String&) { notImplemented(); }
//void IconDatabase::releaseIconForPageURL(const String&) { notImplemented(); }
//void IconDatabase::setIconDataForIconURL(PassRefPtr<SharedBuffer> data, const String&) { notImplemented(); }

void SearchPopupMenu::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems) { notImplemented(); }
void SearchPopupMenu::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems) { notImplemented(); }
SearchPopupMenu::SearchPopupMenu(PopupMenuClient* client) : PopupMenu(client) { notImplemented(); }
bool SearchPopupMenu::enabled() { return true; }

// bool SharedBuffer::hasPlatformData() const { notImplemented(); return false; }
// const char* SharedBuffer::platformData() const { notImplemented(); return NULL; }
// unsigned SharedBuffer::platformDataSize() const { notImplemented(); return 0; }
// void SharedBuffer::maybeTransferPlatformData() { notImplemented(); }
// void SharedBuffer::clearPlatformData() { notImplemented(); }

//const KURL DocumentLoader::unreachableURL() const { notImplemented(); return KURL(); }
bool DocumentLoader::getResponseModifiedHeader(WebCore::String&) const { notImplemented(); return false; }

void PageCache::close() { notImplemented(); }


namespace WebCore {
float userIdleTime() { notImplemented(); return 0; }
Vector<String> supportedKeySizes() { notImplemented(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }
}
