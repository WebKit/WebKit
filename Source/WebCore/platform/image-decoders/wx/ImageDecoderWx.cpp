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

NativeImagePtr ImageFrame::asNewNativeImage() const
{
    wxBitmap* bmp = new wxBitmap(width(), height(), 32);

    {
        typedef wxPixelData<wxBitmap, wxAlphaPixelFormat> WxPixelData;
        WxPixelData data(*bmp);
        
        // NB: It appears that the data is in BGRA format instead of RGBA format.
        // This code works properly on both ppc and intel, meaning the issue is
        // likely not an issue of byte order getting mixed up on different archs. 
        const unsigned char* bytes = (const unsigned char*)m_bytes;
        int rowCounter = 0;
        long pixelCounter = 0;
        WxPixelData::Iterator p(data);
        WxPixelData::Iterator rowStart = p; 
        for (size_t i = 0; i < m_size.width() * m_size.height() * sizeof(PixelData); i += sizeof(PixelData)) {
                p.Red() = bytes[i + 2];
                p.Green() = bytes[i + 1];
                p.Blue() = bytes[i + 0];
                p.Alpha() = bytes[i + 3];
            
            p++;

            pixelCounter++;
            if ((pixelCounter % width()) == 0) {
                rowCounter++;
                p = rowStart;
                p.MoveTo(data, 0, rowCounter);
            }
        }
#if !wxCHECK_VERSION(2,9,0)
        bmp->UseAlpha();
#endif
    } // ensure that WxPixelData is destroyed as it unlocks the bitmap data in
      // its dtor and we can't access it (notably in CreateBitmap() below)
      // before this is done

    ASSERT(bmp->IsOk());

#if USE(WXGC)
    wxGraphicsBitmap* bitmap = new wxGraphicsBitmap(wxGraphicsRenderer::GetDefaultRenderer()->CreateBitmap(*bmp));
    delete bmp;
    return bitmap;
#else
    return bmp;
#endif
}

} // namespace WebCore
