/****************************************************************************
** $Id$
**
** Implementation of date and time classes
**
** Created : 940124
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

#define gettimeofday	__hide_gettimeofday
#include "qdatetime.h"

#ifndef QT_NO_DATASTREAM
#include "qdatastream.h"
#endif

#include <stdio.h>
#include <time.h>
#if defined(_OS_WIN32_)
#if defined(_CC_BOOL_DEF_)
#undef	bool
#include <windows.h>
#define bool int
#else
#include <windows.h>
#endif
#elif defined(_OS_MSDOS_)
#include <dos.h>
#elif defined(_OS_OS2_)
#include <os2.h>
#elif defined(_OS_UNIX_)
#include <unistd.h>
#include <sys/time.h>
#undef	gettimeofday
extern "C" int gettimeofday( struct timeval *, struct timezone * );
#endif

static const uint FIRST_DAY	= 2361222;	// Julian day for 1752/09/14
static const int  FIRST_YEAR	= 1752;		// ### wrong for many countries
static const uint SECS_PER_DAY	= 86400;
static const uint MSECS_PER_DAY = 86400000;
static const uint SECS_PER_HOUR = 3600;
static const uint MSECS_PER_HOUR= 3600000;
static const uint SECS_PER_MIN	= 60;
static const uint MSECS_PER_MIN = 60000;

static const short monthDays[] ={0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// ##### Localize.

const char * const QDate::monthNames[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

const char * const QDate::weekdayNames[] ={
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };


/*****************************************************************************
  QDate member functions
 *****************************************************************************/

// REVISED: aavit

/*!
  \class QDate qdatetime.h
  \brief The QDate class provides date functions.

  \ingroup time

  A QDate object contains a calendar date, i.e. year, month, and day
  numbers in the modern western (Gregorian) calendar. It can read the
  current date from the system clock. It provides functions for
  comparing dates and for manipulating a date by adding a number of
  days.

  A QDate object is typically created either by giving the year, month
  and day numbers explicitly, or by using the static function
  currentDate(), which makes a QDate object which contains the
  system's clock date. An explicit date can also be set using
  setYMD().

  The year(), month(), and day() functions provide access to the year,
  month, and day numbers. Also, dayOfWeek() and dayOfYear() functions
  are provided. The same information is provided in textual format by
  the toString(), dayName(), and monthName() functions.

  QDate provides a full set of operators to compare two QDate
  objects. A date is considered smaller than another if it is earlier
  than the other.

  The date a given number of days later than a given date can be found
  using the addDays() function. Correspondingly, the number of days
  between two dates can be found using the daysTo() function.

  The daysInMonth() and daysInYear() functions tell how many days
  there are in this date's month and year, respectively. The
  isLeapYear() function tells whether this date is in a leap year.

  Note that QDate may not be used for date calculations for dates in
  the remote past, i.e. prior to the introduction of the Gregorian
  calendar. This calendar was adopted by England Sep. 14. 1752 (hence
  this is the earliest valid QDate), and subsequently by most other
  western countries, until 1923.

  The end of time is reached around 8000AD, by which time we expect Qt
  to be obsolete.

  \sa QTime, QDateTime
*/


/*!
  \fn QDate::QDate()
  Constructs a null date. Null dates are invalid.

  \sa isNull(), isValid()
*/


/*!
  Constructs a date with the year \a y, month \a m and day \a d.

  \a y must be in the range 1752-ca. 8000, \a m must be in the range
  1-12, and \a d must be in the range 1-31. Exception: if \a y is in
  the range 0-99, it is interpreted as 1900-1999.

  \sa isValid()
*/

QDate::QDate( int y, int m, int d )
{
    jd = 0;
    setYMD( y, m, d );
}


/*!
  \fn bool QDate::isNull() const

  Returns TRUE if the date is null.  A null date is invalid.

  \sa isValid()
*/


/*!
  Returns TRUE if this date is valid.

  \sa isNull()
*/

bool QDate::isValid() const
{
    return jd >= FIRST_DAY;
}


/*!
  Returns the year (>= 1752) of this date.

  \sa month(), day()
*/

int QDate::year() const
{
    int y, m, d;
    jul2greg( jd, y, m, d );
    return y;
}

/*!
  Returns the month (January=1 .. December=12) of this date.

  \sa year(), day()
*/

int QDate::month() const
{
    int y, m, d;
    jul2greg( jd, y, m, d );
    return m;
}

/*!
  Returns the day of the month (1..31) of this date.

  \sa year(), month(), dayOfWeek()
*/

