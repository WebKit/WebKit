/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

#define WIN32_COMPILE_HACK

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "AXObjectCache.h"
#include "BitmapImage.h"
#include "CachedResource.h"
#include "Clipboard.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "CookieJar.h"
#include "CString.h"
#include "Cursor.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "Editor.h"
#include "EventHandler.h"
#include "FileChooser.h"
#include "Font.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "History.h"
#include "HTMLFormElement.h"
#include "Icon.h"
#include "IconLoader.h"
#include "IconDatabase.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "loader.h"
#include "LocalizedStrings.h"
#include "Node.h"
#include "Page.h"
#include "PageCache.h"
#include "Pasteboard.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "PlatformScrollBar.h"
#include "PlugInInfoStore.h"
#include "PopupMenu.h"
#include "Range.h"
#include "RenderTheme.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "RenderThemeWin.h"
#include "Screen.h"
#include "ScrollBar.h"
#include "SearchPopupMenu.h"
#include "TextBoundaries.h"
#include "Widget.h"

#define notImplemented() do { \
    char buf[256] = {0}; \
    _snprintf(buf, sizeof(buf), "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); \
    OutputDebugStringA(buf); \
} while (0)

namespace WebCore {

static int frameNumber = 0;
static Cursor localCursor;

String contextMenuItemTagOpenLinkInNewWindow() { notImplemented(); }
String contextMenuItemTagDownloadLinkToDisk() { notImplemented(); return String(); }
String contextMenuItemTagCopyLinkToClipboard() { notImplemented(); return String(); }
String contextMenuItemTagOpenImageInNewWindow() { notImplemented(); return String(); }
String contextMenuItemTagDownloadImageToDisk() { notImplemented(); return String(); }
String contextMenuItemTagCopyImageToClipboard() { notImplemented(); return String(); }
String contextMenuItemTagOpenFrameInNewWindow() { notImplemented(); return String(); }
String contextMenuItemTagCopy() { notImplemented(); return String(); }
String contextMenuItemTagGoBack() { notImplemented(); return String(); }
String contextMenuItemTagGoForward() { notImplemented(); return String(); }
String contextMenuItemTagStop() { notImplemented(); return String(); }
String contextMenuItemTagReload() { notImplemented(); return String(); }
String contextMenuItemTagCut() { notImplemented(); return String(); }
String contextMenuItemTagPaste() { notImplemented(); return String(); }
String contextMenuItemTagNoGuessesFound() { notImplemented(); return String(); }
String contextMenuItemTagIgnoreSpelling() { notImplemented(); return String(); }
String contextMenuItemTagLearnSpelling() { notImplemented(); return String(); }
String contextMenuItemTagSearchWeb() { notImplemented(); return String(); }
String contextMenuItemTagLookUpInDictionary() { notImplemented(); return String(); }
String contextMenuItemTagOpenLink() { notImplemented(); return String(); }
String contextMenuItemTagIgnoreGrammar() { notImplemented(); return String(); }
String contextMenuItemTagSpellingMenu() { notImplemented(); return String(); }
String contextMenuItemTagShowSpellingPanel (bool) { notImplemented(); }
String contextMenuItemTagCheckSpelling() { notImplemented(); return String(); }
String contextMenuItemTagCheckSpellingWhileTyping() { notImplemented(); return String(); }
String contextMenuItemTagCheckGrammarWithSpelling() { notImplemented(); return String(); }
String contextMenuItemTagFontMenu() { notImplemented(); return String(); }
String contextMenuItemTagBold() { notImplemented(); return String(); }
String contextMenuItemTagItalic() { notImplemented(); return String(); }
String contextMenuItemTagUnderline() { notImplemented(); return String(); }
String contextMenuItemTagOutline() { notImplemented(); return String(); }
String contextMenuItemTagWritingDirectionMenu() { notImplemented(); return String(); }
String contextMenuItemTagDefaultDirection() { notImplemented(); return String(); }
String contextMenuItemTagLeftToRight() { notImplemented(); return String(); }
String contextMenuItemTagRightToLeft() { notImplemented(); return String(); }

void CheckCacheObjectStatus(DocLoader*, CachedResource*) { notImplemented(); }
bool CheckIfReloading(DocLoader*) { notImplemented(); return false; }
String defaultLanguage() { notImplemented(); return "en"; }
String fileButtonChooseFileLabel() { notImplemented(); return "Browse..."; }
int findNextSentenceFromIndex(UChar const*, int, int, bool) { notImplemented(); return 0; }
int findNextWordFromIndex(UChar const*, int, int, bool) { notImplemented(); return 0; }
void findSentenceBoundary(UChar const*, int, int, int*, int*) { notImplemented(); }
void findWordBoundary(UChar const* str, int len, int position, int* start, int* end) { notImplemented(); *start = position; *end = position; }
Color focusRingColor() { notImplemented(); return 0xFF0000FF; }
bool historyContains(DeprecatedString const&) { notImplemented(); return false; }
String inputElementAltText() { notImplemented(); return DeprecatedString(); }
const Cursor& aliasCursor() { notImplemented(); return localCursor; }
const Cursor& cellCursor() { notImplemented(); return localCursor; }
const Cursor& contextMenuCursor() { notImplemented(); return localCursor; }
const Cursor& copyCursor() { notImplemented(); return localCursor; }
const Cursor& moveCursor() { notImplemented(); return localCursor; }
const Cursor& noDropCursor() { notImplemented(); return localCursor; }
const Cursor& progressCursor() { notImplemented(); return localCursor; }
const Cursor& verticalTextCursor() { notImplemented(); return localCursor; }
void refreshPlugins(bool) { notImplemented(); }
String resetButtonDefaultLabel() { notImplemented(); return "Reset"; }
int screenDepthPerComponent(Widget*) { notImplemented(); return 8; }
bool screenIsMonochrome(Widget*) { notImplemented(); return false; }
String searchableIndexIntroduction() { notImplemented(); return String(); }
Vector<char> ServeSynchronousRequest(Loader*, DocLoader*, const ResourceRequest&, ResourceResponse&) { notImplemented(); return Vector<char>(); }
void setFocusRingColorChangeFunction(void (*)()) { notImplemented(); }
String submitButtonDefaultLabel() { notImplemented(); return "Submit"; }
float userIdleTime() { notImplemented(); return 0; }

bool AXObjectCache::gAccessibilityEnabled = false;

void BitmapImage::drawTiled(GraphicsContext*, const FloatRect&, const FloatRect&, TileRule, TileRule, CompositeOperator) { notImplemented(); }
bool BitmapImage::getHBITMAP(HBITMAP) { notImplemented(); return false; }

ContextMenu::ContextMenu(const HitTestResult& result) : m_hitTestResult(result) { notImplemented(); }
ContextMenu::~ContextMenu() { notImplemented(); }
void ContextMenu::show() { notImplemented(); }
void ContextMenu::appendItem(ContextMenuItem&) { notImplemented(); }

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

static ResourceRequest emptyResourceRequestForDocumentLoader;
static KURL emptyKURLForDocumentLoader;
const KURL DocumentLoader::unreachableURL() const { notImplemented(); return KURL(); }

void Editor::ignoreSpelling() { notImplemented(); }
void Editor::learnSpelling() { notImplemented(); }
bool Editor::isSelectionUngrammatical() { notImplemented(); return false; }
bool Editor::isSelectionMisspelled() { notImplemented(); return false; }
Vector<String> Editor::guessesForMisspelledSelection() { notImplemented(); return Vector<String>(); }
Vector<String> Editor::guessesForUngrammaticalSelection() { notImplemented(); return Vector<String>(); }
void Editor::markMisspellingsAfterTypingToPosition(const VisiblePosition&) { notImplemented(); }
PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy) { notImplemented(); return 0; }

bool EventHandler::tabsToLinks(KeyboardEvent* event) const { notImplemented(); return false; }
bool EventHandler::tabsToAllControls(KeyboardEvent* event) const { notImplemented(); return false; }
void EventHandler::focusDocumentView() { notImplemented(); }
bool EventHandler::handleDrag(const MouseEventWithHitTestResults&) { notImplemented(); return false; }
bool EventHandler::handleMouseUp(const MouseEventWithHitTestResults&) { notImplemented(); return false; }
bool EventHandler::lastEventIsMouseUp() const { notImplemented(); return false; }
bool EventHandler::shouldDragAutoNode(Node*, const IntPoint&) const { notImplemented(); return false; }
bool EventHandler::passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframe) { notImplemented(); return false; }
bool EventHandler::passMouseDownEventToWidget(Widget*) { notImplemented(); return false; }
bool EventHandler::passWheelEventToWidget(Widget*) { notImplemented(); return false; }

