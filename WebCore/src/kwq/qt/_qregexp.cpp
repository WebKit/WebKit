/****************************************************************************
** $Id$
**
** Implementation of QRegExp class
**
** Created : 950126
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

#include "qregexp.h"
#include <ctype.h>
#include <stdlib.h>

// NOT REVISED
/*!
  \class QRegExp qregexp.h
  \ingroup tools
  \ingroup misc
  \brief The QRegExp class provides pattern matching using regular
  expressions or wildcards.

  QRegExp knows these regexp primitives:
  <ul plain>
  <li><dfn>c</dfn> matches the character 'c'
  <li><dfn>.</dfn> matches any character
  <li><dfn>^</dfn> matches start of input
  <li><dfn>$</dfn>  matches end of input
  <li><dfn>[]</dfn> matches a defined set of characters - see below.
  <li><dfn>a*</dfn> matches a sequence of zero or more a's
  <li><dfn>a+</dfn> matches a sequence of one or more a's
  <li><dfn>a?</dfn> matches an optional a
  <li><dfn>\c</dfn> escape code for matching special characters such
  as \, [, *, +, . etc.
  <li><dfn>\t</dfn> matches the TAB character (9)
  <li><dfn>\n</dfn> matches newline (10)
  <li><dfn>\r</dfn> matches return (13)
  <li><dfn>\s</dfn> matches a white space (defined as any character
  for which QChar::isSpace() returns TRUE. This includes at least
  ASCII characters 9 (TAB), 10 (LF), 11 (VT), 12(FF), 13 (CR) and 32
  (Space)).
  <li><dfn>\d</dfn> matches a digit (defined as any character for
  which QChar::isDigit() returns TRUE. This includes at least ASCII
  characters '0'-'9').
  <li><dfn>\x1f6b</dfn> matches the character with unicode point U1f6b
  (hexadecimal 1f6b). \x0012 will match the ASCII/Latin1 character
  0x12 (18 decimal, 12 hexadecimal).
  <li><dfn>\022</dfn> matches the ASCII/Latin1 character 022 (18
  decimal, 22 octal).
  </ul>

  In wildcard mode, it only knows four primitives:
  <ul plain>
  <li><dfn>c</dfn> matches the character 'c'
  <li><dfn>?</dfn> matches any character
  <li><dfn>*</dfn> matches any sequence of characters
  <li><dfn>[]</dfn> matches a defined set of characters - see below.
  </ul>

  QRegExp supports Unicode both in the pattern strings and in the
  strings to be matched.

  When writing regular expressions in C++ code, remember that C++
  processes \ characters.  So in order to match e.g. a "." character,
  you must write "\\." in C++ source, not "\.".

  A character set matches a defined set of characters. For example,
  [BSD] matches any of 'B', 'D' and 'S'. Within a character set, the
  special characters '.', '*', '?', '^', '$', '+' and '[' lose their
  special meanings. The following special characters apply:
  <ul plain>
  <li><dfn>^</dfn> When placed first in the list, changes the
  character set to match any character \e not in the list. To include
  the character '^' itself in the set, escape it or place it anywhere
  but first.
  <li><dfn>-</dfn> Defines a range of characters. To include the
  character '-' itself in the set, escape it or place it last.
  <li><dfn>]</dfn> Ends the character set definition. To include the
  character ']' itself in the set, escape it or place it first (but
  after the negation operator '^', if present)
  </ul>
  Thus, [a-zA-Z0-9.] matches upper and lower case ASCII letters,
  digits and dot; and [^\s] matches everything except white space.

  \bug Case insensitive matching is not supported for non-ASCII/Latin1
  (non-8bit) characters. Any character with a non-zero QChar.row() is
  matched case sensitively even if the QRegExp is in case insensitive
  mode.

  \note In Qt 3.0, the language of regular expressions will contain
  five more special characters, namely '(', ')', '{', '|' and '}'. To
  ease porting, it's a good idea to escape these characters with a
  backslash in all the regular expressions you'll write from now on.
*/


