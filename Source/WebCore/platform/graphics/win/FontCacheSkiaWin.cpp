/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
#include "FontCache.h"

#include <dwrite_3.h>
#include <wtf/FileSystem.h>
#include <wtf/text/win/WCharStringExtras.h>

namespace WebCore {

static String fontsPath()
{
    const wchar_t* fontsEnvironmentVariable = L"WEBKIT_TESTFONTS";
    // <https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getenvironmentvariable>
    // The return size includes the terminating null character.
    DWORD size = GetEnvironmentVariable(fontsEnvironmentVariable, nullptr, 0);
    if (!size)
        return { };
    Vector<UChar> buffer(size);
    // The return size doesn't include the terminating null character.
    if (GetEnvironmentVariable(fontsEnvironmentVariable, wcharFrom(buffer.data()), size) != size - 1)
        return { };
    return buffer.span().first(size - 1);
}

FontCache::CreateDWriteFactoryResult FontCache::createDWriteFactory()
{
    CreateDWriteFactoryResult result;
    COMPtr<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&result.factory));
    if (FAILED(hr))
        return result;

    COMPtr<IDWriteFactory5> factory5(Query, result.factory);
    if (!factory5)
        return result;

    COMPtr<IDWriteFontSetBuilder1> builder;
    hr = factory5->CreateFontSetBuilder(&builder);
    if (FAILED(hr))
        return result;

    COMPtr<IDWriteFontSet> systemFontSet;
    hr = factory5->GetSystemFontSet(&systemFontSet);
    if (FAILED(hr))
        return result;

    builder->AddFontSet(systemFontSet.get());

    String baseFontPath = fontsPath();
    if (baseFontPath.isEmpty())
        return result;

    const std::span<const LChar> fontFilenames[] = {
        "AHEM____.TTF"_span,
        "WebKitWeightWatcher100.ttf"_span,
        "WebKitWeightWatcher200.ttf"_span,
        "WebKitWeightWatcher300.ttf"_span,
        "WebKitWeightWatcher400.ttf"_span,
        "WebKitWeightWatcher500.ttf"_span,
        "WebKitWeightWatcher600.ttf"_span,
        "WebKitWeightWatcher700.ttf"_span,
        "WebKitWeightWatcher800.ttf"_span,
        "WebKitWeightWatcher900.ttf"_span,
    };

    for (auto filename : fontFilenames) {
        String path = FileSystem::pathByAppendingComponent(baseFontPath, filename);
        COMPtr<IDWriteFontFile> file;
        hr = factory5->CreateFontFileReference(path.wideCharacters().data(), nullptr, &file);
        if (FAILED(hr))
            return result;
        builder->AddFontFile(file.get());
    }
    COMPtr<IDWriteFontSet> fontSet;
    hr = builder->CreateFontSet(&fontSet);
    if (FAILED(hr))
        return result;
    COMPtr<IDWriteFontCollection1> collection1;
    hr = factory5->CreateFontCollectionFromFontSet(fontSet.get(), &collection1);
    if (FAILED(hr))
        return result;
    result.fontCollection.query(collection1);
    return result;
}

} // namespace WebCore
