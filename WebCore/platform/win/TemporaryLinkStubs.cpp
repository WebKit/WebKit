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
#include "KWQFontMetrics.h"
#include "GraphicsContext.h"
#include "KWQSlider.h"
#include "Cursor.h"
#include "loader.h"
#include "KWQKJobClasses.h"
#include "FrameView.h"
#include "KWQScrollBar.h"
#include "KWQObject.h"
#include "KWQSignal.h"
#include "dom2_events.h"
#include "KWQKJavaAppletWidget.h"
#include "KWQScrollBar.h"
#include "Path.h"
#include "MouseEvent.h"
#include "KWQKCookieJar.h"
#include "Screen.h"
#include "History.h"
#include "KWQKLocale.h"
#include "PlugInInfoStore.h"
#include "render_theme.h"
#include "FrameWin.h"
#include "BrowserExtensionWin.h"

using namespace WebCore;

static void notImplemented() { puts("Not yet implemented"); _CrtDbgBreak(); }

void QLineEdit::selectAll() { notImplemented(); }
void Widget::enableFlushDrawing() { notImplemented(); }
void GraphicsContext::drawHighlightForText(int,int,int,int,int,QChar const*,int,int,int,int,Color const&,TextDirection,bool,int,int,bool) { notImplemented(); }
String QTextEdit::textWithHardLineBreaks() const { notImplemented(); return String(); }
IntPoint Widget::mapFromGlobal(IntPoint const&) const { notImplemented(); return IntPoint(); }
int QLineEdit::cursorPosition() const { notImplemented(); return 0; }
Color GraphicsContext::selectedTextBackgroundColor() const { notImplemented(); return Color(); }
void QSlider::setValue(double) { notImplemented(); }
void QLineEdit::addSearchResult() { notImplemented(); }
void KWQFileButton::click(bool) { notImplemented(); }
void QLineEdit::setWritingDirection(TextDirection) { notImplemented(); }
void GraphicsContext::drawFocusRing(Color const&) { notImplemented(); }
IntSize KWQFileButton::sizeForCharacterWidth(int) const { notImplemented(); return IntSize(); }
IntSize QTextEdit::sizeWithColumnsAndRows(int,int) const { notImplemented(); return IntSize(); }
void QComboBox::clear() { notImplemented(); }
int GraphicsContext::misspellingLineThickness() const { notImplemented(); return 0; }
void QComboBox::setFrameGeometry(IntRect const&) { notImplemented(); }
int QLineEdit::maxLength() const { notImplemented(); return 0; }
bool Widget::isEnabled() const { notImplemented(); return 0; }
void QTextEdit::setText(String const&) { notImplemented(); }
void Widget::paint(GraphicsContext*,IntRect const&) { notImplemented(); }
void GraphicsContext::addRoundedRectClip(IntRect const&,IntSize const&,IntSize const&,IntSize const&,IntSize const&) { notImplemented(); }
IntPoint FrameView::viewportToGlobal(IntPoint const&) const { notImplemented(); return IntPoint(); }
int QTextEdit::selectionEnd() { notImplemented(); return 0; }
void QTextEdit::setScrollBarModes(ScrollBarMode,ScrollBarMode) { notImplemented(); }
void QTextEdit::setReadOnly(bool) { notImplemented(); }
void QListBox::appendItem(QString const&,KWQListBoxItemType,bool) { notImplemented(); }
void QLineEdit::setPlaceholderString(String const&) { notImplemented(); }
Cursor::Cursor(Cursor const&) { notImplemented(); }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void ScrollView::removeChild(Widget*) { notImplemented(); }
void QTextEdit::selectAll() { notImplemented(); }
void GraphicsContext::endTransparencyLayer() { notImplemented(); }
void ScrollView::addChild(Widget*,int,int) { notImplemented(); }
void QTextEdit::setDisabled(bool) { notImplemented(); }
bool QScrollBar::scroll(KWQScrollDirection,KWQScrollGranularity,float) { notImplemented(); return 0; }
Widget::~Widget() { notImplemented(); }
IntSize QListBox::sizeForNumberOfLines(int) const { notImplemented(); return IntSize(); }
int QLineEdit::selectionStart() const { notImplemented(); return 0; }
QLineEdit::QLineEdit(QLineEdit::Type) { notImplemented(); }
void FrameView::updateBorder() { notImplemented(); }
bool QLineEdit::hasSelectedText() const { notImplemented(); return 0; }
QScrollBar::QScrollBar(Qt::Orientation,Widget*) { notImplemented(); }
void QListBox::doneAppendingItems() { notImplemented(); }
QTextEdit::QTextEdit(Widget*) { notImplemented(); }
bool ScrollView::inWindow() const { notImplemented(); return 0; }
bool QScrollBar::setValue(int) { notImplemented(); return 0; }
bool QTextEdit::hasSelectedText() const { notImplemented(); return 0; }
int QTextEdit::selectionStart() { notImplemented(); return 0; }
int ScrollView::scrollXOffset() const { notImplemented(); return 0; }
bool QListBox::isSelected(int) const { notImplemented(); return 0; }
void QLineEdit::setReadOnly(bool) { notImplemented(); }
void GraphicsContext::drawLineForText(int,int,int,int) { notImplemented(); }
QComboBox::~QComboBox() { notImplemented(); }
Cursor::Cursor(Image*) { notImplemented(); }
Widget::FocusPolicy QComboBox::focusPolicy() const { notImplemented(); return NoFocus; }
void GraphicsContext::clearShadow() { notImplemented(); }
void QTextEdit::setLineHeight(int) { notImplemented(); }
void QScrollBar::setKnobProportion(int,int) { notImplemented(); }
KWQFileButton::KWQFileButton(Frame*) { notImplemented(); }
IntRect QFontMetrics::boundingRect(int,int,int,int,int,QString const&,int,int) const { notImplemented(); return IntRect(); }
void QTextEdit::setSelectionStart(int) { notImplemented(); }
void GraphicsContext::beginTransparencyLayer(float) { notImplemented(); }
IntRect Widget::frameGeometry() const { notImplemented(); return IntRect(); }
void QListBox::setSelected(int,bool) { notImplemented(); }
void GraphicsContext::addFocusRingRect(int,int,int,int) { notImplemented(); }
void QTextEdit::setCursorPosition(int,int) { notImplemented(); }
int QFontMetrics::width(QString const&,int,int,int) const { notImplemented(); return 0; }
void Widget::setEnabled(bool) { notImplemented(); }
void QTextEdit::setSelectionEnd(int) { notImplemented(); }
void QComboBox::populate() { notImplemented(); }
void QTextEdit::setAlignment(Qt::AlignmentFlags) { notImplemented(); }
void QLineEdit::setCursorPosition(int) { notImplemented(); }
void GraphicsContext::drawText(int,int,int,int,int,int,int,QString const&) { notImplemented(); }
KJavaAppletWidget::KJavaAppletWidget(IntSize const&,Frame*,KXMLCore::HashMap<String,String> const&) { notImplemented(); }
QListBox::QListBox() { notImplemented(); }
QString QLineEdit::selectedText() const { notImplemented(); return QString(); }
void Widget::setIsSelected(bool) { notImplemented(); }
String QLineEdit::text() const { notImplemented(); return String(); }
void Widget::unlockDrawingFocus() { notImplemented(); }
void QLineEdit::setLiveSearch(bool) { notImplemented(); }
QComboBox::QComboBox() { notImplemented(); }
void QSlider::setMaxValue(double) { notImplemented(); }
void Widget::lockDrawingFocus() { notImplemented(); }
void QTextEdit::setSelectionRange(int,int) { notImplemented(); }
void ScrollView::scrollPointRecursively(int,int) { notImplemented(); }
IntSize QLineEdit::sizeForCharacterWidth(int) const { notImplemented(); return IntSize(); }
IntRect QFontMetrics::selectionRectForText(int,int,int,int,int,QChar const*,int,int,int,int,bool,bool,int,int,bool) const { notImplemented(); return IntRect(); }
void QTextEdit::getCursorPosition(int*,int*) const { notImplemented(); }
bool FrameView::isFrameView() const { notImplemented(); return 0; }
void QScrollBar::setSteps(int,int) { notImplemented(); }
void QLineEdit::setMaxLength(int) { notImplemented(); }
void QLineEdit::setAutoSaveName(String const&) { notImplemented(); }
int QComboBox::baselinePosition(int) const { notImplemented(); return 0; }
void QComboBox::appendItem(QString const&,KWQListBoxItemType,bool) { notImplemented(); }
void GraphicsContext::setShadow(int,int,int,Color const&) { notImplemented(); }
void Widget::setDrawingAlpha(float) { notImplemented(); }
QSlider::QSlider() { notImplemented(); }
int ScrollView::scrollYOffset() const { notImplemented(); return 0; }
void QComboBox::setCurrentItem(int) { notImplemented(); }
void QComboBox::setWritingDirection(TextDirection) { notImplemented(); }
IntSize QComboBox::sizeHint() const { notImplemented(); return IntSize(); }
void Widget::setFrameGeometry(IntRect const&) { notImplemented(); }
void QLineEdit::setSelection(int,int) { notImplemented(); }
void QLineEdit::setMaxResults(int) { notImplemented(); }
void QListBox::clear() { notImplemented(); }
bool QLineEdit::edited() const { notImplemented(); return 0; }
void GraphicsContext::clearFocusRing() { notImplemented(); }
String QTextEdit::text() const { notImplemented(); return String(); }
void Widget::disableFlushDrawing() { notImplemented(); }
void GraphicsContext::initFocusRing(int,int) { notImplemented(); }
void QSlider::setMinValue(double) { notImplemented(); }
void QTextEdit::setWordWrap(QTextEdit::WrapStyle) { notImplemented(); }
void GraphicsContext::drawLineForMisspelling(int,int,int) { notImplemented(); }
void QLineEdit::setText(String const&) { notImplemented(); }
double QSlider::value() const { notImplemented(); return 0; }
void QListBox::setSelectionMode(QListBox::SelectionMode) { notImplemented(); }
void KWQFileButton::setFilename(QString const&) { notImplemented(); }
void QLineEdit::setEdited(bool) { notImplemented(); }
IntRect QComboBox::frameGeometry() const { notImplemented(); return IntRect(); }
void QListBox::setWritingDirection(TextDirection) { notImplemented(); }
void QLineEdit::setAlignment(Qt::AlignmentFlags) { notImplemented(); }
WebCore::Widget::FocusPolicy KWQFileButton::focusPolicy() const { notImplemented(); return NoFocus; }
bool QLineEdit::checksDescendantsForFocus() const { notImplemented(); return false; }
int KWQFileButton::baselinePosition(int) const { notImplemented(); return 0; }
QSlider::~QSlider() { notImplemented(); }
void KWQFileButton::setFrameGeometry(WebCore::IntRect const&) { notImplemented(); }
QListBox::~QListBox() { notImplemented(); }
WebCore::IntRect KWQFileButton::frameGeometry() const { notImplemented(); return IntRect(); }
KWQFileButton::~KWQFileButton() { notImplemented(); }
WebCore::Widget::FocusPolicy QTextEdit::focusPolicy() const { notImplemented(); return NoFocus; }
WebCore::Widget::FocusPolicy QSlider::focusPolicy() const { notImplemented(); return NoFocus; }
void QListBox::setEnabled(bool) { notImplemented(); }
bool QListBox::checksDescendantsForFocus() const { notImplemented(); return 0; }
WebCore::Widget::FocusPolicy QListBox::focusPolicy() const { notImplemented(); return NoFocus; }
int QLineEdit::baselinePosition(int) const { notImplemented(); return 0; }
WebCore::IntSize QSlider::sizeHint() const { notImplemented(); return IntSize(); }
QLineEdit::~QLineEdit() { notImplemented(); }
QTextEdit::~QTextEdit() { notImplemented(); }
bool QTextEdit::checksDescendantsForFocus() const { notImplemented(); return false; }
WebCore::Widget::FocusPolicy QLineEdit::focusPolicy() const { notImplemented(); return NoFocus; }
QScrollBar::~QScrollBar() { notImplemented(); }
Path::Path(){ notImplemented(); }
Path::Path(const IntRect&, Type){ notImplemented(); }
Path::Path(const IntPointArray&){ notImplemented(); }
Path::~Path(){ notImplemented(); }
Path::Path(const Path&){ notImplemented(); }
Path& Path::operator=(const Path&){ notImplemented(); return*this; }
bool Path::contains(const IntPoint&) const{ notImplemented(); return false; }
void Path::translate(int, int){ notImplemented(); }
IntRect Path::boundingRect() const { notImplemented(); return IntRect(); }
static Cursor localCursor;
const Cursor& WebCore::crossCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::handCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::moveCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::iBeamCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::waitCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::helpCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::eastResizeCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::northResizeCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::northEastResizeCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::northWestResizeCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::southResizeCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::southEastResizeCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::southWestResizeCursor() { notImplemented(); return localCursor; }
const Cursor& WebCore::westResizeCursor() { notImplemented(); return localCursor; }
WebCore::MouseEvent::MouseEvent() { notImplemented(); }
void QLineEdit::setColors(Color const&,Color const&) { notImplemented(); }
void QTextEdit::setColors(Color const&,Color const&) { notImplemented(); }
QString searchableIndexIntroduction() { notImplemented(); return QString(); }
void KWQKCookieJar::setCookie(KURL const&,KURL const&,QString const&) { notImplemented(); }
QString KWQKCookieJar::cookie(KURL const&) { notImplemented(); return QString(); }
IntRect WebCore::screenRect(Widget*) { notImplemented(); return IntRect(); }
void ScrollView::scrollBy(int,int) { notImplemented(); }
void WebCore::Widget::clearFocus() { notImplemented(); }
int KWQFindNextSentenceFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void KWQFindSentenceBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }
int KWQFindNextWordFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void KWQFindWordBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }
bool KWQKCookieJar::cookieEnabled() { notImplemented(); return false; }
void WebCore::Widget::setFocus() { notImplemented(); }
void WebCore::TransferJob::kill() { notImplemented(); }
void WebCore::TransferJob::addMetaData(KXMLCore::HashMap<String,String> const&) { notImplemented(); }
QString WebCore::TransferJob::queryMetaData(QString const&) const { notImplemented(); return QString(); }
int WebCore::TransferJob::error() const { notImplemented(); return 0; }
QString WebCore::TransferJob::errorText() const { notImplemented(); return "Houston, we have a problem."; }
bool WebCore::TransferJob::isErrorPage() const { notImplemented(); return 0; }
WebCore::TransferJob::TransferJob(WebCore::TransferJobClient*, const String&, KURL const&,WebCore::FormData const&) { notImplemented(); }
void WebCore::Widget::hide() { notImplemented(); }
PluginInfo*PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplemented(); return 0; }
void WebCore::refreshPlugins(bool) { notImplemented(); }
int WebCore::screenDepth(WebCore::Widget*) { notImplemented(); return 96; }
static QFontMetrics localFontMetrics;
float QFontMetrics::xHeight() const { notImplemented(); return 0; }
IntRect WebCore::usableScreenRect(WebCore::Widget*) { notImplemented(); return IntRect(0,0,800,600); }
void Widget::setActiveWindow() { notImplemented(); }
RenderTheme* WebCore::theme() { notImplemented(); return 0; }
Array<char> KWQServeSynchronousRequest(Loader*,DocLoader*,TransferJob*,KURL&,QString&) { notImplemented(); return 0; }
Widget* WebCore::FrameView::topLevelWidget() const { notImplemented(); return 0; }
void FrameWin::respondToChangedContents(void) { notImplemented(); }
void FrameWin::unfocusWindow(void) { notImplemented(); }
bool FrameWin::locationbarVisible(void) { notImplemented(); return 0; }
void FrameWin::respondToChangedSelection(WebCore::SelectionController const&,bool) { notImplemented(); }
void FrameWin::clearUndoRedoOperations(void) { notImplemented(); }
void FrameWin::issueRedoCommand(void) { notImplemented(); }
KJS::Bindings::Instance * FrameWin::getObjectInstanceForWidget(WebCore::Widget *) { notImplemented(); return 0; }
KJS::Bindings::Instance * FrameWin::getEmbedInstanceForWidget(WebCore::Widget *) { notImplemented(); return 0; }
bool FrameWin::canRedo(void)const { notImplemented(); return 0; }
bool FrameWin::canUndo(void)const { notImplemented(); return 0; }
bool FrameWin::runJavaScriptPrompt(String const&,String const&,String &) { notImplemented(); return 0; }
void FrameWin::recordFormValue(QString const&,QString const&,WebCore::HTMLFormElementImpl *) { notImplemented(); }
void FrameWin::registerCommandForRedo(WebCore::EditCommandPtr const&) { notImplemented(); }
void FrameWin::runJavaScriptAlert(String const&) { notImplemented(); }
bool FrameWin::runJavaScriptConfirm(String const&) { notImplemented(); return 0; }
bool FrameWin::openURL(KURL const&) { notImplemented(); return 0; }
void FrameWin::urlSelected(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }
void FrameWin::saveDocumentState(void) { notImplemented(); }
void FrameWin::print(void) { notImplemented(); }
KJS::Bindings::Instance * FrameWin::getAppletInstanceForWidget(WebCore::Widget *) { notImplemented(); return 0; }
bool FrameWin::passMouseDownEventToWidget(WebCore::Widget *) { notImplemented(); return 0; }
void FrameWin::registerCommandForUndo(WebCore::EditCommandPtr const&) { notImplemented(); }
void FrameWin::issueCutCommand(void) { notImplemented(); }
void FrameWin::issueCopyCommand(void) { notImplemented(); }
void FrameWin::openURLRequest(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }
void FrameWin::addMessageToConsole(String const&,unsigned int,String const&) { notImplemented(); }
bool FrameWin::passWheelEventToChildWidget(WebCore::NodeImpl *) { notImplemented(); return 0; }
void FrameWin::issueUndoCommand(void) { notImplemented(); }
QString FrameWin::mimeTypeForFileName(QString const&)const { notImplemented(); return QString(); }
void FrameWin::clearRecordedFormValues(void) { notImplemented(); }
void FrameWin::issuePasteCommand(void) { notImplemented(); }
bool FrameWin::shouldChangeSelection(WebCore::SelectionController const&,WebCore::SelectionController const&,WebCore::EAffinity,bool)const { notImplemented(); return 0; }
void FrameWin::scheduleClose(void) { notImplemented(); }
void FrameWin::markMisspellingsInAdjacentWords(WebCore::VisiblePosition const&) { notImplemented(); }
void FrameWin::markMisspellings(WebCore::SelectionController const&) { notImplemented(); }
bool FrameWin::menubarVisible(void) { notImplemented(); return 0; }
bool FrameWin::personalbarVisible(void) { notImplemented(); return 0; }
bool FrameWin::statusbarVisible(void) { notImplemented(); return 0; }
bool FrameWin::toolbarVisible(void) { notImplemented(); return 0; }
void FrameWin::issueTransposeCommand(void) { notImplemented(); }
QString FrameWin::userAgent(void)const { notImplemented(); return QString(); }
void FrameWin::submitForm(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }
bool FrameWin::canPaste(void)const { notImplemented(); return 0; }
QString FrameWin::incomingReferrer(void)const { notImplemented(); return QString(); }
enum WebCore::ObjectContentType FrameWin::objectContentType(KURL const&,QString const&) { notImplemented(); return (ObjectContentType)0; }
WebCore::Frame * FrameWin::createFrame(KURL const&,QString const&,WebCore::RenderPart *,String const&) { notImplemented(); return 0; }
bool FrameWin::canGoBackOrForward(int)const { notImplemented(); return 0; }
void FrameWin::issuePasteAndMatchStyleCommand(void) { notImplemented(); }
WebCore::Plugin * FrameWin::createPlugin(KURL const&,QStringList const&,QStringList const&,QString const&) { notImplemented(); return 0; }
String FrameWin::generateFrameName(void) { notImplemented(); return String(); }
void BrowserExtensionWin::setTypedIconURL(KURL const&,QString const&) { notImplemented(); }
void BrowserExtensionWin::openURLRequest(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }
int BrowserExtensionWin::getHistoryLength(void) { notImplemented(); return 0; }
bool BrowserExtensionWin::canRunModal(void) { notImplemented(); return 0; }
void BrowserExtensionWin::openURLNotify(void) { notImplemented(); }
void BrowserExtensionWin::createNewWindow(KURL const&,struct WebCore::URLArgs const&,struct WebCore::WindowArgs const&,WebCore::Frame * &) { notImplemented(); }
bool BrowserExtensionWin::canRunModalNow(void) { notImplemented(); return 0; }
void BrowserExtensionWin::runModal(void) { notImplemented(); }
void BrowserExtensionWin::goBackOrForward(int) { notImplemented(); }
void BrowserExtensionWin::setIconURL(KURL const&) { notImplemented(); }
void BrowserExtensionWin::createNewWindow(KURL const&,struct WebCore::URLArgs const&) { notImplemented(); }
void QSlider::setFont(class WebCore::Font const&) { notImplemented(); }
void QLineEdit::setFont(class WebCore::Font const&) { notImplemented(); }
static WebCore::Font localFont;
void QListBox::setFont(class WebCore::Font const&) { notImplemented(); }
QFontMetrics::QFontMetrics(class WebCore::FontDescription const&) { notImplemented(); }
void WebCore::Widget::setFont(class WebCore::Font const&) { notImplemented(); }
void QComboBox::setFont(class WebCore::Font const&) { notImplemented(); }
void QTextEdit::setFont(class WebCore::Font const&) { notImplemented(); }
void QTextEdit::setWritingDirection(enum WebCore::TextDirection) { notImplemented(); }
int QFontMetrics::checkSelectionPoint(class QChar const *,int,int,int,int,int,int,int,int,bool,int,bool,bool,bool)const { notImplemented(); return 0; }
GraphicsContext::GraphicsContext() { notImplemented(); }