int QDate::day() const
{
    int y, m, d;
    jul2greg( jd, y, m, d );
    return d;
}

/*!
  Returns the weekday (Monday=1 .. Sunday=7) for this date.

  \sa day(), dayOfYear()
*/

int QDate::dayOfWeek() const
{
    return (((jd+1) % 7) + 6)%7 + 1;
}

/*!
  Returns the day of the year (1..365) for this date.

  \sa day(), dayOfWeek()
*/

int QDate::dayOfYear() const
{
    return jd - greg2jul(year(), 1, 1) + 1;
}

/*!
  Returns the number of days in the month (28..31) for this date.

  \sa day(), daysInYear()
*/

int QDate::daysInMonth() const
{
    int y, m, d;
    jul2greg( jd, y, m, d );
    if ( m == 2 && leapYear(y) )
	return 29;
    else
	return monthDays[m];
}

/*!
  Returns the number of days in the year (365 or 366) for this date.

  \sa day(), daysInMonth()
*/

int QDate::daysInYear() const
{
    int y, m, d;
    jul2greg( jd, y, m, d );
    return leapYear(y) ? 366 : 365;
}


/*!
  Returns the name of the \a month.

  Month 1 == "Jan", month 2 == "Feb" etc.

  \sa toString(), dayName()
*/

QString QDate::monthName( int month ) const
{
#if defined(CHECK_RANGE)
    if ( month < 1 || month > 12 ) {
	qWarning( "QDate::monthName: Parameter out ouf range." );
	month = 1;
    }
#endif
    // ### Remove the fromLatin1 during localization
    return QString::fromLatin1(monthNames[month-1]);
}

/*!
  Returns the name of the \a weekday.

  Weekday 1 == "Mon", day 2 == "Tue" etc.

  \sa toString(), monthName()
*/

QString QDate::dayName( int weekday ) const
{
#if defined(CHECK_RANGE)
    if ( weekday < 1 || weekday > 7 ) {
	qWarning( "QDate::dayName: Parameter out of range." );
	weekday = 1;
    }
#endif
    // ### Remove the fromLatin1 during localization
    return QString::fromLatin1(weekdayNames[weekday-1]);
}


/*!
  Returns the date as a string.

  The string format is "Sat May 20 1995". This function uses the
  dayName() and monthName() functions to generate the string.

  \sa dayName(), monthName()
*/

QString QDate::toString() const
{
    int y, m, d;
    jul2greg( jd, y, m, d );
    QString buf = dayName(dayOfWeek());
    buf += ' ';
    buf += monthName(m);
    QString t;
    t.sprintf( " %d %d", d, y);
    buf += t;
    return buf;
}


/*!
  Sets the year \a y, month \a m and day \a d.

  \a y must be in the range 1752-ca. 8000, \a m must be in the range
  1-12, and \a d must be in the range 1-31. Exception: if \a y is in
  the range 0-99, it is interpreted as 1900-1999.

  Returns TRUE if the date is valid, otherwise FALSE.
*/

bool QDate::setYMD( int y, int m, int d )
{
    if ( !isValid(y,m,d) ) {
#if defined(CHECK_RANGE)
	 qWarning( "QDate::setYMD: Invalid date %04d/%02d/%02d", y, m, d );
#endif
	 return FALSE;
    }
    jd = greg2jul( y, m, d );
#if defined(DEBUG)
    ASSERT( year() == (y > 99 ? y : 1900+y) && month() == m && day() == d );
#endif
    return TRUE;
}

/*!
  Returns a QDate object containing a date \a ndays later than the
  date of this object (or earlier if \a ndays is negative).

  \sa daysTo()
*/

QDate QDate::addDays( int ndays ) const
{
    QDate d;
    d.jd = jd + ndays;
    return d;
}

/*!
  Returns the number of days from this date to \a d (which is negative
  if \a d is earlier than this date).

  Example:
  \code
    QDate d1( 1995, 5, 17 );		// May 17th 1995
    QDate d2( 1995, 5, 20 );		// May 20th 1995
    d1.daysTo( d2 );			// returns 3
    d2.daysTo( d1 );			// returns -3
  \endcode

  \sa addDays()
*/

int QDate::daysTo( const QDate &d ) const
{
    return d.jd - jd;
}


/*!
  \fn bool QDate::operator==( const QDate &d ) const
  Returns TRUE if this date is equal to \a d, or FALSE if
  they are different.
*/

