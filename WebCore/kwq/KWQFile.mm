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

#include <qfile.h>

#ifndef USING_BORROWED_QFILE

#include <kwqdebug.h>
#import <Foundation/Foundation.h>
#include <sys/param.h>
#include <qstring.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <qiodevice.h>

class QFile::KWQFilePrivate
{
public:
    KWQFilePrivate(const QString &name);
    ~KWQFilePrivate();

    char *name;
    int fd;
};


QFile::KWQFilePrivate::KWQFilePrivate(const QString &qname) : name(new char[MAXPATHLEN + 1]), fd(-1)
{
    NSString *nsname = (NSString *)qname.getCFMutableString();

    [nsname getFileSystemRepresentation:name maxLength:MAXPATHLEN];
}

QFile::KWQFilePrivate::~KWQFilePrivate()
{
    delete [] name;
}


QFile::QFile() : d(new QFile::KWQFilePrivate(QString("")))
{
}

QFile::QFile(const QString &name) : d(new QFile::KWQFilePrivate(name))
{
}

QFile::~QFile()
{
    delete d;
}

bool QFile::exists() const
{
    return access(d->name, F_OK) == 0;
}

bool QFile::open(int mode)
{
    close();

    if (mode == IO_ReadOnly) {
	d->fd = ::open(d->name, O_RDONLY);
    }

    return d->fd != -1;
}

void QFile::close()
{
    if (d->fd != -1) {
	::close(d->fd);
    }

    d->fd = -1;
}

int QFile::readBlock(char *data, uint bytesToRead)
{
    if (d->fd == -1) {
	return -1;
    } else {
	return read(d->fd, data, bytesToRead);
    }
}

uint QFile::size() const
{
    struct stat statbuf;

    if (stat(d->name, &statbuf) == 0) {
	return statbuf.st_size;
    } else {
	return 0;
    }
}

bool QFile::exists(const QString &path)
{
    return QFile(path).exists();
}


#endif

