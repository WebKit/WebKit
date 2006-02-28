#include "config.h"

#define WIN32_COMPILE_HACK

#include <stdio.h>
#include <stdlib.h>
#include "NodeImpl.h"
#include "KWQLineEdit.h"
#include "KWQFont.h"
#include "KWQFileButton.h"
#include "KWQTextEdit.h"
#include "KWQComboBox.h"
#include "IntPoint.h"
#include "Widget.h"
#include "KWQFontMetrics.h"
#include "KWQPainter.h"
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

using namespace WebCore;

static void notImplemented() { puts("Not yet implemented"); abort(); }

void QLineEdit::selectAll() { notImplemented(); }
void QPainter::save() { notImplemented(); }
void Widget::enableFlushDrawing() { notImplemented(); }
void QPainter::drawHighlightForText(int,int,int,int,int,QChar const*,int,int,int,int,Color const&,QPainter::TextDirection,bool,int,int,bool) { notImplemented(); }
void QFont::setPrinterFont(bool) { notImplemented(); }
String QTextEdit::textWithHardLineBreaks() const { notImplemented(); return String(); }
IntPoint Widget::mapFromGlobal(IntPoint const&) const { notImplemented(); return IntPoint(); }
void QPainter::addClip(IntRect const&) { notImplemented(); }
int QLineEdit::cursorPosition() const { notImplemented(); return 0; }
void QPainter::setPen(Pen::PenStyle) { notImplemented(); }
Color QPainter::selectedTextBackgroundColor() const { notImplemented(); return Color(); }
QFontMetrics QPainter::fontMetrics() const { notImplemented(); return QFontMetrics(); }
void Widget::show() { notImplemented(); }
void QFont::setItalic(bool) { notImplemented(); }
void QSlider::setValue(double) { notImplemented(); }
void QLineEdit::addSearchResult() { notImplemented(); }
void KWQFileButton::click(bool) { notImplemented(); }
void QLineEdit::setWritingDirection(QPainter::TextDirection) { notImplemented(); }
void QPainter::drawFocusRing(Color const&) { notImplemented(); }
IntSize KWQFileButton::sizeForCharacterWidth(int) const { notImplemented(); return IntSize(); }
IntSize QTextEdit::sizeWithColumnsAndRows(int,int) const { notImplemented(); return IntSize(); }
void QComboBox::clear() { notImplemented(); }
int QPainter::misspellingLineThickness() const { notImplemented(); return 0; }
void QComboBox::setFrameGeometry(IntRect const&) { notImplemented(); }
int QLineEdit::maxLength() const { notImplemented(); return 0; }
bool Widget::isEnabled() const { notImplemented(); return 0; }
bool KWQServeRequest(Loader*,DocLoader*,WebCore::TransferJob*) { notImplemented(); return 0; }
void QTextEdit::setText(String const&) { notImplemented(); }
void Widget::paint(QPainter*,IntRect const&) { notImplemented(); }
void QPainter::addRoundedRectClip(IntRect const&,IntSize const&,IntSize const&,IntSize const&,IntSize const&) { notImplemented(); }
IntPoint FrameView::viewportToGlobal(IntPoint const&) const { notImplemented(); return IntPoint(); }
int QTextEdit::selectionEnd() { notImplemented(); return 0; }
void QFont::determinePitch() const { notImplemented(); }
void QTextEdit::setScrollBarModes(ScrollBarMode,ScrollBarMode) { notImplemented(); }
void QPainter::drawEllipse(int,int,int,int) { notImplemented(); }
void QPainter::setPen(unsigned int) { notImplemented(); }
void QTextEdit::setReadOnly(bool) { notImplemented(); }
void QListBox::appendItem(QString const&,KWQListBoxItemType,bool) { notImplemented(); }
void QLineEdit::setPlaceholderString(String const&) { notImplemented(); }
Cursor::Cursor(Cursor const&) { notImplemented(); }
Widget::FocusPolicy Widget::focusPolicy() const { notImplemented(); return NoFocus; }
void ScrollView::removeChild(Widget*) { notImplemented(); }
void QTextEdit::selectAll() { notImplemented(); }
void QPainter::fillRect(int,int,int,int,Brush const&) { notImplemented(); }
void QPainter::endTransparencyLayer() { notImplemented(); }
QFont::QFont(QFont const&) { notImplemented(); }
void ScrollView::addChild(Widget*,int,int) { notImplemented(); }
void QTextEdit::setDisabled(bool) { notImplemented(); }
bool QScrollBar::scroll(KWQScrollDirection,KWQScrollGranularity,float) { notImplemented(); return 0; }
Widget::~Widget() { notImplemented(); }
IntRect QPainter::xForm(IntRect const&) const { notImplemented(); return IntRect(); }
IntSize QListBox::sizeForNumberOfLines(int) const { notImplemented(); return IntSize(); }
void ScrollView::resizeContents(int,int) { notImplemented(); }
int QLineEdit::selectionStart() const { notImplemented(); return 0; }
QLineEdit::QLineEdit(QLineEdit::Type) { notImplemented(); }
void FrameView::updateBorder() { notImplemented(); }
bool QLineEdit::hasSelectedText() const { notImplemented(); return 0; }
QScrollBar::QScrollBar(Qt::Orientation,Widget*) { notImplemented(); }
void QListBox::doneAppendingItems() { notImplemented(); }
QTextEdit::QTextEdit(Widget*) { notImplemented(); }
bool ScrollView::inWindow() const { notImplemented(); return 0; }
bool QScrollBar::setValue(int) { notImplemented(); return 0; }
void QFont::setFirstFamily(FontFamily const&) { notImplemented(); }
bool QTextEdit::hasSelectedText() const { notImplemented(); return 0; }
int QTextEdit::selectionStart() { notImplemented(); return 0; }
void QFont::setWeight(int) { notImplemented(); }
int ScrollView::scrollXOffset() const { notImplemented(); return 0; }
bool QListBox::isSelected(int) const { notImplemented(); return 0; }
void QLineEdit::setReadOnly(bool) { notImplemented(); }
void QPainter::drawLineForText(int,int,int,int) { notImplemented(); }
QPainter::QPainter() { notImplemented(); }
QComboBox::~QComboBox() { notImplemented(); }
Cursor::Cursor(Image*) { notImplemented(); }
Widget::FocusPolicy QComboBox::focusPolicy() const { notImplemented(); return NoFocus; }
void QPainter::drawImageAtPoint(Image*,IntPoint const&,Image::CompositeOperator) { notImplemented(); }
void QPainter::clearShadow() { notImplemented(); }
void QPainter::setPen(Pen const&) { notImplemented(); }
void QTextEdit::setLineHeight(int) { notImplemented(); }
void QScrollBar::setKnobProportion(int,int) { notImplemented(); }
KWQFileButton::KWQFileButton(Frame*) { notImplemented(); }
IntRect QFontMetrics::boundingRect(int,int,int,int,int,QString const&,int,int) const { notImplemented(); return IntRect(); }
void QTextEdit::setSelectionStart(int) { notImplemented(); }
void QPainter::beginTransparencyLayer(float) { notImplemented(); }
void QFontMetrics::setFont(QFont const&) { notImplemented(); }
void QComboBox::setFont(QFont const&) { notImplemented(); }
IntRect Widget::frameGeometry() const { notImplemented(); return IntRect(); }
void QListBox::setSelected(int,bool) { notImplemented(); }
void QPainter::addFocusRingRect(int,int,int,int) { notImplemented(); }
void QTextEdit::setCursorPosition(int,int) { notImplemented(); }
void QPainter::restore() { notImplemented(); }
int QFontMetrics::width(QString const&,int,int,int) const { notImplemented(); return 0; }
void Widget::setEnabled(bool) { notImplemented(); }
void QTextEdit::setSelectionEnd(int) { notImplemented(); }
void QComboBox::populate() { notImplemented(); }
void ScrollView::setStaticBackground(bool) { notImplemented(); }
static QFont localFont;
QFont const& QPainter::font() const { notImplemented(); return localFont; }
void QTextEdit::setAlignment(Qt::AlignmentFlags) { notImplemented(); }
void QLineEdit::setCursorPosition(int) { notImplemented(); }
void QPainter::drawText(int,int,int,int,int,int,int,QString const&) { notImplemented(); }
static Pen localPen;
Pen const& QPainter::pen() const { notImplemented(); return localPen; }
KJavaAppletWidget::KJavaAppletWidget(IntSize const&,Frame*,KXMLCore::HashMap<String,String> const&) { notImplemented(); }
int QFontMetrics::descent() const { notImplemented(); return 0; }
QListBox::QListBox() { notImplemented(); }
int QFontMetrics::ascent() const { notImplemented(); return 0; }
QString QLineEdit::selectedText() const { notImplemented(); return QString(); }
void Widget::setIsSelected(bool) { notImplemented(); }
String QLineEdit::text() const { notImplemented(); return String(); }
void Widget::unlockDrawingFocus() { notImplemented(); }
void QLineEdit::setLiveSearch(bool) { notImplemented(); }
bool QPainter::paintingDisabled() const { notImplemented(); return 0; }
QComboBox::QComboBox() { notImplemented(); }
void QPainter::drawConvexPolygon(IntPointArray const&) { notImplemented(); }
void Widget::setFont(QFont const&) { notImplemented(); }
void QSlider::setMaxValue(double) { notImplemented(); }
void Widget::lockDrawingFocus() { notImplemented(); }
void QPainter::drawLine(int,int,int,int) { notImplemented(); }
void QPainter::setBrush(Brush::BrushStyle) { notImplemented(); }
void QTextEdit::setSelectionRange(int,int) { notImplemented(); }
void QPainter::drawText(int,int,int,int,QChar const*,int,int,int,int,Color const&,QPainter::TextDirection,bool,int,int,bool) { notImplemented(); }
void ScrollView::scrollPointRecursively(int,int) { notImplemented(); }
IntSize QLineEdit::sizeForCharacterWidth(int) const { notImplemented(); return IntSize(); }
Cursor::~Cursor() { notImplemented(); }
IntRect QFontMetrics::selectionRectForText(int,int,int,int,int,QChar const*,int,int,int,int,bool,bool,int,int,bool) const { notImplemented(); return IntRect(); }
void ScrollView::suppressScrollBars(bool,bool) { notImplemented(); }
int QFontMetrics::checkSelectionPoint(QChar*,int,int,int,int,int,int,int,int,bool,int,bool,bool,bool) const { notImplemented(); return 0; }
void QTextEdit::getCursorPosition(int*,int*) const { notImplemented(); }
bool FrameView::isFrameView() const { notImplemented(); return 0; }
void QScrollBar::setSteps(int,int) { notImplemented(); }
void QLineEdit::setMaxLength(int) { notImplemented(); }
void Widget::setCursor(Cursor const&) { notImplemented(); }
void QLineEdit::setAutoSaveName(String const&) { notImplemented(); }
int QComboBox::baselinePosition(int) const { notImplemented(); return 0; }
void QComboBox::appendItem(QString const&,KWQListBoxItemType,bool) { notImplemented(); }
void QPainter::setShadow(int,int,int,Color const&) { notImplemented(); }
void QTextEdit::setWritingDirection(QPainter::TextDirection) { notImplemented(); }
void Widget::setDrawingAlpha(float) { notImplemented(); }
QSlider::QSlider() { notImplemented(); }
void ScrollView::setVScrollBarMode(ScrollBarMode) { notImplemented(); }
void QPainter::drawScaledAndTiledImage(Image*,int,int,int,int,int,int,int,int,Image::TileRule,Image::TileRule,void*) { notImplemented(); }
int ScrollView::scrollYOffset() const { notImplemented(); return 0; }
void QPainter::drawImage(Image*,int,int,int,int,int,int,int,int,Image::CompositeOperator,void*) { notImplemented(); }
void QPainter::setBrush(Brush const&) { notImplemented(); }
void QComboBox::setCurrentItem(int) { notImplemented(); }
int QFontMetrics::height() const { notImplemented(); return 0; }
void QComboBox::setWritingDirection(QPainter::TextDirection) { notImplemented(); }
void ScrollView::setScrollBarsMode(ScrollBarMode) { notImplemented(); }
IntSize QComboBox::sizeHint() const { notImplemented(); return IntSize(); }
void QPainter::drawRect(int,int,int,int) { notImplemented(); }
void QFont::setPixelSize(float) { notImplemented(); }
void Widget::setFrameGeometry(IntRect const&) { notImplemented(); }
void QLineEdit::setSelection(int,int) { notImplemented(); }
void QLineEdit::setMaxResults(int) { notImplemented(); }
void QListBox::clear() { notImplemented(); }
bool QLineEdit::edited() const { notImplemented(); return 0; }
void QPainter::drawTiledImage(Image*,int,int,int,int,int,int,void*) { notImplemented(); }
void QPainter::clearFocusRing() { notImplemented(); }
bool QFont::operator==(QFont const&) const { notImplemented(); return 0; }
Widget::Widget() { notImplemented(); }
String QTextEdit::text() const { notImplemented(); return String(); }
void QPainter::drawImageInRect(Image*,IntRect const&,Image::CompositeOperator) { notImplemented(); }
void QPainter::setFont(QFont const&) { notImplemented(); }
void Widget::disableFlushDrawing() { notImplemented(); }
void QPainter::initFocusRing(int,int) { notImplemented(); }
void QSlider::setMinValue(double) { notImplemented(); }
void QTextEdit::setWordWrap(QTextEdit::WrapStyle) { notImplemented(); }
void QPainter::drawLineForMisspelling(int,int,int) { notImplemented(); }
void QLineEdit::setText(String const&) { notImplemented(); }
double QSlider::value() const { notImplemented(); return 0; }
void QPainter::setBrush(unsigned int) { notImplemented(); }
void QListBox::setSelectionMode(QListBox::SelectionMode) { notImplemented(); }
void KWQFileButton::setFilename(QString const&) { notImplemented(); }
QFontMetrics::QFontMetrics(QFont const&) { notImplemented(); }
int QFontMetrics::lineSpacing() const { notImplemented(); return 0; }
void QLineEdit::setEdited(bool) { notImplemented(); }
IntRect QComboBox::frameGeometry() const { notImplemented(); return IntRect(); }
void QListBox::setWritingDirection(QPainter::TextDirection) { notImplemented(); }
void QLineEdit::setAlignment(Qt::AlignmentFlags) { notImplemented(); }
void ScrollView::updateContents(const IntRect&,bool) { notImplemented(); }
float QFontMetrics::floatWidth(QChar const*,int,int,int,int,int,int,int,bool) const { notImplemented(); return 0; }
void ScrollView::setHScrollBarMode(ScrollBarMode) { notImplemented(); }
WebCore::Widget::FocusPolicy KWQFileButton::focusPolicy() const { notImplemented(); return NoFocus; }
void QListBox::setFont(QFont const&) { notImplemented(); }
bool QLineEdit::checksDescendantsForFocus() const { notImplemented(); return false; }
int KWQFileButton::baselinePosition(int) const { notImplemented(); return 0; }
QSlider::~QSlider() { notImplemented(); }
void KWQFileButton::setFrameGeometry(WebCore::IntRect const&) { notImplemented(); }
QListBox::~QListBox() { notImplemented(); }
WebCore::IntRect KWQFileButton::frameGeometry() const { notImplemented(); return IntRect(); }
void QTextEdit::setFont(QFont const&) { notImplemented(); }
void QLineEdit::setFont(QFont const&) { notImplemented(); }
KWQFileButton::~KWQFileButton() { notImplemented(); }
WebCore::Widget::FocusPolicy QTextEdit::focusPolicy() const { notImplemented(); return NoFocus; }
WebCore::Widget::FocusPolicy QSlider::focusPolicy() const { notImplemented(); return NoFocus; }
void QSlider::setFont(QFont const&) { notImplemented(); }
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
bool WebCore::historyContains(QString const&) { return false; }
int KWQFindNextSentenceFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void KWQFindSentenceBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }
int KWQFindNextWordFromIndex(QChar const*,int,int,bool) { notImplemented(); return 0; }
void KWQFindWordBoundary(QChar const*,int,int,int*,int*) { notImplemented(); }
QString submitButtonDefaultLabel() { return "Submit"; }
QString inputElementAltText() { return QString(); }
QString resetButtonDefaultLabel() { return "Reset"; }
bool KWQKCookieJar::cookieEnabled() { notImplemented(); return false; }
void WebCore::Widget::setFocus() { notImplemented(); }
void WebCore::ScrollView::setContentsPos(int,int) { }
void WebCore::QPainter::setPaintingDisabled(bool) { notImplemented(); }
void WebCore::QPainter::fillRect(WebCore::IntRect const&,WebCore::Brush const&) { notImplemented(); }
WebCore::QPainter::~QPainter() { notImplemented(); }
WebCore::QPainter::QPainter(bool) { notImplemented(); }
void WebCore::ScrollView::viewportToContents(int,int,int&,int&) { notImplemented(); }
void WebCore::TransferJob::kill() { notImplemented(); }
void WebCore::TransferJob::addMetaData(QString const&,QString const&) { notImplemented(); }
void WebCore::TransferJob::addMetaData(KXMLCore::HashMap<WebCore::String,WebCore::String> const&) { notImplemented(); }
QString WebCore::TransferJob::queryMetaData(QString const&) const { notImplemented(); return QString(); }
int WebCore::TransferJob::error() const { notImplemented(); return 0; }
QString WebCore::TransferJob::errorText() const { notImplemented(); return "Houston, we have a problem."; }
bool WebCore::TransferJob::isErrorPage() const { notImplemented(); return 0; }
WebCore::TransferJob::TransferJob(WebCore::TransferJobClient*,KURL const&) { notImplemented(); }
WebCore::TransferJob::TransferJob(WebCore::TransferJobClient*,KURL const&,WebCore::FormData const&) { notImplemented(); }
void WebCore::Widget::hide() { notImplemented(); }
QString KLocale::language() { return "en"; }
PluginInfo*PlugInInfoStore::createPluginInfoForPluginAtIndex(unsigned) { notImplemented(); return 0;}
unsigned PlugInInfoStore::pluginCount() const { notImplemented(); return 0; }
void WebCore::refreshPlugins(bool) { notImplemented(); }
int WebCore::screenDepth(WebCore::Widget*) { notImplemented(); return 96; }
QFont::QFont() { notImplemented(); }
QFont::~QFont() { notImplemented(); }
bool QFont::italic() const { notImplemented(); return false; }
int QFont::weight() const { notImplemented(); return QFont::Normal; }
static QFontMetrics localFontMetrics;
QFontMetrics::QFontMetrics() { notImplemented(); }
QFontMetrics::~QFontMetrics() { notImplemented(); }
QFontMetrics::QFontMetrics(QFontMetrics const&) { notImplemented(); }
QFontMetrics& QFontMetrics::operator=(QFontMetrics const&) { notImplemented(); return localFontMetrics; }
float QFontMetrics::xHeight() const { notImplemented(); return 0; }
IntRect WebCore::usableScreenRect(WebCore::Widget*) { notImplemented(); return IntRect(0,0,800,600); }
QFont& QFont::operator=(QFont const&) { notImplemented(); return localFont; }
void Widget::setActiveWindow() { notImplemented(); }
bool KWQCheckIfReloading(WebCore::DocLoader*) { notImplemented(); return false; }
int WebCore::ScrollView::contentsX() const { notImplemented(); return 0; }
int WebCore::ScrollView::contentsY() const { notImplemented(); return 0; }
int WebCore::ScrollView::contentsHeight() const { notImplemented(); return 0; }
int WebCore::ScrollView::contentsWidth() const { notImplemented(); return 0; }
int WebCore::ScrollView::visibleHeight() const { notImplemented(); return 0; }
int WebCore::ScrollView::visibleWidth() const { notImplemented(); return 0; }
WebCore::ScrollBarMode WebCore::ScrollView::hScrollBarMode() const { notImplemented(); return ScrollBarAlwaysOff; }
WebCore::ScrollBarMode WebCore::ScrollView::vScrollBarMode() const { notImplemented(); return ScrollBarAlwaysOff; }
RenderTheme* WebCore::theme() { notImplemented(); return 0; }
void KWQCheckCacheObjectStatus(DocLoader*, CachedObject*) { notImplemented(); }
bool KWQServeRequest(Loader*,Request*, TransferJob*) { notImplemented(); }