/*!
  \fn bool QDate::operator!=( const QDate &d ) const
  Returns TRUE if this date is different from \a d, or FALSE if
  they are equal.
*/

/*!
  \fn bool QDate::operator<( const QDate &d ) const
  Returns TRUE if this date is earlier than \a d, otherwise FALSE.
*/

/*!
  \fn bool QDate::operator<=( const QDate &d ) const
  Returns TRUE if this date is earlier than or equal to \a d, otherwise FALSE.
*/

/*!
  \fn bool QDate::operator>( const QDate &d ) const
  Returns TRUE if this date is later than \a d, otherwise FALSE.
*/

/*!
  \fn bool QDate::operator>=( const QDate &d ) const
  Returns TRUE if this date is later than or equal to \a d, otherwise FALSE.
*/


/*!
  Returns the current date, as reported by the system clock.

  \sa QTime::currentTime(), QDateTime::currentDateTime()
*/

QDate QDate::currentDate()
{
#if defined(_OS_WIN32_)

    SYSTEMTIME t;
    GetLocalTime( &t );
    QDate d;
    d.jd = greg2jul( t.wYear, t.wMonth, t.wDay );
    return d;

#else

    time_t ltime;
    time( &ltime );
    tm *t = localtime( &ltime );
    QDate d;
    d.jd = greg2jul( t->tm_year + 1900, t->tm_mon + 1, t->tm_mday );
    return d;

#endif
}

/*!
  Returns TRUE if the specified date (year \a y, month \a m and day \a
  d) is valid.

  Example:
  \code
    QDate::isValid( 2002, 5, 17 );	// TRUE;  May 17th 2002 is OK.
    QDate::isValid( 2002, 2, 30 );	// FALSE; Feb 30th does not exist
    QDate::isValid( 2004, 2, 29 );	// TRUE; 2004 is a leap year
    QDate::isValid( 1202, 6, 6 );	// FALSE; 1202 is pre-Gregorian
  \endcode

  Note that a \a y value in the range 00-99 is interpreted as
  1900-1999.

  \sa isNull(), setYMD()
*/

bool QDate::isValid( int y, int m, int d )
{
    if ( y >= 0 && y <= 99 )
	y += 1900;
    else if ( y < FIRST_YEAR || (y == FIRST_YEAR && (m < 9 ||
						    (m == 9 && d < 14))) )
	return FALSE;
    return (d > 0 && m > 0 && m <= 12) &&
	   (d <= monthDays[m] || (d == 29 && m == 2 && leapYear(y)));
}

/*!
  Returns TRUE if the specified year \a y is a leap year.
*/

bool QDate::leapYear( int y )
{
    return y % 4 == 0 && y % 100 != 0 || y % 400 == 0;
}

/*!
  \internal
  Converts a Gregorian date to a Julian day.
  This algorithm is taken from Communications of the ACM, Vol 6, No 8.
  \sa jul2greg()
*/

uint QDate::greg2jul( int y, int m, int d )
{
    uint c, ya;
    if ( y <= 99 )
	y += 1900;
    if ( m > 2 ) {
	m -= 3;
    } else {
	m += 9;
	y--;
    }
    c = y;					// NOTE: Sym C++ 6.0 bug
    c /= 100;
    ya = y - 100*c;
    return 1721119 + d + (146097*c)/4 + (1461*ya)/4 + (153*m+2)/5;
}

/*!
  \internal
  Converts a Julian day to a Gregorian date.
  This algorithm is taken from Communications of the ACM, Vol 6, No 8.
  \sa greg2jul()
*/

void QDate::jul2greg( uint jd, int &y, int &m, int &d )
{
    uint x;
    uint j = jd - 1721119;
    y = (j*4 - 1)/146097;
    j = j*4 - 146097*y - 1;
    x = j/4;
    j = (x*4 + 3) / 1461;
    y = 100*y + j;
    x = (x*4) + 3 - 1461*j;
    x = (x + 4)/4;
    m = (5*x - 3)/153;
    x = 5*x - 3 - 153*m;
    d = (x + 5)/5;
    if ( m < 10 ) {
	m += 3;
    } else {
	m -= 9;
	y++;
    }
}


/*****************************************************************************
  QTime member functions
 *****************************************************************************/

