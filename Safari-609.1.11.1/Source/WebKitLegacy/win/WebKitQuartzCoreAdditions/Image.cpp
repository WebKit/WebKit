/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Image.h"

#include <wtf/RefPtr.h>
#include <wtf/win/GDIObject.h>

namespace WKQCA {

static const unsigned bitCount = 32;

static size_t numBytesForSize(const CSize& size)
{
    return size.cx * size.cy * bitCount / 8;
}

RefPtr<Image> Image::create(const CSize& size)
{
    HANDLE mapping = ::CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, numBytesForSize(size), 0);
    if (!mapping)
        return nullptr;

    return adoptRef(new Image(mapping, size));
}

Image::Image(HANDLE fileMapping, const CSize& size)
    : m_fileMapping(fileMapping)
    , m_size(size)
{
    ASSERT_ARG(fileMapping, fileMapping);
}

Image::~Image()
{
    ::CloseHandle(m_fileMapping);
}

static BITMAPINFO bitmapInfo(const CSize& size)
{
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biBitCount = bitCount;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biWidth = size.cx;
    info.bmiHeader.biHeight = -size.cy;

    return info;
}

GDIObject<HBITMAP> Image::createDIB() const
{
    BITMAPINFO info = bitmapInfo(m_size);

    void* unusedBits;
    return adoptGDIObject(::CreateDIBSection(0, &info, DIB_RGB_COLORS, &unusedBits, m_fileMapping, 0));
}

HANDLE Image::copyFileMapping(size_t& fileMappingSize)
{
    HANDLE result;
    if (!::DuplicateHandle(::GetCurrentProcess(), m_fileMapping, ::GetCurrentProcess(), &result, 0, FALSE, DUPLICATE_SAME_ACCESS))
        return 0;
    fileMappingSize = numBytesForSize(m_size);
    return result;
}

} // namespace WKQCA