// Completely empty stubs (mostly to allow DRT to run):
bool WebCore::historyContains(QString const&) { return false; }
QString submitButtonDefaultLabel() { return "Submit"; }
QString inputElementAltText() { return QString(); }
QString resetButtonDefaultLabel() { return "Reset"; }
QString KLocale::language() { return "en"; }

Cursor::~Cursor() { }

QFontMetrics::QFontMetrics() { }
QFontMetrics::~QFontMetrics() { }
QFontMetrics::QFontMetrics(QFontMetrics const&) { }
QFontMetrics& QFontMetrics::operator=(QFontMetrics const&) { return localFontMetrics; }
float QFontMetrics::floatWidth(QChar const*,int,int,int,int,int,int,int,bool) const { return 14.0; }
int QFontMetrics::lineSpacing() const { return 10; }
int QFontMetrics::ascent() const { return 10; }
int QFontMetrics::height() const { return 10; }
int QFontMetrics::descent() const { return 10; }
void QFontMetrics::setFontDescription(class WebCore::FontDescription const&) { }
bool QFontMetrics::isFixedPitch(void)const { return 0; }

void FrameWin::restoreDocumentState() { }
void FrameWin::partClearedInBegin() { }
void FrameWin::createEmptyDocument() { }
QString FrameWin::overrideMediaType() const { return QString(); }
void FrameWin::setTitle(String const&) { }
void FrameWin::handledOnloadEvents() { }
RangeImpl* FrameWin::markedTextRange() const { return 0; }
bool FrameWin::passSubframeEventToSubframe(WebCore::MouseEventWithHitTestResults&) { return false; }
bool FrameWin::lastEventIsMouseUp() const { return false; }

