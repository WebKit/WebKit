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
#include <qmovie.h>

class QMoviePrivate {
friend class QMovie;
public:
    QMoviePrivate() {
    }
    
    ~QMoviePrivate() {
    }
    
private:
    QRect rect;    
    QPixmap pixmap;
    QImage image;    
};

QMovie::QMovie()
{
    _logNotYetImplemented();
    d = new QMoviePrivate();
}


QMovie::QMovie(QDataSource*, int bufsize)
{
    _logPartiallyImplemented();
    d = new QMoviePrivate();
}


QMovie::QMovie(const QMovie &other)
{
    _logPartiallyImplemented();
    d->rect = other.d->rect;
    d->pixmap = other.d->pixmap;
    d->image = other.d->image;
}


QMovie::~QMovie()
{
    _logPartiallyImplemented();
    delete d;
}


void QMovie::unpause()
{
    _logNotYetImplemented();
}


void QMovie::pause()
{
    _logNotYetImplemented();
}


void QMovie::restart()
{
    _logNotYetImplemented();
}


bool QMovie::finished()
{
    _logNotYetImplemented();
    return FALSE;    
}


bool QMovie::running()
{
    _logNotYetImplemented();
    return FALSE;    
}


int QMovie::frameNumber() const
{
    _logNotYetImplemented();
    return 0;    
}


const QRect &QMovie::getValidRect() const
{
    _logPartiallyImplemented();
    return d->rect;
}


const QPixmap &QMovie::framePixmap() const
{
    _logPartiallyImplemented();
    return d->pixmap;
}


const QImage &QMovie::frameImage() const
{
    _logPartiallyImplemented();
    return d->image;
}


void QMovie::connectResize(QObject* receiver, const char *member)
{
    _logNotYetImplemented();
}


void QMovie::connectUpdate(QObject* receiver, const char *member)
{
    _logNotYetImplemented();
}


void QMovie::connectStatus(QObject* receiver, const char *member)
{
    _logNotYetImplemented();
}


void QMovie::disconnectResize(QObject* receiver, const char *member=0)
{
    _logNotYetImplemented();
}


void QMovie::disconnectUpdate(QObject* receiver, const char *member=0)
{
    _logNotYetImplemented();
}


void QMovie::disconnectStatus(QObject* receiver, const char *member=0)
{
    _logNotYetImplemented();
}


QMovie &QMovie::operator=(const QMovie &)
{
    _logNotYetImplemented();
    return *this;
}