/*!
  \class QTime qdatetime.h

  \brief The QTime class provides clock time functions.

  \ingroup time

  A QTime object contains a clock time, i.e. a number of hours,
  minutes, seconds and milliseconds since midnight. It can read the
  current time from the system clock, and measure a span of elapsed
  time. It provides functions for comparing times and for manipulating
  a time by adding a number of (milli)seconds.

  QTime operates with 24-hour clock format; it has no concept of
  AM/PM. It operates with local time; it does not know anything about
  time zones or daylight savings time.

  A QTime object is typically created either by giving the number of
  hours, minutes, seconds, and milliseconds explicitly, or by using
  the static function currentTime(), which makes a QTime object which
  contains the system's clock time. Note that the accuracy depends on
  the accuracy of the underlying operating system; not all systems
  provide 1-millisecond accuracy.

  The hour(), minute(), second(), and msec() functions provide access
  to the number of hours, minutes, seconds, and milliseconds of the
  time. The same information is provided in textual format by the
  toString() function.

  QTime provides a full set of operators to compare two QTime
  objects. A time is considered smaller than another if it is earlier
  than the other.

  The time a given number of seconds or milliseconds later than a
  given time can be found using the addSecs() or addMSecs()
  functions. Correspondingly, the number of (milli)seconds between two
  times can be found using the secsTo() or msecsTo() functions.

  QTime can be used to measure a span of elapsed time using the
  start(), restart(), and elapsed() functions.

  \sa QDate, QDateTime
*/

/*!
  \fn QTime::QTime()

  Constructs the time 0 hours, minutes, seconds and milliseconds,
  i.e. 00:00:00.000 (midnight). This is a valid time.

  \sa isValid()
*/

/*!
  Constructs a time with hour \a h, minute \a m, seconds \a s and
  milliseconds \a ms.

  \a h must be in the range 0-23, \a m and \a s must be in the range
  0-59, and \a ms must be in the range 0-999.

  \sa isValid()
*/

QTime::QTime( int h, int m, int s, int ms )
{
    setHMS( h, m, s, ms );
}


/*!
  \fn bool QTime::isNull() const
  Returns TRUE if the time is equal to 00:00:00.000. A null time is valid.

  \sa isValid()
*/

/*!
  Returns TRUE if the time is valid, or FALSE if the time is invalid.
  The time 23:30:55.746 is valid, while 24:12:30 is invalid.

  \sa isNull()
*/

bool QTime::isValid() const
{
    return ds < MSECS_PER_DAY;
}


/*!
  Returns the hour part (0..23) of the time.
*/

int QTime::hour() const
{
    return ds / MSECS_PER_HOUR;
}

/*!
  Returns the minute part (0..59) of the time.
*/

int QTime::minute() const
{
    return (ds % MSECS_PER_HOUR)/MSECS_PER_MIN;
}

/*!
  Returns the second part (0..59) of the time.
*/

int QTime::second() const
{
    return (ds / 1000)%SECS_PER_MIN;
}

/*!
  Returns the millisecond part (0..999) of the time.
*/

int QTime::msec() const
{
    return ds % 1000;
}


/*!
  Returns the time of this object in a textual format. Milliseconds
  are not included. The string format is HH:MM:SS, e.g. 1 second
  before midnight would be "23:59:59".
*/

QString QTime::toString() const
{
    QString buf;
    buf.sprintf( "%.2d:%.2d:%.2d", hour(), minute(), second() );
    return buf;
}


/*!
  Sets the time to hour \a h, minute \a m, seconds \a s and
  milliseconds \a ms.

  \a h must be in the range 0-23, \a m and \a s must be in the range
  0-59, and \a ms must be in the range 0-999. Returns TRUE if the set
  time is valid, otherwise FALSE.

  \sa isValid()
*/

bool QTime::setHMS( int h, int m, int s, int ms )
{
    if ( !isValid(h,m,s,ms) ) {
#if defined(CHECK_RANGE)
	qWarning( "QTime::setHMS Invalid time %02d:%02d:%02d.%03d", h, m, s,
		 ms );
#endif
	ds = MSECS_PER_DAY;		// make this invalid
	return FALSE;
    }
    ds = (h*SECS_PER_HOUR + m*SECS_PER_MIN + s)*1000 + ms;
    return TRUE;
}

/*!
  Returns a QTime object containing a time \a nsecs seconds later than
  the time of this object (or earlier if \a ms is negative).

  Note that the time will wrap if it passes midnight.

  Example:
  \code
    QTime n( 14, 0, 0 );                // n == 14:00:00
    QTime t;
    t = n.addSecs( 70 );                // t == 14:01:10
    t = n.addSecs( -70 );               // t == 13:58:50
    t = n.addSecs( 10*60*60 + 5 );      // t == 00:00:05
    t = n.addSecs( -15*60*60 );         // t == 23:00:00
  \endcode

  \sa addMSecs(), secsTo(), QDateTime::addSecs()
*/