FileChooser::FileChooser(Document*, RenderFileUploadControl*) { notImplemented(); }
FileChooser::~FileChooser() { notImplemented(); }
PassRefPtr<FileChooser> FileChooser::create(Document*, RenderFileUploadControl*) { notImplemented(); return 0; }
void FileChooser::openFileChooser() { notImplemented(); }
String FileChooser::basenameForWidth(int width) const { notImplemented(); return String(); }
void FileChooser::disconnectUploadControl() { notImplemented(); }
void FileChooser::chooseFile(const String& filename) { notImplemented(); }

void Frame::setNeedsReapplyStyles() { notImplemented(); }

String FrameLoader::overrideMediaType() const { notImplemented(); return String(); }
Widget* FrameLoader::createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>&) { notImplemented(); return 0; }
void FrameLoader::redirectDataToPlugin(Widget* pluginWidget) { notImplemented(); }
Frame* FrameLoader::createFrame(const KURL& URL, const String& name, HTMLFrameOwnerElement*, const String& referrer) { notImplemented(); return 0; }
void FrameLoader::partClearedInBegin() { notImplemented(); }
ObjectContentType FrameLoader::objectContentType(const KURL&, const String&) { notImplemented(); return ObjectContentNone; }
Widget* FrameLoader::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&) { notImplemented(); return 0; }
void FrameView::updateBorder() { notImplemented(); }

