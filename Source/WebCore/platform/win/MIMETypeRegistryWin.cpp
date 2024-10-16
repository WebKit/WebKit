/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/WindowsExtras.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static String mimeTypeForExtensionFromRegistry(const String& extension)
{
    auto ext = makeString('.', extension);
    WCHAR contentTypeStr[256];
    DWORD contentTypeStrLen = sizeof(contentTypeStr);
    DWORD keyType;

    HRESULT result = getRegistryValue(HKEY_CLASSES_ROOT, ext.wideCharacters().data(), L"Content Type", &keyType, contentTypeStr, &contentTypeStrLen);

    if (result == ERROR_SUCCESS && keyType == REG_SZ)
        return String(contentTypeStr, contentTypeStrLen / sizeof(contentTypeStr[0]) - 1);

    return String();
}

String MIMETypeRegistry::preferredExtensionForMIMEType(const String& type)
{
    auto path = makeString("MIME\\Database\\Content Type\\"_s, type);
    WCHAR extStr[MAX_PATH];
    DWORD extStrLen = sizeof(extStr);
    DWORD keyType;

    HRESULT result = getRegistryValue(HKEY_CLASSES_ROOT, path.wideCharacters().data(), L"Extension", &keyType, extStr, &extStrLen);

    if (result == ERROR_SUCCESS && keyType == REG_SZ)
        return String(extStr + 1, extStrLen / sizeof(extStr[0]) - 2);

    return String();
}

String MIMETypeRegistry::mimeTypeForExtension(StringView string)
{
    ASSERT(isMainThread());

    if (string.isEmpty())
        return String();

    auto ext = string.toString();
    static UncheckedKeyHashMap<String, String> mimetypeMap;
    if (mimetypeMap.isEmpty()) {
        //fill with initial values
        mimetypeMap.add("txt"_s, "text/plain"_s);
        mimetypeMap.add("pdf"_s, "application/pdf"_s);
        mimetypeMap.add("ps"_s, "application/postscript"_s);
        mimetypeMap.add("css"_s, "text/css"_s);
        mimetypeMap.add("html"_s, "text/html"_s);
        mimetypeMap.add("htm"_s, "text/html"_s);
        mimetypeMap.add("xml"_s, "text/xml"_s);
        mimetypeMap.add("xsl"_s, "text/xsl"_s);
        mimetypeMap.add("js"_s, "application/x-javascript"_s);
        mimetypeMap.add("xht"_s, "application/xhtml+xml"_s);
        mimetypeMap.add("xhtml"_s, "application/xhtml+xml"_s);
        mimetypeMap.add("rss"_s, "application/rss+xml"_s);
        mimetypeMap.add("webarchive"_s, "application/x-webarchive"_s);
#if USE(AVIF)
        mimetypeMap.add("avif"_s, "image/avif"_s);
#endif
        mimetypeMap.add("svg"_s, "image/svg+xml"_s);
        mimetypeMap.add("svgz"_s, "image/svg+xml"_s);
        mimetypeMap.add("jpg"_s, "image/jpeg"_s);
        mimetypeMap.add("jpeg"_s, "image/jpeg"_s);
        mimetypeMap.add("png"_s, "image/png"_s);
        mimetypeMap.add("tif"_s, "image/tiff"_s);
        mimetypeMap.add("tiff"_s, "image/tiff"_s);
        mimetypeMap.add("ico"_s, "image/ico"_s);
        mimetypeMap.add("cur"_s, "image/ico"_s);
        mimetypeMap.add("bmp"_s, "image/bmp"_s);
        mimetypeMap.add("wml"_s, "text/vnd.wap.wml"_s);
        mimetypeMap.add("wmlc"_s, "application/vnd.wap.wmlc"_s);
        mimetypeMap.add("m4a"_s, "audio/x-m4a"_s);
#if USE(JPEGXL)
        mimetypeMap.add("jxl"_s, "image/jxl"_s);
#endif
    }
    String result = mimetypeMap.get(ext);
    if (result.isEmpty()) {
        result = mimeTypeForExtensionFromRegistry(ext);
        if (!result.isEmpty())
            mimetypeMap.add(ext, result);
    }
    return result;
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

Vector<String> MIMETypeRegistry::extensionsForMIMEType(const String&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

}
