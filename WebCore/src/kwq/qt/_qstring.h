/****************************************************************************
** $Id$
**
** Definition of the QString class, and related Unicode
** functions.
**
** Created : 920609
**
** Copyright (C) 1992-2000 Trolltech AS.  All rights reserved.
**
** This file is part of the tools module of the Qt GUI Toolkit.
**
** This file may be distributed under the terms of the Q Public License
** as defined by Trolltech AS of Norway and appearing in the file
** LICENSE.QPL included in the packaging of this file.
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.
**
** Licensees holding valid Qt Enterprise Edition or Qt Professional Edition
** licenses may use this file in accordance with the Qt Commercial License
** Agreement provided with the Software.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** See http://www.trolltech.com/pricing.html or email sales@trolltech.com for
**   information about Qt Commercial License Agreements.
** See http://www.trolltech.com/qpl/ for QPL licensing information.
** See http://www.trolltech.com/gpl/ for GPL licensing information.
**
** Contact info@trolltech.com if any conditions of this licensing are
** not clear to you.
**
**********************************************************************/

#ifndef QSTRING_H
#define QSTRING_H

#ifndef QT_H
#include "qcstring.h"
#endif // QT_H

// KWQ hacks ---------------------------------------------------------------

#ifndef _KWQ_COMPLETE_
#define _KWQ_COMPLETE_
#endif

#include <KWQDef.h>

// -------------------------------------------------------------------------

/*****************************************************************************
  QString class
 *****************************************************************************/

class QRegExp;
class QString;
class QCharRef;

class Q_EXPORT QChar {
public:
    QChar();
    QChar( char c );
    QChar( uchar c );
    QChar( uchar c, uchar r );
    QChar( const QChar& c );
    QChar( ushort rc );
    QChar( short rc );
    QChar( uint rc );
    QChar( int rc );

    QT_STATIC_CONST QChar null;            // 0000
    QT_STATIC_CONST QChar replacement;     // FFFD
    QT_STATIC_CONST QChar byteOrderMark;     // FEFF
    QT_STATIC_CONST QChar byteOrderSwapped;     // FFFE
    QT_STATIC_CONST QChar nbsp;            // 00A0

    // Unicode information

    enum Category
    {
	NoCategory,

	Mark_NonSpacing,	  //   Mn
	Mark_SpacingCombining,	  //   Mc
	Mark_Enclosing,		  //   Me

	Number_DecimalDigit,	  //   Nd
	Number_Letter,		  //   Nl
	Number_Other,		  //   No

	Separator_Space,	  //   Zs
	Separator_Line,		  //   Zl
	Separator_Paragraph,	  //   Zp

	Other_Control,		  //   Cc
	Other_Format,		  //   Cf
	Other_Surrogate,	  //   Cs
	Other_PrivateUse,	  //   Co
	Other_NotAssigned,	  //   Cn

	Letter_Uppercase,	  //   Lu
	Letter_Lowercase,	  //   Ll
	Letter_Titlecase,	  //   Lt
	Letter_Modifier,	  //   Lm
	Letter_Other,		  //   Lo

	Punctuation_Connector,	  //   Pc
	Punctuation_Dask,	  //   Pd
	Punctuation_Open,	  //   Ps
	Punctuation_Close,	  //   Pe
	Punctuation_InitialQuote, //   Pi
	Punctuation_FinalQuote,	  //   Pf
	Punctuation_Other,	  //   Po

	Symbol_Math,		  //   Sm
	Symbol_Currency,	  //   Sc
	Symbol_Modifier,	  //   Sk
	Symbol_Other		  //   So
    };

    enum Direction
    {
	DirL, DirR, DirEN, DirES, DirET, DirAN, DirCS, DirB, DirS, DirWS, DirON,
	DirLRE, DirLRO, DirAL, DirRLE, DirRLO, DirPDF, DirNSM, DirBN
    };

