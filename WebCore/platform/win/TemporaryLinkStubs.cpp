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

#define WIN32_COMPILE_HACK

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "CString.h"
#include "Node.h"
#include "TextField.h"
#include "FileChooser.h"
#include "Font.h"
#include "PopUpButton.h"
#include "IntPoint.h"
#include "Widget.h"
#include "GraphicsContext.h"
#include "Slider.h"
#include "Cursor.h"
#include "loader.h"
#include "FrameView.h"
#include "KURL.h"
#include "PlatformScrollBar.h"
#include "ScrollBar.h"
#include "JavaAppletWidget.h"
#include "Path.h"
#include "PlatformMouseEvent.h"
#include "CookieJar.h"
#include "Screen.h"
#include "History.h"
#include "Language.h"
#include "LocalizedStrings.h"
#include "PlugInInfoStore.h"
#include "RenderTheme.h"
#include "FrameWin.h"
#include "BrowserExtensionWin.h"
#include "ResourceLoader.h"
#include "RenderThemeWin.h"
#include "TextBoundaries.h"
#include "AXObjectCache.h"
#include "RenderPopupMenuWin.h"
#include "EditCommand.h"
#include "Icon.h"
#include "IconLoader.h"
#include "IconDatabase.h"

using namespace WebCore;

#define notImplemented() do { \
    char buf[256] = {0}; \
    _snprintf(buf, sizeof(buf), "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); \
    OutputDebugStringA(buf); \
} while (0)

void FrameView::updateBorder() { notImplemented(); }
bool FrameView::isFrameView() const { notImplemented(); return 0; }

Widget::FocusPolicy PopUpButton::focusPolicy() const { notImplemented(); return NoFocus; }
void PopUpButton::populate() { notImplemented(); }

void Widget::enableFlushDrawing() { notImplemented(); }
bool Widget::isEnabled() const { notImplemented(); return 0; }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void Widget::disableFlushDrawing() { notImplemented(); }
GraphicsContext* Widget::lockDrawingFocus() { notImplemented(); return 0; }
void Widget::unlockDrawingFocus(GraphicsContext*) { notImplemented(); }

JavaAppletWidget::JavaAppletWidget(IntSize const&,Element*,WTF::HashMap<String,String> const&) { notImplemented(); }

void TextField::selectAll() { notImplemented(); }
void TextField::addSearchResult() { notImplemented(); }
int TextField::selectionStart() const { notImplemented(); return 0; }
bool TextField::hasSelectedText() const { notImplemented(); return 0; }
String TextField::selectedText() const { notImplemented(); return String(); }
void TextField::setAutoSaveName(String const&) { notImplemented(); }
bool TextField::checksDescendantsForFocus() const { notImplemented(); return false; }
void TextField::setSelection(int,int) { notImplemented(); }
void TextField::setMaxResults(int) { notImplemented(); }
bool TextField::edited() const { notImplemented(); return 0; }

Slider::Slider() { notImplemented(); }
IntSize Slider::sizeHint() const { notImplemented(); return IntSize(); }
void Slider::setValue(double) { notImplemented(); }
void Slider::setMaxValue(double) { notImplemented(); }
void Slider::setMinValue(double) { notImplemented(); }
Slider::~Slider() { notImplemented(); }
void Slider::setFont(WebCore::Font const&) { notImplemented(); }
double Slider::value() const { notImplemented(); return 0; }

void ListBox::setSelected(int,bool) { notImplemented(); }
IntSize ListBox::sizeForNumberOfLines(int) const { notImplemented(); return IntSize(); }
bool ListBox::isSelected(int) const { notImplemented(); return 0; }
void ListBox::appendItem(DeprecatedString const&,ListBoxItemType,bool) { notImplemented(); }
void ListBox::doneAppendingItems() { notImplemented(); }
void ListBox::setWritingDirection(TextDirection) { notImplemented(); }
void ListBox::setEnabled(bool) { notImplemented(); }
void ListBox::clear() { notImplemented(); }
bool ListBox::checksDescendantsForFocus() const { notImplemented(); return 0; }

FileChooser::FileChooser(Document*, RenderFileUploadControl*) { notImplemented(); }
FileChooser::~FileChooser() { notImplemented(); }
void FileChooser::openFileChooser() { notImplemented(); }
String FileChooser::basenameForWidth(int width) const { notImplemented(); return String(); }
void FileChooser::uploadControlDetaching() { notImplemented(); }
void FileChooser::chooseFile(const String& filename) { notImplemented(); }

Widget::FocusPolicy Slider::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy ListBox::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy TextField::focusPolicy() const { notImplemented(); return NoFocus; }

Cursor::Cursor(Image*) { notImplemented(); }

PlatformMouseEvent::PlatformMouseEvent(const CurrentEventTag&) { notImplemented(); }
String WebCore::searchableIndexIntroduction() { notImplemented(); return String(); }

int WebCore::findNextSentenceFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }
void WebCore::findSentenceBoundary(UChar const*,int,int,int*,int*) { notImplemented(); }
int WebCore::findNextWordFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }

