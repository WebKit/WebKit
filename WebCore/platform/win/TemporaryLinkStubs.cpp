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

using namespace WebCore;

static void notImplemented() { puts("Not yet implemented"); abort(); }
void  QLineEdit::selectAll(void) { notImplemented(); }
void  QPainter::save(void) { notImplemented(); }
void  Widget::enableFlushDrawing(void) { notImplemented(); }
void  QPainter::drawHighlightForText(int,int,int,int,int,class QChar const *,int,int,int,int,class Color const &,enum QPainter::TextDirection,bool,int,int,bool) { notImplemented(); }
void  QFont::setPrinterFont(bool) { notImplemented(); }
class String  QTextEdit::textWithHardLineBreaks(void)const  { notImplemented(); return String(); }
class IntPoint  Widget::mapFromGlobal(class IntPoint const &)const  { notImplemented(); return IntPoint(); }
void  QPainter::addClip(class IntRect const &) { notImplemented(); }
int  QLineEdit::cursorPosition(void)const  { notImplemented(); return 0; }
void  QPainter::setPen(enum Pen::PenStyle) { notImplemented(); }
class Color  QPainter::selectedTextBackgroundColor(void)const  { notImplemented(); return Color(); }
class QFontMetrics  QPainter::fontMetrics(void)const  { notImplemented(); return QFontMetrics(); }
void  Widget::show(void) { notImplemented(); }
void  QFont::setItalic(bool) { notImplemented(); }
void  QSlider::setValue(double) { notImplemented(); }
void  QLineEdit::addSearchResult(void) { notImplemented(); }
void  KWQFileButton::click(bool) { notImplemented(); }
void  QLineEdit::setWritingDirection(enum QPainter::TextDirection) { notImplemented(); }
void  QPainter::drawFocusRing(class Color const &) { notImplemented(); }
class IntSize  KWQFileButton::sizeForCharacterWidth(int)const  { notImplemented(); return IntSize(); }
class IntSize  QTextEdit::sizeWithColumnsAndRows(int,int)const  { notImplemented(); return IntSize(); }
void  QComboBox::clear(void) { notImplemented(); }
int  QPainter::misspellingLineThickness(void)const  { notImplemented(); return 0; }
void  QComboBox::setFrameGeometry(class IntRect const &) { notImplemented(); }
int  QLineEdit::maxLength(void)const  { notImplemented(); return 0; }
bool  Widget::isEnabled(void)const  { notImplemented(); return 0; }
bool __cdecl KWQServeRequest(class Loader *,class DocLoader *,class WebCore::TransferJob *) { notImplemented(); return 0; }
void  QTextEdit::setText(class String const &) { notImplemented(); }
void  Widget::paint(class QPainter *,class IntRect const &) { notImplemented(); }
void  QPainter::addRoundedRectClip(class IntRect const &,class IntSize const &,class IntSize const &,class IntSize const &,class IntSize const &) { notImplemented(); }
class IntPoint  FrameView::viewportToGlobal(class IntPoint const &)const  { notImplemented(); return IntPoint(); }
int  QTextEdit::selectionEnd(void) { notImplemented(); return 0; }
void  QFont::determinePitch(void)const  { notImplemented(); }
void  QTextEdit::setScrollBarModes(ScrollBarMode,ScrollBarMode) { notImplemented(); }
void  QPainter::drawEllipse(int,int,int,int) { notImplemented(); }
void  QPainter::setPen(unsigned int) { notImplemented(); }
void  QTextEdit::setReadOnly(bool) { notImplemented(); }
void  QListBox::appendItem(class QString const &,enum KWQListBoxItemType,bool) { notImplemented(); }
void  QLineEdit::setPlaceholderString(class String const &) { notImplemented(); }
Cursor::Cursor(class Cursor const &) { notImplemented(); }
enum Widget::FocusPolicy  Widget::focusPolicy(void)const  { notImplemented(); return NoFocus; }
void  ScrollView::removeChild(class Widget *) { notImplemented(); }
void  QTextEdit::selectAll(void) { notImplemented(); }
void  QPainter::fillRect(int,int,int,int,class Brush const &) { notImplemented(); }
void  QPainter::endTransparencyLayer(void) { notImplemented(); }
QFont::QFont(class QFont const &) { notImplemented();  }
void  ScrollView::addChild(class Widget *,int,int) { notImplemented(); }
void  QTextEdit::setDisabled(bool) { notImplemented(); }
bool  QScrollBar::scroll(KWQScrollDirection,KWQScrollGranularity,float) { notImplemented(); return 0; }
Widget::~Widget(void) { notImplemented(); }
class IntRect  QPainter::xForm(class IntRect const &)const  { notImplemented(); return IntRect(); }
class IntSize  QListBox::sizeForNumberOfLines(int)const  { notImplemented(); return IntSize(); }
void  ScrollView::resizeContents(int,int) { notImplemented(); }
int  QLineEdit::selectionStart(void)const  { notImplemented(); return 0; }
QLineEdit::QLineEdit(enum QLineEdit::Type) { notImplemented(); }
void  FrameView::updateBorder(void) { notImplemented(); }
bool  QLineEdit::hasSelectedText(void)const  { notImplemented(); return 0; }
QScrollBar::QScrollBar(enum Qt::Orientation,class Widget *) { notImplemented(); }
void  QListBox::doneAppendingItems(void) { notImplemented(); }
QTextEdit::QTextEdit(class Widget *) { notImplemented(); }
bool  ScrollView::inWindow(void)const  { notImplemented(); return 0; }
bool  QScrollBar::setValue(int) { notImplemented(); return 0; }
void  QFont::setFirstFamily(class FontFamily const &) { notImplemented(); }
bool  QTextEdit::hasSelectedText(void)const  { notImplemented(); return 0; }
int  QTextEdit::selectionStart(void) { notImplemented(); return 0; }
void  QFont::setWeight(int) { notImplemented(); }
int  ScrollView::scrollXOffset(void)const  { notImplemented(); return 0; }
bool  QListBox::isSelected(int)const  { notImplemented(); return 0; }
void  QLineEdit::setReadOnly(bool) { notImplemented(); }
void  QPainter::drawLineForText(int,int,int,int) { notImplemented(); }
QPainter::QPainter(void) { notImplemented(); }
QComboBox::~QComboBox(void) { notImplemented(); }
Cursor::Cursor(class Image *) { notImplemented(); }
enum Widget::FocusPolicy  QComboBox::focusPolicy(void)const  { notImplemented(); return NoFocus; }
void  QPainter::drawImageAtPoint(class Image *,class IntPoint const &,enum Image::CompositeOperator) { notImplemented(); }
void  QPainter::clearShadow(void) { notImplemented(); }
void  QPainter::setPen(class Pen const &) { notImplemented(); }
void  QTextEdit::setLineHeight(int) { notImplemented(); }
void  QScrollBar::setKnobProportion(int,int) { notImplemented(); }
KWQFileButton::KWQFileButton(class Frame *) { notImplemented(); }
class IntRect  QFontMetrics::boundingRect(int,int,int,int,int,class QString const &,int,int)const  { notImplemented(); return IntRect(); }
void  QTextEdit::setSelectionStart(int) { notImplemented(); }
void  QPainter::beginTransparencyLayer(float) { notImplemented(); }
void  QFontMetrics::setFont(class QFont const &) { notImplemented(); }
void  QComboBox::setFont(class QFont const &) { notImplemented(); }
class IntRect  Widget::frameGeometry(void)const  { notImplemented(); return IntRect(); }
void  QListBox::setSelected(int,bool) { notImplemented(); }
void  QPainter::addFocusRingRect(int,int,int,int) { notImplemented(); }
void  QTextEdit::setCursorPosition(int,int) { notImplemented(); }
void  QPainter::restore(void) { notImplemented(); }
int  QFontMetrics::width(class QString const &,int,int,int)const  { notImplemented(); return 0; }
void  Widget::setEnabled(bool) { notImplemented(); }
void  QTextEdit::setSelectionEnd(int) { notImplemented(); }
void  QComboBox::populate(void) { notImplemented(); }
void  ScrollView::setStaticBackground(bool) { notImplemented(); }
static QFont localFont;
class QFont const &  QPainter::font(void)const  { notImplemented(); return localFont; }
void  QTextEdit::setAlignment(enum Qt::AlignmentFlags) { notImplemented(); }
void  QLineEdit::setCursorPosition(int) { notImplemented(); }
void  QPainter::drawText(int,int,int,int,int,int,int,class QString const &) { notImplemented(); }
static Pen localPen;
class Pen const &  QPainter::pen(void)const  { notImplemented(); return localPen; }
KJavaAppletWidget::KJavaAppletWidget(class IntSize const &,class Frame *,class KXMLCore::HashMap<class String,class String> const &) { notImplemented(); }
int  QFontMetrics::descent(void)const  { notImplemented(); return 0; }
QListBox::QListBox(void) { notImplemented(); }
int  QFontMetrics::ascent(void)const  { notImplemented(); return 0; }
class QString  QLineEdit::selectedText(void)const  { notImplemented(); return QString(); }
void  Widget::setIsSelected(bool) { notImplemented(); }
class String  QLineEdit::text(void)const  { notImplemented(); return String(); }
void  Widget::unlockDrawingFocus(void) { notImplemented(); }
void  QLineEdit::setLiveSearch(bool) { notImplemented(); }
bool  QPainter::paintingDisabled(void)const  { notImplemented(); return 0; }
QComboBox::QComboBox(void) { notImplemented(); }
void  QPainter::drawConvexPolygon(class IntPointArray const &) { notImplemented(); }
void  Widget::setFont(class QFont const &) { notImplemented(); }
void  QSlider::setMaxValue(double) { notImplemented(); }
void  Widget::lockDrawingFocus(void) { notImplemented(); }
void  QPainter::drawLine(int,int,int,int) { notImplemented(); }
void  QPainter::setBrush(enum Brush::BrushStyle) { notImplemented(); }
void  QTextEdit::setSelectionRange(int,int) { notImplemented(); }
void  QPainter::drawText(int,int,int,int,class QChar const *,int,int,int,int,class Color const &,enum QPainter::TextDirection,bool,int,int,bool) { notImplemented(); }
void  ScrollView::scrollPointRecursively(int,int) { notImplemented(); }
class IntSize  QLineEdit::sizeForCharacterWidth(int)const  { notImplemented(); return IntSize(); }
Cursor::~Cursor(void) { notImplemented(); }
class IntRect  QFontMetrics::selectionRectForText(int,int,int,int,int,class QChar const *,int,int,int,int,bool,bool,int,int,bool)const  { notImplemented(); return IntRect(); }
void  ScrollView::suppressScrollBars(bool,bool) { notImplemented(); }
int  QFontMetrics::checkSelectionPoint(class QChar *,int,int,int,int,int,int,int,int,bool,int,bool,bool,bool)const  { notImplemented(); return 0; }
void  QTextEdit::getCursorPosition(int *,int *)const  { notImplemented(); }
bool  FrameView::isFrameView(void)const  { notImplemented(); return 0; }
void  QScrollBar::setSteps(int,int) { notImplemented(); }
void  QLineEdit::setMaxLength(int) { notImplemented(); }
void  Widget::setCursor(class Cursor const &) { notImplemented(); }
void  QLineEdit::setAutoSaveName(class String const &) { notImplemented(); }
int  QComboBox::baselinePosition(int)const  { notImplemented(); return 0; }
void  QComboBox::appendItem(class QString const &,enum KWQListBoxItemType,bool) { notImplemented(); }
void  QPainter::setShadow(int,int,int,class Color const &) { notImplemented(); }
void  QTextEdit::setWritingDirection(enum QPainter::TextDirection) { notImplemented(); }
void  Widget::setDrawingAlpha(float) { notImplemented(); }
QSlider::QSlider(void) { notImplemented(); }
void  ScrollView::setVScrollBarMode(ScrollBarMode) { notImplemented(); }
void  QPainter::drawScaledAndTiledImage(class Image *,int,int,int,int,int,int,int,int,enum Image::TileRule,enum Image::TileRule,void *) { notImplemented(); }
int  ScrollView::scrollYOffset(void)const  { notImplemented(); return 0; }
void  QPainter::drawImage(class Image *,int,int,int,int,int,int,int,int,enum Image::CompositeOperator,void *) { notImplemented(); }
void  QPainter::setBrush(class Brush const &) { notImplemented(); }
void  QComboBox::setCurrentItem(int) { notImplemented(); }
int  QFontMetrics::height(void)const  { notImplemented(); return 0; }
void  QComboBox::setWritingDirection(enum QPainter::TextDirection) { notImplemented(); }
void  ScrollView::setScrollBarsMode(ScrollBarMode) { notImplemented(); }
class IntSize  QComboBox::sizeHint(void)const  { notImplemented(); return IntSize(); }
void  QPainter::drawRect(int,int,int,int) { notImplemented(); }
void  QFont::setPixelSize(float) { notImplemented(); }
void  Widget::setFrameGeometry(class IntRect const &) { notImplemented(); }
void  QLineEdit::setSelection(int,int) { notImplemented(); }
void  QLineEdit::setMaxResults(int) { notImplemented(); }
void  QListBox::clear(void) { notImplemented(); }
bool  QLineEdit::edited(void)const  { notImplemented(); return 0; }
void  QPainter::drawTiledImage(class Image *,int,int,int,int,int,int,void *) { notImplemented(); }
void  QPainter::clearFocusRing(void) { notImplemented(); }
bool  QFont::operator==(class QFont const &)const  { notImplemented(); return 0; }
Widget::Widget(void) { notImplemented(); }
class String  QTextEdit::text(void)const  { notImplemented(); return String(); }
void  QPainter::drawImageInRect(class Image *,class IntRect const &,enum Image::CompositeOperator) { notImplemented(); }
void  QPainter::setFont(class QFont const &) { notImplemented(); }
void  Widget::disableFlushDrawing(void) { notImplemented(); }
void  QPainter::initFocusRing(int,int) { notImplemented(); }
void  QSlider::setMinValue(double) { notImplemented(); }
void  QTextEdit::setWordWrap(enum QTextEdit::WrapStyle) { notImplemented(); }
void  QPainter::drawLineForMisspelling(int,int,int) { notImplemented(); }
void  QLineEdit::setText(class String const &) { notImplemented(); }
double  QSlider::value(void)const  { notImplemented(); return 0; }
void  QPainter::setBrush(unsigned int) { notImplemented(); }
void  QListBox::setSelectionMode(enum QListBox::SelectionMode) { notImplemented(); }
void  KWQFileButton::setFilename(class QString const &) { notImplemented(); }
QFontMetrics::QFontMetrics(class QFont const &) { notImplemented(); }
int  QFontMetrics::lineSpacing(void)const  { notImplemented(); return 0; }
void  QLineEdit::setEdited(bool) { notImplemented(); }
class IntRect  QComboBox::frameGeometry(void)const  { notImplemented(); return IntRect(); }
void  QListBox::setWritingDirection(enum QPainter::TextDirection) { notImplemented(); }
void  QLineEdit::setAlignment(enum Qt::AlignmentFlags) { notImplemented(); }
void  ScrollView::updateContents(const IntRect&,bool) { notImplemented(); }
float  QFontMetrics::floatWidth(class QChar const *,int,int,int,int,int,int,int,bool)const  { notImplemented(); return 0; }
void  ScrollView::setHScrollBarMode(ScrollBarMode) { notImplemented(); }
enum WebCore::Widget::FocusPolicy  KWQFileButton::focusPolicy(void)const  { notImplemented(); return NoFocus; }
void  QListBox::setFont(class QFont const &) { notImplemented(); }
bool  QLineEdit::checksDescendantsForFocus(void)const  { notImplemented(); return false; }
int  KWQFileButton::baselinePosition(int)const  { notImplemented(); return 0; }
QSlider::~QSlider(void) { notImplemented(); }
void  KWQFileButton::setFrameGeometry(class WebCore::IntRect const &) { notImplemented(); }
QListBox::~QListBox(void) { notImplemented(); }
class WebCore::IntRect  KWQFileButton::frameGeometry(void)const  { notImplemented(); return IntRect(); }
void  QTextEdit::setFont(class QFont const &) { notImplemented(); }
void  QLineEdit::setFont(class QFont const &) { notImplemented(); }
KWQFileButton::~KWQFileButton(void) { notImplemented(); }
enum WebCore::Widget::FocusPolicy  QTextEdit::focusPolicy(void)const  { notImplemented(); return NoFocus; }
enum WebCore::Widget::FocusPolicy  QSlider::focusPolicy(void)const  { notImplemented(); return NoFocus; }
void  QSlider::setFont(class QFont const &) { notImplemented(); }
void  QListBox::setEnabled(bool) { notImplemented(); }
bool  QListBox::checksDescendantsForFocus(void)const  { notImplemented(); return 0; }
enum WebCore::Widget::FocusPolicy  QListBox::focusPolicy(void)const  { notImplemented(); return NoFocus; }
int  QLineEdit::baselinePosition(int)const  { notImplemented(); return 0; }
class WebCore::IntSize  QSlider::sizeHint(void)const  { notImplemented(); return IntSize(); }
QLineEdit::~QLineEdit(void) { notImplemented(); }
QTextEdit::~QTextEdit(void) { notImplemented(); }
bool  QTextEdit::checksDescendantsForFocus(void)const  { notImplemented(); return false; }
enum WebCore::Widget::FocusPolicy  QLineEdit::focusPolicy(void)const  { notImplemented(); return NoFocus; }
QScrollBar::~QScrollBar(void) { notImplemented(); }
Path::Path(){ notImplemented(); }
Path::Path(const IntRect&, Type){ notImplemented(); }
Path::Path(const IntPointArray&){ notImplemented(); }
Path::~Path(){ notImplemented(); }
Path::Path(const Path&){ notImplemented(); }
Path& Path::operator=(const Path&){ notImplemented(); return *this; }
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
void QLineEdit::setColors(class Color const &,class Color const &) { notImplemented(); }
void QTextEdit::setColors(class Color const &,class Color const &) { notImplemented(); }
class QString searchableIndexIntroduction(void) { notImplemented(); return QString(); }
void KWQKCookieJar::setCookie(class KURL const &,class KURL const &,class QString const &) { notImplemented(); }
class QString KWQKCookieJar::cookie(class KURL const &) { notImplemented(); return QString(); }
class IntRect WebCore::screenRect(class Widget *) { notImplemented(); return IntRect(); }
void WebCore::ScrollView::scrollBy(int,int) { notImplemented(); }
void WebCore::Widget::clearFocus(void) { notImplemented(); }
bool WebCore::historyContains(class QString const &) { return false; }
int KWQFindNextSentenceFromIndex(class QChar const *,int,int,bool) { notImplemented(); return 0; }
void KWQFindSentenceBoundary(class QChar const *,int,int,int *,int *) { notImplemented(); }
int KWQFindNextWordFromIndex(class QChar const *,int,int,bool) { notImplemented(); return 0; }
void KWQFindWordBoundary(class QChar const *,int,int,int *,int *) { notImplemented(); }
class QString submitButtonDefaultLabel(void) { return "Submit"; }
class QString inputElementAltText(void) { return QString(); }
class QString resetButtonDefaultLabel(void) { return "Reset"; }
bool KWQKCookieJar::cookieEnabled(void) { notImplemented(); return false; }
void WebCore::Widget::setFocus(void) { notImplemented(); }
void WebCore::ScrollView::setContentsPos(int,int) { }
void WebCore::QPainter::setPaintingDisabled(bool) { notImplemented(); }
void WebCore::QPainter::fillRect(class WebCore::IntRect const &,class WebCore::Brush const &) { notImplemented(); }
WebCore::QPainter::~QPainter(void) { notImplemented(); }
WebCore::QPainter::QPainter(bool) { notImplemented(); }
void WebCore::ScrollView::viewportToContents(int,int,int &,int &) { notImplemented(); }
void WebCore::TransferJob::kill(void) { notImplemented(); }
void WebCore::TransferJob::addMetaData(class QString const &,class QString const &) { notImplemented(); }
void WebCore::TransferJob::addMetaData(class KXMLCore::HashMap<class WebCore::String,class WebCore::String> const &) { notImplemented(); }
class QString WebCore::TransferJob::queryMetaData(class QString const &)const { notImplemented(); return QString(); }
int WebCore::TransferJob::error(void)const { notImplemented(); return 0; }
WebCore::TransferJob::TransferJob(class WebCore::TransferJobClient *,class KURL const &) { notImplemented(); }
WebCore::TransferJob::TransferJob(class WebCore::TransferJobClient *,class KURL const &,class WebCore::FormData const &) { notImplemented(); }
void WebCore::Widget::hide(void) { }
class QString KLocale::language(void) { return "en"; }
