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

#include <stdio.h>
#include <stdlib.h>
#include "Node.h"
#include "KWQLineEdit.h"
#include "Font.h"
#include "KWQFileButton.h"
#include "KWQTextEdit.h"
#include "KWQComboBox.h"
#include "IntPoint.h"
#include "Widget.h"
#include "GraphicsContext.h"
#include "KWQSlider.h"
#include "Cursor.h"
#include "loader.h"
#include "FrameView.h"
#include "KWQScrollBar.h"
#include "JavaAppletWidget.h"
#include "KWQScrollBar.h"
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
#include "TransferJob.h"
#include "RenderThemeWin.h"
#include "TextBoundaries.h"
#include "AccessibilityObjectCache.h"

using namespace WebCore;

static void notImplemented() { puts("Not yet implemented"); _CrtDbgBreak(); }

void FrameView::updateBorder() { notImplemented(); }
bool FrameView::isFrameView() const { notImplemented(); return 0; }

QTextEdit::QTextEdit(Widget*) { notImplemented(); }
QTextEdit::~QTextEdit() { notImplemented(); }
String QTextEdit::textWithHardLineBreaks() const { notImplemented(); return String(); }
IntSize QTextEdit::sizeWithColumnsAndRows(int,int) const { notImplemented(); return IntSize(); }
void QTextEdit::setText(String const&) { notImplemented(); }
void QTextEdit::setColors(Color const&,Color const&) { notImplemented(); }
void QTextEdit::setFont(WebCore::Font const&) { notImplemented(); }
void QTextEdit::setWritingDirection(enum WebCore::TextDirection) { notImplemented(); }
bool QTextEdit::checksDescendantsForFocus() const { notImplemented(); return false; }
int QTextEdit::selectionStart() { notImplemented(); return 0; }
bool QTextEdit::hasSelectedText() const { notImplemented(); return 0; }
int QTextEdit::selectionEnd() { notImplemented(); return 0; }
void QTextEdit::setScrollBarModes(ScrollBarMode,ScrollBarMode) { notImplemented(); }
void QTextEdit::setReadOnly(bool) { notImplemented(); }
void QTextEdit::selectAll() { notImplemented(); }
void QTextEdit::setDisabled(bool) { notImplemented(); }
void QTextEdit::setLineHeight(int) { notImplemented(); }
void QTextEdit::setSelectionStart(int) { notImplemented(); }
void QTextEdit::setCursorPosition(int,int) { notImplemented(); }
String QTextEdit::text() const { notImplemented(); return String(); }
void QTextEdit::setWordWrap(QTextEdit::WrapStyle) { notImplemented(); }
void QTextEdit::setAlignment(HorizontalAlignment) { notImplemented(); }
void QTextEdit::setSelectionEnd(int) { notImplemented(); }
void QTextEdit::getCursorPosition(int*,int*) const { notImplemented(); }
void QTextEdit::setSelectionRange(int,int) { notImplemented(); }

Widget::FocusPolicy QComboBox::focusPolicy() const { notImplemented(); return NoFocus; }
void QComboBox::populate() { notImplemented(); }

void Widget::enableFlushDrawing() { notImplemented(); }
bool Widget::isEnabled() const { notImplemented(); return 0; }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void Widget::disableFlushDrawing() { notImplemented(); }
GraphicsContext* Widget::lockDrawingFocus() { notImplemented(); return 0; }
void Widget::unlockDrawingFocus(GraphicsContext*) { notImplemented(); }

JavaAppletWidget::JavaAppletWidget(IntSize const&,Element*,WTF::HashMap<String,String> const&) { notImplemented(); }