namespace WebCore {

Vector<char> ServeSynchronousRequest(Loader*,DocLoader*,ResourceLoader*,KURL&,DeprecatedString&) { notImplemented(); return Vector<char>(); }

}

void FrameWin::focusWindow() { notImplemented(); }
void FrameWin::unfocusWindow() { notImplemented(); }
bool FrameWin::locationbarVisible() { notImplemented(); return 0; }
void FrameWin::issueRedoCommand(void) { notImplemented(); }
KJS::Bindings::Instance* FrameWin::getObjectInstanceForWidget(Widget *) { notImplemented(); return 0; }
KJS::Bindings::Instance* FrameWin::getEmbedInstanceForWidget(Widget *) { notImplemented(); return 0; }
KJS::Bindings::RootObject* FrameWin::bindingRootObject() { notImplemented(); return 0; }
bool FrameWin::canRedo() const { notImplemented(); return 0; }
bool FrameWin::canUndo() const { notImplemented(); return 0; }
void FrameWin::registerCommandForUndo(PassRefPtr<WebCore::EditCommand>) { notImplemented(); }
void FrameWin::registerCommandForRedo(PassRefPtr<WebCore::EditCommand>) { notImplemented(); }
bool FrameWin::runJavaScriptPrompt(String const&,String const&,String &) { notImplemented(); return 0; }
bool FrameWin::shouldInterruptJavaScript() { notImplemented(); return false; }
bool FrameWin::openURL(KURL const&) { notImplemented(); return 0; }
void FrameWin::print() { notImplemented(); }
KJS::Bindings::Instance* FrameWin::getAppletInstanceForWidget(Widget*) { notImplemented(); return 0; }
bool FrameWin::passMouseDownEventToWidget(Widget*) { notImplemented(); return 0; }
void FrameWin::issueCutCommand() { notImplemented(); }
void FrameWin::issueCopyCommand() { notImplemented(); }
void FrameWin::openURLRequest(struct WebCore::ResourceRequest const&) { notImplemented(); }
bool FrameWin::passWheelEventToChildWidget(Node*) { notImplemented(); return 0; }
void FrameWin::issueUndoCommand() { notImplemented(); }
String FrameWin::mimeTypeForFileName(String const&) const { notImplemented(); return String(); }
void FrameWin::issuePasteCommand() { notImplemented(); }
void FrameWin::scheduleClose() { notImplemented(); }
void FrameWin::markMisspellings(const WebCore::Selection&) { notImplemented(); }
bool FrameWin::menubarVisible() { notImplemented(); return 0; }
bool FrameWin::personalbarVisible() { notImplemented(); return 0; }
bool FrameWin::statusbarVisible() { notImplemented(); return 0; }
bool FrameWin::toolbarVisible() { notImplemented(); return 0; }
void FrameWin::issueTransposeCommand() { notImplemented(); }
bool FrameWin::canPaste() const { notImplemented(); return 0; }
bool FrameWin::canGoBackOrForward(int) const { notImplemented(); return 0; }
void FrameWin::issuePasteAndMatchStyleCommand() { notImplemented(); }
KURL FrameWin::originalRequestURL() const { return KURL(); }

