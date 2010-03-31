/*
 * Copyright (C) 2007 Staikos Computing Services Inc.
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FileSystem.h"

#include "PlatformString.h"
#include <wtf/text/CString.h>

#include <QDateTime>
#include <QFile>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

namespace WebCore {

bool fileExists(const String& path)
{
    return QFile::exists(path);
}


bool deleteFile(const String& path)
{
    return QFile::remove(path);
}

bool deleteEmptyDirectory(const String& path)
{
    return QDir::root().rmdir(path);
}

bool getFileSize(const String& path, long long& result)
{
    QFileInfo info(path);
    result = info.size();
    return info.exists();
}

bool getFileModificationTime(const String& path, time_t& result)
{
    QFileInfo info(path);
    result = info.lastModified().toTime_t();
    return info.exists();
}

bool makeAllDirectories(const String& path)
{
    return QDir::root().mkpath(path);
}

String pathByAppendingComponent(const String& path, const String& component)
{
    return QDir::toNativeSeparators(QDir(path).filePath(component));
}

String homeDirectoryPath()
{
    return QDir::homePath();
}

String pathGetFileName(const String& path)
{
    return QFileInfo(path).fileName();
}

String directoryName(const String& path)
{
    return String(QFileInfo(path).absolutePath());
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;

    QStringList nameFilters;
    if (!filter.isEmpty())
        nameFilters.append(filter);
    QFileInfoList fileInfoList = QDir(path).entryInfoList(nameFilters, QDir::AllEntries | QDir::NoDotAndDotDot);
    foreach (const QFileInfo fileInfo, fileInfoList) {
        String entry = String(fileInfo.canonicalFilePath());
        entries.append(entry);
    }

    return entries;
}

CString openTemporaryFile(const char* prefix, PlatformFileHandle& handle)
{
    QTemporaryFile* tempFile = new QTemporaryFile(QLatin1String(prefix));
    tempFile->setAutoRemove(false);
    QFile* temp = tempFile;
    if (temp->open(QIODevice::ReadWrite)) {
        handle = temp;
        return String(temp->fileName()).utf8();
    }
    handle = invalidPlatformFileHandle;
    return CString();
}

void closeFile(PlatformFileHandle& handle)
{
    if (handle) {
        handle->close();
        delete handle;
    }
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    if (handle && handle->exists() && handle->isWritable())
        return handle->write(data, length);

    return 0;
}

bool unloadModule(PlatformModule module)
{
#if defined(Q_WS_MAC)
    CFRelease(module);
    return true;

#elif defined(Q_OS_WIN)
    return ::FreeLibrary(module);

#else
#ifndef QT_NO_LIBRARY
    if (module->unload()) {
        delete module;
        return true;
    }
#endif
    return false;
#endif
}

}

// vim: ts=4 sw=4 et
