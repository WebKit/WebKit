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

#include <qmovie.h>

QMovie::QMovie()
{
}


QMovie::QMovie(QDataSource*, int bufsize)
{
}


QMovie::QMovie(const QMovie &)
{
}


QMovie::~QMovie()
{
}


void QMovie::unpause()
{
}


void QMovie::pause()
{
}


void QMovie::restart()
{
}


bool QMovie::finished()
{
}


bool QMovie::running()
{
}


int QMovie::frameNumber() const
{
}


const QRect& QMovie::getValidRect() const
{
}


const QPixmap& QMovie::framePixmap() const
{
}


const QImage& QMovie::frameImage() const
{
}


void QMovie::connectResize(QObject* receiver, const char *member)
{
}


void QMovie::connectUpdate(QObject* receiver, const char *member)
{
}


void QMovie::connectStatus(QObject* receiver, const char *member)
{
}


void QMovie::disconnectResize(QObject* receiver, const char *member=0)
{
}


void QMovie::disconnectUpdate(QObject* receiver, const char *member=0)
{
}


void QMovie::disconnectStatus(QObject* receiver, const char *member=0)
{
}


QMovie &QMovie::operator=(const QMovie &)
{
}



