/***********************************************************************
 *
 * This file contains temporary implementations of classes from QT/KDE
 *
 * This code is mostly borrowed from QT/KDE and is only modified
 * so it builds and works with KWQ.
 *
 * Eventually we will need to either replace this file, or arrive at
 * some other solution that will enable us to ship this code.
 *
 ***********************************************************************/

// KWQ hacks ---------------------------------------------------------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// -------------------------------------------------------------------------

#include <qrect.h>

// KWQ hacks ---------------------------------------------------------------

#ifdef USING_BORROWED_QRECT

QRect::QRect()
{
    x1 = x2 = y1 = y2 = 0;
}

QRect::QRect(int left, int top, int width, int height)
{
    x1 = (QCOORD)left;
    y1 = (QCOORD)top;
    x2 = (QCOORD)(left + width - 1);
    y2 = (QCOORD)(top + height - 1);
}

QRect::QRect(const QRect &other)
{
    x1 = other.x1;
    y1 = other.y1;
    x2 = other.x2;
    y2 = other.y2;
}

bool QRect::isNull() const
{ 
    return x2 == x1 - 1 && y2 == y1 - 1; 
}

bool QRect::isValid() const
{ 
    return x1 <= x2 && y1 <= y2; 
}

int QRect::left() const
{ 
    return x1; 
}

int QRect::top() const
{ 
    return y1; 
}

int QRect::right() const
{ 
    return x2; 
}

int QRect::bottom() const
{ 
    return y2; 
}

int QRect::x() const
{ 
    return x1; 
}

int QRect::y() const
{ 
    return y1; 
}

int QRect::width() const
{ 
    return  x2 - x1 + 1; 
}

int QRect::height() const
{ 
    return  y2 - y1 + 1; 
}

QSize QRect::size() const
{ 
    return QSize( x2 - x1 + 1, y2 - y1 + 1); 
}

void QRect::setWidth(int w)
{
    x2 = (QCOORD)(x1 + w - 1);
}

void QRect::setHeight(int h)
{
    y2 = (QCOORD)(y1 + h - 1);
}

QRect QRect::intersect(const QRect &r) const
{
    return *this & r;
}

bool QRect::intersects(const QRect &r) const
{
    return (QMAX(x1, r.x1) <= QMIN(x2, r.x2) &&
            QMAX(y1, r.y1) <= QMIN(y2, r.y2));
}

QRect QRect::operator&(const QRect &r) const
{
    QRect tmp;
    tmp.x1 = QMAX(x1, r.x1);
    tmp.x2 = QMIN(x2, r.x2);
    tmp.y1 = QMAX(y1, r.y1);
    tmp.y2 = QMIN(y2, r.y2);
    return tmp;
}

#ifdef _KWQ_IOSTREAM_
ostream &operator<<(ostream &o, const QRect &r)
{
    return o <<
        "QRect: [left: " <<
        (Q_INT32)r.left() <<
        "; top: " <<
        (Q_INT32)r.top() <<
        "; right: " <<
        (Q_INT32)r.right() <<
        "; bottom: " <<
        (Q_INT32)r.bottom() <<
        ']';
}
#endif

// -------------------------------------------------------------------------

QRect::QRect( const QPoint &topLeft, const QPoint &bottomRight )
{
    x1 = (QCOORD)topLeft.x();
    y1 = (QCOORD)topLeft.y();
    x2 = (QCOORD)bottomRight.x();
    y2 = (QCOORD)bottomRight.y();
}

QRect::QRect( const QPoint &topLeft, const QSize &size )
{
    x1 = (QCOORD)topLeft.x();
    y1 = (QCOORD)topLeft.y();
    x2 = (QCOORD)(x1 + size.width() - 1);
    y2 = (QCOORD)(y1 + size.height() - 1);
}

bool QRect::isEmpty() const
{ 
    return x1 > x2 || y1 > y2; 
}

QRect QRect::normalize() const
{
    QRect r;
    if ( x2 < x1 ) {                            // swap bad x values
        r.x1 = x2;
        r.x2 = x1;
    } else {
        r.x1 = x1;
        r.x2 = x2;
    }
    if ( y2 < y1 ) {                            // swap bad y values
        r.y1 = y2;
        r.y2 = y1;
    } else {
        r.y1 = y1;
        r.y2 = y2;
    }
    return r;
}

void QRect::setLeft(int pos)
{ 
    x1 = (QCOORD)pos; 
}

void QRect::setTop(int pos)
{ 
    y1 = (QCOORD)pos; 
}

void QRect::setRight(int pos)
{ 
    x2 = (QCOORD)pos; 
}

void QRect::setBottom(int pos)
{ 
    y2 = (QCOORD)pos; 
}

void QRect::setX(int x)
{ 
    x1 = (QCOORD)x; 
}

void QRect::setY(int y)
{ 
    y1 = (QCOORD)y; 
}

QPoint QRect::topLeft() const
{ 
    return QPoint(x1, y1); 
}

QPoint QRect::bottomRight() const
{ 
    return QPoint(x2, y2); 
}

