/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "ActivateFonts.h"
#include <shlwapi.h>
#include <wininet.h>

namespace WTR {

JSRetainPtr<JSStringRef> TestRunner::pathToLocalResource(JSStringRef)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> TestRunner::inspectorTestStubURL()
{
    wchar_t exePath[MAX_PATH];
    if (::GetModuleFileName(nullptr, exePath, MAX_PATH)) {
        wchar_t drive[_MAX_DRIVE];
        wchar_t dir[_MAX_DIR];
        _wsplitpath(exePath, drive, dir, nullptr, nullptr);

        wchar_t stubPath[MAX_PATH];
        wcsncpy(stubPath, drive, MAX_PATH);
        wcsncat(stubPath, dir, MAX_PATH - wcslen(stubPath));
        wcsncat(stubPath, L"\\WebKit.resources\\WebInspectorUI\\TestStub.html", MAX_PATH - wcslen(stubPath));

        wchar_t fileURI[INTERNET_MAX_PATH_LENGTH];
        DWORD fileURILength = INTERNET_MAX_PATH_LENGTH;
        UrlCreateFromPathW(stubPath, fileURI, &fileURILength, 0);

        return JSStringCreateWithCharacters(fileURI, fileURILength);
    }

    return nullptr;
}

void TestRunner::platformInitialize()
{
}

void TestRunner::installFakeHelvetica(JSStringRef configuration)
{
    WTR::installFakeHelvetica(toWK(configuration).get());
}

} // namespace WTR