QTime QTime::addSecs( int nsecs ) const
{
    return addMSecs(nsecs*1000);
}

/*!
  Returns the number of seconds from this time to \a t (which is
  negative if \a t is earlier than this time).

  Since QTime measures time within a day and there are 86400 seconds
  in a day, the result is between -86400 and 86400.

  \sa addSecs() QDateTime::secsTo()
*/

int QTime::secsTo( const QTime &t ) const
{
    return ((int)t.ds - (int)ds)/1000;
}

/*!
  Returns a QTime object containing a time \a ms milliseconds later than
  the time of this object (or earlier if \a ms is negative).

  Note that the time will wrap if it passes midnight. See addSecs()
  for an example.

  \sa addSecs(), msecsTo()
*/

QTime QTime::addMSecs( int ms ) const
{
    QTime t;
    if ( ms < 0 ) {
	// % not well-defined for -ve, but / is.
	int negdays = (MSECS_PER_DAY-ms) / MSECS_PER_DAY;
	t.ds = ((int)ds + ms + negdays*MSECS_PER_DAY)
		% MSECS_PER_DAY;
    } else {
	t.ds = ((int)ds + ms) % MSECS_PER_DAY;
    }
    return t;
}

/*!
  Returns the number of milliseconds from this time to \a t (which is
  negative if \a t is earlier than this time).

  Since QTime measures time within a day and there are 86400000
  milliseconds in a day, the result is between -86400000 and 86400000.

  \sa secsTo()
*/

int QTime::msecsTo( const QTime &t ) const
{
    return (int)t.ds - (int)ds;
}


/*!
  \fn bool QTime::operator==( const QTime &t ) const

  Returns TRUE if this time is equal to \a t, or FALSE if they are
  different.
*/

/*!
  \fn bool QTime::operator!=( const QTime &t ) const

  Returns TRUE if this time is different from \a t, or FALSE if they
  are equal.
*/

/*!
  \fn bool QTime::operator<( const QTime &t ) const

  Returns TRUE if this time is earlier than \a t, otherwise FALSE.
*/

/*!
  \fn bool QTime::operator<=( const QTime &t ) const

  Returns TRUE if this time is earlier than or equal to \a t,
  otherwise FALSE.
*/

/*!
  \fn bool QTime::operator>( const QTime &t ) const

  Returns TRUE if this time is later than \a t, otherwise FALSE.
*/

/*!
  \fn bool QTime::operator>=( const QTime &t ) const

  Returns TRUE if this time is later than or equal to \a t, otherwise
  FALSE.
*/



/*!
  Returns the current time, as reported by the system clock.

  Note that the accuracy depends on the accuracy of the underlying
  operating system; not all systems provide 1-millisecond accuracy.
*/

QTime QTime::currentTime()
{
    QTime ct;
    currentTime( &ct );
    return ct;
}

/*!
  \internal

  Fetches the current time and returns TRUE if the time is within one
  minute after midnight, otherwise FALSE. The return value is used by
  QDateTime::currentDateTime() to ensure that the date there is correct.
*/

bool QTime::currentTime( QTime *ct )
{
    if ( !ct ) {
#if defined(CHECK_NULL)
	qWarning( "QTime::currentTime(QTime *): Null pointer not allowed" );
#endif
	return FALSE;
    }

#if defined(_OS_WIN32_)

    SYSTEMTIME t;
    GetLocalTime( &t );
    ct->ds = MSECS_PER_HOUR*t.wHour + MSECS_PER_MIN*t.wMinute +
	     1000*t.wSecond + t.wMilliseconds;
    return (t.wHour == 0 && t.wMinute == 0);

#elif defined(_OS_OS2_)

    DATETIME t;
    DosGetDateTime( &t );
    ct->ds = MSECS_PER_HOUR*t.hours + MSECS_PER_MIN*t.minutes +
	     1000*t.seconds + 10*t.hundredths;
    return (t.hours == 0 && t.minutes == 0);

#elif defined(_OS_MSDOS_)

    _dostime_t t;
    _dos_gettime( &t );
    ct->ds = MSECS_PER_HOUR*t.hour + MSECS_PER_MIN*t.minute +
	     t.second*1000 + t.hsecond*10;
    return (t.hour== 0 && t.minute == 0);

#elif defined(_OS_UNIX_)

    struct timeval tv;
    gettimeofday( &tv, 0 );
    time_t ltime = tv.tv_sec;
    tm *t = localtime( &ltime );
    ct->ds = (uint)( MSECS_PER_HOUR*t->tm_hour + MSECS_PER_MIN*t->tm_min +
		     1000*t->tm_sec + tv.tv_usec/1000 );
    return (t->tm_hour== 0 && t->tm_min == 0);

#else

    time_t ltime;			// no millisecond resolution!!
    ::time( &ltime );
    tm *t = localtime( &ltime );
    ct->ds = MSECS_PER_HOUR*t->tm_hour + MSECS_PER_MIN*t->tm_min +
	     1000*t->tm_sec;
    return (t->tm_hour== 0 && t->tm_min == 0);
#endif
}