void FrameWin::focusWindow() { notImplemented(); }
void FrameWin::unfocusWindow() { notImplemented(); }
KJS::Bindings::Instance* FrameWin::getObjectInstanceForWidget(Widget*) { notImplemented(); return 0; }
KJS::Bindings::Instance* FrameWin::getEmbedInstanceForWidget(Widget*) { notImplemented(); return 0; }
KJS::Bindings::RootObject* FrameWin::bindingRootObject() { notImplemented(); return 0; }
bool FrameWin::runJavaScriptPrompt(String const&, String const&, String &) { notImplemented(); return 0; }
bool FrameWin::shouldInterruptJavaScript() { notImplemented(); return false; }
void FrameWin::print() { notImplemented(); }
KJS::Bindings::Instance* FrameWin::getAppletInstanceForWidget(Widget*) { notImplemented(); return 0; }
bool FrameWin::passMouseDownEventToWidget(Widget*) { notImplemented(); return 0; }
void FrameWin::issueCutCommand() { notImplemented(); }
void FrameWin::issueCopyCommand() { notImplemented(); }
String FrameWin::mimeTypeForFileName(String const&) const { notImplemented(); return String(); }
void FrameWin::issuePasteCommand() { notImplemented(); }
void FrameWin::issueTransposeCommand() { notImplemented(); }
void FrameWin::issuePasteAndMatchStyleCommand() { notImplemented(); }
bool FrameWin::isLoadTypeReload() { notImplemented(); return false; }
Range* FrameWin::markedTextRange() const { notImplemented(); return 0; }
bool FrameWin::shouldChangeSelection(const Selection&, const Selection&, EAffinity, bool) const { notImplemented(); return true; }
void FrameWin::respondToChangedSelection(const Selection&, bool) { notImplemented(); }

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight) { notImplemented(); }
void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness) { notImplemented(); }
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
void GraphicsContext::setCompositeOperation(CompositeOperator) { notImplemented(); }
void GraphicsContext::clip(const Path&) { notImplemented(); }
void GraphicsContext::rotate(float) { notImplemented(); }
void GraphicsContext::scale(const FloatSize&) { notImplemented(); }

