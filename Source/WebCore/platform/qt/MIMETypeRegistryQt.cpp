/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "MIMETypeRegistry.h"

#include <QMimeDatabase>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>

namespace WebCore {

struct ExtensionMap {
    const char* extension;
    const char* dotExtension;
    const char* mimeType;
};

// This is a short list of extensions not recognized by the freedesktop shared mimetype database 1.0.
static const ExtensionMap extensionMap[] = {
    { "mht", ".mht", "application/x-mimearchive" },
    { "mhtml", ".mhtml", "application/x-mimearchive" },
    { "text", ".text", "text/plain" },
    { "wmlc", ".wmlc", "application/vnd.wap.wmlc" },
    { 0, 0, 0 }
};

String MIMETypeRegistry::getMIMETypeForExtension(const String &ext)
{
    // QMimeDatabase lacks the ability to query by extension alone, so we create a fake filename to lookup.
    String suffix = ext.lower();
    const QString filename = QStringLiteral("filename.") + QString(suffix);

    QMimeType mimeType = QMimeDatabase().mimeTypeForFile(filename, QMimeDatabase::MatchExtension);
    if (mimeType.isValid() && !mimeType.isDefault())
        return mimeType.name();

    const ExtensionMap *e = extensionMap;
    while (e->extension) {
        if (suffix == e->extension)
            return e->mimeType;
        ++e;
    }

    return String();
}

String MIMETypeRegistry::getMIMETypeForPath(const String& path)
{
    QMimeType type = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    if (type.isValid() && !type.isDefault())
        return type.name();

    const ExtensionMap *e = extensionMap;
    while (e->extension) {
        if (path.endsWith(e->dotExtension))
            return e->mimeType;
        ++e;
    }

    return defaultMIMEType();
}

Vector<String> MIMETypeRegistry::getExtensionsForMIMEType(const String& mimeTypeName)
{
    Vector<String> extensions;
    QMimeType mimeType = QMimeDatabase().mimeTypeForName(mimeTypeName);
    if (mimeType.isValid() && !mimeType.isDefault()) {
        Q_FOREACH(const QString& suffix, mimeType.suffixes()) {
            extensions.append(suffix);
        }
    }

    return extensions;
}

String MIMETypeRegistry::getPreferredExtensionForMIMEType(const String& mimeTypeName)
{
    QMimeType mimeType = QMimeDatabase().mimeTypeForName(mimeTypeName);
    if (mimeType.isValid() && !mimeType.isDefault())
        return mimeType.preferredSuffix();

    return String();
}

String MIMETypeRegistry::getNormalizedMIMEType(const String& mimeTypeName)
{
    // This looks up the mime type object by preferred name or alias, and returns the preferred name.
    QMimeType mimeType = QMimeDatabase().mimeTypeForName(mimeTypeName);
    if (mimeType.isValid() && !mimeType.isDefault())
        return mimeType.name();

    return mimeTypeName;
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String& mimeType)
{
    return mimeType.startsWith("application/x-qt-plugin", false)
        || mimeType.startsWith("application/x-qt-styled-widget", false);
}

}