void QLineEdit::selectAll() { notImplemented(); }
void QLineEdit::addSearchResult() { notImplemented(); }
int QLineEdit::selectionStart() const { notImplemented(); return 0; }
bool QLineEdit::hasSelectedText() const { notImplemented(); return 0; }
String QLineEdit::selectedText() const { notImplemented(); return String(); }
void QLineEdit::setAutoSaveName(String const&) { notImplemented(); }
bool QLineEdit::checksDescendantsForFocus() const { notImplemented(); return false; }
void QLineEdit::setSelection(int,int) { notImplemented(); }
void QLineEdit::setMaxResults(int) { notImplemented(); }
bool QLineEdit::edited() const { notImplemented(); return 0; }

QSlider::QSlider() { notImplemented(); }
IntSize QSlider::sizeHint() const { notImplemented(); return IntSize(); }
void QSlider::setValue(double) { notImplemented(); }
void QSlider::setMaxValue(double) { notImplemented(); }
void QSlider::setMinValue(double) { notImplemented(); }
QSlider::~QSlider() { notImplemented(); }
void QSlider::setFont(WebCore::Font const&) { notImplemented(); }
double QSlider::value() const { notImplemented(); return 0; }

void QListBox::setSelected(int,bool) { notImplemented(); }
IntSize QListBox::sizeForNumberOfLines(int) const { notImplemented(); return IntSize(); }
bool QListBox::isSelected(int) const { notImplemented(); return 0; }
void QListBox::appendItem(DeprecatedString const&,KWQListBoxItemType,bool) { notImplemented(); }
void QListBox::doneAppendingItems() { notImplemented(); }
void QListBox::setWritingDirection(TextDirection) { notImplemented(); }
void QListBox::setEnabled(bool) { notImplemented(); }
void QListBox::clear() { notImplemented(); }
bool QListBox::checksDescendantsForFocus() const { notImplemented(); return 0; }

KWQFileButton::KWQFileButton(Frame*) { notImplemented(); }
void KWQFileButton::click(bool) { notImplemented(); }
IntSize KWQFileButton::sizeForCharacterWidth(int) const { notImplemented(); return IntSize(); }
Widget::FocusPolicy KWQFileButton::focusPolicy() const { notImplemented(); return NoFocus; }
WebCore::IntRect KWQFileButton::frameGeometry() const { notImplemented(); return IntRect(); }
void KWQFileButton::setFilename(DeprecatedString const&) { notImplemented(); }
int KWQFileButton::baselinePosition(int) const { notImplemented(); return 0; }
void KWQFileButton::setFrameGeometry(WebCore::IntRect const&) { notImplemented(); }
void KWQFileButton::setDisabled(bool) { notImplemented(); }

Widget::FocusPolicy QTextEdit::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy QSlider::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy QListBox::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy QLineEdit::focusPolicy() const { notImplemented(); return NoFocus; }

Cursor::Cursor(Image*) { notImplemented(); }

PlatformMouseEvent::PlatformMouseEvent() { notImplemented(); }
bool PlatformMouseEvent::isMouseButtonDown(MouseButton) { notImplemented(); return false; }
String WebCore::searchableIndexIntroduction() { notImplemented(); return String(); }

int WebCore::findNextSentenceFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }
void WebCore::findSentenceBoundary(UChar const*,int,int,int*,int*) { notImplemented(); }
int WebCore::findNextWordFromIndex(UChar const*,int,int,bool) { notImplemented(); return 0; }

DeprecatedArray<char> KWQServeSynchronousRequest(Loader*,DocLoader*,TransferJob*,KURL&,DeprecatedString&) { notImplemented(); return 0; }

