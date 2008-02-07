/*
 * Copyright (C) 2007 Staikos Computing Services Inc.
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Apple, Inc. All rights reserved.
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

#include "NotImplemented.h"
#include "PlatformString.h"
#include <QFile>
#include <QFileInfo>
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

bool getFileModificationTime(const String&, time_t&)
{
    notImplemented();
    return false;
}

bool makeAllDirectories(const String& path)
{
    return QDir::root().mkpath(path);
}

String pathByAppendingComponent(const String& path, const String& component)
{
    return QDir(path).filePath(component);
}

}

// vim: ts=4 sw=4 et