    enum Decomposition
    {
        Single, Canonical, Font, NoBreak, Initial, Medial,
        Final, Isolated, Circle, Super, Sub, Vertical,
        Wide, Narrow, Small, Square, Compat, Fraction
    };

    enum Joining
    {
	OtherJoining, Dual, Right, Center
    };

    // ****** WHEN ADDING FUNCTIONS, CONSIDER ADDING TO QCharRef TOO

    int digitValue() const;
    QChar lower() const;
    QChar upper() const;

    Category category() const;
    Direction direction() const;
    Joining joining() const;
    bool mirrored() const;
    QChar mirroredChar() const;
    QString decomposition() const;
    Decomposition decompositionTag() const;

    char latin1() const { return rw ? 0 : cl; }
    ushort unicode() const { return (rw << 8) | cl; }
#ifndef QT_NO_CAST_ASCII
    // like all ifdef'd code this is undocumented
    operator char() const { return latin1(); }
#endif

    bool isNull() const { return unicode()==0; }
    bool isPrint() const;
    bool isPunct() const;
    bool isSpace() const;
    bool isMark() const;
    bool isLetter() const;
    bool isNumber() const;
    bool isLetterOrNumber() const;
    bool isDigit() const;

    uchar& cell() { return cl; }
    uchar& row() { return rw; }
    uchar cell() const { return cl; }
    uchar row() const { return rw; }

    static bool networkOrdered() { return (int)net_ordered == 1; }

    friend inline int operator==( char ch, QChar c );
    friend inline int operator==( QChar c, char ch );
    friend inline int operator==( QChar c1, QChar c2 );
    friend inline int operator!=( QChar c1, QChar c2 );
    friend inline int operator!=( char ch, QChar c );
    friend inline int operator!=( QChar c, char ch );
    friend inline int operator<=( QChar c, char ch );
    friend inline int operator<=( char ch, QChar c );
    friend inline int operator<=( QChar c1, QChar c2 );

private:
#if defined(_WS_X11_) || defined(_OS_WIN32_BYTESWAP_) || defined( _WS_QWS_ )
    // XChar2b on X11, ushort on _OS_WIN32_BYTESWAP_
    //### QWS must be defined on a platform by platform basis
    uchar rw;
    uchar cl;
#if defined(QT_QSTRING_UCS_4)
    ushort grp;
#endif
    enum { net_ordered = 1 };
#else
    // ushort on _OS_WIN32_
    uchar cl;
    uchar rw;
#if defined(QT_QSTRING_UCS_4)
    ushort grp;
#endif
    enum { net_ordered = 0 };
#endif
} Q_PACKED;