Icon::Icon() { notImplemented(); }
Icon::~Icon() { notImplemented(); }
PassRefPtr<Icon> Icon::newIconForFile(const String& filename) { notImplemented(); return PassRefPtr<Icon>(new Icon()); }
void Icon::paint(GraphicsContext*, const IntRect&) { notImplemented(); }

// FIXME: All this IconDatabase stuff could go away if much of
// WebCore/loader/icon was linked in.  Unfortunately that requires SQLite,
// which isn't currently part of the build.
Image* IconDatabase::iconForPageURL(const String&, const IntSize&, bool cache) { notImplemented(); return 0; }
Image* IconDatabase::defaultIcon(const IntSize&) { notImplemented(); return 0; }
void IconDatabase::retainIconForPageURL(const String&) { notImplemented(); }
void IconDatabase::releaseIconForPageURL(const String&) { notImplemented(); }
bool IconDatabase::isIconExpiredForIconURL(const String& url) { notImplemented(); return false; }
bool IconDatabase::hasEntryForIconURL(const String& url) { notImplemented(); return false; }
IconDatabase* IconDatabase::sharedIconDatabase() { notImplemented(); return 0; }
bool IconDatabase::setIconURLForPageURL(const String& iconURL, const String& pageURL) { notImplemented(); return false; }
void IconDatabase::setIconDataForIconURL(const void* data, int size, const String&) { notImplemented(); }

HINSTANCE Page::s_instanceHandle = 0;

void PageCache::close() { notImplemented(); }

Pasteboard* Pasteboard::generalPasteboard() { notImplemented(); return 0; }
void Pasteboard::writeSelection(Range*, bool canSmartCopyOrDelete, Frame*) { notImplemented(); }
void Pasteboard::writeURL(const KURL&, const String&, Frame*) { notImplemented(); }
void Pasteboard::clear() { notImplemented(); }
bool Pasteboard::canSmartReplace() { notImplemented(); return false; }
PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame*, PassRefPtr<Range>, bool allowPlainText, bool& chosePlainText) { notImplemented(); return 0; }
String Pasteboard::plainText(Frame* frame) { notImplemented(); return String(); }
Pasteboard::Pasteboard() { notImplemented(); }
Pasteboard::~Pasteboard() { notImplemented(); }

Path::Path() { notImplemented(); }
Path::~Path() { notImplemented(); }
Path::Path(const Path&) { notImplemented(); }
bool Path::contains(const FloatPoint&, WindRule rule) const { notImplemented(); return false; }
void Path::translate(const FloatSize&) { notImplemented(); }
FloatRect Path::boundingRect() const { notImplemented(); return FloatRect(); }
Path& Path::operator=(const Path&) { notImplemented(); return*this; }
void Path::clear() { notImplemented(); }
void Path::moveTo(const FloatPoint&) { notImplemented(); }
void Path::addLineTo(const FloatPoint&) { notImplemented(); }
void Path::addQuadCurveTo(const FloatPoint&, const FloatPoint&) { notImplemented(); }
void Path::addBezierCurveTo(const FloatPoint&, const FloatPoint&, const FloatPoint&) { notImplemented(); }
void Path::addArcTo(const FloatPoint&, const FloatPoint&, float) { notImplemented(); }
void Path::closeSubpath() { notImplemented(); }
void Path::addArc(const FloatPoint&, float, float, float, bool) { notImplemented(); }
void Path::addRect(const FloatRect&) { notImplemented(); }
void Path::addEllipse(const FloatRect&) { notImplemented(); }
void Path::transform(const AffineTransform&) { notImplemented(); }