bool BrowserExtensionWin::canRunModal() { notImplemented(); return 0; }
void BrowserExtensionWin::createNewWindow(struct WebCore::ResourceRequest const&,struct WebCore::WindowArgs const&,Frame*&) { notImplemented(); }
bool BrowserExtensionWin::canRunModalNow() { notImplemented(); return 0; }
void BrowserExtensionWin::runModal() { notImplemented(); }
void BrowserExtensionWin::goBackOrForward(int) { notImplemented(); }
KURL BrowserExtensionWin::historyURL(int distance) { notImplemented(); return KURL(); }
void BrowserExtensionWin::createNewWindow(struct WebCore::ResourceRequest const&) { notImplemented(); }

void GraphicsContext::addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
    const IntSize& bottomLeft, const IntSize& bottomRight) { notImplemented(); }
void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness) { notImplemented(); }

int WebCore::screenDepthPerComponent(const Page*) { notImplemented(); return 8; }
bool WebCore::screenIsMonochrome(const Page*) { notImplemented(); return false; }

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
static Cursor localCursor;
const Cursor& WebCore::moveCursor() { return localCursor; }

bool AXObjectCache::gAccessibilityEnabled = false;

bool WebCore::historyContains(DeprecatedString const&) { return false; }
String WebCore::submitButtonDefaultLabel() { return "Submit"; }
String WebCore::inputElementAltText() { return DeprecatedString(); }
String WebCore::resetButtonDefaultLabel() { return "Reset"; }
String WebCore::fileButtonChooseFileLabel() { return "Browse..."; }
String WebCore::fileButtonNoFileSelectedLabel() { return String(); }

String WebCore::defaultLanguage() { return "en"; }

void WebCore::findWordBoundary(UChar const* str,int len,int position,int* start, int* end) {*start=position; *end=position; }

PluginInfo*PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { return 0;}
unsigned PlugInInfoStore::pluginCount() const { return 0; }
bool WebCore::PlugInInfoStore::supportsMIMEType(const WebCore::String&) { return false; }
void WebCore::refreshPlugins(bool) { }

void WebCore::ResourceLoader::assembleResponseHeaders() const { }
void WebCore::ResourceLoader::retrieveCharset() const { }

void FrameWin::restoreDocumentState() { }
void FrameWin::partClearedInBegin() { }
void FrameWin::createEmptyDocument() { }
String FrameWin::overrideMediaType() const { return String(); }
Range* FrameWin::markedTextRange() const { return 0; }
bool FrameWin::passSubframeEventToSubframe(WebCore::MouseEventWithHitTestResults&, Frame*) { return false; }
bool FrameWin::lastEventIsMouseUp() const { return false; }
void FrameWin::addMessageToConsole(String const&,unsigned int,String const&) { }
bool FrameWin::shouldChangeSelection(const WebCore::Selection&,const WebCore::Selection&,WebCore::EAffinity,bool) const { return true; }
void FrameWin::respondToChangedSelection(const WebCore::Selection&,bool) { }
static int frameNumber = 0;
Frame* FrameWin::createFrame(KURL const&,String const&,Element*,String const&) { return 0; }
void FrameWin::saveDocumentState() { }
void FrameWin::clearUndoRedoOperations(void) { }
String FrameWin::incomingReferrer() const { return String(); }
void FrameWin::markMisspellingsInAdjacentWords(WebCore::VisiblePosition const&) { }
void FrameWin::respondToChangedContents(const Selection&) { }
void FrameWin::handledOnloadEvents() { }
Plugin* FrameWin::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&) { return 0; }
ObjectContentType FrameWin::objectContentType(const KURL&, const String&) { return ObjectContentNone; }