/*!
  Returns TRUE if the specified time is valid, otherwise FALSE.

  The time is valid if \a h is in the range 0-23, \a m and \a s are in
  the range 0-59, and \a ms is in the range 0-999.

  Example:
  \code
    QTime::isValid(21, 10, 30);		// returns TRUE
    QTime::isValid(22, 5,  62);		// returns FALSE
  \endcode
*/

bool QTime::isValid( int h, int m, int s, int ms )
{
    return (uint)h < 24 && (uint)m < 60 && (uint)s < 60 && (uint)ms < 1000;
}


/*!
  Sets this time to the current time. This is practical for timing:

  \code
    QTime t;
    t.start();				// start clock
    ... // some lengthy task
    qDebug( "%d\n", t.elapsed() );	// prints # msecs elapsed
  \endcode

  \sa restart(), elapsed(), currentTime()
*/

void QTime::start()
{
    *this = currentTime();
}

/*!
  Sets this time to the current time, and returns the number of
  milliseconds that have elapsed since the last time start() or
  restart() was called.

  This function is guaranteed to be atomic, and is thus very handy for
  repeated measurements: call start() to start the first measurement,
  then restart() for each later measurement.

  Note that the counter wraps to zero 24 hours after the last call to
  start() or restart().

  \warning If the system's clock setting has been changed since the
  last time start() or restart() was called, the result is undefined.
  This can happen e.g. when daylight saving is turned on or off.

  \sa start(), elapsed(), currentTime()
*/

int QTime::restart()
{
    QTime t = currentTime();
    int n = msecsTo( t );
    if ( n < 0 )				// passed midnight
	n += 86400*1000;
    *this = t;
    return n;
}

/*!
  Returns the number of milliseconds that have elapsed since the last
  time start() or restart() was called.

  Note that the counter wraps to zero 24 hours after the last call to
  start() or restart.

  Note that the accuracy depends on the accuracy of the underlying
  operating system; not all systems provide 1-millisecond accuracy.

  \warning If the system's clock setting has been changed since the
  last time start() or restart() was called, the result is undefined.
  This can happen e.g. when daylight saving is turned on or off.

  \sa start(), restart()
*/

int QTime::elapsed()
{
    int n = msecsTo( currentTime() );
    if ( n < 0 )				// passed midnight
	n += 86400*1000;
    return n;
}


/*****************************************************************************
  QDateTime member functions
 *****************************************************************************/

/*!
  \class QDateTime qdatetime.h
  \brief The QDateTime class provides date and time functions.

  \ingroup time

  A QDateTime object contains a calendar date and a clock time (a
  "datetime"). It is a combination of the QDate and QTime classes. It
  can read the current datetime from the system clock. It provides
  functions for comparing datetimes and for manipulating a datetime by
  adding a number of seconds or days.

  A QDateTime object is typically created either by giving a date and
  time explicitly, or by using the static function currentTime(),
  which makes a QDateTime object which contains the system's clock
  time.

  The date() and time() functions provide access to the date and time
  parts of the datetime. The same information is provided in textual
  format by the toString() function.

  QDateTime provides a full set of operators to compare two QDateTime
  objects. A datetime is considered smaller than another if it is
  earlier than the other.

  The datetime a given number of days or seconds later than a given
  datetime can be found using the addDays() and addSecs()
  functions. Correspondingly, the number of days or seconds between
  two times can be found using the daysTo() or secsTo() functions.

  A datetime can also be set using the setTime_t() function, which
  takes a POSIX-standard "number of seconds since 00:00:00 on January
  1, 1970" value.

  The limitations regarding range and resolution mentioned in the
  QDate and QTime documentation apply for QDateTime also.

  \sa QDate, QTime
*/


