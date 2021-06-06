/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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
#include "DirectWriteUtilities.h"

#if USE(DIRECT2D)

#include "COMPtr.h"
#include "SharedBuffer.h"
#include <dwrite_3.h>


namespace WebCore {

namespace DirectWrite {

IDWriteFactory5* factory()
{
    static IDWriteFactory5* directWriteFactory = nullptr;
    if (!directWriteFactory) {
        HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(directWriteFactory), reinterpret_cast<IUnknown**>(&directWriteFactory));
        directWriteFactory->AddRef();
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return directWriteFactory;
}

IDWriteGdiInterop* gdiInterop()
{
    static IDWriteGdiInterop* directWriteGdiInterop = nullptr;
    if (!directWriteGdiInterop) {
        HRESULT hr = factory()->GetGdiInterop(&directWriteGdiInterop);
        directWriteGdiInterop->AddRef();
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return directWriteGdiInterop;
}

static IDWriteInMemoryFontFileLoader* memoryFontLoader()
{
    static IDWriteInMemoryFontFileLoader* memoryFontLoader = nullptr;
    if (!memoryFontLoader) {
        HRESULT hr = factory()->CreateInMemoryFontFileLoader(&memoryFontLoader);
        RELEASE_ASSERT(SUCCEEDED(hr));
        memoryFontLoader->AddRef();

        hr = factory()->RegisterFontFileLoader(memoryFontLoader);
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return memoryFontLoader;
}

static IDWriteFontSetBuilder1* fontSetBuilder()
{
    static IDWriteFontSetBuilder1* fontSetBuilder = nullptr;
    if (!fontSetBuilder) {
        HRESULT hr = factory()->CreateFontSetBuilder(&fontSetBuilder);
        RELEASE_ASSERT(SUCCEEDED(hr));
        fontSetBuilder->AddRef();
    }

    return fontSetBuilder;
}

static COMPtr<IDWriteFontSet> fontSet;
static COMPtr<IDWriteFontCollection1> fontCollection;
static UINT32 fontCount = 0;

IDWriteFontCollection1* webProcessFontCollection()
{
    HRESULT hr = S_OK;

    if (!fontSet) {
        hr = fontSetBuilder()->CreateFontSet(&fontSet);
        if (!SUCCEEDED(hr))
            return nullptr;

        fontCollection = nullptr;
        hr = factory()->CreateFontCollectionFromFontSet(fontSet.get(), &fontCollection);
        if (!SUCCEEDED(hr))
            return nullptr;

        ASSERT(fontCollection->GetFontFamilyCount() == fontCount);
    }

    return fontCollection.get();
}

HRESULT addFontFromDataToProcessCollection(const Vector<uint8_t>& fontData)
{
    COMPtr<IDWriteFontFile> fontFile;
    HRESULT hr = memoryFontLoader()->CreateInMemoryFontFileReference(factory(), fontData.data(), fontData.size(), nullptr, &fontFile);
    if (!SUCCEEDED(hr))
        return hr;

    hr = fontSetBuilder()->AddFontFile(fontFile.get());
    if (!SUCCEEDED(hr))
        return hr;

    fontSet = nullptr;
    ++fontCount;

    return hr;
}

Vector<wchar_t> familyNameForLocale(IDWriteFontFamily* fontFamily, const Vector<wchar_t>& localeName)
{
    COMPtr<IDWriteLocalizedStrings> familyNames;
    HRESULT hr = fontFamily->GetFamilyNames(&familyNames);
    if (FAILED(hr))
        return { };

    BOOL exists = false;
    unsigned localeIndex = 0;
    if (!localeName.isEmpty())
        hr = familyNames->FindLocaleName(localeName.data(), &localeIndex, &exists);

    if (SUCCEEDED(hr) && !exists)
        hr = familyNames->FindLocaleName(L"en-us", &localeIndex, &exists);

    if (FAILED(hr))
        return { };

    unsigned familyNameLength = 0;
    hr = familyNames->GetStringLength(localeIndex, &familyNameLength);
    if (!SUCCEEDED(hr))
        return { };

    Vector<wchar_t> familyName(familyNameLength + 1);
    hr = familyNames->GetString(localeIndex, familyName.data(), familyName.size());
    if (!SUCCEEDED(hr))
        return { };

    return familyName;
}

COMPtr<IDWriteFontFamily> fontFamilyForCollection(IDWriteFontCollection* fontCollection, const Vector<wchar_t>& localeName, const LOGFONT& logFont)
{
    COMPtr<IDWriteFontFamily> fontFamily;

    unsigned fontFamilyCount = fontCollection->GetFontFamilyCount();
    for (unsigned fontIndex = 0; fontIndex < fontFamilyCount; ++fontIndex) {
        HRESULT hr = fontCollection->GetFontFamily(fontIndex, &fontFamily);
        if (FAILED(hr))
            return nullptr;

        auto familyName = familyNameForLocale(fontFamily.get(), localeName);

        if (!wcscmp(logFont.lfFaceName, familyName.data()))
            return fontFamily;

        fontFamily = nullptr;
    }

    return nullptr;
}

COMPtr<IDWriteFont> createWithPlatformFont(const LOGFONT& logFont)
{
    COMPtr<IDWriteFontCollection> systemFontCollection;
    HRESULT hr = factory()->GetSystemFontCollection(&systemFontCollection);
    if (FAILED(hr))
        return nullptr;

    Vector<wchar_t> localeName(LOCALE_NAME_MAX_LENGTH);
    int localeLength = GetUserDefaultLocaleName(localeName.data(), LOCALE_NAME_MAX_LENGTH);
    RELEASE_ASSERT(localeLength <= LOCALE_NAME_MAX_LENGTH);

    COMPtr<IDWriteFontCollection1> collection(Query, systemFontCollection);

    COMPtr<IDWriteFontFamily> fontFamily = fontFamilyForCollection(collection.get(), localeName, logFont);
    if (!fontFamily) {
        collection = webProcessFontCollection();
        fontFamily = fontFamilyForCollection(collection.get(), localeName, logFont);
    }

    if (!fontFamily) {
        // Just return the first system font.
        collection = COMPtr<IDWriteFontCollection1>(Query, systemFontCollection);
        hr = collection->GetFontFamily(0, &fontFamily);
        if (FAILED(hr))
            return nullptr;

#ifndef NDEBUG
        auto familyName = familyNameForLocale(fontFamily.get(), localeName);
        UNUSED_VARIABLE(familyName);
#endif
    }

    DWRITE_FONT_WEIGHT weight = static_cast<DWRITE_FONT_WEIGHT>(logFont.lfWeight);
    DWRITE_FONT_STRETCH stretch = static_cast<DWRITE_FONT_STRETCH>(logFont.lfQuality);
    DWRITE_FONT_STYLE style = logFont.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

    COMPtr<IDWriteFont> dwFont;
    hr = fontFamily->GetFirstMatchingFont(weight, stretch, style, &dwFont);
    ASSERT(SUCCEEDED(hr));

    return dwFont;
}

DWRITE_FONT_WEIGHT fontWeight(const FontPlatformData&)
{
    return DWRITE_FONT_WEIGHT_REGULAR;
}

DWRITE_FONT_STYLE fontStyle(const FontPlatformData&)
{
    return DWRITE_FONT_STYLE_NORMAL;
}

DWRITE_FONT_STRETCH fontStretch(const FontPlatformData&)
{
    return DWRITE_FONT_STRETCH_NORMAL;
}

} // namespace DirectWrite

} // namespace WebCore

#endif // USE(DIRECT2D)
