/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#ifndef QDATETIME_H_
#define QDATETIME_H_

#include <KWQDef.h>

#ifdef _KWQ_IOSTREAM_
#include <iosfwd>
#endif

class QTime {
public:
    QTime();
    QTime(int, int);

    bool isNull() const;
    int hour() const;
    int minute() const;
    int second() const;
    int msec() const;
    void start();
    int elapsed();
    int restart();
    int secsTo( const QTime & ) const;
    
private:
    uint getCurrentTime();
    void setCurrentTime();
    
    uint timeMS;  // time is stored in milliseconds 
    
    friend class QDateTime;
#ifdef _KWQ_IOSTREAM_
    friend std::ostream &operator<<( std::ostream &, const QTime & );
#endif
    
};

class QDate {
public:

    QDate();
    QDate(int, int, int);
        
    int	   year() const;
    int	   month() const;
    int	   day() const;	
    
    int daysTo( const QDate & ) const;
    
protected:
    uint greg2jul( int, int, int ) const;
    void jul2greg( uint jd, int &y, int &m, int &d ) const;

private:

    int dateDays; //date is stored in days
    
    void setCurrentDate();
    
    friend class QDateTime;
#ifdef _KWQ_IOSTREAM_
    friend std::ostream &operator<<( std::ostream &, const QDate & );
#endif

};

class QDateTime {
public:

    static QDateTime currentDateTime();

    QDateTime();
    QDateTime(const QDate &, const QTime &);
    
    int secsTo(const QDateTime &) const;
    QTime time() const;
    void   setTime_t( uint );
    
private:
    QTime timeDT;
    QDate dateDT;

#ifdef _KWQ_IOSTREAM_
    friend std::ostream &operator<<( std::ostream &, const QDateTime & );
#endif
};

#endif