BrowserExtensionWin::BrowserExtensionWin(WebCore::Frame*) { }

bool KWQServeRequest(Loader*, DocLoader*, WebCore::TransferJob*) { return false; } // false may not be "right" as a stub.
bool KWQCheckIfReloading(WebCore::DocLoader*) { return false; }
void KWQCheckCacheObjectStatus(DocLoader*, CachedObject*) { }

Widget::Widget() { }
void Widget::show() { }
void Widget::setCursor(Cursor const&) { }

void ScrollView::resizeContents(int,int) { }
WebCore::ScrollBarMode ScrollView::hScrollBarMode() const { return ScrollBarAlwaysOff; }
WebCore::ScrollBarMode ScrollView::vScrollBarMode() const { return ScrollBarAlwaysOff; }
void ScrollView::suppressScrollBars(bool,bool) { }
void ScrollView::setHScrollBarMode(ScrollBarMode) { }
void ScrollView::setVScrollBarMode(ScrollBarMode) { }
void ScrollView::setScrollBarsMode(ScrollBarMode) { }
int ScrollView::visibleHeight() const { return 600; }
int ScrollView::visibleWidth() const { return 800; }
void ScrollView::setContentsPos(int,int) { }
int ScrollView::contentsX() const { return 0; }
int ScrollView::contentsY() const { return 0; }
int ScrollView::contentsHeight() const { return 600; }
int ScrollView::contentsWidth() const { return 800; }
void ScrollView::viewportToContents(int x1, int y1, int& x2, int& y2) { x2 = x1; y2 = y2; }

void ScrollView::updateContents(const IntRect&,bool) { }
void ScrollView::setStaticBackground(bool) { }

TransferJob::TransferJob(WebCore::TransferJobClient*,const String&,KURL const&) { }
void TransferJob::addMetaData(QString const&,QString const&) { }

Font const& GraphicsContext::font() const { return localFont; }
void GraphicsContext::setFont(Font const&) { }
void GraphicsContext::drawText(int,int,int,int,QChar const*,int,int,int,int,Color const&,TextDirection,bool,int,int,bool) { }
QFontMetrics GraphicsContext::fontMetrics() const { return QFontMetrics(); }