/*!
  \fn QDateTime::QDateTime()

  Constructs a null datetime (i.e. null date and null time).  A null
  datetime is invalid, since the date is invalid.

  \sa isValid()
*/


/*!
  Constructs a datetime with date \a date and null time (00:00:00.000).
*/

QDateTime::QDateTime( const QDate &date )
    : d(date)
{
}

/*!
  Constructs a datetime with date \a date and time \a time.
*/

QDateTime::QDateTime( const QDate &date, const QTime &time )
    : d(date), t(time)
{
}


/*!
  \fn bool QDateTime::isNull() const

  Returns TRUE if both the date and the time are null.	A null date is invalid.

  \sa QDate::isNull(), QTime::isNull()
*/

/*!
  \fn bool QDateTime::isValid() const

  Returns TRUE if both the date and the time are valid.

  \sa QDate::isValid(), QTime::isValid()
*/

/*!
  \fn QDate QDateTime::date() const

  Returns the date part of this datetime.

  \sa setDate(), time()
*/

/*!
  \fn QTime QDateTime::time() const

  Returns the time part of this datetime.

  \sa setTime(), date()
*/

/*!
  \fn void QDateTime::setDate( const QDate &date )

  Sets the date part of this datetime.

  \sa date(), setTime()
*/

/*!
  \fn void QDateTime::setTime( const QTime &time )

  Sets the time part of this datetime.

  \sa time(), setDate()
*/


/*!
  Sets the local date and time given the number of seconds that have passed
  since 00:00:00 on January 1, 1970, Coordinated Universal Time (UTC).
  On systems that do not support timezones this function will behave as if
  local time were UTC.

  Note that Microsoft Windows supports only a limited range of values for
  \a secsSince1Jan1970UTC.
*/

void QDateTime::setTime_t( uint secsSince1Jan1970UTC )
{
    time_t tmp = (time_t) secsSince1Jan1970UTC;
    tm *tM = localtime( &tmp );
    if ( !tM ) {
	tM = gmtime( &tmp );
	if ( !tM ) {
	    d.jd = QDate::greg2jul( 1970, 1, 1 );
	    t.ds = 0;
	    return;
	}
    }
    d.jd = QDate::greg2jul( tM->tm_year + 1900, tM->tm_mon + 1, tM->tm_mday );
    t.ds = MSECS_PER_HOUR*tM->tm_hour + MSECS_PER_MIN*tM->tm_min +
	    1000*tM->tm_sec;
}


/*!
  Returns the datetime as a string.

  The string format is "Sat May 20 03:40:13 1998".

  This function uses QDate::dayName(), QDate::monthName(), and
  QTime::toString() to generate the string.

*/

QString QDateTime::toString() const
{
    QString buf = d.dayName(d.dayOfWeek());
    buf += ' ';
    buf += d.monthName(d.month());
    buf += ' ';
    buf += QString().setNum(d.day());
    buf += ' ';
    buf += t.toString();
    buf += ' ';
    buf += QString().setNum(d.year());
    return buf;
}

/*!
  Returns a QDateTime object containing a datetime \a ndays days later
  than the datetime of this object (or earlier if \a ndays is
  negative).

  \sa daysTo(), addSecs()
*/

QDateTime QDateTime::addDays( int ndays ) const
{
    return QDateTime( d.addDays(ndays), t );
}

/*!
  Returns a QDateTime object containing a datetime \a nsecs seconds
  later than the datetime of this object (or earlier if \a nsecs is
  negative).

  \sa secsTo(), addDays()
*/

QDateTime QDateTime::addSecs( int nsecs ) const
{
    uint dd = d.jd;
    int  tt = t.ds;
    int  sign = 1;
    if ( nsecs < 0 ) {
	nsecs = -nsecs;
	sign = -1;
    }
    if ( nsecs >= (int)SECS_PER_DAY ) {
	dd += sign*(nsecs/SECS_PER_DAY);
	nsecs %= SECS_PER_DAY;
    }
    tt += sign*nsecs*1000;
    if ( tt < 0 ) {
	tt = MSECS_PER_DAY - tt - 1;
	dd -= tt / MSECS_PER_DAY;
	tt = tt % MSECS_PER_DAY;
	tt = MSECS_PER_DAY - tt - 1;
    } else if ( tt >= (int)MSECS_PER_DAY ) {
	dd += ( tt / MSECS_PER_DAY );
	tt = tt % MSECS_PER_DAY;
    }
    QDateTime ret;
    ret.t.ds = tt;
    ret.d.jd = dd;
    return ret;
}

