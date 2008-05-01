/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Staikos Computing Services Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
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
#include "PluginDatabase.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include "CString.h"
#include "PluginPackage.h"

namespace WebCore {

void PluginDatabase::getPluginPathsInDirectories(HashSet<String>& paths) const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;

    QStringList nameFilters;
    nameFilters << "*.so";
    Vector<String>::const_iterator end = m_pluginDirectories.end();
    for (Vector<String>::const_iterator it = m_pluginDirectories.begin(); it != end; ++it) {
        QDir dir = QDir(QString(*it));
        if (!dir.exists())
            continue;

        QFileInfoList fl = dir.entryInfoList(nameFilters, QDir::Files);
        foreach (const QFileInfo fi, fl) {
            String filename = String(fi.absoluteFilePath());
            paths.add(filename);
        }
    }
}

static void addQtWebKitPluginDirectories(Vector<String>& paths)
{
    QString qtPath(getenv("QTWEBKIT_PLUGIN_PATH"));
    QStringList qtPaths = qtPath.split(":", QString::SkipEmptyParts);
    for(int i = 0; i < qtPaths.size(); i++) {
        paths.append(qtPaths.at(i));
    }
}

static void addMozillaPluginDirectories(Vector<String>& paths)
{
    QDir path = QDir::home(); path.cd(".mozilla/plugins"); 
    paths.append(path.absolutePath());
    paths.append("/usr/lib/browser/plugins");
    paths.append("/usr/local/lib/mozilla/plugins");
    paths.append("/usr/lib/mozilla/plugins");
    
    QString mozPath(getenv("MOZ_PLUGIN_PATH"));
    QStringList mozPaths = mozPath.split(":", QString::SkipEmptyParts);
    for(int i = 0; i < mozPaths.size(); i++) {
        paths.append(mozPaths.at(i));
    }
}

Vector<String> PluginDatabase::defaultPluginDirectories()
{
    Vector<String> paths;

    addQtWebKitPluginDirectories(paths);
    addMozillaPluginDirectories(paths);

    return paths;
}

bool PluginDatabase::isPreferredPluginDirectory(const String& path)
{
    QDir prefPath = QDir::home();
    prefPath.cd(".mozilla/plugins");
    return (path == prefPath.absolutePath());
}

}