inline QChar::QChar()
{
    rw = 0; cl = 0;
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( char c )
{
    rw = 0; cl = (uchar)c;
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( uchar c )
{
    rw = 0; cl = c;
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( uchar c, uchar r )
{
    rw = r; cl = c;
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( const QChar& c )
{
    rw = c.rw; cl = c.cl;
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( ushort rc )
{
    rw = (uchar)((rc>>8)&0xff); cl = (uchar)(rc&0xff);
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( short rc )
{
    rw = (uchar)((rc>>8)&0xff); cl = (uchar)(rc&0xff);
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( uint rc )
{
    rw = (uchar)((rc>>8)&0xff); cl = (uchar)(rc&0xff);
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}
inline QChar::QChar( int rc )
{
    rw = (uchar)((rc>>8)&0xff); cl = (uchar)(rc&0xff);
#ifdef QT_QSTRING_UCS_4
    grp = 0;
#endif
}


inline int operator==( char ch, QChar c )
{
    return ch == c.cl && !c.rw;
}

inline int operator==( QChar c, char ch )
{
    return ch == c.cl && !c.rw;
}

inline int operator==( QChar c1, QChar c2 )
{
    return c1.cl == c2.cl
	&& c1.rw == c2.rw;
}

inline int operator!=( QChar c1, QChar c2 )
{
    return c1.cl != c2.cl
	|| c1.rw != c2.rw;
}

inline int operator!=( char ch, QChar c )
{
    return ch != c.cl || c.rw;
}

inline int operator!=( QChar c, char ch )
{
    return ch != c.cl || c.rw;
}

inline int operator<=( QChar c, char ch )
{
    return !(ch < c.cl || c.rw);
}

inline int operator<=( char ch, QChar c )
{
    return ch <= c.cl || c.rw;
}

inline int operator<=( QChar c1, QChar c2 )
{
    return c1.rw > c2.rw
	? FALSE
	: c1.rw < c2.rw
	    ? TRUE
	    : c1.cl <= c2.cl;
}

inline int operator>=( QChar c, char ch ) { return ch <= c; }
inline int operator>=( char ch, QChar c ) { return c <= ch; }
inline int operator>=( QChar c1, QChar c2 ) { return c2 <= c1; }
inline int operator<( QChar c, char ch ) { return !(ch<=c); }
inline int operator<( char ch, QChar c ) { return !(c<=ch); }
inline int operator<( QChar c1, QChar c2 ) { return !(c2<=c1); }
inline int operator>( QChar c, char ch ) { return !(ch>=c); }
inline int operator>( char ch, QChar c ) { return !(c>=ch); }
inline int operator>( QChar c1, QChar c2 ) { return !(c2>=c1); }

// internal
struct Q_EXPORT QStringData : public QShared {
    QStringData() :
	unicode(0), ascii(0), len(0), maxl(0), dirtyascii(0) { ref(); }
    QStringData(QChar *u, uint l, uint m) :
	unicode(u), ascii(0), len(l), maxl(m), dirtyascii(0) { }

    ~QStringData() { if ( unicode ) delete[] ((char*)unicode);
                     if ( ascii ) delete[] ascii; }

    void deleteSelf();
    QChar *unicode;
    char *ascii;
    uint len;
    uint maxl:30;
    uint dirtyascii:1;
};


class Q_EXPORT QString
{
public:
    QString();					// make null string
    QString( QChar );				// one-char string
    QString( const QString & );			// impl-shared copy
    QString( const QByteArray& );		// deep copy
    QString( const QChar* unicode, uint length ); // deep copy
#ifndef QT_NO_CAST_ASCII
    QString( const char *str );			// deep copy
#endif
    ~QString();

    QString    &operator=( const QString & );	// impl-shared copy
#ifndef QT_NO_CAST_ASCII
    QString    &operator=( const char * );	// deep copy
#endif
    QString    &operator=( const QCString& );	// deep copy
    QString    &operator=( QChar c );
    QString    &operator=( char c );

    QT_STATIC_CONST QString null;

    bool	isNull()	const;
    bool	isEmpty()	const;
    uint	length()	const;
    void	truncate( uint pos );

#if QT_VERSION >= 300
#error "fill() Should return *this, or QChar constructor should take count=1"
#endif
    void	fill( QChar c, int len = -1 );

    QString	copy()	const;

    QString arg(long a, int fieldwidth=0, int base=10) const;
    QString arg(ulong a, int fieldwidth=0, int base=10) const;
    QString arg(int a, int fieldwidth=0, int base=10) const;
    QString arg(uint a, int fieldwidth=0, int base=10) const;
    QString arg(short a, int fieldwidth=0, int base=10) const;
    QString arg(ushort a, int fieldwidth=0, int base=10) const;
    QString arg(char a, int fieldwidth=0) const;
    QString arg(QChar a, int fieldwidth=0) const;
    QString arg(const QString& a, int fieldwidth=0) const;
    QString arg(double a, int fieldwidth=0, char fmt='g', int prec=-1) const;

    QString    &sprintf( const char* format, ... )
#if defined(_CC_GNU_) && !defined(__INSURE__)
	__attribute__ ((format (printf, 2, 3)))
#endif
	;

    int		find( QChar c, int index=0, bool cs=TRUE ) const;
    int		find( char c, int index=0, bool cs=TRUE ) const;
    int		find( const QString &str, int index=0, bool cs=TRUE ) const;
    int		find( const QRegExp &, int index=0 ) const;
#ifndef QT_NO_CAST_ASCII
    int		find( const char* str, int index=0 ) const;
#endif
    int		findRev( QChar c, int index=-1, bool cs=TRUE) const;
    int		findRev( char c, int index=-1, bool cs=TRUE) const;
    int		findRev( const QString &str, int index=-1, bool cs=TRUE) const;
    int		findRev( const QRegExp &, int index=-1 ) const;
#ifndef QT_NO_CAST_ASCII
    int		findRev( const char* str, int index=-1 ) const;
#endif
    int		contains( QChar c, bool cs=TRUE ) const;
    int		contains( char c, bool cs=TRUE ) const
		    { return contains(QChar(c), cs); }
#ifndef QT_NO_CAST_ASCII
    int		contains( const char* str, bool cs=TRUE ) const;
#endif
    int		contains( const QString &str, bool cs=TRUE ) const;
    int		contains( const QRegExp & ) const;

    QString	left( uint len )  const;
    QString	right( uint len ) const;
    QString	mid( uint index, uint len=0xffffffff) const;

    QString	leftJustify( uint width, QChar fill=' ', bool trunc=FALSE)const;
    QString	rightJustify( uint width, QChar fill=' ',bool trunc=FALSE)const;

    QString	lower() const;
    QString	upper() const;

    QString	stripWhiteSpace()	const;
    QString	simplifyWhiteSpace()	const;

    QString    &insert( uint index, const QString & );
    QString    &insert( uint index, const QChar*, uint len );
    QString    &insert( uint index, QChar );
    QString    &insert( uint index, char c ) { return insert(index,QChar(c)); }
    QString    &append( char );
    QString    &append( QChar );
    QString    &append( const QString & );
    QString    &prepend( char );
    QString    &prepend( QChar );
    QString    &prepend( const QString & );
    QString    &remove( uint index, uint len );
    QString    &replace( uint index, uint len, const QString & );
    QString    &replace( uint index, uint len, const QChar*, uint clen );
    QString    &replace( const QRegExp &, const QString & );

    short	toShort( bool *ok=0, int base=10 )	const;
    ushort	toUShort( bool *ok=0, int base=10 )	const;
    int		toInt( bool *ok=0, int base=10 )	const;
    uint	toUInt( bool *ok=0, int base=10 )	const;
    long	toLong( bool *ok=0, int base=10 )	const;
    ulong	toULong( bool *ok=0, int base=10 )	const;
    float	toFloat( bool *ok=0 )	const;
    double	toDouble( bool *ok=0 )	const;

    QString    &setNum( short, int base=10 );
    QString    &setNum( ushort, int base=10 );
    QString    &setNum( int, int base=10 );
    QString    &setNum( uint, int base=10 );
    QString    &setNum( long, int base=10 );
    QString    &setNum( ulong, int base=10 );
    QString    &setNum( float, char f='g', int prec=6 );
    QString    &setNum( double, char f='g', int prec=6 );

    static QString number( long, int base=10 );
    static QString number( ulong, int base=10);
    static QString number( int, int base=10 );
    static QString number( uint, int base=10);
    static QString number( double, char f='g', int prec=6 );

    void	setExpand( uint index, QChar c );

    QString    &operator+=( const QString &str );
    QString    &operator+=( QChar c );
    QString    &operator+=( char c );

    // Your compiler is smart enough to use the const one if it can.
    QChar at( uint i ) const
	{ return i<d->len ? d->unicode[i] : QChar::null; }
    QChar operator[]( int i ) const { return at((uint)i); }
    QCharRef at( uint i );
    QCharRef operator[]( int i );

    QChar constref(uint i) const
	{ return at(i); }
    QChar& ref(uint i)
	{ // Optimized for easy-inlining by simple compilers.
	    if (d->count!=1 || i>=d->len)
		subat(i);
	    d->dirtyascii=1;
	    return d->unicode[i];
	}

    const QChar* unicode() const { return d->unicode; }
    const char* ascii() const;
    const char* latin1() const;
    static QString fromLatin1(const char*, int len=-1);

// FIXME: we need utf8() to compile khtml
#ifdef QT_NO_TEXTCODEC
    QCString utf8() const { return latin1(); }
#endif

#ifndef QT_NO_TEXTCODEC
    QCString utf8() const;
    static QString fromUtf8(const char*, int len=-1);
#endif
    QCString local8Bit() const;
    static QString fromLocal8Bit(const char*, int len=-1);
    bool operator!() const;
#ifndef QT_NO_ASCII_CAST
    operator const char *() const { return latin1(); }
#endif

    QString &setUnicode( const QChar* unicode, uint len );
    QString &setUnicodeCodes( const ushort* unicode_as_ushorts, uint len );
    QString &setLatin1( const char*, int len=-1 );

    int compare( const QString& s ) const;
    static int compare( const QString& s1, const QString& s2 )
	{ return s1.compare(s2); }

#ifndef QT_NO_DATASTREAM
    friend Q_EXPORT QDataStream &operator>>( QDataStream &, QString & );
#endif
    // new functions for BiDi
    void compose();
    QChar::Direction basicDirection();
    QString visual(int index = 0, int len = -1);

#ifndef QT_NO_COMPAT
    const char* data() const { return latin1(); }
#endif

    bool startsWith( const QString& ) const;

private:
    QString( int size, bool /*dummy*/ );	// allocate size incl. \0

    void deref();
    void real_detach();
    void setLength( uint pos );
    void subat( uint );
    bool findArg(int& pos, int& len) const;

    static QChar* asciiToUnicode( const char*, uint * len, uint maxlen=(uint)-1 );
    static QChar* asciiToUnicode( const QByteArray&, uint * len );
    static char* unicodeToAscii( const QChar*, uint len );

    QStringData *d;
    static QStringData* shared_null;
    static QStringData* makeSharedNull();

    friend class QConstString;
    QString(QStringData* dd, bool /*dummy*/) : d(dd) { }
};

class Q_EXPORT QCharRef {
    friend class QString;
    QString& s;
    uint p;
    QCharRef(QString* str, uint pos) : s(*str), p(pos) { }

public:
    // Most QChar operations repeated here...

    // all this is not documented: We just say "like QChar" and let it be.
#if 1
    ushort unicode() const { return s.constref(p).unicode(); }
    char latin1() const { return s.constref(p).latin1(); }

    // An operator= for each QChar cast constructor...
    QCharRef operator=(char c ) { s.ref(p)=c; return *this; }
    QCharRef operator=(uchar c ) { s.ref(p)=c; return *this; }
    QCharRef operator=(QChar c ) { s.ref(p)=c; return *this; }
    QCharRef operator=(const QCharRef& c ) { s.ref(p)=c.unicode(); return *this; }
    QCharRef operator=(ushort rc ) { s.ref(p)=rc; return *this; }
    QCharRef operator=(short rc ) { s.ref(p)=rc; return *this; }
    QCharRef operator=(uint rc ) { s.ref(p)=rc; return *this; }
    QCharRef operator=(int rc ) { s.ref(p)=rc; return *this; }

    operator QChar () const { return s.constref(p); }

    // each function...
    bool isNull() const { return unicode()==0; }
    bool isPrint() const { return s.constref(p).isPrint(); }
    bool isPunct() const { return s.constref(p).isPunct(); }
    bool isSpace() const { return s.constref(p).isSpace(); }
    bool isMark() const { return s.constref(p).isMark(); }
    bool isLetter() const { return s.constref(p).isLetter(); }
    bool isNumber() const { return s.constref(p).isNumber(); }
    bool isLetterOrNumber() { return s.constref(p).isLetterOrNumber(); }
    bool isDigit() const { return s.constref(p).isDigit(); }

    int digitValue() const { return s.constref(p).digitValue(); }
    QChar lower() { return s.constref(p).lower(); }
    QChar upper() { return s.constref(p).upper(); }

    QChar::Category category() const { return s.constref(p).category(); }
    QChar::Direction direction() const { return s.constref(p).direction(); }
    QChar::Joining joining() const { return s.constref(p).joining(); }
    bool mirrored() const { return s.constref(p).mirrored(); }
    QChar mirroredChar() const { return s.constref(p).mirroredChar(); }
    QString decomposition() const { return s.constref(p).decomposition(); }
    QChar::Decomposition decompositionTag() const { return s.constref(p).decompositionTag(); }

    // Not the non-const ones of these.
    uchar cell() const { return s.constref(p).cell(); }
    uchar row() const { return s.constref(p).row(); }
#endif
};

inline QCharRef QString::at( uint i ) { return QCharRef(this,i); }
inline QCharRef QString::operator[]( int i ) { return at((uint)i); }


class Q_EXPORT QConstString : private QString {
public:
    QConstString( QChar* unicode, uint length );
    ~QConstString();
    const QString& string() const { return *this; }
};


/*****************************************************************************
  QString stream functions
 *****************************************************************************/
#ifndef QT_NO_DATASTREAM
Q_EXPORT QDataStream &operator<<( QDataStream &, const QString & );
Q_EXPORT QDataStream &operator>>( QDataStream &, QString & );
#endif

/*****************************************************************************
  QString inline functions
 *****************************************************************************/

// These two move code into makeSharedNull() and deletesData()
// to improve cache-coherence (and reduce code bloat), while
// keeping the common cases fast.
//
// No safe way to pre-init shared_null on ALL compilers/linkers.
inline QString::QString() :
    d(shared_null ? shared_null : makeSharedNull())
{
    d->ref();
}
//
inline QString::~QString()
{
    if ( d->deref() ) {
	if ( d == shared_null )
	    shared_null = 0;
	d->deleteSelf();
    }
}

inline QString &QString::operator=( QChar c )
{ return *this = QString(c); }

inline QString &QString::operator=( char c )
{ return *this = QString(QChar(c)); }

inline bool QString::isNull() const
{ return unicode() == 0; }

inline bool QString::operator!() const
{ return isNull(); }

inline uint QString::length() const
{ return d->len; }

inline bool QString::isEmpty() const
{ return length() == 0; }

inline QString QString::copy() const
{ return QString( *this ); }

inline QString &QString::prepend( const QString & s )
{ return insert(0,s); }

inline QString &QString::prepend( QChar c )
{ return insert(0,c); }

inline QString &QString::prepend( char c )
{ return insert(0,c); }

inline QString &QString::append( const QString & s )
{ return operator+=(s); }

inline QString &QString::append( QChar c )
{ return operator+=(c); }

inline QString &QString::append( char c )
{ return operator+=(c); }

inline QString &QString::setNum( short n, int base )
{ return setNum((long)n, base); }

inline QString &QString::setNum( ushort n, int base )
{ return setNum((ulong)n, base); }

inline QString &QString::setNum( int n, int base )
{ return setNum((long)n, base); }

inline QString &QString::setNum( uint n, int base )
{ return setNum((ulong)n, base); }

inline QString &QString::setNum( float n, char f, int prec )
{ return setNum((double)n,f,prec); }

inline QString QString::arg(int a, int fieldwidth, int base) const
{ return arg((long)a, fieldwidth, base); }

inline QString QString::arg(uint a, int fieldwidth, int base) const
{ return arg((ulong)a, fieldwidth, base); }

inline QString QString::arg(short a, int fieldwidth, int base) const
{ return arg((long)a, fieldwidth, base); }

inline QString QString::arg(ushort a, int fieldwidth, int base) const
{ return arg((ulong)a, fieldwidth, base); }

inline int QString::find( char c, int index, bool cs ) const
{ return find(QChar(c), index, cs); }

inline int QString::findRev( char c, int index, bool cs) const
{ return findRev( QChar(c), index, cs ); }


#ifndef QT_NO_CAST_ASCII
inline int QString::find( const char* str, int index ) const
{ return find(QString::fromLatin1(str), index); }

inline int QString::findRev( const char* str, int index ) const
{ return findRev(QString::fromLatin1(str), index); }
#endif


/*****************************************************************************
  QString non-member operators
 *****************************************************************************/

Q_EXPORT bool operator!=( const QString &s1, const QString &s2 );
Q_EXPORT bool operator<( const QString &s1, const QString &s2 );
Q_EXPORT bool operator<=( const QString &s1, const QString &s2 );
Q_EXPORT bool operator==( const QString &s1, const QString &s2 );
Q_EXPORT bool operator>( const QString &s1, const QString &s2 );
Q_EXPORT bool operator>=( const QString &s1, const QString &s2 );
#ifndef QT_NO_CAST_ASCII
Q_EXPORT bool operator!=( const QString &s1, const char *s2 );
Q_EXPORT bool operator<( const QString &s1, const char *s2 );
Q_EXPORT bool operator<=( const QString &s1, const char *s2 );
Q_EXPORT bool operator==( const QString &s1, const char *s2 );
Q_EXPORT bool operator>( const QString &s1, const char *s2 );
Q_EXPORT bool operator>=( const QString &s1, const char *s2 );
Q_EXPORT bool operator!=( const char *s1, const QString &s2 );
Q_EXPORT bool operator<( const char *s1, const QString &s2 );
Q_EXPORT bool operator<=( const char *s1, const QString &s2 );
Q_EXPORT bool operator==( const char *s1, const QString &s2 );
//Q_EXPORT bool operator>( const char *s1, const QString &s2 ); // MSVC++
Q_EXPORT bool operator>=( const char *s1, const QString &s2 );
#endif

Q_EXPORT inline QString operator+( const QString &s1, const QString &s2 )
{
    QString tmp( s1 );
    tmp += s2;
    return tmp;
}

#ifndef QT_NO_CAST_ASCII
Q_EXPORT inline QString operator+( const QString &s1, const char *s2 )
{
    QString tmp( s1 );
    tmp += QString::fromLatin1(s2);
    return tmp;
}

Q_EXPORT inline QString operator+( const char *s1, const QString &s2 )
{
    QString tmp = QString::fromLatin1( s1 );
    tmp += s2;
    return tmp;
}
#endif

Q_EXPORT inline QString operator+( const QString &s1, QChar c2 )
{
    QString tmp( s1 );
    tmp += c2;
    return tmp;
}

Q_EXPORT inline QString operator+( const QString &s1, char c2 )
{
    QString tmp( s1 );
    tmp += c2;
    return tmp;
}

Q_EXPORT inline QString operator+( QChar c1, const QString &s2 )
{
    QString tmp;
    tmp += c1;
    tmp += s2;
    return tmp;
}

Q_EXPORT inline QString operator+( char c1, const QString &s2 )
{
    QString tmp;
    tmp += c1;
    tmp += s2;
    return tmp;
}

#if defined(_OS_WIN32_)
extern Q_EXPORT QString qt_winQString(void*);
extern Q_EXPORT const void* qt_winTchar(const QString& str, bool addnul);
extern Q_EXPORT void* qt_winTchar_new(const QString& str);
extern Q_EXPORT QCString qt_winQString2MB( const QString& s, int len=-1 );
extern Q_EXPORT QString qt_winMB2QString( const char* mb, int len=-1 );
#endif

#endif // QSTRING_H
