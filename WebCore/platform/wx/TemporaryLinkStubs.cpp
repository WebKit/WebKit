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

#include "AffineTransform.h"
#include "AXObjectCache.h"
#include "BitmapImage.h"
#include "CachedResource.h"
#include "Clipboard.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "CookieJar.h"
#include "Cursor.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DragController.h"
#include "Editor.h"
#include "EditCommand.h"
#include "EventHandler.h"
#include "FileChooser.h"
#include "Font.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "Icon.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "GraphicsContext.h"
#include "History.h"
#include "KURL.h"
#include "Language.h"
#include "loader.h"
#include "Node.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "Path.h"
#include "PlatformMenuDescription.h"
#include "PlatformMouseEvent.h"
#include "PlatformScrollBar.h"
#include "PluginInfoStore.h"
#include "PopupMenu.h"
#include "RenderTheme.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "ResourceLoader.h"
#include "Screen.h"
#include "SearchPopupMenu.h"
#include "ScrollBar.h"
#include "SharedBuffer.h"
#include "SharedTimer.h"
#include "TextBoundaries.h"
#include "Widget.h"

using namespace WebCore;

Vector<char> loadResourceIntoArray(const char* resourceName)
{
    Vector<char> resource;
    return resource;
}

void Widget::removeFromParent() { notImplemented(); }


int findNextSentenceFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }
void findSentenceBoundary(UChar const*,int,int,int*,int*) { notImplemented(); }

int WebCore::findNextWordFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }

void Frame::clearPlatformScriptObjects() { notImplemented(); }

void Frame::dashboardRegionsChanged() { notImplemented(); }
DragImageRef Frame::dragImageForSelection() { notImplemented(); return 0; }

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness) { notImplemented(); }

// cookies (we'll need a place to store these
void WebCore::setCookies(Document* document, const KURL& url, const KURL& policyURL, const String& value) { notImplemented(); }
String WebCore::cookies(const Document* document, const KURL& url) { notImplemented(); return String(); }
bool WebCore::cookiesEnabled(const Document* document) { notImplemented(); return false; }

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
static WebCore::Cursor localCursor;
const WebCore::Cursor& WebCore::moveCursor() { return localCursor; }

namespace WebCore {
    bool historyContains(const UChar*, unsigned) { return false; }
}

void WebCore::findWordBoundary(UChar const* str,int len,int position,int* start, int* end) { notImplemented(); *start=position; *end=position; }

PluginInfo*PluginInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PluginInfoStore::pluginCount() const { notImplemented(); return 0; }
bool WebCore::PluginInfoStore::supportsMIMEType(const WebCore::String&) { notImplemented(); return false; }
String PluginInfoStore::pluginNameForMIMEType(const String& mimeType) { notImplemented(); return String(); }
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

void Editor::showColorPanel() { notImplemented(); }
void Editor::showFontPanel() { notImplemented(); }
void Editor::showStylesPanel() { notImplemented(); }

bool EventHandler::tabsToAllControls(KeyboardEvent* event) const { notImplemented(); return false; }
bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe, HitTestResult*) { notImplemented(); return false; }
bool EventHandler::passMouseDownEventToWidget(Widget*) { notImplemented(); return false; }
bool EventHandler::passWheelEventToWidget(PlatformWheelEvent&, Widget*) { notImplemented(); return false; }

void SearchPopupMenu::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems) { notImplemented(); }
void SearchPopupMenu::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems) { notImplemented(); }
SearchPopupMenu::SearchPopupMenu(PopupMenuClient* client) : PopupMenu(client) { notImplemented(); }
bool SearchPopupMenu::enabled() { return true; }

namespace WebCore {
float userIdleTime() { notImplemented(); return 0; }
Vector<String> supportedKeySizes() { notImplemented(); return Vector<String>(); }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }
const char* currentTextBreakLocaleID() { notImplemented(); return "en_us"; }

String KURL::fileSystemPath() const { notImplemented(); return String(); }

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String&) { notImplemented(); return 0; }
}
