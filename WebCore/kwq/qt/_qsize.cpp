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

#include <qsize.h>

QSize::QSize() 
{
    w = h = -1;
}

QSize::QSize(int _w, int _h) 
{
    w = (QCOORD)_w;
    h = (QCOORD)_h;
}

QSize::QSize(const QSize &other) 
{
    w = other.w;
    h = other.h;
}

bool QSize::isValid() const
{
    return (w >= 0 && h >= 0);
}

int QSize::width() const 
{
    return w;
}

int QSize::height() const 
{
    return h;
}

void QSize::setWidth(int _w) 
{
    w = _w;
}

void QSize::setHeight(int _h) 
{
    h = _h;
}

QSize QSize::expandedTo(const QSize &other) const 
{
    QCOORD _w;    
    QCOORD _h;    

    _w = w > other.w ? w : other.w;
    _h = h > other.h ? h : other.h;

    return QSize(_w, _h);
}

QSize operator+(const QSize &s1, const QSize &s2)
{
    return QSize(s1.w + s2.w, s1.h + s2.h);
}

bool operator==(const QSize &s1, const QSize &s2) 
{
    return (s1.w == s2.w && s1.h == s2.h);
}

bool operator!=(const QSize &s1, const QSize &s2) 
{
    return (s1.w != s2.w || s1.h != s2.h);
}

#ifdef _KWQ_IOSTREAM_
ostream &operator<<(ostream &o, const QSize &s) 
{
    return o << 
        "QSize: [w: " <<     
            (Q_INT32)s.w << 
            "; h: " << 
            (Q_INT32)s.h << 
        ']';
}
#endif

// KWQ_COMPLETE implementations ------------------------------------------

#ifdef _KWQ_COMPLETE_

bool QSize::isNull() const
{
    return w == 0 && h == 0;
}

bool QSize::isEmpty() const
{
    return w < 1 && h < 1;
}

void QSize::transpose()
{
    QCOORD swap;

    swap = w;
    w = h;
    h = swap;
}

QSize QSize::boundedTo(const QSize &other) const
{
    return QSize(QMIN(w,other.w), QMIN(h,other.h));
}

QSize &QSize::operator+=(const QSize &other)
{
    w += other.w;
    h += other.h;

    return *this;
}

QSize &QSize::operator-=(const QSize &other)
{
    w -= other.w;
    h -= other.h;

    return *this;
}

QSize &QSize::operator*=(int i)
{
    w *= (QCOORD)i;
    h *= (QCOORD)i;

    return *this;
}

QSize &QSize::operator*=(double d)
{
    w = (QCOORD)(w * d);
    h = (QCOORD)(h * d);

    return *this;
}

QSize &QSize::operator/=(int i)
{
    w /= (QCOORD)i;
    h /= (QCOORD)i;

    return *this;
}

QSize &QSize::operator/=(double d)
{
    w = (QCOORD)(w / d);
    h = (QCOORD)(h / d);

    return *this;
}

QSize operator-(const QSize &s1, const QSize &s2)
{
    return QSize(s1.w - s2.w, s1.h - s2.h);
}

QSize operator*(const QSize &s, int i)
{
    return QSize(s.w * (QCOORD)i, s.h * (QCOORD)i);
}

QSize operator*(int i, const QSize &s)
{
    return QSize(s.w * (QCOORD)i, s.h * (QCOORD)i);
}

QSize operator*(const QSize &s, double d)
{
    return QSize((QCOORD)(s.w * d), (QCOORD)(s.h * d));
}

QSize operator*(double d, const QSize &s)
{
    return QSize((QCOORD)(s.w * d), (QCOORD)(s.h * d));
}

QSize operator/(const QSize &s, int i)
{
    return QSize(s.w / (QCOORD)i, s.h / (QCOORD)i);
}

QSize operator/(const QSize &s, double d)
{
    return QSize((QCOORD)(s.w / d), (QCOORD)(s.h / d));
}

#endif // _KWQ_COMPLETE_
