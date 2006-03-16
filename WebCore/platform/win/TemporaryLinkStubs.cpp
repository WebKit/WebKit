#include "config.h"

#define WIN32_COMPILE_HACK

#include <stdio.h>
#include <stdlib.h>
#include "NodeImpl.h"
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
#include "KWQKJavaAppletWidget.h"
#include "KWQScrollBar.h"
#include "Path.h"
#include "MouseEvent.h"
#include "CookieJar.h"
#include "Screen.h"
#include "History.h"
#include "Language.h"
#include "LocalizedStrings.h"
#include "PlugInInfoStore.h"
#include "render_theme.h"
#include "FrameWin.h"
#include "BrowserExtensionWin.h"
#include "TransferJob.h"
#include "RenderThemeWin.h"
#include "TextBoundaries.h"
#include "render_canvasimage.h"

using namespace WebCore;

static void notImplemented() { puts("Not yet implemented"); _CrtDbgBreak(); }

IntPoint FrameView::viewportToGlobal(IntPoint const&) const { notImplemented(); return IntPoint(); }
void FrameView::updateBorder() { notImplemented(); }
bool FrameView::isFrameView() const { notImplemented(); return 0; }
Widget* FrameView::topLevelWidget() const { notImplemented(); return 0; }

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
void Widget::lockDrawingFocus() { notImplemented(); }
void Widget::unlockDrawingFocus() { notImplemented(); }
void Widget::setDrawingAlpha(float) { notImplemented(); }

KJavaAppletWidget::KJavaAppletWidget(IntSize const&,Frame*,KXMLCore::HashMap<String,String> const&) { notImplemented(); }

void QLineEdit::selectAll() { notImplemented(); }
void QLineEdit::addSearchResult() { notImplemented(); }
int QLineEdit::selectionStart() const { notImplemented(); return 0; }
bool QLineEdit::hasSelectedText() const { notImplemented(); return 0; }
QString QLineEdit::selectedText() const { notImplemented(); return QString(); }
void QLineEdit::setAutoSaveName(String const&) { notImplemented(); }
bool QLineEdit::checksDescendantsForFocus() const { notImplemented(); return false; }
void QLineEdit::setSelection(int,int) { notImplemented(); }
void QLineEdit::setMaxResults(int) { notImplemented(); }
bool QLineEdit::edited() const { notImplemented(); return 0; }

GraphicsContext::GraphicsContext() { notImplemented(); }

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
void QListBox::appendItem(QString const&,KWQListBoxItemType,bool) { notImplemented(); }
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
void KWQFileButton::setFilename(QString const&) { notImplemented(); }
int KWQFileButton::baselinePosition(int) const { notImplemented(); return 0; }
void KWQFileButton::setFrameGeometry(WebCore::IntRect const&) { notImplemented(); }
KWQFileButton::~KWQFileButton() { notImplemented(); }

Widget::FocusPolicy QTextEdit::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy QSlider::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy QListBox::focusPolicy() const { notImplemented(); return NoFocus; }
Widget::FocusPolicy QLineEdit::focusPolicy() const { notImplemented(); return NoFocus; }

Cursor::Cursor(Image*) { notImplemented(); }

MouseEvent::MouseEvent() { notImplemented(); }
bool MouseEvent::isMouseButtonDown(MouseButton) { notImplemented(); return false; }
String WebCore::searchableIndexIntroduction() { notImplemented(); return String(); }

int WebCore::findNextSentenceFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void WebCore::findSentenceBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }
int WebCore::findNextWordFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void WebCore::findWordBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }

Array<char> KWQServeSynchronousRequest(Loader*,DocLoader*,TransferJob*,KURL&,QString&) { notImplemented(); return 0; }

void FrameWin::respondToChangedContents() { notImplemented(); }
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
void FrameWin::registerCommandForUndo(WebCore::EditCommandPtr const&) { notImplemented(); }
void FrameWin::issueCutCommand() { notImplemented(); }
void FrameWin::issueCopyCommand() { notImplemented(); }
void FrameWin::openURLRequest(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }
bool FrameWin::passWheelEventToChildWidget(NodeImpl*) { notImplemented(); return 0; }
void FrameWin::issueUndoCommand() { notImplemented(); }
QString FrameWin::mimeTypeForFileName(QString const&) const { notImplemented(); return QString(); }
void FrameWin::issuePasteCommand() { notImplemented(); }
void FrameWin::scheduleClose() { notImplemented(); }
void FrameWin::markMisspellingsInAdjacentWords(WebCore::VisiblePosition const&) { notImplemented(); }
void FrameWin::markMisspellings(WebCore::SelectionController const&) { notImplemented(); }
bool FrameWin::menubarVisible() { notImplemented(); return 0; }
bool FrameWin::personalbarVisible() { notImplemented(); return 0; }
bool FrameWin::statusbarVisible() { notImplemented(); return 0; }
bool FrameWin::toolbarVisible() { notImplemented(); return 0; }
void FrameWin::issueTransposeCommand() { notImplemented(); }
bool FrameWin::canPaste() const { notImplemented(); return 0; }
enum WebCore::ObjectContentType FrameWin::objectContentType(KURL const&,QString const&) { notImplemented(); return (ObjectContentType)0; }
bool FrameWin::canGoBackOrForward(int) const { notImplemented(); return 0; }
void FrameWin::issuePasteAndMatchStyleCommand() { notImplemented(); }
Plugin* FrameWin::createPlugin(KURL const&,QStringList const&,QStringList const&,QString const&) { notImplemented(); return 0; }

