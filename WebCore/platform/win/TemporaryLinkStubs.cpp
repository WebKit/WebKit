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

static void notImplemented() { puts("Not yet implemented"); abort(); }
void  QLineEdit::selectAll(void) { notImplemented(); }
void  WebCore::QPainter::save(void) { notImplemented(); }
void  WebCore::Widget::enableFlushDrawing(void) { notImplemented(); }
void  WebCore::QPainter::drawHighlightForText(int,int,int,int,int,class QChar const *,int,int,int,int,class WebCore::Color const &,enum WebCore::QPainter::TextDirection,bool,int,int,bool) { notImplemented(); }
void  QFont::setPrinterFont(bool) { notImplemented(); }
class WebCore::String  QTextEdit::textWithHardLineBreaks(void)const  { notImplemented(); return WebCore::String(); }
class WebCore::IntPoint  WebCore::Widget::mapFromGlobal(class WebCore::IntPoint const &)const  { notImplemented(); return WebCore::IntPoint(); }
void  WebCore::QPainter::addClip(class WebCore::IntRect const &) { notImplemented(); }
int  QLineEdit::cursorPosition(void)const  { notImplemented(); return 0; }
void  WebCore::QPainter::setPen(enum WebCore::Pen::PenStyle) { notImplemented(); }
class WebCore::Color  WebCore::QPainter::selectedTextBackgroundColor(void)const  { notImplemented(); return WebCore::Color(); }
class QFontMetrics  WebCore::QPainter::fontMetrics(void)const  { notImplemented(); return QFontMetrics(); }
void  WebCore::Widget::show(void) { notImplemented(); }
void  QFont::setItalic(bool) { notImplemented(); }
void  QSlider::setValue(double) { notImplemented(); }
void  QLineEdit::addSearchResult(void) { notImplemented(); }
void  KWQFileButton::click(bool) { notImplemented(); }
void  QLineEdit::setWritingDirection(enum WebCore::QPainter::TextDirection) { notImplemented(); }
void  WebCore::QPainter::drawFocusRing(class WebCore::Color const &) { notImplemented(); }
class WebCore::IntSize  KWQFileButton::sizeForCharacterWidth(int)const  { notImplemented(); return WebCore::IntSize(); }
class WebCore::IntSize  QTextEdit::sizeWithColumnsAndRows(int,int)const  { notImplemented(); return WebCore::IntSize(); }
static WebCore::Cursor localCursor;
class WebCore::Cursor const& WebCore::iBeamCursor(void) { notImplemented(); return localCursor; }
void  QComboBox::clear(void) { notImplemented(); }
int  WebCore::QPainter::misspellingLineThickness(void)const  { notImplemented(); return 0; }
void  QComboBox::setFrameGeometry(class WebCore::IntRect const &) { notImplemented(); }
int  QLineEdit::maxLength(void)const  { notImplemented(); return 0; }
class WebCore::Cursor const & WebCore::westResizeCursor(void) { notImplemented(); return localCursor; }
bool  WebCore::Widget::isEnabled(void)const  { notImplemented(); return 0; }
bool __cdecl KWQServeRequest(class WebCore::Loader *,class WebCore::DocLoader *,class KIO::TransferJob *) { notImplemented(); return 0; }
void  QTextEdit::setText(class WebCore::String const &) { notImplemented(); }
int  QScrollView::childY(class WebCore::Widget *) { notImplemented(); return 0; }
class WebCore::Cursor const & __cdecl WebCore::northWestResizeCursor(void) { notImplemented(); return localCursor; }
void  WebCore::Widget::paint(class WebCore::QPainter *,class WebCore::IntRect const &) { notImplemented(); }
void  WebCore::QPainter::addRoundedRectClip(class WebCore::IntRect const &,class WebCore::IntSize const &,class WebCore::IntSize const &,class WebCore::IntSize const &,class WebCore::IntSize const &) { notImplemented(); }
class WebCore::IntPoint  WebCore::FrameView::viewportToGlobal(class WebCore::IntPoint const &)const  { notImplemented(); return WebCore::IntPoint(); }
int  QTextEdit::selectionEnd(void) { notImplemented(); return 0; }
void  QFont::determinePitch(void)const  { notImplemented(); }
void  QTextEdit::setScrollBarModes(enum QScrollView::ScrollBarMode,enum QScrollView::ScrollBarMode) { notImplemented(); }
void  WebCore::QPainter::drawEllipse(int,int,int,int) { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::southWestResizeCursor(void) { notImplemented(); return localCursor; }
void  WebCore::QPainter::setPen(unsigned int) { notImplemented(); }
void  QTextEdit::setReadOnly(bool) { notImplemented(); }
void  QListBox::appendItem(class QString const &,enum KWQListBoxItemType,bool) { notImplemented(); }
void  QLineEdit::setPlaceholderString(class WebCore::String const &) { notImplemented(); }
WebCore::Cursor::Cursor(class WebCore::Cursor const &) { notImplemented(); }
enum WebCore::Widget::FocusPolicy  WebCore::Widget::focusPolicy(void)const  { notImplemented(); return NoFocus; }
void  QScrollView::removeChild(class WebCore::Widget *) { notImplemented(); }
void  QTextEdit::selectAll(void) { notImplemented(); }
void  WebCore::QPainter::fillRect(int,int,int,int,class WebCore::Brush const &) { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::waitCursor(void) { notImplemented(); return localCursor; }
void  WebCore::QPainter::endTransparencyLayer(void) { notImplemented(); }
QFont::QFont(class QFont const &) { notImplemented();  }
void  QScrollView::addChild(class WebCore::Widget *,int,int) { notImplemented(); }
void  QTextEdit::setDisabled(bool) { notImplemented(); }
bool  QScrollBar::scroll(KWQScrollDirection,KWQScrollGranularity,float) { notImplemented(); return 0; }
WebCore::Widget::~Widget(void) { notImplemented(); }
class WebCore::IntRect  WebCore::QPainter::xForm(class WebCore::IntRect const &)const  { notImplemented(); return WebCore::IntRect(); }
class WebCore::IntSize  QListBox::sizeForNumberOfLines(int)const  { notImplemented(); return WebCore::IntSize(); }
void  QScrollView::resizeContents(int,int) { notImplemented(); }
int  QLineEdit::selectionStart(void)const  { notImplemented(); return 0; }
QLineEdit::QLineEdit(enum QLineEdit::Type) { notImplemented(); }
void  WebCore::FrameView::updateBorder(void) { notImplemented(); }
bool  QLineEdit::hasSelectedText(void)const  { notImplemented(); return 0; }
QScrollBar::QScrollBar(enum Qt::Orientation,class WebCore::Widget *) { notImplemented(); }
void  QListBox::doneAppendingItems(void) { notImplemented(); }
QTextEdit::QTextEdit(class WebCore::Widget *) { notImplemented(); }
bool  QScrollView::inWindow(void)const  { notImplemented(); return 0; }
bool  QScrollBar::setValue(int) { notImplemented(); return 0; }
void  QFont::setFirstFamily(class WebCore::FontFamily const &) { notImplemented(); }
bool  QTextEdit::hasSelectedText(void)const  { notImplemented(); return 0; }
int  QTextEdit::selectionStart(void) { notImplemented(); return 0; }
void  QFont::setWeight(int) { notImplemented(); }
int  QScrollView::scrollXOffset(void)const  { notImplemented(); return 0; }
bool  QListBox::isSelected(int)const  { notImplemented(); return 0; }
void  QLineEdit::setReadOnly(bool) { notImplemented(); }
void  WebCore::QPainter::drawLineForText(int,int,int,int) { notImplemented(); }
void QObject::disconnect(class QObject const *,char const *,class QObject const *,char const *) { notImplemented(); }
WebCore::QPainter::QPainter(void) { notImplemented(); }
QComboBox::~QComboBox(void) { notImplemented(); }
WebCore::Cursor::Cursor(class WebCore::Image *) { notImplemented(); }
enum WebCore::Widget::FocusPolicy  QComboBox::focusPolicy(void)const  { notImplemented(); return NoFocus; }
void  WebCore::QPainter::drawImageAtPoint(class WebCore::Image *,class WebCore::IntPoint const &,enum WebCore::Image::CompositeOperator) { notImplemented(); }
void  WebCore::QPainter::clearShadow(void) { notImplemented(); }
void  WebCore::QPainter::setPen(class WebCore::Pen const &) { notImplemented(); }
void  QTextEdit::setLineHeight(int) { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::eastResizeCursor(void) { notImplemented(); return localCursor; }
void  QScrollBar::setKnobProportion(int,int) { notImplemented(); }
KWQFileButton::KWQFileButton(class WebCore::Frame *) { notImplemented(); }
class WebCore::IntRect  QFontMetrics::boundingRect(int,int,int,int,int,class QString const &,int,int)const  { notImplemented(); return WebCore::IntRect(); }
void  QTextEdit::setSelectionStart(int) { notImplemented(); }
void  WebCore::QPainter::beginTransparencyLayer(float) { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::northEastResizeCursor(void) { notImplemented(); return localCursor; }
void  QFontMetrics::setFont(class QFont const &) { notImplemented(); }
void  QComboBox::setFont(class QFont const &) { notImplemented(); }
class WebCore::IntRect  WebCore::Widget::frameGeometry(void)const  { notImplemented(); return WebCore::IntRect(); }
void  QListBox::setSelected(int,bool) { notImplemented(); }
void  WebCore::QPainter::addFocusRingRect(int,int,int,int) { notImplemented(); }
void  QTextEdit::setCursorPosition(int,int) { notImplemented(); }
void  WebCore::QPainter::restore(void) { notImplemented(); }
int  QFontMetrics::width(class QString const &,int,int,int)const  { notImplemented(); return 0; }
void  WebCore::Widget::setEnabled(bool) { notImplemented(); }
void  QTextEdit::setSelectionEnd(int) { notImplemented(); }
void  QComboBox::populate(void) { notImplemented(); }
void  QScrollView::setStaticBackground(bool) { notImplemented(); }
static QFont localFont;
class QFont const &  WebCore::QPainter::font(void)const  { notImplemented(); return localFont; }
void  QTextEdit::setAlignment(enum Qt::AlignmentFlags) { notImplemented(); }
void  QLineEdit::setCursorPosition(int) { notImplemented(); }
void  WebCore::QPainter::drawText(int,int,int,int,int,int,int,class QString const &) { notImplemented(); }
static WebCore::Pen localPen;
class WebCore::Pen const &  WebCore::QPainter::pen(void)const  { notImplemented(); return localPen; }
KJavaAppletWidget::KJavaAppletWidget(class WebCore::IntSize const &,class WebCore::Frame *,class KXMLCore::HashMap<class WebCore::String,class WebCore::String,struct KXMLCore::StrHash<class WebCore::String>,struct KXMLCore::HashTraits<class WebCore::String>,struct KXMLCore::HashTraits<class WebCore::String> > const &) { notImplemented(); }
int  QFontMetrics::descent(void)const  { notImplemented(); return 0; }
QListBox::QListBox(void) { notImplemented(); }
int  QFontMetrics::ascent(void)const  { notImplemented(); return 0; }
class QString  QLineEdit::selectedText(void)const  { notImplemented(); return QString(); }
class WebCore::Cursor const & __cdecl WebCore::crossCursor(void) { notImplemented(); return localCursor; }
void  WebCore::Widget::setIsSelected(bool) { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::handCursor(void) { notImplemented(); return localCursor; }
class WebCore::String  QLineEdit::text(void)const  { notImplemented(); return WebCore::String(); }
void  WebCore::Widget::unlockDrawingFocus(void) { notImplemented(); }
void  QLineEdit::setLiveSearch(bool) { notImplemented(); }
bool  WebCore::QPainter::paintingDisabled(void)const  { notImplemented(); return 0; }
class WebCore::Cursor const & __cdecl WebCore::northResizeCursor(void) { notImplemented(); return localCursor; }
QComboBox::QComboBox(void) { notImplemented(); }
void  WebCore::QPainter::drawConvexPolygon(class WebCore::IntPointArray const &) { notImplemented(); }
void  WebCore::Widget::setFont(class QFont const &) { notImplemented(); }
void  QSlider::setMaxValue(double) { notImplemented(); }
void  WebCore::Widget::lockDrawingFocus(void) { notImplemented(); }
void  WebCore::QPainter::drawLine(int,int,int,int) { notImplemented(); }
void  WebCore::QPainter::setBrush(enum WebCore::Brush::BrushStyle) { notImplemented(); }
void  QTextEdit::setSelectionRange(int,int) { notImplemented(); }
void  WebCore::QPainter::drawText(int,int,int,int,class QChar const *,int,int,int,int,class WebCore::Color const &,enum WebCore::QPainter::TextDirection,bool,int,int,bool) { notImplemented(); }
void  QScrollView::scrollPointRecursively(int,int) { notImplemented(); }
class WebCore::IntSize  QLineEdit::sizeForCharacterWidth(int)const  { notImplemented(); return WebCore::IntSize(); }
WebCore::Cursor::~Cursor(void) { notImplemented(); }
class WebCore::IntRect  QFontMetrics::selectionRectForText(int,int,int,int,int,class QChar const *,int,int,int,int,bool,bool,int,int,bool)const  { notImplemented(); return WebCore::IntRect(); }
class WebCore::Cursor const & __cdecl WebCore::southResizeCursor(void) { notImplemented(); return localCursor; }
void  QScrollView::suppressScrollBars(bool,bool) { notImplemented(); }
int  QFontMetrics::checkSelectionPoint(class QChar *,int,int,int,int,int,int,int,int,bool,int,bool,bool,bool)const  { notImplemented(); return 0; }
void  QTextEdit::getCursorPosition(int *,int *)const  { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::moveCursor(void) { notImplemented(); return localCursor; }
bool  WebCore::FrameView::isFrameView(void)const  { notImplemented(); return 0; }
void  QScrollBar::setSteps(int,int) { notImplemented(); }
void  QLineEdit::setMaxLength(int) { notImplemented(); }
void  WebCore::Widget::setCursor(class WebCore::Cursor const &) { notImplemented(); }
void  QLineEdit::setAutoSaveName(class WebCore::String const &) { notImplemented(); }
int  QComboBox::baselinePosition(int)const  { notImplemented(); return 0; }
void  QComboBox::appendItem(class QString const &,enum KWQListBoxItemType,bool) { notImplemented(); }
void  WebCore::QPainter::setShadow(int,int,int,class WebCore::Color const &) { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::helpCursor(void) { notImplemented(); return localCursor; }
void  QTextEdit::setWritingDirection(enum WebCore::QPainter::TextDirection) { notImplemented(); }
void  WebCore::Widget::setDrawingAlpha(float) { notImplemented(); }
QSlider::QSlider(void) { notImplemented(); }
class WebCore::Cursor const & __cdecl WebCore::southEastResizeCursor(void) { notImplemented(); return localCursor; }
void  QScrollView::setVScrollBarMode(enum QScrollView::ScrollBarMode) { notImplemented(); }
void  WebCore::QPainter::drawScaledAndTiledImage(class WebCore::Image *,int,int,int,int,int,int,int,int,enum WebCore::Image::TileRule,enum WebCore::Image::TileRule,void *) { notImplemented(); }
int  QScrollView::scrollYOffset(void)const  { notImplemented(); return 0; }
void  WebCore::QPainter::drawImage(class WebCore::Image *,int,int,int,int,int,int,int,int,enum WebCore::Image::CompositeOperator,void *) { notImplemented(); }
void  WebCore::QPainter::setBrush(class WebCore::Brush const &) { notImplemented(); }
void  QComboBox::setCurrentItem(int) { notImplemented(); }
int  QFontMetrics::height(void)const  { notImplemented(); return 0; }
void  QComboBox::setWritingDirection(enum WebCore::QPainter::TextDirection) { notImplemented(); }
void  QScrollView::setScrollBarsMode(enum QScrollView::ScrollBarMode) { notImplemented(); }
class WebCore::IntSize  QComboBox::sizeHint(void)const  { notImplemented(); return WebCore::IntSize(); }
void  WebCore::QPainter::drawRect(int,int,int,int) { notImplemented(); }
void  QFont::setPixelSize(float) { notImplemented(); }
void  WebCore::Widget::setFrameGeometry(class WebCore::IntRect const &) { notImplemented(); }
void  QLineEdit::setSelection(int,int) { notImplemented(); }
void  QLineEdit::setMaxResults(int) { notImplemented(); }
void  QListBox::clear(void) { notImplemented(); }
bool  QLineEdit::edited(void)const  { notImplemented(); return 0; }
void  WebCore::QPainter::drawTiledImage(class WebCore::Image *,int,int,int,int,int,int,void *) { notImplemented(); }
void  WebCore::QPainter::clearFocusRing(void) { notImplemented(); }
bool  QFont::operator==(class QFont const &)const  { notImplemented(); return 0; }
WebCore::Widget::Widget(void) { notImplemented(); }
class WebCore::String  QTextEdit::text(void)const  { notImplemented(); return WebCore::String(); }
void  WebCore::QPainter::drawImageInRect(class WebCore::Image *,class WebCore::IntRect const &,enum WebCore::Image::CompositeOperator) { notImplemented(); }
void  WebCore::QPainter::setFont(class QFont const &) { notImplemented(); }
void  WebCore::Widget::disableFlushDrawing(void) { notImplemented(); }
void  WebCore::QPainter::initFocusRing(int,int) { notImplemented(); }
void  QSlider::setMinValue(double) { notImplemented(); }
void  QTextEdit::setWordWrap(enum QTextEdit::WrapStyle) { notImplemented(); }
void  WebCore::QPainter::drawLineForMisspelling(int,int,int) { notImplemented(); }
void  QLineEdit::setText(class WebCore::String const &) { notImplemented(); }
double  QSlider::value(void)const  { notImplemented(); return 0; }
void  WebCore::QPainter::setBrush(unsigned int) { notImplemented(); }
void  QListBox::setSelectionMode(enum QListBox::SelectionMode) { notImplemented(); }
void  KWQFileButton::setFilename(class QString const &) { notImplemented(); }
QFontMetrics::QFontMetrics(class QFont const &) { notImplemented(); }
int  QFontMetrics::lineSpacing(void)const  { notImplemented(); return 0; }
void  QLineEdit::setEdited(bool) { notImplemented(); }
int  QScrollView::childX(class WebCore::Widget *) { notImplemented(); return 0; }
class WebCore::IntRect  QComboBox::frameGeometry(void)const  { notImplemented(); return WebCore::IntRect(); }
void  QListBox::setWritingDirection(enum WebCore::QPainter::TextDirection) { notImplemented(); }
void  QLineEdit::setAlignment(enum Qt::AlignmentFlags) { notImplemented(); }
void  QScrollView::updateContents(int,int,int,int,bool) { notImplemented(); }
float  QFontMetrics::floatWidth(class QChar const *,int,int,int,int,int,int,int,bool)const  { notImplemented(); return 0; }
void  QScrollView::setHScrollBarMode(enum QScrollView::ScrollBarMode) { notImplemented(); }
