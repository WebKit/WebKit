/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ImageDecoder.h"

// FIXME: Are all these needed?
#include <wx/defs.h>
#include <wx/bitmap.h>
#if USE(WXGC)
#include <wx/graphics.h>
#endif
#include <wx/image.h>
#include <wx/rawbmp.h>

namespace WebCore {

RGBA32Buffer::RGBA32Buffer()
    : m_hasAlpha(false)
    , m_status(FrameEmpty)
    , m_duration(0)
    , m_disposalMethod(DisposeNotSpecified)
{
} 

void RGBA32Buffer::clear()
{
    m_bytes.clear();
    m_status = FrameEmpty;
    // NOTE: Do not reset other members here; clearFrameBufferCache()
    // calls this to free the bitmap data, but other functions like
    // initFrameBuffer() and frameComplete() may still need to read
    // other metadata out of this frame later.
}

void RGBA32Buffer::zeroFill()
{
    m_bytes.fill(0);
    m_hasAlpha = true;
}

void RGBA32Buffer::copyBitmapData(const RGBA32Buffer& other)
{
    if (this == &other)
        return;

    m_bytes = other.m_bytes;
    m_size = other.m_size;
    setHasAlpha(other.m_hasAlpha);
}

bool RGBA32Buffer::setSize(int newWidth, int newHeight)
{
    // NOTE: This has no way to check for allocation failure if the
    // requested size was too big...
    m_bytes.resize(newWidth * newHeight);
    m_size = IntSize(newWidth, newHeight);

    // Zero the image.
    zeroFill();

    return true;
}

NativeImagePtr RGBA32Buffer::asNewNativeImage() const
{
    const unsigned char* bytes = (const unsigned char*)m_bytes.data();
    
    typedef wxPixelData<wxBitmap, wxAlphaPixelFormat> WxPixelData;

    wxBitmap* bmp = new wxBitmap(width(), height(), 32);
    WxPixelData data(*bmp);
    
    int rowCounter = 0;
    long pixelCounter = 0;
    
    WxPixelData::Iterator p(data);
    
    WxPixelData::Iterator rowStart = p; 
    
    // NB: It appears that the data is in BGRA format instead of RGBA format.
    // This code works properly on both ppc and intel, meaning the issue is
    // likely not an issue of byte order getting mixed up on different archs. 
    for (long i = 0; i < m_bytes.size() * sizeof(PixelData); i += sizeof(PixelData)) {
        p.Red() = bytes[i+2];
        p.Green() = bytes[i+1];
        p.Blue() = bytes[i+0];
        p.Alpha() = bytes[i+3];
        
        p++;

        pixelCounter++;
        if ( (pixelCounter % width() ) == 0 ) {
            rowCounter++;
            p = rowStart;
            p.MoveTo(data, 0, rowCounter);
        }

    }
#if !wxCHECK_VERSION(2,9,0)
    bmp->UseAlpha();
#endif
    ASSERT(bmp->IsOk());

#if USE(WXGC)
    wxGraphicsBitmap* bitmap =  new wxGraphicsBitmap(wxGraphicsRenderer::GetDefaultRenderer()->CreateBitmap(*bmp));
    delete bmp;
    return bitmap;
#else
    return bmp;
#endif
}

bool RGBA32Buffer::hasAlpha() const
{
    return m_hasAlpha;
}

void RGBA32Buffer::setHasAlpha(bool alpha)
{
    m_hasAlpha = alpha;
}

void RGBA32Buffer::setStatus(FrameStatus status)
{
    m_status = status;
}

RGBA32Buffer& RGBA32Buffer::operator=(const RGBA32Buffer& other)
{
    if (this == &other)
        return *this;

    copyBitmapData(other);
    setRect(other.rect());
    setStatus(other.status());
    setDuration(other.duration());
    setDisposalMethod(other.disposalMethod());
    return *this;
}

int RGBA32Buffer::width() const {
    return m_size.width();
}

int RGBA32Buffer::height() const {
    return m_size.height();
}

} // namespace WebCore