BrowserExtensionWin::BrowserExtensionWin(WebCore::Frame*) { }
void BrowserExtensionWin::setTypedIconURL(KURL const&, const String&) { }
void BrowserExtensionWin::setIconURL(KURL const&) { }
int BrowserExtensionWin::getHistoryLength() { return 0; }

namespace WebCore {

bool CheckIfReloading(WebCore::DocLoader*) { return false; }
void CheckCacheObjectStatus(DocLoader*, CachedResource*) { }

}

void Widget::setEnabled(bool) { }
void Widget::paint(GraphicsContext*,IntRect const&) { }
void Widget::setIsSelected(bool) { }

void ScrollView::addChild(Widget*,int,int) { }
void ScrollView::removeChild(Widget*) { }
void ScrollView::scrollPointRecursively(int x, int y) { }
bool ScrollView::inWindow() const { return true; }

void GraphicsContext::setShadow(IntSize const&,int,Color const&) { }
void GraphicsContext::clearShadow() { }
void GraphicsContext::beginTransparencyLayer(float) { }
void GraphicsContext::endTransparencyLayer() { }
void GraphicsContext::clearRect(const FloatRect&) { }
void GraphicsContext::strokeRect(const FloatRect&, float) { }
void GraphicsContext::setLineWidth(float) { }
void GraphicsContext::setLineCap(LineCap) { }
void GraphicsContext::setLineJoin(LineJoin) { }
void GraphicsContext::setMiterLimit(float) { }
void GraphicsContext::setAlpha(float) { }
void GraphicsContext::setCompositeOperation(CompositeOperator) { }
void GraphicsContext::clip(const Path&) { }
void GraphicsContext::translate(const FloatSize&) { }
void GraphicsContext::rotate(float) { }
void GraphicsContext::scale(const FloatSize&) { }

Path::Path(){ }
Path::~Path(){ }
Path::Path(const Path&){ }
bool Path::contains(const FloatPoint&, WindRule rule) const { return false; }
void Path::translate(const FloatSize&){ }
FloatRect Path::boundingRect() const { return FloatRect(); }
Path& Path::operator=(const Path&){ return*this; }
void Path::clear() { }
void Path::moveTo(const FloatPoint&) { }
void Path::addLineTo(const FloatPoint&) { }
void Path::addQuadCurveTo(const FloatPoint&, const FloatPoint&) { }
void Path::addBezierCurveTo(const FloatPoint&, const FloatPoint&, const FloatPoint&) { }
void Path::addArcTo(const FloatPoint&, const FloatPoint&, float) { }
void Path::closeSubpath() { }
void Path::addArc(const FloatPoint&, float, float, float, bool) { }
void Path::addRect(const FloatRect&) { }
void Path::addEllipse(const FloatRect&) { }

TextField::TextField(TextField::Type type) 
{ 
    m_type = type;
}

TextField::~TextField() { }
void TextField::setFont(WebCore::Font const&) { }
void TextField::setAlignment(HorizontalAlignment) { }
void TextField::setWritingDirection(TextDirection) { }
int TextField::maxLength() const { return 0; }
void TextField::setMaxLength(int) { }
String TextField::text() const { return String(); }
void TextField::setText(String const&) { }
int TextField::cursorPosition() const { return 0; }
void TextField::setCursorPosition(int) { }
void TextField::setEdited(bool) { }
void TextField::setReadOnly(bool) { }
void TextField::setPlaceholderString(String const&) { }
void TextField::setColors(Color const&,Color const&) { }
IntSize TextField::sizeForCharacterWidth(int) const { return IntSize(); }
int TextField::baselinePosition(int) const { return 0; }
void TextField::setLiveSearch(bool) { }

