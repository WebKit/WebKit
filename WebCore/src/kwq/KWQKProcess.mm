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

#include <kwqdebug.h>
#include <kprocess.h>

KProcess::KProcess()
{
    _logNotYetImplemented();
}


KProcess::~KProcess()
{
    _logNotYetImplemented();
}


QValueList<QCString> KProcess::args()
{
    _logNotYetImplemented();
    return QValueList<QCString>();
}


bool KProcess::isRunning() const
{
    _logNotYetImplemented();
    return FALSE;    
}


bool KProcess::writeStdin(const char *buffer, int buflen)
{
    _logNotYetImplemented();
    return FALSE;    
}


bool KProcess::start(RunMode runmode, Communication comm)
{
    _logNotYetImplemented();
    return FALSE;    
}


bool KProcess::kill(int signo=SIGTERM)
{
    _logNotYetImplemented();
    return FALSE;    
}


void KProcess::resume()
{
    _logNotYetImplemented();
}


KProcess &KProcess::operator<<(const QString& arg)
{
    _logNotYetImplemented();
    return *this;
}
