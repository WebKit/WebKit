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

#ifndef QASYNCIO_H_
#define QASYNCIO_H_

#include <KWQDef.h>

// class QAsyncIO ==============================================================

class QAsyncIO {

// Note : all class members in protected scope
protected:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    QAsyncIO() {};
    virtual ~QAsyncIO();

    // member functions --------------------------------------------------------

    void ready();

    // operators ---------------------------------------------------------------

// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QAsyncIO(const QAsyncIO &);
    QAsyncIO &operator=(const QAsyncIO &);

}; // class QAsyncIO ===========================================================


// class QDataSource ===========================================================

class QDataSource : public QAsyncIO {
public:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QDataSource() {}
    ~QDataSource() {}
    
    // member functions --------------------------------------------------------

    virtual void rewind();
    void maybeReady();

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QDataSource(const QDataSource &);
    QDataSource &operator=(const QDataSource &);

}; // end class QDataSource ====================================================


// class QDataSink =============================================================

class QDataSink : public QAsyncIO {
public:

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QDataSink() {}
    ~QDataSink() {}
    
    // member functions --------------------------------------------------------

    virtual void receive(const uchar*, int count)=0;

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QDataSink(const QDataSink &);
    QDataSink &operator=(const QDataSink &);

}; // class QDataSink ==========================================================

#endif
