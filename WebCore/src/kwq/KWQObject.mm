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
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool QObject::disconnect( const QObject *, const char *, const QObject *, 
    const char *)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QObject::QObject(QObject *parent=0, const char *name=0)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QObject::~QObject()
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


// member functions --------------------------------------------------------

const char *QObject::name() const
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QObject::setName(const char *)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}

#ifdef DO_QVARIANT
QVariant QObject::property(const char *name) const
{
}
#endif

bool QObject::inherits(const char *) const
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool QObject::connect(const QObject *, const char *, const char *) const
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


int QObject:: startTimer(int)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QObject::killTimer(int)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QObject::killTimers()
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QObject::installEventFilter(const QObject *)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QObject::removeEventFilter(const QObject *)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QObject::blockSignals(bool)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}

