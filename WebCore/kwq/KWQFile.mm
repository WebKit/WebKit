/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KWQFile.h"

// This NSString call can't throw so no need to block exceptions
QFile::QFile(const QString &n) : name(strdup(n.isEmpty() ? "" : [n.getNSString() fileSystemRepresentation])), fd(-1)
{
}

QFile::~QFile()
{
    free(name);
}

bool QFile::exists() const
{
    return access(name, F_OK) == 0;
}

bool QFile::open(int mode)
{
    close();

    if (mode == IO_ReadOnly) {
        fd = ::open(name, O_RDONLY);
    }

    return fd != -1;
}

void QFile::close()
{
    if (fd != -1) {
        ::close(fd);
    }

    fd = -1;
}

int QFile::readBlock(char *data, uint bytesToRead)
{
    if (fd == -1) {
        return -1;
    } else {
        return read(fd, data, bytesToRead);
    }
}

uint QFile::size() const
{
    struct stat statbuf;

    if (stat(name, &statbuf) == 0) {
        return statbuf.st_size;
    } else {
        return 0;
    }
}

bool QFile::exists(const QString &path)
{
    return QFile(path).exists();
}