//
// The regexp pattern is internally represented as an array of uints,
// each element containing an 16-bit character or a 32-bit code
// (listed below).  User-defined character classes (e.g. [a-zA-Z])
// are encoded as this:
// uint no:	1		2		3		...
// value:	CCL | n		from | to	from | to
//
// where n is the (16-bit) number of following range definitions and
// from and to define the ranges inclusive. from <= to is always true,
// otherwise it is a built-in charclass (Pxx, eg \s - PWS). Single
// characters in the class are coded as from==to.  Negated classes
// (e.g. [^a-z]) use CCN instead of CCL.

const uint END	= 0x00000000;
const uint PWS	= 0x10010000;		// predef charclass: whitespace (\s)
const uint PDG	= 0x10020000;		// predef charclass: digit (\d)
const uint CCL	= 0x20010000;		// character class	[]
const uint CCN	= 0x20020000;		// neg character class	[^]
const uint CHR	= 0x40000000;		// character
const uint BOL	= 0x80010000;		// beginning of line	^
const uint EOL	= 0x80020000;		// end of line		$
const uint BOW	= 0x80030000;		// beginning of word	\<
const uint EOW	= 0x80040000;		// end of word		\>
const uint ANY	= 0x80050000;		// any character	.
const uint CLO	= 0x80070000;		// Kleene closure	*
const uint OPT	= 0x80080000;		// Optional closure	?

const uint MCC  = 0x20000000;		// character class bitmask
const uint MCD  = 0xffff0000;		// code mask
const uint MVL  = 0x0000ffff;		// value mask

//
// QRegExp::error codes (internal)
//

const int PatOk		= 0;			// pattern ok
const int PatNull	= 1;			// no pattern defined
const int PatSyntax	= 2;			// pattern syntax error
const int PatOverflow	= 4;			// pattern too long


/*****************************************************************************
  QRegExp member functions
 *****************************************************************************/

/*!
  Constructs an empty regular expression.
*/

QRegExp::QRegExp()
{
    rxdata = 0;
    cs = TRUE;
    wc = FALSE;
    error = PatOk;
}

/*!
  Constructs a regular expression.

  \arg \e pattern is the regular expression pattern string.
  \arg \e caseSensitive specifies whether or not to use case sensitive
  matching.
  \arg \e wildcard specifies whether the pattern string should be used for
  wildcard matching (also called globbing expression), normally used for
  matching file names.

  \sa setWildcard()
*/

QRegExp::QRegExp( const QString &pattern, bool caseSensitive, bool wildcard )
{
    rxstring = pattern;
    rxdata = 0;
    cs = caseSensitive;
    wc = wildcard;
    compile();
}

/*!
  Constructs a regular expression which is a copy of \e r.
  \sa operator=(const QRegExp&)
*/

QRegExp::QRegExp( const QRegExp &r )
{
    rxstring = r.pattern();
    rxdata = 0;
    cs = r.caseSensitive();
    wc = r.wildcard();
    compile();
}

/*!
  Destructs the regular expression and cleans up its internal data.
*/

QRegExp::~QRegExp()
{
    if ( rxdata )                      // Avoid purify complaints
	delete [] rxdata;
}

/*!
  Copies the regexp \e r and returns a reference to this regexp.
  The case sensitivity and wildcard options are copied, as well.
*/

QRegExp &QRegExp::operator=( const QRegExp &r )
{
    rxstring = r.rxstring;
    cs = r.cs;
    wc = r.wc;
    compile();
    return *this;
}

/*!
  \obsolete
  Consider using setPattern() instead of this method.

  Sets the pattern string to \e pattern and returns a reference to this regexp.
  The case sensitivity or wildcard options do not change.
*/

QRegExp &QRegExp::operator=( const QString &pattern )
{
    rxstring = pattern;
    compile();
    return *this;
}


/*!
  Returns TRUE if this regexp is equal to \e r.

  Two regexp objects are equal if they have equal pattern strings,
  case sensitivity options and wildcard options.
*/

bool QRegExp::operator==( const QRegExp &r ) const
{
    return rxstring == r.rxstring && cs == r.cs && wc == r.wc;
}

/*!
  \fn bool QRegExp::operator!=( const QRegExp &r ) const

  Returns TRUE if this regexp is \e not equal to \e r.

  \sa operator==()
*/

/*!
  \fn bool QRegExp::isEmpty() const
  Returns TRUE if the regexp is empty.
*/

