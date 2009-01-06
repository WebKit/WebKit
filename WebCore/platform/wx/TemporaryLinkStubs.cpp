/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
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
#include <float.h>

#include "TransformationMatrix.h"
#include "AXObjectCache.h"
#include "BitmapImage.h"
#include "CachedResource.h"
#include "Clipboard.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "CookieJar.h"
#include "Cursor.h"
#include "DNS.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "DragController.h"
#include "Editor.h"
#include "EditCommand.h"
#include "EventHandler.h"
#include "FileChooser.h"
#include "Font.h"
#include "Frame.h"
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
#include "PopupMenu.h"
#include "RenderTheme.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "ResourceLoader.h"
#include "Screen.h"
#include "ScrollbarTheme.h"
#include "SearchPopupMenu.h"
#include "Scrollbar.h"
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

int findNextSentenceFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }
void findSentenceBoundary(UChar const*,int,int,int*,int*) { notImplemented(); }

int WebCore::findNextWordFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }

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

void WebCore::findWordBoundary(UChar const* str,int len,int position,int* start, int* end) { notImplemented(); *start=position; *end=position; }

void Widget::setIsSelected(bool) { notImplemented(); }

void GraphicsContext::setPlatformShadow(IntSize const&,int,Color const&) { notImplemented(); }
void GraphicsContext::clearPlatformShadow() { notImplemented(); }
void GraphicsContext::beginTransparencyLayer(float) { notImplemented(); }
void GraphicsContext::endTransparencyLayer() { notImplemented(); }
void GraphicsContext::clearRect(const FloatRect&) { notImplemented(); }
void GraphicsContext::strokeRect(const FloatRect&, float) { notImplemented(); }
void GraphicsContext::setLineCap(LineCap) { notImplemented(); }
void GraphicsContext::setLineJoin(LineJoin) { notImplemented(); }
void GraphicsContext::setMiterLimit(float) { notImplemented(); }
void GraphicsContext::setAlpha(float) { notImplemented(); }

Color WebCore::focusRingColor() { return 0xFF0000FF; }

void Image::drawPattern(GraphicsContext*, const FloatRect& srcRect, const TransformationMatrix& patternTransform, const FloatPoint& phase, CompositeOperator, const FloatRect& destRect) { notImplemented(); } 

ScrollbarTheme* ScrollbarTheme::nativeTheme() { notImplemented(); static ScrollbarTheme theme; return &theme; }

String FileChooser::basenameForWidth(const Font&, int width) const { notImplemented(); return String(); }

Icon::~Icon() { }
PassRefPtr<Icon> Icon::createIconForFile(const String& filename) { notImplemented(); return 0; }
PassRefPtr<Icon> Icon::createIconForFiles(const Vector<String>& filenames) { notImplemented(); return 0; }
void Icon::paint(GraphicsContext*, const IntRect&) { notImplemented(); }

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
float userIdleTime() { notImplemented(); return FLT_MAX; } // return an arbitrarily high userIdleTime so that releasing pages from the page cache isn't postponed
void getSupportedKeySizes(Vector<String>&) { notImplemented(); }
String signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String &challengeString, const KURL &url) { return String(); }
const char* currentTextBreakLocaleID() { notImplemented(); return "en_us"; }

String KURL::fileSystemPath() const { notImplemented(); return String(); }

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String&) { notImplemented(); return 0; }

void prefetchDNS(const String& hostname) { notImplemented(); }

}
