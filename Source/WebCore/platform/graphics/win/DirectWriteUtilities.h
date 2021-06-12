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

#pragma once

#if USE(DIRECT2D)

#include "COMPtr.h"

interface IDWriteFactory5;
interface IDWriteFont;
interface IDWriteFontCollection;
interface IDWriteFontCollection1;
interface IDWriteFontFamily;
interface IDWriteGdiInterop;

enum DWRITE_FONT_WEIGHT;
enum DWRITE_FONT_STYLE;
enum DWRITE_FONT_STRETCH;

namespace WebCore {

class FontPlatformData;
class SharedBuffer;

namespace DirectWrite {

IDWriteFactory5* factory();
IDWriteGdiInterop* gdiInterop();

IDWriteFontCollection1* webProcessFontCollection();

HRESULT addFontFromDataToProcessCollection(const Vector<uint8_t>& fontData);
COMPtr<IDWriteFontFamily> fontFamilyForCollection(IDWriteFontCollection*, const Vector<wchar_t>& localeName, const LOGFONT&);
COMPtr<IDWriteFont> createWithPlatformFont(const LOGFONT&);
Vector<wchar_t> familyNameForLocale(IDWriteFontFamily*, const Vector<wchar_t>& localeName);

DWRITE_FONT_WEIGHT fontWeight(const FontPlatformData&);
DWRITE_FONT_STYLE fontStyle(const FontPlatformData&);
DWRITE_FONT_STRETCH fontStretch(const FontPlatformData&);

} // namespace DirectWrite

} // namespace WebCore

#endif // USE(DIRECT2D)