/*!
  \fn bool QRegExp::isValid() const
  Returns TRUE if the regexp is valid, or FALSE if it is invalid.

  The pattern "[a-z" is an example of an invalid pattern, since it lacks a
  closing bracket.
*/


/*!
  \fn bool QRegExp::wildcard() const
  Returns TRUE if wildcard mode is on, otherwise FALSE. \sa setWildcard().
*/

/*!
  Sets the wildcard option for the regular expression.	The default
  is FALSE.

  Setting \e wildcard to TRUE makes it convenient to match filenames
  instead of plain text.

  For example, "qr*.cpp" matches the string "qregexp.cpp" in wildcard mode,
  but not "qicpp" (which would be matched in normal mode).

  \sa wildcard()
*/

void QRegExp::setWildcard( bool wildcard )
{
    if ( wildcard != wc ) {
	wc = wildcard;
	compile();
    }
}

/*!
  \fn bool QRegExp::caseSensitive() const

  Returns TRUE if case sensitivity is enabled, otherwise FALSE.	 The
  default is TRUE.

  \sa setCaseSensitive()
*/

/*!
  Enables or disables case sensitive matching.

  In case sensitive mode, "a.e" matches "axe" but not "Axe".

  See also: caseSensitive()
*/

void QRegExp::setCaseSensitive( bool enable )
{
    if ( cs != enable ) {
	cs = enable;
	compile();
    }
}


/*!
  \fn QString QRegExp::pattern() const
  Returns the pattern string of the regexp.
*/


/*!
  \fn void QRegExp::setPattern(const QString & pattern)
  Sets the pattern string to \a pattern and returns a reference to this regexp.
  The case sensitivity or wildcard options do not change.
*/

static inline bool iswordchar( int x )
{
    return isalnum(x) || x == '_';	//# Only 8-bit support
}


/*!
  \internal
  Match character class
*/

static bool matchcharclass( uint *rxd, QChar c )
{
    uint *d = rxd;
    uint clcode = *d & MCD;
    bool neg = clcode == CCN;
    if ( clcode != CCL && clcode != CCN)
	qWarning("QRegExp: Internal error, please report to qt-bugs@trolltech.com");
    uint numFields = *d & MVL;
    uint cval = (((uint)(c.row())) << 8) | ((uint)c.cell());
    bool found = FALSE;
    for ( int i = 0; i < (int)numFields; i++ ) {
	d++;
	if ( *d == PWS && c.isSpace() ) {
	    found = TRUE;
	    break;
	}
	if ( *d == PDG && c.isDigit() ) {
	    found = TRUE;
	    break;
	}
	else {
	    uint from = ( *d & MCD ) >> 16;
	    uint to = *d & MVL;
	    if ( (cval >= from) && (cval <= to) ) {
		found = TRUE;
		break;
	    }
	}
    }
    return neg ? !found : found;
}



/*
  Internal: Recursively match string.
*/

