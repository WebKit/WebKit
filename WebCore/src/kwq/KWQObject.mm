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

#include <qobject.h>

bool QObject::connect(const QObject *, const char *, const QObject *, 
    const char *)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) bool QObject::connect\n");
}


bool QObject::disconnect( const QObject *, const char *, const QObject *, 
    const char *)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) bool QObject::disconnect\n");
}


QObject::QObject(QObject *parent=0, const char *name=0)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) QObject::QObject(QObject *parent=0, const char *name=0)\n");
}


QObject::~QObject()
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) QObject::~QObject()\n");
}


// member functions --------------------------------------------------------

const char *QObject::name() const
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) const char *QObject::name() const\n");
}


void QObject::setName(const char *)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) void QObject::setName(const char *)\n");
}

#ifdef DO_QVARIANT
QVariant QObject::property(const char *name) const
{
}
#endif

bool QObject::inherits(const char *) const
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) bool QObject::inherits(const char *) const\n");
}


bool QObject::connect(const QObject *, const char *, const char *) const
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) bool QObject::connect(const QObject *, const char *, const char *) const\n");
}


int QObject:: startTimer(int)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) int QObject:: startTimer(int)\n");
}


void QObject::killTimer(int)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) void QObject::killTimer(int)\n");
}


void QObject::killTimers()
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) void QObject::killTimers()\n");
}


void QObject::installEventFilter(const QObject *)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) void QObject::installEventFilter(const QObject *)\n");
}


void QObject::removeEventFilter(const QObject *)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) void QObject::removeEventFilter(const QObject *)\n");
}


void QObject::blockSignals(bool)
{
    NSLog (@"ERROR (NOT YET IMPLEMENTED) void QObject::blockSignals(bool)\n");
}

