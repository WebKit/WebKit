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

#ifndef JOBCLASSES_H_
#define JOBCLASSES_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kurl.h>
#include <qobject.h>
#include <qstring.h>

#ifdef _KWQ_
#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
#include <WCURLHandle.h>
#endif
#endif


namespace KIO {

class TransferJobPrivate;

// class Job ===================================================================

class Job : public QObject {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    Job() {}
#endif

    virtual ~Job();

    // member functions --------------------------------------------------------

    virtual int error();
    const QString & errorText();
    QString errorString();
    virtual void kill(bool quietly=TRUE);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    Job(const Job &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    Job &operator=(const Job &);
#endif

}; // class Job ================================================================


// class SimpleJob =============================================================

class SimpleJob : public KIO::Job {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    SimpleJob() {}
#endif

    ~SimpleJob();

    // member functions --------------------------------------------------------
    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    SimpleJob(const SimpleJob &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    SimpleJob &operator=(const SimpleJob &);
#endif

}; // class SimpleJob ==========================================================


// class TransferJob ===========================================================

class TransferJob : public SimpleJob {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------
    
    TransferJob(const KURL &, bool reload=false, bool showProgressInfo=true);

// add no-arg constructor
#ifdef _KWQ_PEDANTIC_
    TransferJob() {}
#endif

    ~TransferJob();

    // member functions --------------------------------------------------------

    int error();
    bool isErrorPage() const;
    QString queryMetaData(const QString &key);
    void addMetaData(const QString &key, const QString &value);
    void kill(bool quietly=TRUE);

#ifdef _KWQ_
#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    void begin(id <WCURLHandleClient> client, void *userData);
#else
    void begin(void *requestor, void *userData);
#endif
#endif

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

#ifdef _KWQ_
private:
    KURL _url;
    bool _reload;
    bool _showProgressInfo;
    int _status;
    TransferJobPrivate *d;
#endif

// add copy constructor
// this private declaration prevents copying
#ifdef _KWQ_PEDANTIC_
    TransferJob(const TransferJob &);
#endif

// add assignment operator 
// this private declaration prevents assignment
#ifdef _KWQ_PEDANTIC_
    TransferJob &operator=(const TransferJob &);
#endif

}; // class TransferJob ========================================================


} // namespace KIO

#endif