static int matchstring( uint *rxd, const QChar *str, uint strlength,
			const QChar *bol, bool cs )
{
    const QChar *p = str;
    const QChar *start = p;
    uint pl = strlength;
    uint *d = rxd;

    //### in all cases here: handle pl == 0! (don't read past strlen)
    while ( *d ) {
	if ( *d & CHR ) {			// match char
	    if ( !pl )
		return -1;
	    QChar c( *d );
	    if ( !cs && !c.row() ) {		// case insensitive, #Only 8bit
		if ( p->row() || tolower(p->cell()) != c.cell() )
		    return -1;
		p++;
		pl--;
	    } else {				// case insensitive
		if ( *p != c )
		    return -1;
		p++;
		pl--;
	    }
	    d++;
	}
	else if ( *d & MCC ) {			// match char class
	    if ( !pl )
		return -1;
	    if ( !matchcharclass( d, *p ) )
		return -1;
	    p++;
	    pl--;
	    d += (*d & MVL) + 1;
	}
	else switch ( *d++ ) {
	    case PWS:				// match whitespace
		if ( !pl || !p->isSpace() )
		    return -1;
		p++;
		pl--;
		break;
	    case PDG:				// match digits
		if ( !pl || !p->isDigit() )
		    return -1;
		p++;
		pl--;
		break;
	    case ANY:				// match anything
		if ( !pl )
		    return -1;
		p++;
		pl--;
		break;
	    case BOL:				// match beginning of line
		if ( p != bol )
		    return -1;
		break;
	    case EOL:				// match end of line
		if ( pl )
		    return -1;
		break;
	    case BOW:				// match beginning of word
		if ( !iswordchar(*p) || (p > bol && iswordchar(*(p-1)) ) )
		    return -1;
		break;
	    case EOW:				// match end of word
		if ( iswordchar(*p) || p == bol || !iswordchar(*(p-1)) )
		    return -1;
		break;
	    case CLO:				// Kleene closure
		{
		const QChar *first_p = p;
		if ( *d & CHR ) {		// match char
		    QChar c( *d );
		    if ( !cs && !c.row() ) {	// case insensitive, #only 8bit
			while ( pl && !p->row() && tolower(p->cell())==c.cell() ) {
			    p++;
			    pl--;
			}
		    }
		    else {			// case sensitive
			while ( pl && *p == c ) {
			    p++;
			    pl--;
			}
		    }
		    d++;
		}
		else if ( *d & MCC ) {			// match char class
		    while( pl && matchcharclass( d, *p ) ) {
			p++;
			pl--;
		    }
		    d += (*d & MVL) + 1;
		}
		else if ( *d == PWS ) {
		    while ( pl && p->isSpace() ) {
			p++;
			pl--;
		    }
		    d++;
		}
		else if ( *d == PDG ) {
		    while ( pl && p->isDigit() ) {
			p++;
			pl--;
		    }
		    d++;
		}
		else if ( *d == ANY ) {
		    p += pl;
		    pl = 0;
		    d++;
		}
		else {
		    return -1;			// error
		}
		d++;				// skip CLO's END
		while ( p >= first_p ) {	// go backwards
		    int end = matchstring( d, p, pl, bol, cs );
		    if ( end >= 0 )
			return ( p - start ) + end;
		    if ( !p )
			return -1;
		    --p;
		    ++pl;
		}
		}
		return -1;
	    case OPT:				// optional closure
		{
		const QChar *first_p = p;
		if ( *d & CHR ) {		// match char
		    QChar c( *d );
		    if ( !cs && !c.row() ) {	// case insensitive, #only 8bit
			if ( pl && !p->row() && tolower(p->cell()) == c.cell() ) {
			    p++;
			    pl--;
			}
		    }
		    else {			// case sensitive
			if ( pl && *p == c ) {
			    p++;
			    pl--;
			}
		    }
		    d++;
		}
		else if ( *d & MCC ) {			// match char class
		    if ( pl && matchcharclass( d, *p ) ) {
			p++;
			pl--;
		    }
		    d += (*d & MVL) + 1;
		}
		else if ( *d == PWS ) {
		    if ( pl && p->isSpace() ) {
			p++;
			pl--;
		    }
		    d++;
		}
		else if ( *d == PDG ) {
		    if ( pl && p->isDigit() ) {
			p++;
			pl--;
		    }
		    d++;
		}
		else if ( *d == ANY ) {
		    if ( pl ) {
			p++;
			pl--;
		    }
		    d++;
		}
		else {
		    return -1;			// error
		}
		d++;				// skip OPT's END
		while ( p >= first_p ) {	// go backwards
		    int end = matchstring( d, p, pl, bol, cs );
		    if ( end >= 0 )
			return ( p - start ) + end;
		    if ( !p )
			return -1;
		    --p;
		    ++pl;
		}
		}
		return -1;

	    default:				// error
		return -1;
	}
    }
    return p - start;
}


/*!
  \internal
  Recursively match string.
*/

// This is obsolete now, but since it is protected (not private), it
// is still implemented on the off-chance that somebody has made a
// class derived from QRegExp and calls this directly.
// Qt 3.0: Remove this?


const QChar *QRegExp::matchstr( uint *rxd, const QChar *str, uint strlength,
				const QChar *bol ) const
{
    int len = matchstring( rxd, str, strlength, bol, cs );
    if ( len < 0 )
	return 0;
    return str + len;
}

