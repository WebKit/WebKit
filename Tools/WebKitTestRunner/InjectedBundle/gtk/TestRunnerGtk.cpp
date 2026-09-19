/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
#include "TestRunner.h"
#include "InjectedBundle.h"

#include <JavaScriptCore/JSStringRefCPP.h>
#include <glib.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTR {

void TestRunner::platformInitialize()
{
}

JSRetainPtr<JSStringRef> TestRunner::pathToLocalResource(JSStringRef url)
{
    auto urlString = utf8CString(url);

    if (!g_str_has_prefix(urlString.legacyCStringPointer(), "file:///tmp/LayoutTests/"))
        return JSStringRetain(url);

    const gchar* layoutTestsSuffix = urlString.legacyCStringPointer() + strlen("file:///tmp/");
    GUniquePtr<gchar> testPath(g_build_filename(FileSystem::webkitTopLevelDirectory().legacyCStringPointer(), layoutTestsSuffix, nullptr));
    GUniquePtr<gchar> testURI(g_filename_to_uri(testPath.get(), 0, 0));
    return JSStringCreateWithUTF8CString(testURI.get());
}

JSRetainPtr<JSStringRef> TestRunner::inspectorTestStubURL()
{
    return JSStringCreateWithUTF8CString("resource:///org/webkit/inspector/UserInterface/TestStub.html");
}

} // namespace WTR
