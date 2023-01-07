/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreBundleWin.h"

#include "WebCoreInstanceHandle.h"
#include <windows.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>

#if USE(CF)
#include <CoreFoundation/CFBundle.h>
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>
#endif

namespace WebCore {

#if USE(CF)
static RetainPtr<CFBundleRef> createWebKitBundle()
{
    if (CFBundleRef existingBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit")))
        return existingBundle;

    wchar_t dllPathBuffer[MAX_PATH];
    DWORD length = ::GetModuleFileNameW(WebCore::instanceHandle(), dllPathBuffer, std::size(dllPathBuffer));
    ASSERT(length);
    ASSERT(length < std::size(dllPathBuffer));

    RetainPtr<CFStringRef> dllPath = adoptCF(CFStringCreateWithCharactersNoCopy(0, reinterpret_cast<const UniChar*>(dllPathBuffer), length, kCFAllocatorNull));
    RetainPtr<CFURLRef> dllURL = adoptCF(CFURLCreateWithFileSystemPath(0, dllPath.get(), kCFURLWindowsPathStyle, false));
    RetainPtr<CFURLRef> dllDirectoryURL = adoptCF(CFURLCreateCopyDeletingLastPathComponent(0, dllURL.get()));
    RetainPtr<CFURLRef> resourcesDirectoryURL = adoptCF(CFURLCreateCopyAppendingPathComponent(0, dllDirectoryURL.get(), CFSTR("WebKit.resources"), true));

    return adoptCF(CFBundleCreate(0, resourcesDirectoryURL.get()));
}

CFBundleRef webKitBundle()
{
    static NeverDestroyed<RetainPtr<CFBundleRef>> bundle = createWebKitBundle();
    ASSERT(bundle.get());
    return bundle.get().get();
}

String webKitBundlePath()
{
    URL bundleURL = adoptCF(CFBundleCopyBundleURL(webKitBundle())).get();
    return bundleURL.fileSystemPath();
}

String webKitBundlePath(StringView path)
{
    auto pathString = path.toStringWithoutCopying();
    auto directory = FileSystem::parentPath(pathString);
    auto fileName = FileSystem::pathFileName(pathString);
    auto splitAt = fileName.reverseFind('.');

    return webKitBundlePath(fileName.left(splitAt), fileName.substring(splitAt + 1), directory);
}

String webKitBundlePath(StringView name, StringView type, StringView directory)
{
    auto resourceURL = adoptCF(CFBundleCopyResourceURL(webKitBundle(), name.createCFString().get(), type.createCFString().get(), directory.createCFString().get()));
    if (!resourceURL)
        return nullString();

    return adoptCF(CFURLCopyFileSystemPath(resourceURL.get(), kCFURLWindowsPathStyle)).get();
}

#else

static String dllDirectory()
{
    WCHAR buffer[MAX_PATH];
    DWORD length = ::GetModuleFileNameW(WebCore::instanceHandle(), buffer, MAX_PATH);
    if (!length || (length == MAX_PATH && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        return emptyString();

    String path(buffer, length);
    return FileSystem::parentPath(path);
}

String webKitBundlePath()
{
    static NeverDestroyed<String> bundle = FileSystem::pathByAppendingComponent(dllDirectory(), "WebKit.resources"_s);
    return bundle;
}

String webKitBundlePath(StringView path)
{
    auto resource = FileSystem::pathByAppendingComponent(webKitBundlePath(), path);
    if (!FileSystem::fileExists(resource))
        return nullString();

    return resource;
}

String webKitBundlePath(StringView name, StringView type, StringView directory)
{
    auto fileName = makeString(name, '.', type);

    // CF seems to search in .lproj directories for files as well but since
    // there's only one file there just flag it here
    if (name == "mediaControlsLocalizedStrings"_s)
        return webKitBundlePath(FileSystem::pathByAppendingComponent("en.lproj"_s, fileName));

    return webKitBundlePath(FileSystem::pathByAppendingComponent(directory, fileName));
}

#endif // USE(CF)

String webKitBundlePath(const Vector<StringView>& components)
{
    return webKitBundlePath(FileSystem::pathByAppendingComponents(emptyString(), components));
}

} // namespace WebCore