PopUpButton::PopUpButton() { }
PopUpButton::~PopUpButton() { }
void PopUpButton::setFont(WebCore::Font const&) { }
int PopUpButton::baselinePosition(int) const { return 0; }
void PopUpButton::setWritingDirection(TextDirection) { }
void PopUpButton::clear() { }
void PopUpButton::appendItem(DeprecatedString const&,ListBoxItemType,bool) { }
void PopUpButton::setCurrentItem(int) { }
IntSize PopUpButton::sizeHint() const { return IntSize(); }
IntRect PopUpButton::frameGeometry() const { return IntRect(); }
void PopUpButton::setFrameGeometry(IntRect const&) { }

PlatformScrollBar::PlatformScrollBar(ScrollBarClient* client, ScrollBarOrientation orientation) : ScrollBar(client, orientation) { }
PlatformScrollBar::~PlatformScrollBar() { }
int PlatformScrollBar::width() const { return 0; }
int PlatformScrollBar::height() const { return 0; }
void PlatformScrollBar::setEnabled(bool) { }
void PlatformScrollBar::paint(GraphicsContext*, const IntRect& damageRect) { }
void PlatformScrollBar::setScrollBarValue(int v) { }
void PlatformScrollBar::setKnobProportion(int visibleSize, int totalSize) { }
void PlatformScrollBar::setRect(const IntRect&) { }

ScrollBar::ScrollBar(ScrollBarClient*, ScrollBarOrientation) { }
void ScrollBar::setSteps(int, int) { }
bool ScrollBar::scroll(ScrollDirection, ScrollGranularity, float) { return false; }
bool ScrollBar::setValue(int) { return false; }
void ScrollBar::setKnobProportion(int, int) { }

ListBox::ListBox() { }
ListBox::~ListBox() { }
void ListBox::setSelectionMode(ListBox::SelectionMode) { }
void ListBox::setFont(WebCore::Font const&) { }

Color WebCore::focusRingColor() { return 0xFF0000FF; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { }

void Frame::setNeedsReapplyStyles() { }

void Image::drawTiled(GraphicsContext*, const FloatRect&, const FloatRect&, TileRule, TileRule, CompositeOperator) { }

RenderPopupMenuWin::~RenderPopupMenuWin() { notImplemented(); }
void RenderPopupMenuWin::clear() { notImplemented(); }
void RenderPopupMenuWin::populate() { notImplemented(); }
void RenderPopupMenuWin::showPopup(const IntRect&, FrameView*, int index) { notImplemented(); }
void RenderPopupMenuWin::hidePopup() { notImplemented(); }
void RenderPopupMenuWin::addSeparator() { notImplemented(); }
void RenderPopupMenuWin::addGroupLabel(HTMLOptGroupElement*) { notImplemented(); }
void RenderPopupMenuWin::addOption(HTMLOptionElement*) { notImplemented(); }

void RenderThemeWin::systemFont(int propId, FontDescription& fontDescription) const {}
bool RenderThemeWin::paintMenuList(RenderObject *, const RenderObject::PaintInfo&, const IntRect&) { return false; }
void RenderThemeWin::adjustMenuListStyle(CSSStyleSelector*, RenderStyle*, Element*) const { }

Icon::Icon() { notImplemented(); }
Icon::~Icon() { notImplemented(); }
PassRefPtr<Icon> Icon::newIconForFile(const String& filename) { notImplemented(); return PassRefPtr<Icon>(new Icon()); }
void Icon::paint(GraphicsContext*, const IntRect&) { notImplemented(); }

void IconLoader::stopLoading() { notImplemented(); } 
void IconLoader::startLoading() { notImplemented(); } 
IconLoader* IconLoader::createForFrame(Frame *frame) { return 0; } 

bool IconDatabase::isIconExpiredForIconURL(const String& url) { return false; }
bool IconDatabase::hasEntryForIconURL(const String& url) { return false; }
IconDatabase* IconDatabase::sharedIconDatabase() { return 0; }
bool IconDatabase::setIconURLForPageURL(const String iconURL, const String pageURL) { return false; }

