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

#ifndef QDATETIME_H_
#define QDATETIME_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// USING_BORROWED_QDATETIME ====================================================

#ifdef USING_BORROWED_QDATETIME
#include <_qdatetime.h>
#else

#include <iostream>

// class QTime =================================================================

class QTime {
public:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QTime();
    QTime(int, int);

// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QTime() {}
#endif

    // member functions --------------------------------------------------------

    bool isNull() const;
    int hour() const;
    int minute() const;
    int second() const;
    int msec() const;
    void start();
    int elapsed();
    int restart();
    int secsTo( const QTime & ) const;
    
    // operators ---------------------------------------------------------------

    //QTime &operator=(const QTime &);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    
    uint getCurrentTime();
    void setCurrentTime();
    uint timeMS;  // time is stored in milliseconds 
    
    friend class QDateTime;
    friend std::ostream &operator<<( std::ostream &, const QTime & );
    
// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QTime(const QTime &);
#endif
    
}; // class QTime ==============================================================


// class QDate =================================================================

class QDate {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------
    QDate();
    QDate(int, int, int);

    
// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QDate() {}
#endif

    // member functions --------------------------------------------------------
        
    int	   year() const;
    int	   month() const;
    int	   day() const;	
    
    int daysTo( const QDate & ) const;
    
    // operators ---------------------------------------------------------------

   //QDate &operator=(const QDate &);
    
// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

protected:
    uint greg2jul( int, int, int ) const;
    void jul2greg( uint jd, int &y, int &m, int &d ) const;

private:

    int dateDays; //date is stored in days
    
    void setCurrentDate();
    
    friend class QDateTime;
    friend std::ostream &operator<<( std::ostream &, const QDate & );
    
// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    QDate(const QDate &);
#endif

}; // class QDate ==============================================================


// class QDateTime =============================================================

class QDateTime {
public:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    static QDateTime currentDateTime();

    // constructors, copy constructors, and destructors ------------------------

    QDateTime();
    QDateTime(const QDateTime &);
    QDateTime(const QDate &, const QTime &);
    
// add no-op destructor
#ifdef _KWQ_PEDANTIC_
    ~QDateTime() {}
#endif

    // member functions --------------------------------------------------------

    int secsTo(const QDateTime &) const;
    QTime time() const;
    void   setTime_t( uint );
    
    // operators ---------------------------------------------------------------

    // this is not declared in the code, although assignment of this type
    // is used in the code... i guess a pointer copy is what they want
    //
    //QDateTime &operator=(const QDateTime &);
    //

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
private:
    QTime timeDT;
    QDate dateDT;


    friend std::ostream &operator<<( std::ostream &, const QDateTime & );
}; // class QDateTime ==========================================================

#endif // USING_BORROWED_QDATETIME

#endif