QPoint QRect::topRight() const
{ 
    return QPoint(x2, y1); 
}

QPoint QRect::bottomLeft() const
{ 
    return QPoint(x1, y2); 
}

QPoint QRect::center() const
{ 
    return QPoint((x1 + x2) / 2, (y1 + y2) / 2); 
}

void QRect::rect(int *x, int *y, int *w, int *h) const
{
    *x = x1;
    *y = y1;
    *w = x2-x1+1;
    *h = y2-y1+1;
}

void QRect::coords(int *xp1, int *yp1, int *xp2, int *yp2) const
{
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

void QRect::setSize(const QSize &s)
{
    x2 = (QCOORD)(s.width() + x1 - 1);
    y2 = (QCOORD)(s.height() + y1 - 1);
}

void QRect::setRect(int x, int y, int w, int h)
{
    x1 = (QCOORD)x;
    y1 = (QCOORD)y;
    x2 = (QCOORD)(x + w - 1);
    y2 = (QCOORD)(y + h - 1);
}

void QRect::setCoords(int xp1, int yp1, int xp2, int yp2)
{
    x1 = (QCOORD)xp1;
    y1 = (QCOORD)yp1;
    x2 = (QCOORD)xp2;
    y2 = (QCOORD)yp2;
}

void QRect::moveTopLeft(const QPoint &p)
{
    x2 += (QCOORD)(p.x() - x1);
    y2 += (QCOORD)(p.y() - y1);
    x1 = (QCOORD)p.x();
    y1 = (QCOORD)p.y();
}

void QRect::moveBottomRight(const QPoint &p)
{
    x1 += (QCOORD)(p.x() - x2);
    y1 += (QCOORD)(p.y() - y2);
    x2 = (QCOORD)p.x();
    y2 = (QCOORD)p.y();
}

void QRect::moveTopRight(const QPoint &p)
{
    x1 += (QCOORD)(p.x() - x2);
    y2 += (QCOORD)(p.y() - y1);
    x2 = (QCOORD)p.x();
    y1 = (QCOORD)p.y();
}

void QRect::moveBottomLeft(const QPoint &p)
{
    x2 += (QCOORD)(p.x() - x1);
    y1 += (QCOORD)(p.y() - y2);
    x1 = (QCOORD)p.x();
    y2 = (QCOORD)p.y();
}

void QRect::moveCenter(const QPoint &p)
{
    QCOORD w = x2 - x1;
    QCOORD h = y2 - y1;
    x1 = (QCOORD)(p.x() - w/2);
    y1 = (QCOORD)(p.y() - h/2);
    x2 = x1 + w;
    y2 = y1 + h;
}

void QRect::moveBy(int dx, int dy)
{
    x1 += (QCOORD)dx;
    y1 += (QCOORD)dy;
    x2 += (QCOORD)dx;
    y2 += (QCOORD)dy;
}

bool QRect::contains( int x, int y, bool proper ) const
{
    if (proper) {
        return x > x1 && x < x2 &&
               y > y1 && y < y2;
    }
    else {
        return x >= x1 && x <= x2 &&
               y >= y1 && y <= y2;
    }
}

bool QRect::contains(const QPoint &p, bool proper) const
{
    if (proper) {
        return p.x() > x1 && p.x() < x2 &&
               p.y() > y1 && p.y() < y2;
    }
    else {
        return p.x() >= x1 && p.x() <= x2 &&
               p.y() >= y1 && p.y() <= y2;
    }
}

bool QRect::contains(const QRect &r, bool proper) const
{
    if (proper) {
        return r.x1 > x1 && r.x2 < x2 && r.y1 > y1 && r.y2 < y2;
    }
    else {
        return r.x1 >= x1 && r.x2 <= x2 && r.y1 >= y1 && r.y2 <= y2;
    }
}

QRect QRect::unite(const QRect &r) const
{
    return *this | r;
}

QRect& QRect::operator|=(const QRect &r)
{
    *this = *this | r;

    return *this;
}

QRect& QRect::operator&=(const QRect &r)
{
    *this = *this & r;

    return *this;
}

QRect QRect::operator|(const QRect &r) const
{
    if (isValid()) {
        if (r.isValid()) {
            QRect tmp;
            tmp.setLeft(QMIN(x1, r.x1));
            tmp.setRight(QMAX(x2, r.x2));
            tmp.setTop(QMIN(y1, r.y1));
            tmp.setBottom(QMAX(y2, r.y2));
            return tmp;
        } else {
            return *this;
        }
    } else {
        return r;
    }
}

bool operator==(const QRect &r1, const QRect &r2)
{
    return r1.x1==r2.x1 && r1.x2==r2.x2 && r1.y1==r2.y1 && r1.y2==r2.y2;
}

bool operator!=(const QRect &r1, const QRect &r2)
{
    return r1.x1!=r2.x1 || r1.x2!=r2.x2 || r1.y1!=r2.y1 || r1.y2!=r2.y2;
}

// KWQ hacks ---------------------------------------------------------------

#endif // USING_BORROWED_QRECT

// -------------------------------------------------------------------------
