/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <Foundation/Foundation.h>
#include <qdatetime.h>
#include <time.h>

#ifndef USING_BORROWED_QDATETIME

// QTime class ===============================================================

// constructors, copy constructors, and destructors ----------------------------

QTime::QTime()
{
    timeMS = 0;
}    

QTime::QTime(int hours, int minutes)
{
    timeMS = (hours*3600 + minutes*60)*1000; 
}

QTime::QTime(const QTime &newTime)
{
    timeMS = newTime.timeMS;
}


// member functions --------------------------------------------------------

bool QTime::isNull() const
{
    return timeMS == 0;
}

void QTime::start()
{
    setCurrentTime();
}

int QTime::hour() const
{
    return timeMS / 3600000;
}


int QTime::minute() const
{
    return (timeMS % 3600000)/60000;
}


int QTime::second() const
{
    return (timeMS / 1000) % 60;
}



int QTime::msec() const
{
    return timeMS % 1000;
}

int QTime::elapsed()
{
    uint currentTime, elapsedTime;
    
    currentTime = getCurrentTime();
    elapsedTime = currentTime - timeMS;
    if(elapsedTime < 0) elapsedTime = elapsedTime + 86400000; // watch out for the next day
    
    return elapsedTime;
}


int QTime::restart()
{
    uint currentTime, elapsedTime;
    
    currentTime = getCurrentTime();
    elapsedTime = currentTime - timeMS;
    if(elapsedTime < 0){
        elapsedTime = elapsedTime + 86400000; // watch out for the next day
    }
    timeMS = currentTime;
    
    return elapsedTime;

}

int QTime::secsTo( const QTime &time ) const
{
    return ((int)time.timeMS - (int)timeMS)/1000;
}

// private ---------------------------------------------------------------------

void QTime::setCurrentTime()
{
    time_t currentTime;
    tm *localTime;
    
    time(&currentTime);
    localTime = localtime(&currentTime);
    timeMS =  (localTime->tm_hour*3600 + localTime->tm_min*60 + localTime->tm_sec) * 1000;
}

uint QTime::getCurrentTime()
{
    time_t currentTime;
    tm *localTime;
    
    time(&currentTime);
    localTime = localtime(&currentTime);
    return (localTime->tm_hour*3600 + localTime->tm_min*60 + localTime->tm_sec) * 1000;
}




// class QDate =================================================================

QDate::QDate()
{
 // do nothing
}

QDate::QDate(int year, int month, int day)
{
    dateDays = greg2jul(year, month, day);
}


int QDate::year() const
{
    int year, month, day;
    
    jul2greg(dateDays, year, month, day);
    
    return year;
}


int QDate::month() const
{
    int year, month, day;
    
    jul2greg(dateDays, year, month, day);
    
    return month;
}


int QDate::day() const
{
    int year, month, day;
    
    jul2greg(dateDays, year, month, day);
    
    return day;
}

int QDate::daysTo( const QDate &date ) const
{
    return date.dateDays - dateDays;
}


// member functions --------------------------------------------------------
// private -----------------------------------------------------------------
void QDate::setCurrentDate()
{
    time_t currentDate;
    tm *localDate;
    
    time(&currentDate);
    localDate = localtime(&currentDate);
    dateDays = greg2jul(localDate->tm_year + 1900, localDate->tm_mon + 1, localDate->tm_mday);
}

// greg2jul and jul2greg algorithms are copied from Communications of the ACM, Vol 6, No 8
// variable names changed slightly from _qdatetime.cpp


uint QDate::greg2jul( int year, int month, int day ) const
{
    uint c, ya;
    
    if ( year <= 99 ){
	year += 1900;
    }
    if ( month > 2 ) {
	month -= 3;
    } else {
	month += 9;
	year--;
    }
    c = year;					
    c /= 100;
    ya = year - 100*c;
    return 1721119 + day + (146097*c)/4 + (1461*ya)/4 + (153*month+2)/5;
}

void QDate::jul2greg( uint ms, int &year, int &month, int &day ) const
{
    uint x, julian;
    
    julian = ms - 1721119;
    year = (julian*4 - 1)/146097;
    julian = julian*4 - 146097*year - 1;
    x = julian/4;
    julian = (x*4 + 3) / 1461;
    year = 100*year + julian;
    x = (x*4) + 3 - 1461*julian;
    x = (x + 4)/4;
    month = (5*x - 3)/153;
    x = 5*x - 3 - 153*month;
    day = (x + 5)/5;
    if ( month < 10 ) {
	month += 3;
    } else {
	month -= 9;
	year++;
    }
}


// class QDateTime =============================================================


// static member functions -------------------------------------------------

QDateTime QDateTime::currentDateTime()
{
    QTime currentTime;
    QDate currentDate;
    
    currentTime.setCurrentTime();
    currentDate.setCurrentDate();
    return QDateTime( currentDate, currentTime );
}

// constructors, copy constructors, and destructors ------------------------

QDateTime::QDateTime()
{
 //do nothing
}

QDateTime::QDateTime(const QDate &newDate, const QTime &newTime)
{
    dateDT = newDate;
    timeDT = newTime;
}

QDateTime::QDateTime(const QDateTime &newDateTime)
{
    dateDT = newDateTime.dateDT;
    timeDT = newDateTime.timeDT;
}


// member functions --------------------------------------------------------

int QDateTime::secsTo( const QDateTime &newerDateTime ) const
{
    return timeDT.secsTo(newerDateTime.timeDT) + dateDT.daysTo(newerDateTime.dateDT)*86400;
}

QTime QDateTime::time() const
{
    return timeDT;
}

//FIX ME: this looks too much like qt's setTime_t

void QDateTime::setTime_t( uint secsSince1Jan1970UTC )
{
    time_t myTime = (time_t) secsSince1Jan1970UTC;
    tm *localTime;
    
    localTime = localtime( &myTime );
    if (!localTime) {
	localTime = gmtime( &myTime );
	if ( !localTime ) {
	    dateDT.dateDays = dateDT.greg2jul( 1970, 1, 1);
	    timeDT.timeMS = 0;
	    return;
	}
    }
    dateDT.dateDays = dateDT.greg2jul(localTime->tm_year + 1900, localTime->tm_mon + 1, localTime->tm_mday );
    timeDT.timeMS = 3600000*localTime->tm_hour + 60000*localTime->tm_min + 1000*localTime->tm_sec;
}


ostream &operator<<(ostream &o, const QDate &date)
{
    return o <<
        "QDate: [yy/mm/dd: " <<
        date.year() <<
        '/' <<
        date.month() <<
        '/' <<
        date.day() <<
        ']';
}

ostream &operator<<(ostream &o, const QTime &time)
{
    return o <<
        "QTime: [hh:mm:ss:ms = " <<
        time.hour() <<
        ':' <<
        time.minute() <<
        ':' <<
        time.second() <<
        ':' <<
        time.msec() <<
        ']';
}

ostream &operator<<(ostream &o, const QDateTime &dateTime)
{
    return o <<
        "QDateTime: [yy/mm/dd hh:mm:ss:ms = " <<
        dateTime.dateDT.year() <<
        '/' <<
        dateTime.dateDT.month() <<
        '/' <<
        dateTime.dateDT.day() <<
        ' ' << 
        dateTime.timeDT.hour() <<
        ':' <<
        dateTime.timeDT.minute() <<
        ':' <<
        dateTime.timeDT.second() <<
        ':' <<
        dateTime.timeDT.msec() <<
        ']';
}

#endif // USING_BORROWED_QDATETIME