void FrameWin::focusWindow() { notImplemented(); }
void FrameWin::unfocusWindow() { notImplemented(); }
bool FrameWin::locationbarVisible() { notImplemented(); return 0; }
void FrameWin::issueRedoCommand(void) { notImplemented(); }
KJS::Bindings::Instance* FrameWin::getObjectInstanceForWidget(Widget *) { notImplemented(); return 0; }
KJS::Bindings::Instance* FrameWin::getEmbedInstanceForWidget(Widget *) { notImplemented(); return 0; }
bool FrameWin::canRedo() const { notImplemented(); return 0; }
bool FrameWin::canUndo() const { notImplemented(); return 0; }
void FrameWin::registerCommandForRedo(WebCore::EditCommandPtr const&) { notImplemented(); }
bool FrameWin::runJavaScriptPrompt(String const&,String const&,String &) { notImplemented(); return 0; }
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
void FrameWin::markMisspellings(WebCore::SelectionController const&) { notImplemented(); }
bool FrameWin::menubarVisible() { notImplemented(); return 0; }
bool FrameWin::personalbarVisible() { notImplemented(); return 0; }
bool FrameWin::statusbarVisible() { notImplemented(); return 0; }
bool FrameWin::toolbarVisible() { notImplemented(); return 0; }
void FrameWin::issueTransposeCommand() { notImplemented(); }
bool FrameWin::canPaste() const { notImplemented(); return 0; }
enum WebCore::ObjectContentType FrameWin::objectContentType(KURL const&,String const&) { notImplemented(); return (ObjectContentType)0; }
bool FrameWin::canGoBackOrForward(int) const { notImplemented(); return 0; }
void FrameWin::issuePasteAndMatchStyleCommand() { notImplemented(); }
Plugin* FrameWin::createPlugin(Element*, KURL const&, const Vector<String>&, const Vector<String>&,String const&) { notImplemented(); return 0; }

bool BrowserExtensionWin::canRunModal() { notImplemented(); return 0; }
void BrowserExtensionWin::createNewWindow(struct WebCore::ResourceRequest const&,struct WebCore::WindowArgs const&,Frame*&) { notImplemented(); }
bool BrowserExtensionWin::canRunModalNow() { notImplemented(); return 0; }
void BrowserExtensionWin::runModal() { notImplemented(); }
void BrowserExtensionWin::goBackOrForward(int) { notImplemented(); }
void BrowserExtensionWin::createNewWindow(struct WebCore::ResourceRequest const&) { notImplemented(); }

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
static Cursor localCursor;
const Cursor& WebCore::moveCursor() { return localCursor; }

bool AccessibilityObjectCache::gAccessibilityEnabled = false;

bool WebCore::historyContains(DeprecatedString const&) { return false; }
String WebCore::submitButtonDefaultLabel() { return "Submit"; }
String WebCore::inputElementAltText() { return DeprecatedString(); }
String WebCore::resetButtonDefaultLabel() { return "Reset"; }
String WebCore::defaultLanguage() { return "en"; }

void WebCore::findWordBoundary(UChar const* str,int len,int position,int* start, int* end) {*start=position; *end=position; }

PluginInfo*PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { return 0;}
unsigned PlugInInfoStore::pluginCount() const { return 0; }
void WebCore::refreshPlugins(bool) { }

void WebCore::TransferJob::assembleResponseHeaders() const { }
void WebCore::TransferJob::retrieveCharset() const { }

void FrameWin::restoreDocumentState() { }
void FrameWin::partClearedInBegin() { }
void FrameWin::createEmptyDocument() { }
String FrameWin::overrideMediaType() const { return String(); }
void FrameWin::setTitle(String const&) { }
void FrameWin::handledOnloadEvents() { }
Range* FrameWin::markedTextRange() const { return 0; }
bool FrameWin::passSubframeEventToSubframe(WebCore::MouseEventWithHitTestResults&, Frame*) { return false; }
bool FrameWin::lastEventIsMouseUp() const { return false; }
void FrameWin::addMessageToConsole(String const&,unsigned int,String const&) { }
bool FrameWin::shouldChangeSelection(SelectionController const&,SelectionController const&,WebCore::EAffinity,bool) const { return true; }
void FrameWin::respondToChangedSelection(WebCore::SelectionController const&,bool) { }
static int frameNumber = 0;
Frame* FrameWin::createFrame(KURL const&,String const&,RenderPart*,String const&) { return 0; }
void FrameWin::saveDocumentState() { }
void FrameWin::registerCommandForUndo(WebCore::EditCommandPtr const&) { }
void FrameWin::clearUndoRedoOperations(void) { }
String FrameWin::incomingReferrer() const { return String(); }
void FrameWin::markMisspellingsInAdjacentWords(WebCore::VisiblePosition const&) { }
void FrameWin::respondToChangedContents() { }