/*!
  Attempts to match in \e str, starting from position \e index.
  Returns the position of the match, or -1 if there was no match.

  If \e len is not a null pointer, the length of the match is stored in
  \e *len.

  If \e indexIsStart is TRUE (the default), the position \e index in
  the string will match the start-of-input primitive (^) in the
  regexp, if present. Otherwise, position 0 in \e str will match.

  Example:
  \code
    QRegExp r("[0-9]*\\.[0-9]+");		// matches floating point
    int len;
    r.match("pi = 3.1416", 0, &len);		// returns 5, len == 6
  \endcode

  \note In Qt 3.0, this function will be replaced by find().
*/

int QRegExp::match( const QString &str, int index, int *len,
		    bool indexIsStart ) const
{
    if ( !isValid() || isEmpty() )
	return -1;
    if ( str.length() < (uint)index )
	return -1;
    const QChar *start = str.unicode();
    const QChar *p = start + index;
    uint pl = str.length() - index;
    uint *d  = rxdata;
    int ep = -1;

    if ( *d == BOL ) {				// match from beginning of line
	ep = matchstring( d, p, pl, indexIsStart ? p : start, cs );
    } else {
	if ( *d & CHR ) {
	    QChar c( *d );
	    if ( !cs && !c.row() ) {		// case sensitive, # only 8bit
		while ( pl && ( p->row() || tolower(p->cell()) != c.cell() ) ) {
		    p++;
		    pl--;
		}
	    } else {				// case insensitive
		while ( pl && *p != c ) {
		    p++;
		    pl--;
		}
	    }
	}
	while( 1 ) {				// regular match
	    ep = matchstring( d, p, pl, indexIsStart ? start+index : start, cs );
	    if ( ep >= 0 )
		break;
	    if ( !pl )
		break;
	    p++;
	    pl--;
	}
    }
    if ( len )
	*len = ep >= 0 ? ep : 0;      // No match -> 0, for historical reasons
    return ep >= 0 ? (int)(p - start) : -1;		// return index;
}

/*! \fn int QRegExp::find( const QString& str, int index )

  Attempts to match in \e str, starting from position \e index.
  Returns the position of the match, or -1 if there was no match.

  \sa match()
*/

//
// Translate wildcard pattern to standard regexp pattern.
// Ex:	 *.cpp	==> ^.*\.cpp$
//

static QString wc2rx( const QString &pattern )
{
    int patlen = (int)pattern.length();
    QString wcpattern = QString::fromLatin1("^");

    QChar c;
    for( int i = 0; i < patlen; i++ ) {
	c = pattern[i];
	switch ( (char)c ) {
	case '*':				// '*' ==> '.*'
	    wcpattern += '.';
	    break;
	case '?':				// '?' ==> '.'
	    c = '.';
	    break;
	case '.':				// quote special regexp chars
	case '+':
	case '\\':
	case '$':
	case '^':
	    wcpattern += '\\';
	    break;
	case '[':
	    if ( (char)pattern[i+1] == '^' ) { // don't quote '^' after '['
		wcpattern += '[';
		c = pattern[i+1];
		i++;
	    }
	    break;
	}
	wcpattern += c;

    }
    wcpattern += '$';
    return wcpattern;				// return new regexp pattern
}


//
// Internal: Get char value and increment pointer.
//

static uint char_val( const QChar **str, uint *strlength )   // get char value
{
    const QChar *p = *str;
    uint pl = *strlength;
    uint len = 1;
    uint v = 0;
    if ( (char)*p == '\\' ) {			// escaped code
	p++;
	pl--;
	if ( !pl ) {				// it is just a '\'
	    (*str)++;
	    (*strlength)--;
	    return '\\';
	}
	len++;					// length at least 2
	int i;
	char c;
	char ch = tolower((char)*p);
	switch ( ch ) {
	    case 'b':  v = '\b';  break;	// bell
	    case 'f':  v = '\f';  break;	// form feed
	    case 'n':  v = '\n';  break;	// newline
	    case 'r':  v = '\r';  break;	// return
	    case 't':  v = '\t';  break;	// tab
	    case 's':  v = PWS; break;		// whitespace charclass
	    case 'd':  v = PDG; break;		// digit charclass
	    case '<':  v = BOW; break;		// word beginning matcher
	    case '>':  v = EOW; break;		// word ending matcher

	    case 'x': {				// hex code
		p++;
		pl--;
		for ( i = 0; (i < 4) && pl; i++ ) {	//up to 4 hex digits
		    c = tolower((char)*p);
		    bool a = ( c >= 'a' && c <= 'f' );
		    if ( (c >= '0' && c <= '9') || a ) {
			v <<= 4;
			v += a ? 10 + c - 'a' : c - '0';
			len++;
		    }
		    else {
			break;
		    }
		    p++;
		    pl--;
		}
	    }
	    break;

	    default: {
		if ( ch >= '0' && ch <= '7' ) {	//octal code
		    len--;
		    for ( i = 0; (i < 3) && pl; i++ ) {	// up to 3 oct digits
			c = (char)*p;
			if ( c >= '0' && c <= '7' ) {
			    v <<= 3;
			    v += c - '0';
			    len++;
			}
			else {
			    break;
			}
			p++;
			pl--;
		    }
		}
		else {				// not an octal number
		    v = (((uint)(p->row())) << 8) | ((uint)p->cell());
		}
	    }
	}
    } else {
	v = (((uint)(p->row())) << 8) | ((uint)p->cell());
    }
    *str += len;
    *strlength -= len;
    return v;
}


