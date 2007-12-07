/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "FontDatabase.h"

#include "CString.h"
#include "FileSystem.h"
#include "PlatformString.h"
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <shlobj.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

static String systemFontsDirectory()
{
    static bool initialized;
    static String directory;

    if (!initialized) {
        initialized = true;

        Vector<UChar> buffer(MAX_PATH);
        if (FAILED(SHGetFolderPath(0, CSIDL_FONTS | CSIDL_FLAG_CREATE, 0, 0, buffer.data())))
            return directory;
        buffer.resize(wcslen(buffer.data()));

        directory = String::adopt(buffer);
    }

    return directory;
}

static String fontsPlistPath()
{
    static String path = pathByAppendingComponent(localUserSpecificStorageDirectory(), "FontsList.plist");
    return path;
}

static bool systemHasFontsNewerThanFontsPlist()
{
    WIN32_FILE_ATTRIBUTE_DATA plistAttributes = {0};
    if (!GetFileAttributesEx(fontsPlistPath().charactersWithNullTermination(), GetFileExInfoStandard, &plistAttributes))
        return true;

    WIN32_FILE_ATTRIBUTE_DATA fontsDirectoryAttributes = {0};
    if (!GetFileAttributesEx(systemFontsDirectory().charactersWithNullTermination(), GetFileExInfoStandard, &fontsDirectoryAttributes))
        return true;

    return CompareFileTime(&plistAttributes.ftLastWriteTime, &fontsDirectoryAttributes.ftLastWriteTime) < 0;
}

static RetainPtr<CFPropertyListRef> readFontPlist()
{
    CString plistPath = fontsPlistPath().utf8();

    RetainPtr<CFURLRef> url(AdoptCF, CFURLCreateFromFileSystemRepresentation(0, reinterpret_cast<const UInt8*>(plistPath.data()), plistPath.length(), false));
    if (!url)
        return 0;

    RetainPtr<CFReadStreamRef> stream(AdoptCF, CFReadStreamCreateWithFile(0, url.get()));
    if (!stream)
        return 0;

    if (!CFReadStreamOpen(stream.get()))
        return 0;

    CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0 | kCFPropertyListXMLFormat_v1_0;
    RetainPtr<CFPropertyListRef> plist(AdoptCF, CFPropertyListCreateFromStream(0, stream.get(), 0, kCFPropertyListMutableContainersAndLeaves, &format, 0));

    CFReadStreamClose(stream.get());

    return plist;
}

static bool populateFontDatabaseFromPlist()
{
    RetainPtr<CFPropertyListRef> plist = readFontPlist();
    if (!plist)
        return false;

    RetainPtr<CFDataRef> data(AdoptCF, CFPropertyListCreateXMLData(0, plist.get()));
    if (!data)
        return false;

    wkAddFontsFromPlistRepresentation(data.get());
    return true;
}

static bool populateFontDatabaseFromFileSystem()
{
    RetainPtr<CFStringRef> directory(AdoptCF, systemFontsDirectory().createCFString());
    if (!directory)
        return false;

    wkAddFontsInDirectory(directory.get());
    return true;
}

static void writeFontDatabaseToPlist()
{
    RetainPtr<CFDataRef> data(AdoptCF, wkCreateFontsPlistRepresentation());
    if (!data)
        return;

    safeCreateFile(fontsPlistPath(), data.get());
}

void populateFontDatabase()
{
    static bool initialized;
    if (initialized)
        return;
    initialized = true;

    if (!systemHasFontsNewerThanFontsPlist())
        if (populateFontDatabaseFromPlist())
            return;

    if (populateFontDatabaseFromFileSystem())
        writeFontDatabaseToPlist();
}

} // namespace WebCore
