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
void GraphicsContext::save() { notImplemented(); }
void Widget::enableFlushDrawing() { notImplemented(); }
void GraphicsContext::drawHighlightForText(int,int,int,int,int,QChar const*,int,int,int,int,Color const&,TextDirection,bool,int,int,bool) { notImplemented(); }
String QTextEdit::textWithHardLineBreaks() const { notImplemented(); return String(); }
IntPoint Widget::mapFromGlobal(IntPoint const&) const { notImplemented(); return IntPoint(); }
void GraphicsContext::addClip(IntRect const&) { notImplemented(); }
int QLineEdit::cursorPosition() const { notImplemented(); return 0; }
void GraphicsContext::setPen(Pen::PenStyle) { notImplemented(); }
Color GraphicsContext::selectedTextBackgroundColor() const { notImplemented(); return Color(); }
QFontMetrics GraphicsContext::fontMetrics() const { notImplemented(); return QFontMetrics(); }
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
bool KWQServeRequest(Loader*,DocLoader*,WebCore::TransferJob*) { notImplemented(); return 0; }
void QTextEdit::setText(String const&) { notImplemented(); }
void Widget::paint(GraphicsContext*,IntRect const&) { notImplemented(); }
void GraphicsContext::addRoundedRectClip(IntRect const&,IntSize const&,IntSize const&,IntSize const&,IntSize const&) { notImplemented(); }
IntPoint FrameView::viewportToGlobal(IntPoint const&) const { notImplemented(); return IntPoint(); }
int QTextEdit::selectionEnd() { notImplemented(); return 0; }
void QTextEdit::setScrollBarModes(ScrollBarMode,ScrollBarMode) { notImplemented(); }
void GraphicsContext::drawEllipse(int,int,int,int) { notImplemented(); }
void GraphicsContext::setPen(unsigned int) { notImplemented(); }
void QTextEdit::setReadOnly(bool) { notImplemented(); }
void QListBox::appendItem(QString const&,KWQListBoxItemType,bool) { notImplemented(); }
void QLineEdit::setPlaceholderString(String const&) { notImplemented(); }
Cursor::Cursor(Cursor const&) { notImplemented(); }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void ScrollView::removeChild(Widget*) { notImplemented(); }
void QTextEdit::selectAll() { notImplemented(); }
void GraphicsContext::fillRect(int,int,int,int,Brush const&) { notImplemented(); }
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
GraphicsContext::GraphicsContext() { notImplemented(); }
QComboBox::~QComboBox() { notImplemented(); }
Cursor::Cursor(Image*) { notImplemented(); }
Widget::FocusPolicy QComboBox::focusPolicy() const { notImplemented(); return NoFocus; }
void GraphicsContext::drawImageAtPoint(Image*,IntPoint const&,Image::CompositeOperator) { notImplemented(); }
void GraphicsContext::clearShadow() { notImplemented(); }
void GraphicsContext::setPen(Pen const&) { notImplemented(); }
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
void GraphicsContext::restore() { notImplemented(); }
int QFontMetrics::width(QString const&,int,int,int) const { notImplemented(); return 0; }
void Widget::setEnabled(bool) { notImplemented(); }
void QTextEdit::setSelectionEnd(int) { notImplemented(); }
void QComboBox::populate() { notImplemented(); }
void QTextEdit::setAlignment(Qt::AlignmentFlags) { notImplemented(); }
void QLineEdit::setCursorPosition(int) { notImplemented(); }
void GraphicsContext::drawText(int,int,int,int,int,int,int,QString const&) { notImplemented(); }
static Pen localPen;
Pen const& GraphicsContext::pen() const { notImplemented(); return localPen; }
KJavaAppletWidget::KJavaAppletWidget(IntSize const&,Frame*,KXMLCore::HashMap<String,String> const&) { notImplemented(); }
QListBox::QListBox() { notImplemented(); }
QString QLineEdit::selectedText() const { notImplemented(); return QString(); }
void Widget::setIsSelected(bool) { notImplemented(); }
String QLineEdit::text() const { notImplemented(); return String(); }
void Widget::unlockDrawingFocus() { notImplemented(); }
void QLineEdit::setLiveSearch(bool) { notImplemented(); }
bool GraphicsContext::paintingDisabled() const { notImplemented(); return 0; }
QComboBox::QComboBox() { notImplemented(); }
void GraphicsContext::drawConvexPolygon(IntPointArray const&) { notImplemented(); }
void QSlider::setMaxValue(double) { notImplemented(); }
void Widget::lockDrawingFocus() { notImplemented(); }
void GraphicsContext::drawLine(int,int,int,int) { notImplemented(); }
void GraphicsContext::setBrush(Brush::BrushStyle) { notImplemented(); }
void QTextEdit::setSelectionRange(int,int) { notImplemented(); }
void GraphicsContext::drawText(int,int,int,int,QChar const*,int,int,int,int,Color const&,TextDirection,bool,int,int,bool) { notImplemented(); }
void ScrollView::scrollPointRecursively(int,int) { notImplemented(); }
IntSize QLineEdit::sizeForCharacterWidth(int) const { notImplemented(); return IntSize(); }
IntRect QFontMetrics::selectionRectForText(int,int,int,int,int,QChar const*,int,int,int,int,bool,bool,int,int,bool) const { notImplemented(); return IntRect(); }
void QTextEdit::getCursorPosition(int*,int*) const { notImplemented(); }
bool FrameView::isFrameView() const { notImplemented(); return 0; }
void QScrollBar::setSteps(int,int) { notImplemented(); }
void QLineEdit::setMaxLength(int) { notImplemented(); }
void Widget::setCursor(Cursor const&) { notImplemented(); }
void QLineEdit::setAutoSaveName(String const&) { notImplemented(); }
int QComboBox::baselinePosition(int) const { notImplemented(); return 0; }
void QComboBox::appendItem(QString const&,KWQListBoxItemType,bool) { notImplemented(); }
void GraphicsContext::setShadow(int,int,int,Color const&) { notImplemented(); }
void Widget::setDrawingAlpha(float) { notImplemented(); }
QSlider::QSlider() { notImplemented(); }
void GraphicsContext::drawScaledAndTiledImage(Image*,int,int,int,int,int,int,int,int,Image::TileRule,Image::TileRule,void*) { notImplemented(); }
int ScrollView::scrollYOffset() const { notImplemented(); return 0; }
void GraphicsContext::drawImage(Image*,int,int,int,int,int,int,int,int,Image::CompositeOperator,void*) { notImplemented(); }
void GraphicsContext::setBrush(Brush const&) { notImplemented(); }
void QComboBox::setCurrentItem(int) { notImplemented(); }
void QComboBox::setWritingDirection(TextDirection) { notImplemented(); }
IntSize QComboBox::sizeHint() const { notImplemented(); return IntSize(); }
void GraphicsContext::drawRect(int,int,int,int) { notImplemented(); }
void Widget::setFrameGeometry(IntRect const&) { notImplemented(); }
void QLineEdit::setSelection(int,int) { notImplemented(); }
void QLineEdit::setMaxResults(int) { notImplemented(); }
void QListBox::clear() { notImplemented(); }
bool QLineEdit::edited() const { notImplemented(); return 0; }
void GraphicsContext::drawTiledImage(Image*,int,int,int,int,int,int,void*) { notImplemented(); }
void GraphicsContext::clearFocusRing() { notImplemented(); }
String QTextEdit::text() const { notImplemented(); return String(); }
void GraphicsContext::drawImageInRect(Image*,IntRect const&,Image::CompositeOperator) { notImplemented(); }
void Widget::disableFlushDrawing() { notImplemented(); }
void GraphicsContext::initFocusRing(int,int) { notImplemented(); }
void QSlider::setMinValue(double) { notImplemented(); }
void QTextEdit::setWordWrap(QTextEdit::WrapStyle) { notImplemented(); }
void GraphicsContext::drawLineForMisspelling(int,int,int) { notImplemented(); }
void QLineEdit::setText(String const&) { notImplemented(); }
double QSlider::value() const { notImplemented(); return 0; }
void GraphicsContext::setBrush(unsigned int) { notImplemented(); }
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
void WebCore::ScrollView::scrollBy(int,int) { notImplemented(); }
void WebCore::Widget::clearFocus() { notImplemented(); }
int KWQFindNextSentenceFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void KWQFindSentenceBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }
int KWQFindNextWordFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void KWQFindWordBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }
bool KWQKCookieJar::cookieEnabled() { notImplemented(); return false; }
void WebCore::Widget::setFocus() { notImplemented(); }
void WebCore::GraphicsContext::setPaintingDisabled(bool) { notImplemented(); }
void WebCore::GraphicsContext::fillRect(WebCore::IntRect const&,WebCore::Brush const&) { notImplemented(); }
WebCore::GraphicsContext::~GraphicsContext() { notImplemented(); }
WebCore::GraphicsContext::GraphicsContext(bool) { notImplemented(); }
void WebCore::ScrollView::viewportToContents(int,int,int&,int&) { notImplemented(); }
void WebCore::TransferJob::kill() { notImplemented(); }
void WebCore::TransferJob::addMetaData(KXMLCore::HashMap<WebCore::String,WebCore::String> const&) { notImplemented(); }
QString WebCore::TransferJob::queryMetaData(QString const&) const { notImplemented(); return QString(); }
int WebCore::TransferJob::error() const { notImplemented(); return 0; }
QString WebCore::TransferJob::errorText() const { notImplemented(); return "Houston, we have a problem."; }
bool WebCore::TransferJob::isErrorPage() const { notImplemented(); return 0; }
WebCore::TransferJob::TransferJob(WebCore::TransferJobClient*,KURL const&,WebCore::FormData const&) { notImplemented(); }
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
void WebCore::FrameWin::respondToChangedContents(void) { notImplemented(); }
void WebCore::FrameWin::unfocusWindow(void) { notImplemented(); }
bool WebCore::FrameWin::locationbarVisible(void) { notImplemented(); return 0; }
void WebCore::FrameWin::respondToChangedSelection(WebCore::SelectionController const &,bool) { notImplemented(); }
void WebCore::FrameWin::clearUndoRedoOperations(void) { notImplemented(); }
void WebCore::FrameWin::issueRedoCommand(void) { notImplemented(); }
KJS::Bindings::Instance * WebCore::FrameWin::getObjectInstanceForWidget(WebCore::Widget *) { notImplemented(); return 0; }
KJS::Bindings::Instance * WebCore::FrameWin::getEmbedInstanceForWidget(WebCore::Widget *) { notImplemented(); return 0; }
bool WebCore::FrameWin::canRedo(void)const { notImplemented(); return 0; }
bool WebCore::FrameWin::canUndo(void)const { notImplemented(); return 0; }
bool WebCore::FrameWin::runJavaScriptPrompt(WebCore::String const &,WebCore::String const &,WebCore::String &) { notImplemented(); return 0; }
void WebCore::FrameWin::recordFormValue(QString const &,QString const &,WebCore::HTMLFormElementImpl *) { notImplemented(); }
void WebCore::FrameWin::registerCommandForRedo(WebCore::EditCommandPtr const &) { notImplemented(); }
void WebCore::FrameWin::runJavaScriptAlert(WebCore::String const &) { notImplemented(); }
bool WebCore::FrameWin::runJavaScriptConfirm(WebCore::String const &) { notImplemented(); return 0; }
bool WebCore::FrameWin::openURL(KURL const &) { notImplemented(); return 0; }
void WebCore::FrameWin::urlSelected(KURL const &,struct WebCore::URLArgs const &) { notImplemented(); }
void WebCore::FrameWin::saveDocumentState(void) { notImplemented(); }
void WebCore::FrameWin::print(void) { notImplemented(); }
KJS::Bindings::Instance * WebCore::FrameWin::getAppletInstanceForWidget(WebCore::Widget *) { notImplemented(); return 0; }
bool WebCore::FrameWin::passMouseDownEventToWidget(WebCore::Widget *) { notImplemented(); return 0; }
void WebCore::FrameWin::registerCommandForUndo(WebCore::EditCommandPtr const &) { notImplemented(); }
void WebCore::FrameWin::issueCutCommand(void) { notImplemented(); }
void WebCore::FrameWin::issueCopyCommand(void) { notImplemented(); }
void WebCore::FrameWin::openURLRequest(KURL const &,struct WebCore::URLArgs const &) { notImplemented(); }
void WebCore::FrameWin::addMessageToConsole(WebCore::String const &,unsigned int,WebCore::String const &) { notImplemented(); }
bool WebCore::FrameWin::passWheelEventToChildWidget(WebCore::NodeImpl *) { notImplemented(); return 0; }
void WebCore::FrameWin::issueUndoCommand(void) { notImplemented(); }
QString WebCore::FrameWin::mimeTypeForFileName(QString const &)const { notImplemented(); return QString(); }
void WebCore::FrameWin::clearRecordedFormValues(void) { notImplemented(); }
void WebCore::FrameWin::issuePasteCommand(void) { notImplemented(); }
WebCore::RangeImpl * WebCore::FrameWin::markedTextRange(void)const { notImplemented(); return 0; }
bool WebCore::FrameWin::shouldChangeSelection(WebCore::SelectionController const &,WebCore::SelectionController const &,WebCore::EAffinity,bool)const { notImplemented(); return 0; }
void WebCore::FrameWin::scheduleClose(void) { notImplemented(); }
void WebCore::FrameWin::markMisspellingsInAdjacentWords(WebCore::VisiblePosition const &) { notImplemented(); }
void WebCore::FrameWin::markMisspellings(WebCore::SelectionController const &) { notImplemented(); }
bool WebCore::FrameWin::menubarVisible(void) { notImplemented(); return 0; }
bool WebCore::FrameWin::personalbarVisible(void) { notImplemented(); return 0; }
bool WebCore::FrameWin::lastEventIsMouseUp(void)const { notImplemented(); return 0; }
bool WebCore::FrameWin::statusbarVisible(void) { notImplemented(); return 0; }
bool WebCore::FrameWin::toolbarVisible(void) { notImplemented(); return 0; }
void WebCore::FrameWin::issueTransposeCommand(void) { notImplemented(); }
QString WebCore::FrameWin::userAgent(void)const { notImplemented(); return QString(); }
void WebCore::FrameWin::submitForm(KURL const &,struct WebCore::URLArgs const &) { notImplemented(); }
bool WebCore::FrameWin::canPaste(void)const { notImplemented(); return 0; }
QString WebCore::FrameWin::incomingReferrer(void)const { notImplemented(); return QString(); }
enum WebCore::ObjectContentType WebCore::FrameWin::objectContentType(KURL const &,QString const &) { notImplemented(); return (ObjectContentType)0; }
bool WebCore::FrameWin::passSubframeEventToSubframe(WebCore::MouseEventWithHitTestResults &) { notImplemented(); return 0; }
WebCore::Frame * WebCore::FrameWin::createFrame(KURL const &,QString const &,WebCore::RenderPart *,WebCore::String const &) { notImplemented(); return 0; }
bool WebCore::FrameWin::canGoBackOrForward(int)const { notImplemented(); return 0; }
void WebCore::FrameWin::issuePasteAndMatchStyleCommand(void) { notImplemented(); }
WebCore::Plugin * WebCore::FrameWin::createPlugin(KURL const &,QStringList const &,QStringList const &,QString const &) { notImplemented(); return 0; }
WebCore::String WebCore::FrameWin::generateFrameName(void) { notImplemented(); return String(); }
void WebCore::BrowserExtensionWin::setTypedIconURL(KURL const &,QString const &) { notImplemented(); }
void WebCore::BrowserExtensionWin::openURLRequest(KURL const &,struct WebCore::URLArgs const &) { notImplemented(); }
int WebCore::BrowserExtensionWin::getHistoryLength(void) { notImplemented(); return 0; }
bool WebCore::BrowserExtensionWin::canRunModal(void) { notImplemented(); return 0; }
void WebCore::BrowserExtensionWin::openURLNotify(void) { notImplemented(); }
void WebCore::BrowserExtensionWin::createNewWindow(KURL const &,struct WebCore::URLArgs const &,struct WebCore::WindowArgs const &,WebCore::Frame * &) { notImplemented(); }
bool WebCore::BrowserExtensionWin::canRunModalNow(void) { notImplemented(); return 0; }
void WebCore::BrowserExtensionWin::runModal(void) { notImplemented(); }
void WebCore::BrowserExtensionWin::goBackOrForward(int) { notImplemented(); }
void WebCore::BrowserExtensionWin::setIconURL(KURL const &) { notImplemented(); }
void WebCore::BrowserExtensionWin::createNewWindow(KURL const &,struct WebCore::URLArgs const &) { notImplemented(); }
void  QSlider::setFont(class WebCore::Font const &) { notImplemented(); }
void  QLineEdit::setFont(class WebCore::Font const &) { notImplemented(); }
static WebCore::Font localFont;
class WebCore::Font const &  WebCore::GraphicsContext::font(void)const  { notImplemented(); return localFont; }
void  QListBox::setFont(class WebCore::Font const &) { notImplemented(); }
QFontMetrics::QFontMetrics(class WebCore::FontDescription const &) { notImplemented(); }
void  WebCore::Widget::setFont(class WebCore::Font const &) { notImplemented(); }
void  QComboBox::setFont(class WebCore::Font const &) { notImplemented(); }
void  QTextEdit::setFont(class WebCore::Font const &) { notImplemented(); }
void  WebCore::GraphicsContext::setFont(class WebCore::Font const &) { notImplemented(); }
void  QTextEdit::setWritingDirection(enum WebCore::TextDirection) { notImplemented(); }
int  QFontMetrics::checkSelectionPoint(class QChar const *,int,int,int,int,int,int,int,int,bool,int,bool,bool,bool)const  { notImplemented(); return 0; }


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
void  QFontMetrics::setFontDescription(class WebCore::FontDescription const &) { }
bool  QFontMetrics::isFixedPitch(void)const  { return 0; }