void BrowserExtensionWin::openURLRequest(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }
bool BrowserExtensionWin::canRunModal() { notImplemented(); return 0; }
void BrowserExtensionWin::openURLNotify() { notImplemented(); }
void BrowserExtensionWin::createNewWindow(KURL const&,struct WebCore::URLArgs const&,struct WebCore::WindowArgs const&,Frame*&) { notImplemented(); }
bool BrowserExtensionWin::canRunModalNow() { notImplemented(); return 0; }
void BrowserExtensionWin::runModal() { notImplemented(); }
void BrowserExtensionWin::goBackOrForward(int) { notImplemented(); }
void BrowserExtensionWin::createNewWindow(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }

void RenderCanvasImage::setNeedsImageUpdate() { notImplemented(); }


/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/
static Cursor localCursor;
const Cursor& WebCore::moveCursor() { return localCursor; }

bool WebCore::historyContains(QString const&) { return false; }
String WebCore::submitButtonDefaultLabel() { return "Submit"; }
String WebCore::inputElementAltText() { return QString(); }
String WebCore::resetButtonDefaultLabel() { return "Reset"; }
String WebCore::defaultLanguage() { return "en"; }

void WebCore::setCookies(KURL const&,KURL const&,String const&) { }
String WebCore::cookies(KURL const&) { return String(); }
bool WebCore::cookiesEnabled() { return false; }

PluginInfo*PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { return 0;}
unsigned PlugInInfoStore::pluginCount() const { return 0; }
void WebCore::refreshPlugins(bool) { }

void WebCore::TransferJob::assembleResponseHeaders() const { }
void WebCore::TransferJob::retrieveCharset() const { }

void FrameWin::restoreDocumentState() { }
void FrameWin::partClearedInBegin() { }
void FrameWin::createEmptyDocument() { }
QString FrameWin::overrideMediaType() const { return QString(); }
void FrameWin::setTitle(String const&) { }
void FrameWin::handledOnloadEvents() { }
RangeImpl* FrameWin::markedTextRange() const { return 0; }
bool FrameWin::passSubframeEventToSubframe(WebCore::MouseEventWithHitTestResults&) { return false; }
bool FrameWin::lastEventIsMouseUp() const { return false; }
void FrameWin::addMessageToConsole(String const&,unsigned int,String const&) { }
bool FrameWin::shouldChangeSelection(SelectionController const&,SelectionController const&,WebCore::EAffinity,bool) const { return true; }
void FrameWin::respondToChangedSelection(WebCore::SelectionController const&,bool) { }
static int frameNumber = 0;
Frame* FrameWin::createFrame(KURL const&,QString const&,RenderPart*,String const&) { return 0; }
void FrameWin::saveDocumentState(void) { }
void FrameWin::clearUndoRedoOperations(void) { }
QString FrameWin::incomingReferrer() const { return QString(); }
void FrameWin::clearRecordedFormValues() { }
void FrameWin::recordFormValue(QString const&,QString const&,WebCore::HTMLFormElementImpl*) { }
void FrameWin::submitForm(KURL const&,struct WebCore::URLArgs const&) { }

BrowserExtensionWin::BrowserExtensionWin(WebCore::Frame*) { }
void BrowserExtensionWin::setTypedIconURL(KURL const&,QString const&) { }
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

void GraphicsContext::setShadow(int,int,int,Color const&) { }
void GraphicsContext::clearShadow() { }
void GraphicsContext::beginTransparencyLayer(float) { }
void GraphicsContext::endTransparencyLayer() { }
Color GraphicsContext::selectedTextBackgroundColor() const { return Color(0,0,255); }
void GraphicsContext::addRoundedRectClip(IntRect const&,IntSize const&,IntSize const&,IntSize const&,IntSize const&) { }

Path::Path(){ }
Path::~Path(){ }
Path::Path(const Path&){ }
Path::Path(const IntRect&, Type){ }
Path::Path(const IntPointArray&){ }
bool Path::contains(const IntPoint&) const{ return false; }
void Path::translate(int, int){ }
IntRect Path::boundingRect() const { return IntRect(); }
Path& Path::operator=(const Path&){ return*this; }

bool RenderThemeWin::paintCheckbox(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return false; }
bool RenderThemeWin::paintRadio(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return false; }
bool RenderThemeWin::paintButton(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return false; }
bool RenderThemeWin::paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&) { return false; }

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
void QComboBox::appendItem(QString const&,KWQListBoxItemType,bool) { }
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