#if defined(DEBUG)
static uint *dump( uint *p )
{
    while ( *p != END ) {
	if ( *p & CHR ) {
	    QChar uc = (QChar)*p;
	    char c = (char)uc;
	    uint u = (((uint)(uc.row())) << 8) | ((uint)uc.cell());
	    qDebug( "\tCHR\tU%04x (%c)", u, (c ? c : ' '));
	    p++;
	}
	else if ( *p & MCC ) {
	    uint clcode = *p & MCD;
	    uint numFields = *p & MVL;
	    if ( clcode == CCL )
		qDebug( "\tCCL\t%i", numFields );
	    else if ( clcode == CCN )
		qDebug( "\tCCN\t%i", numFields );
	    else
		qDebug("coding error!");
	    for ( int i = 0; i < (int)numFields; i++ ) {
		p++;
		if ( *p == PWS )
		    qDebug( "\t\tPWS" );
		else if ( *p == PDG )
		    qDebug( "\t\tPDG" );
		else {
		    uint from = ( *p & MCD ) >> 16;
		    uint to = *p & MVL;
		    char fc = (char)QChar(from);
		    char tc = (char)QChar(to);
		    qDebug( "\t\tU%04x (%c) - U%04x (%c)", from,
			   (fc ? fc : ' '), to, (tc ? tc : ' ') );
		}
	    }
	    p++;
	}
	else switch ( *p++ ) {
	    case PWS:
		qDebug( "\tPWS" );
		break;
	    case PDG:
		qDebug( "\tPDG" );
		break;
	    case BOL:
		qDebug( "\tBOL" );
		break;
	    case EOL:
		qDebug( "\tEOL" );
		break;
	    case BOW:
		qDebug( "\tBOW" );
		break;
	    case EOW:
		qDebug( "\tEOW" );
		break;
	    case ANY:
		qDebug( "\tANY" );
		break;
	    case CLO:
		qDebug( "\tCLO" );
		p = dump( p );
		break;
	    case OPT:
		qDebug( "\tOPT" );
		p = dump( p );
		break;
	}
    }
    qDebug( "\tEND" );
    return p+1;
}
#endif // DEBUG


static const int maxlen = 1024;			// max length of regexp array
static uint rxarray[ maxlen ];			// tmp regexp array

/*!
  \internal
  Compiles the regular expression and stores the result in rxdata.
  The 'error' flag is set to non-zero if an error is detected.
  NOTE! This function is not reentrant!
*/