void WebCore::FrameWin::restoreDocumentState() { }
void WebCore::FrameWin::partClearedInBegin() { }
void WebCore::FrameWin::createEmptyDocument() { }

WebCore::BrowserExtensionWin::BrowserExtensionWin(WebCore::Frame*) { }

bool KWQServeRequest(Loader*,Request*, TransferJob*) { return false; } // false may not be "right" as a stub.
bool KWQCheckIfReloading(WebCore::DocLoader*) { return false; }
void KWQCheckCacheObjectStatus(DocLoader*, CachedObject*) { }

Widget::Widget() { }
void Widget::show() { }

QString WebCore::FrameWin::overrideMediaType() const { return QString(); }
void WebCore::FrameWin::setTitle(WebCore::String const &) { }
void WebCore::FrameWin::handledOnloadEvents(void) { }

void ScrollView::resizeContents(int,int) { }
WebCore::ScrollBarMode ScrollView::hScrollBarMode() const { return ScrollBarAlwaysOff; }
WebCore::ScrollBarMode ScrollView::vScrollBarMode() const { return ScrollBarAlwaysOff; }
void ScrollView::suppressScrollBars(bool,bool) { }
void ScrollView::setHScrollBarMode(ScrollBarMode) { }
void ScrollView::setVScrollBarMode(ScrollBarMode) { }
void ScrollView::setScrollBarsMode(ScrollBarMode) { }
int WebCore::ScrollView::visibleHeight() const { return 600; }
int WebCore::ScrollView::visibleWidth() const { return 800; }
void WebCore::ScrollView::setContentsPos(int,int) { }
int WebCore::ScrollView::contentsX() const { return 0; }
int WebCore::ScrollView::contentsY() const { return 0; }
int WebCore::ScrollView::contentsHeight() const { return 600; }
int WebCore::ScrollView::contentsWidth() const { return 800; }
void ScrollView::updateContents(const IntRect&,bool) { }
void ScrollView::setStaticBackground(bool) { }

WebCore::TransferJob::TransferJob(WebCore::TransferJobClient*,KURL const&) { }
void WebCore::TransferJob::addMetaData(QString const&,QString const&) { }