/*!
  Returns the number of days from this datetime to \a dt (which is
  negative if \a dt is earlier than this datetime).

  \sa addDays(), secsTo()
*/

int QDateTime::daysTo( const QDateTime &dt ) const
{
    return d.daysTo( dt.d );
}

/*!
  Returns the number of seconds from this datetime to \a dt (which is
  negative if \a dt is earlier than this datetime).

  Example:
  \code
    QDateTime dt = QDateTime::currentDateTime();
    QDateTime x( QDate(dt.year(),12,24), QTime(17,00) );
    qDebug( "There are %d seconds to Christmas", dt.secsTo(x) );
  \endcode

  \sa addSecs(), daysTo(), QTime::secsTo()
*/

int QDateTime::secsTo( const QDateTime &dt ) const
{
    return t.secsTo(dt.t) + d.daysTo(dt.d)*SECS_PER_DAY;
}


/*!
  Returns TRUE if this datetime is equal to \a dt, or FALSE if
  they are different.
  \sa operator!=()
*/

bool QDateTime::operator==( const QDateTime &dt ) const
{
    return  t == dt.t && d == dt.d;
}

/*!
  Returns TRUE if this datetime is different from \a dt, or FALSE if
  they are equal.
  \sa operator==()
*/

bool QDateTime::operator!=( const QDateTime &dt ) const
{
    return  t != dt.t || d != dt.d;
}

/*!
  Returns TRUE if this datetime is earlier than \a dt, otherwise FALSE.
*/

bool QDateTime::operator<( const QDateTime &dt ) const
{
    if ( d < dt.d )
	return TRUE;
    return d == dt.d ? t < dt.t : FALSE;
}

/*!
  Returns TRUE if this datetime is earlier than or equal to \a dt,
  otherwise FALSE.
*/

bool QDateTime::operator<=( const QDateTime &dt ) const
{
    if ( d < dt.d )
	return TRUE;
    return d == dt.d ? t <= dt.t : FALSE;
}

/*!
  Returns TRUE if this datetime is later than \a dt, otherwise FALSE.
*/

bool QDateTime::operator>( const QDateTime &dt ) const
{
    if ( d > dt.d )
	return TRUE;
    return d == dt.d ? t > dt.t : FALSE;
}

/*!
  Returns TRUE if this datetime is later than or equal to \a dt,
  otherwise FALSE.
*/

bool QDateTime::operator>=( const QDateTime &dt ) const
{
    if ( d > dt.d )
	return TRUE;
    return d == dt.d ? t >= dt.t : FALSE;
}

/*!
  Returns the current datetime, as reported by the system clock.

  \sa QDate::currentDate(), QTime::currentTime()
*/

QDateTime QDateTime::currentDateTime()
{
    QDate cd = QDate::currentDate();
    QTime ct;
    if ( QTime::currentTime(&ct) )		// too close to midnight?
	cd = QDate::currentDate();		// YES! time for some midnight
						// voodoo, fetch date again
    return QDateTime( cd, ct );
}


/*****************************************************************************
  Date/time stream functions
 *****************************************************************************/

#ifndef QT_NO_DATASTREAM
/*!
  \relates QDate
  Writes the date to the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator<<( QDataStream &s, const QDate &d )
{
    return s << (Q_UINT32)(d.jd);
}

/*!
  \relates QDate
  Reads a date from the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator>>( QDataStream &s, QDate &d )
{
    Q_UINT32 jd;
    s >> jd;
    d.jd = jd;
    return s;
}

/*!
  \relates QTime
  Writes a time to the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator<<( QDataStream &s, const QTime &t )
{
    return s << (Q_UINT32)(t.ds);
}

/*!
  \relates QTime
  Reads a time from the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator>>( QDataStream &s, QTime &t )
{
    Q_UINT32 ds;
    s >> ds;
    t.ds = ds;
    return s;
}

/*!
  \relates QDateTime
  Writes a datetime to the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator<<( QDataStream &s, const QDateTime &dt )
{
    return s << dt.d << dt.t;
}

/*!
  \relates QDateTime
  Reads a datetime from the stream.

  \sa \link datastreamformat.html Format of the QDataStream operators \endlink
*/

QDataStream &operator>>( QDataStream &s, QDateTime &dt )
{
    s >> dt.d >> dt.t;
    return s;
}
#endif //QT_NO_DATASTREAM
