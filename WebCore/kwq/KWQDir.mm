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

#include <qdir.h>

#ifndef USING_BORROWED_QDIR

#include <kwqdebug.h>
#import <Foundation/Foundation.h>

#include <qfile.h>
#include <qstring.h>
#include <qregexp.h>
#include <sys/param.h>
#include <unistd.h>

class QDir::KWQDirPrivate
{
public:
    KWQDirPrivate(const QString &n);
    KWQDirPrivate(const KWQDirPrivate &other);
    ~KWQDirPrivate();

    QString name;
};

QDir::KWQDirPrivate::KWQDirPrivate(const QString &n) : name(n)
{
}

QDir::KWQDirPrivate::KWQDirPrivate(const KWQDirPrivate &other) : name(other.name)
{
}

QDir::KWQDirPrivate::~KWQDirPrivate()
{
}



QDir::QDir() : d(new QDir::KWQDirPrivate(QString(".")))
{
}

QDir::QDir(const QString &name) : d(new QDir::KWQDirPrivate(name))
{
}

QDir::QDir(const QDir &other) : d(new QDir::KWQDirPrivate(*other.d))
{
}

QDir::~QDir()
{
    delete d;
}

QString QDir::absPath() const
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    QString path;
    if (d->name[0] == '/') {
	path = d->name;
    } else {
	QString cwdString = QString::fromCFString((CFStringRef)[[NSFileManager defaultManager] currentDirectoryPath]);

	if (cwdString[cwdString.length()-1] != '/') {
	    cwdString += '/';
	}
	
	path = cwdString + d->name;
    }

    // Resolve . and ..
    path.replace(QRegExp("/./"), "/");
    path.replace(QRegExp("/[^/]*/../"), "/");
    path.replace(QRegExp("/[^/]*/..$"), "");

    [pool release];
    return path;
}

QString QDir::absFilePath(const QString &fileName) const
{
    QString dirPath = absPath();

    if (dirPath[dirPath.length()-1] != '/') {
	dirPath = dirPath + "/";
    }

    return dirPath + fileName;
}

bool QDir::cdUp()
{
    QString dirPath = absPath();

    if (dirPath[dirPath.length()-1] != '/') {
	dirPath += "/";
    }

    dirPath += "..";

    dirPath.replace(QRegExp("/[^/]*/..$"), "");
   
    if (QFile::exists(dirPath)) {
	d->name = dirPath;
	return true;
    } else {
	return false;
    }
}

bool QDir::exists(const QString &fileName) const
{
    return QFile::exists(absFilePath(fileName));
}

QStringList QDir::entryList(const QString &nameFilter)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    QStringList results;
    {
    QRegExp filter(nameFilter, TRUE, TRUE);
	
    NSArray *contents = [[NSFileManager defaultManager] directoryContentsAtPath:(NSString *)(absPath().getCFMutableString())];

    contents = [contents arrayByAddingObjectsFromArray:[NSArray arrayWithObjects:@".", @"..", nil]];
    contents = [contents sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
    
    unsigned length = [contents count];

    for (unsigned i = 0; i < length; i++) {
	QString item = QString::fromCFString((CFStringRef)[contents objectAtIndex:i]);

	if (item.find(filter) != -1 && QFile::exists(absFilePath(item))) {
	    results.append(item);
	}
    }
    }

    [pool release];
    return results;
}

QDir &QDir::operator=(const QDir &other)
{
    QDir tmp(other);

    swap (tmp);

    return *this;
}

void QDir::swap(QDir &other)
{
    QDir::KWQDirPrivate *tmp;

    tmp = other.d;
    other.d = d;
    d = tmp;
}

#endif

