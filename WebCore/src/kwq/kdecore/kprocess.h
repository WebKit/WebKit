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

#ifndef KPROCESS_H_
#define KPROCESS_H_

#include <signal.h>

#include <qobject.h>
#include <qstring.h>
#include <KWQStrList.h>

// class KProcess ==============================================================

class KProcess : public QObject {
public:

    // structs -----------------------------------------------------------------
    
    // typedefs ----------------------------------------------------------------

    enum Communication { NoCommunication = 0, Stdin = 1, Stdout = 2, NoRead };
    enum RunMode { DontCare, NotifyOnExit, Block };

    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    KProcess();
    
    ~KProcess();
    
    // member functions --------------------------------------------------------

    QStrList *args();
    bool isRunning() const;
    bool writeStdin(const char *buffer, int buflen);
    virtual bool start(RunMode runmode=NotifyOnExit, 
        Communication comm=NoCommunication);
    virtual bool kill(int signo=SIGTERM);
    void resume();

    // operators ---------------------------------------------------------------

    KProcess &operator<<(const QString& arg);

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    KProcess(const KProcess &);
    KProcess &operator=(const KProcess &);

}; // class KProcess ===========================================================

#endif
