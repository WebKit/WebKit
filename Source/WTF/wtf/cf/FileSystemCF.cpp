/*
 * Copyright (C) 2007, 2011 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include <wtf/FileSystem.h>

#include <CoreFoundation/CFString.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

CString FileSystem::fileSystemRepresentation(const String& path)
{
    RetainPtr<CFStringRef> cfString = path.createCFString();

    if (!cfString)
        return CString();

    CFIndex size = CFStringGetMaximumSizeOfFileSystemRepresentation(cfString.get());

    Vector<char> buffer(size);

    if (!CFStringGetFileSystemRepresentation(cfString.get(), buffer.data(), buffer.size())) {
        LOG_ERROR("Failed to get filesystem representation to create CString from cfString");
        return CString();
    }

    return CString(buffer.data(), strlen(buffer.data()));
}

String FileSystem::stringFromFileSystemRepresentation(const char* fileSystemRepresentation)
{
    return adoptCF(CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, fileSystemRepresentation)).get();
}

RetainPtr<CFURLRef> FileSystem::pathAsURL(const String& path)
{
    CFURLPathStyle pathStyle;
#if PLATFORM(WIN)
    pathStyle = kCFURLWindowsPathStyle;
#else
    pathStyle = kCFURLPOSIXPathStyle;
#endif
    return adoptCF(CFURLCreateWithFileSystemPath(nullptr, path.createCFString().get(), pathStyle, FALSE));
}

} // namespace WTF
