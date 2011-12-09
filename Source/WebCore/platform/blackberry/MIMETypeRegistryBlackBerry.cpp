/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
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

#include "NotImplemented.h"

namespace WebCore {

struct ExtensionMap {
    const char* extension;
    const char* mimeType;
};

static const ExtensionMap extensionMap[] = {
    { "bmp", "image/bmp" },
    { "css", "text/css" },
    { "gif", "image/gif" },
    { "html", "text/html" },
    { "htm", "text/html" },
    { "ico", "image/x-icon" },
    { "jpg", "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "js", "application/x-javascript" },
    { "mng", "video/x-mng" },
    { "pbm", "image/x-portable-bitmap" },
    { "pgm", "image/x-portable-graymap" },
    { "pdf", "application/pdf" },
    { "png", "image/png" },
    { "ppm", "image/x-portable-pixmap" },
    { "rss", "application/rss+xml" },
    { "svg", "image/svg+xml" },
    { "svgz", "image/svg+xml" },
    { "txt", "text/plain" },
    { "text", "text/plain" },
    { "tiff", "image/tiff" },
    { "tif", "image/tiff" },
    { "xbm", "image/x-xbitmap" },
    { "xml", "text/xml" },
    { "xpm", "image/x-xpm" },
    { "xsl", "text/xsl" },
    { "xhtml", "application/xhtml+xml" },
    { "wml", "text/vnd.wap.wml" },
    { "wmlc", "application/vnd.wap.wmlc" },
    { "m4a", "audio/m4a" },
    { "midi", "audio/midi" },
    { "mid", "audio/mid" },
    { "mp3", "audio/mp3" },
    { "wma", "audio/x-ms-wma" },
    { "3gp", "video/3gpp" },
    { "3gpp", "video/3gpp" },
    { "3gpp2", "video/3gpp2" },
    { "3g2", "video/3gpp2" },
    { "3gp2", "video/3gpp2" },
    { "mp4", "video/mp4" },
    { "m4v", "video/m4v" },
    { "avi", "video/x-msvideo" },
    { "mov", "video/quicktime" },
    { "divx", "video/divx" },
    { "mpeg", "video/mpeg" },
    { "sbv", "video/sbv" },
    { "asf", "video/x-ms-asf" },
    { "wm", "video/x-ms-wm" },
    { "wmv", "video/x-ms-wmv" },
    { "wmx", "video/x-ms-wmx" },
    { "wav", "audio/x-wav" },
    { "amr", "audio/amr" },
    { "aac", "audio/aac" },
    { "x-gsm", "audio/x-gsm" },
    { "swf", "application/x-shockwave-flash" },
    { "m3u8", "application/vnd.apple.mpegurl" },
    { "m3url", "audio/mpegurl" },
    { "m3u", "audio/mpegurl" },
    // FIXME: wince also maps ttf and otf to text/plain. Should we do that too?
    { 0, 0 }
};

String MIMETypeRegistry::getMIMETypeForExtension(const String& extension)
{
    String lowerExtension = extension.lower();

    const ExtensionMap* entry = extensionMap;
    while (entry->extension) {
        if (lowerExtension == entry->extension)
            return entry->mimeType;
        ++entry;
    }

    return String();
}

String MIMETypeRegistry::getPreferredExtensionForMIMEType(const String& type)
{
    if (type.isEmpty())
        return String();

    String lowerType = type.lower();

    const ExtensionMap* entry = extensionMap;
    while (entry->mimeType) {
        if (lowerType == entry->mimeType)
            return entry->extension;
        ++entry;
    }

    return String();
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    notImplemented();
    return false;
}

}
