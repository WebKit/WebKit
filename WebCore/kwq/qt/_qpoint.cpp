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

// -------------------------------------------------------------------------

#include "qpoint.h"

// KWQ hacks ---------------------------------------------------------------

#ifdef USING_BORROWED_QPOINT

// for abs()
#include <stdlib.h>

QPoint::QPoint()
{ 
    xx = 0; 
    yy = 0; 
}

QPoint::QPoint(int xpos, int ypos)
{ 
    xx = (QCOORD)xpos; 
    yy = (QCOORD)ypos; 
}

QPoint::QPoint(const QPoint &other)
{ 
    xx = other.xx; 
    yy = other.yy; 
}

int QPoint::x() const
{ 
    return xx; 
}

int QPoint::y() const
{ 
    return yy; 
}

QPoint operator+(const QPoint &p1, const QPoint &p2)
{ 
    return QPoint(p1.xx + p2.xx, p1.yy + p2.yy); 
}

QPoint operator-(const QPoint &p1, const QPoint &p2)
{ 
    return QPoint(p1.xx - p2.xx, p1.yy - p2.yy); 
}

int QPoint::manhattanLength() const
{
    return abs(xx) + abs(yy);
}

#ifdef _KWQ_IOSTREAM_
ostream &operator<<(ostream &o, const QPoint &p)
{
    return o <<
        "QPoint: [x: " <<
        (Q_INT32)p.xx <<
        "; h: " <<
        (Q_INT32)p.yy <<
        ']';
}
#endif

// -------------------------------------------------------------------------

bool QPoint::isNull() const
{ 
    return xx == 0 && yy == 0; 
}

void QPoint::setX(int x)
{ 
    xx = (QCOORD)x; 
}

void QPoint::setY(int y)
{ 
    yy = (QCOORD)y; 
}

QPoint &QPoint::operator+=(const QPoint &other)
{ 
    xx += other.xx; 
    yy += other.yy; 

    return *this; 
}

QPoint &QPoint::operator-=(const QPoint &other)
{ 
    xx -= other.xx; 
    yy -= other.yy; 

    return *this; 
}

QPoint &QPoint::operator*=(int i)
{ 
    xx *= (QCOORD)i; 
    yy *= (QCOORD)i; 

    return *this; 
}

QPoint &QPoint::operator*=(double d)
{ 
    xx = (QCOORD)(xx * d); 
    yy = (QCOORD)(yy * d); 

    return *this; 
}

bool operator==(const QPoint &p1, const QPoint &p2)
{ 
    return p1.xx == p2.xx && p1.yy == p2.yy; 
}

bool operator!=(const QPoint &p1, const QPoint &p2)
{ 
    return p1.xx != p2.xx || p1.yy != p2.yy; 
}

QPoint operator-(const QPoint &p)
{ 
    return QPoint(-p.xx, -p.yy); 
}

QPoint operator*(const QPoint &p, int i)
{ 
    return QPoint(p.xx * i, p.yy * i); 
}

QPoint operator*(int i, const QPoint &p)
{ 
    return QPoint(p.xx * i, p.yy * i); 
}

QPoint operator*(const QPoint &p, double d)
{ 
    return QPoint((QCOORD)(p.xx * d), (QCOORD)(p.yy * d)); 
}

QPoint operator*(double d, const QPoint &p)
{ 
    return QPoint((QCOORD)(p.xx * d), (QCOORD)(p.yy * d)); 
}

QPoint &QPoint::operator/=(int i)
{
    xx /= (QCOORD)i;
    yy /= (QCOORD)i;

    return *this;
}

QPoint &QPoint::operator/=(double d)
{
    xx = (QCOORD)(xx / d);
    yy = (QCOORD)(yy / d);

    return *this;
}

QPoint operator/(const QPoint &p, int i)
{
    return QPoint(p.xx / i, p.yy / i);
}

QPoint operator/(const QPoint &p, double d)
{ 
    return QPoint((QCOORD)(p.xx / d), (QCOORD)(p.yy / d)); 
}

// KWQ hacks ---------------------------------------------------------------

#endif USING_BORROWED_QPOINT

// -------------------------------------------------------------------------