PlatformScrollbar::PlatformScrollbar(ScrollbarClient* client, ScrollbarOrientation orientation, ScrollbarControlSize controlSize) : Scrollbar(client, orientation, controlSize) { notImplemented(); }
PlatformScrollbar::~PlatformScrollbar() { notImplemented(); }
int PlatformScrollbar::width() const { notImplemented(); return 0; }
int PlatformScrollbar::height() const { notImplemented(); return 0; }
void PlatformScrollbar::setEnabled(bool) { notImplemented(); }
void PlatformScrollbar::paint(GraphicsContext*, const IntRect& damageRect) { notImplemented(); }
void PlatformScrollbar::updateThumbPosition() { notImplemented(); }
void PlatformScrollbar::updateThumbProportion() { notImplemented(); }
void PlatformScrollbar::setRect(const IntRect&) { notImplemented(); }
bool PlatformScrollbar::handleMouseMoveEvent(const PlatformMouseEvent&) { notImplemented(); return false; }
bool PlatformScrollbar::handleMouseReleaseEvent(const PlatformMouseEvent&) { notImplemented(); return false; }

PluginInfo* PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplemented(); return 0; }
bool PlugInInfoStore::supportsMIMEType(const String&) { notImplemented(); return false; }

PopupMenu::PopupMenu(PopupMenuClient*) { notImplemented(); }
PopupMenu::~PopupMenu() { notImplemented(); }
void PopupMenu::show(const IntRect&, FrameView*, int index) { notImplemented(); }
void PopupMenu::hide() { notImplemented(); }
void PopupMenu::updateFromElement() { notImplemented(); }

void RenderThemeWin::systemFont(int propId, FontDescription& fontDescription) const { notImplemented(); }

bool ResourceHandle::willLoadFromCache(ResourceRequest&) { notImplemented(); return false; }
bool ResourceHandle::loadsBlocked() { notImplemented(); return false; }

void ScrollView::addChild(Widget*) { notImplemented(); }
void ScrollView::removeChild(Widget*) { notImplemented(); }
void ScrollView::scrollRectIntoViewRecursively(const IntRect&) { notImplemented(); }
bool ScrollView::inWindow() const { notImplemented(); return true; }
void ScrollView::paint(GraphicsContext*, const IntRect&) { notImplemented(); }
void ScrollView::wheelEvent(PlatformWheelEvent&) { notImplemented(); }
void ScrollView::themeChanged() { notImplemented(); }
IntPoint ScrollView::convertChildToSelf(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
IntPoint ScrollView::convertSelfToChild(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
void ScrollView::geometryChanged() const { notImplemented(); };
PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent) { notImplemented(); return 0; }
void ScrollView::setFrameGeometry(const IntRect& r) { notImplemented(); Widget::setFrameGeometry(r); }
IntRect ScrollView::windowResizerRect() { notImplemented(); return IntRect(); }
bool ScrollView::resizerOverlapsContent() const { notImplemented(); return false; }

void SearchPopupMenu::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems) { notImplemented(); }
void SearchPopupMenu::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems) { notImplemented(); }
SearchPopupMenu::SearchPopupMenu(PopupMenuClient* client) : PopupMenu(client) { notImplemented(); }

void Widget::enableFlushDrawing() { notImplemented(); }
bool Widget::isEnabled() const { notImplemented(); return 0; }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void Widget::disableFlushDrawing() { notImplemented(); }
void Widget::removeFromParent() { notImplemented(); }
GraphicsContext* Widget::lockDrawingFocus() { notImplemented(); return 0; }
void Widget::unlockDrawingFocus(GraphicsContext*) { notImplemented(); }
bool Widget::capturingMouse() const { notImplemented(); return false; }
void Widget::setCapturingMouse(bool capturingMouse) { notImplemented(); }
Widget* Widget::capturingTarget() { notImplemented(); return this; }
Widget* Widget::capturingChild() { notImplemented(); return 0; }
void Widget::setCapturingChild(Widget* w) { notImplemented(); }
IntPoint Widget::convertChildToSelf(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
IntPoint Widget::convertSelfToChild(const Widget*, const IntPoint& p) const { notImplemented(); return p; }
void Widget::setParent(ScrollView*) { notImplemented(); }
ScrollView* Widget::parent() const { notImplemented(); return 0; }
void Widget::setEnabled(bool) { notImplemented(); }
void Widget::paint(GraphicsContext*,IntRect const&) { notImplemented(); }
void Widget::setIsSelected(bool) { notImplemented(); }
void Widget::invalidate() { notImplemented(); }
void Widget::invalidateRect(const IntRect& r) { notImplemented(); }

} // namespace WebCore