void QRegExp::compile()
{
    if ( rxdata ) {				// delete old data
	delete [] rxdata;
	rxdata = 0;
    }
    if ( rxstring.isEmpty() ) {			// no regexp pattern set
	error = PatNull;
	return;
    }

    error = PatOk;				// assume pattern is ok

    QString pattern;
    if ( wc )
	pattern = wc2rx(rxstring);
    else
	pattern = rxstring;
    const QChar *start = pattern.unicode();	// pattern pointer
    const QChar *p = start;			// pattern pointer
    uint pl = pattern.length();
    uint *d = rxarray;				// data pointer
    uint *prev_d = 0;

#define GEN(x)	*d++ = (x)

    while ( pl ) {
	char ch = (char)*p;
	switch ( ch ) {

	    case '^':				// beginning of line
		prev_d = d;
		GEN( p == start ? BOL : (CHR | ch) );
		p++;
		pl--;
		break;

	    case '$':				// end of line
		prev_d = d;
		GEN( pl == 1 ? EOL : (CHR | ch) );
		p++;
		pl--;
		break;

	    case '.':				// any char
		prev_d = d;
		GEN( ANY );
		p++;
		pl--;
		break;

	    case '[':				// character class
		{
		prev_d = d;
		p++;
		pl--;
		if ( !pl ) {
		    error = PatSyntax;
		    return;
		}
		bool firstIsEscaped = ( (char)*p == '\\' );
		uint cch = char_val( &p, &pl );
		if ( cch == '^' && !firstIsEscaped ) {	// negate!
		    GEN( CCN );
		    if ( !pl ) {
			error = PatSyntax;
			return;
		    }
		    cch = char_val( &p, &pl );
		} else {
		    GEN( CCL );
		}
		uint numFields = 0;
		while ( pl ) {
		    if ((pl>2) && ((char)*p == '-') && ((char)*(p+1) != ']')) {
			// Found a range
		       	char_val( &p, &pl ); // Read the '-'
			uint cch2 = char_val( &p, &pl ); // Read the range end
			if ( cch > cch2 ) { 		// swap start and stop
			    int tmp = cch;
			    cch = cch2;
			    cch2 = tmp;
			}
			GEN( (cch << 16) | cch2 );	// from < to
			numFields++;
		    }
		    else {
			// Found a single character
			if ( cch & MCD ) // It's a code; will not be mistaken
			    GEN( cch );	 // for a range, since from > to
			else
			    GEN( (cch << 16) | cch ); // from == to range
			numFields++;
		    }
		    if ( d >= rxarray + maxlen ) {	// pattern too long
			error = PatOverflow;		
			return;
		    }
		    if ( !pl ) {		// At least ']' should be left
			error = PatSyntax;
			return;
		    }
		    bool nextIsEscaped = ( (char)*p == '\\' );
		    cch = char_val( &p, &pl );
		    if ( cch == (uint)']' && !nextIsEscaped )
			break;
		    if ( !pl ) {		// End, should have seen ']'
			error = PatSyntax;
			return;
		    }
		}
		*prev_d |= numFields;		// Store number of fields
		}
		break;

	    case '*':				// Kleene closure, or
	    case '+':				// positive closure, or
	    case '?':				// optional closure
		{
		if ( prev_d == 0 ) {		// no previous expression
		    error = PatSyntax;		// empty closure
		    return;
		}
		switch ( *prev_d ) {		// test if invalid closure
		    case BOL:
		    case BOW:
		    case EOW:
		    case CLO:
		    case OPT:
			error = PatSyntax;
			return;
		}
		int ddiff = d - prev_d;
		if ( *p == '+' ) {		// convert to Kleene closure
		    if ( d + ddiff >= rxarray + maxlen ) {
			error = PatOverflow;	// pattern too long
			return;
		    }
		    memcpy( d, prev_d, ddiff*sizeof(uint) );
		    d += ddiff;
		    prev_d += ddiff;
		}
		memmove( prev_d+1, prev_d, ddiff*sizeof(uint) );
		*prev_d = ch == '?' ? OPT : CLO;
		d++;
		GEN( END );
		p++;
		pl--;
		}
		break;

	    default:
		{
		prev_d = d;
		uint cv = char_val( &p, &pl );
		if ( cv & MCD ) {			// It's a code
		    GEN( cv );
		}
		else {
		    if ( !cs && cv <= 0xff )		// #only 8bit support
			cv = tolower( cv );
		    GEN( CHR | cv );
		}
		}
	}
	if ( d >= rxarray + maxlen ) {		// oops!
	    error = PatOverflow;		// pattern too long
	    return;
	}
    }
    GEN( END );
    int len = d - rxarray;
    rxdata = new uint[ len ];			// copy from rxarray to rxdata
    CHECK_PTR( rxdata );
    memcpy( rxdata, rxarray, len*sizeof(uint) );
#if defined(DEBUG)
    //dump( rxdata );	// uncomment this line for debugging
#endif
}