BrowserExtensionWin::BrowserExtensionWin(WebCore::Frame*) { }
void BrowserExtensionWin::setTypedIconURL(KURL const&, const String&) { }
void BrowserExtensionWin::setIconURL(KURL const&) { }
int BrowserExtensionWin::getHistoryLength() { return 0; }

bool KWQCheckIfReloading(WebCore::DocLoader*) { return false; }
void KWQCheckCacheObjectStatus(DocLoader*, CachedObject*) { }

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
Color GraphicsContext::selectedTextBackgroundColor() const { return Color(0,0,255); }
void GraphicsContext::addRoundedRectClip(IntRect const&,IntSize const&,IntSize const&,IntSize const&,IntSize const&) { }
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
bool Path::contains(const FloatPoint&) const{ return false; }
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

QLineEdit::QLineEdit(QLineEdit::Type) { }
QLineEdit::~QLineEdit() { }
void QLineEdit::setFont(WebCore::Font const&) { }
void QLineEdit::setAlignment(HorizontalAlignment) { }
void QLineEdit::setWritingDirection(TextDirection) { }
int QLineEdit::maxLength() const { return 0; }
void QLineEdit::setMaxLength(int) { }
String QLineEdit::text() const { return String(); }
void QLineEdit::setText(String const&) { }
int QLineEdit::cursorPosition() const { return 0; }
void QLineEdit::setCursorPosition(int) { }
void QLineEdit::setEdited(bool) { }
void QLineEdit::setReadOnly(bool) { }
void QLineEdit::setPlaceholderString(String const&) { }
void QLineEdit::setColors(Color const&,Color const&) { }
IntSize QLineEdit::sizeForCharacterWidth(int) const { return IntSize(); }
int QLineEdit::baselinePosition(int) const { return 0; }
void QLineEdit::setLiveSearch(bool) { }

QComboBox::QComboBox() { }
QComboBox::~QComboBox() { }
void QComboBox::setFont(WebCore::Font const&) { }
int QComboBox::baselinePosition(int) const { return 0; }
void QComboBox::setWritingDirection(TextDirection) { }
void QComboBox::clear() { }
void QComboBox::appendItem(DeprecatedString const&,KWQListBoxItemType,bool) { }
void QComboBox::setCurrentItem(int) { }
IntSize QComboBox::sizeHint() const { return IntSize(); }
IntRect QComboBox::frameGeometry() const { return IntRect(); }
void QComboBox::setFrameGeometry(IntRect const&) { }

QScrollBar::QScrollBar(ScrollBarOrientation) { }
QScrollBar::~QScrollBar() { }
void QScrollBar::setSteps(int, int) { }
bool QScrollBar::scroll(KWQScrollDirection, KWQScrollGranularity, float) { return 0; }
bool QScrollBar::setValue(int) { return 0; }
void QScrollBar::setKnobProportion(int, int) { }

QListBox::QListBox() { }
QListBox::~QListBox() { }
void QListBox::setSelectionMode(QListBox::SelectionMode) { }
void QListBox::setFont(WebCore::Font const&) { }

Color WebCore::focusRingColor() { return 0xFF0000FF; }
void WebCore::setFocusRingColorChangeFunction(void (*)()) { }

void Frame::setNeedsReapplyStyles() { }

void Image::drawTiled(GraphicsContext*, const FloatRect&, const FloatRect&, TileRule, TileRule) { }
