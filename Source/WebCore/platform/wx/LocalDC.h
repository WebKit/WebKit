/*
 * Copyright (C) 2011 Kevin Ollivier <kevino@theolliviers.com>
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

#include "IntRect.h"

#include <wtf/Assertions.h>

#include <wx/defs.h>

#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/rawbmp.h>

namespace WebCore {

wxBitmap* transparentBitmap(int width, int height);

class LocalDC {

public:
    LocalDC(wxDC* host, const IntRect& r)
    {
#ifndef __WXMAC__
        m_host = host;
        int width = r.width();
        int height = r.height();
        m_bitmap = new wxBitmap(width, height, 32);
        // we scope this to make sure that wxAlphaPixelData isn't holding a ref
        // to m_bitmap when we create the wxMemoryDC, as this will invoke a copy op.
        {
            wxAlphaPixelData pixData(*m_bitmap, wxPoint(0,0), wxSize(width, height));
            ASSERT(pixData);
            if (pixData) {
                wxAlphaPixelData::Iterator p(pixData);
                for (int y=0; y<height; y++) {
                    wxAlphaPixelData::Iterator rowStart = p;
                    for (int x=0; x<width; x++) {
                        p.Alpha() = 0;
                        ++p; 
                    }
                    p = rowStart;
                    p.OffsetY(pixData, 1);
                }
            }
        }

        m_context = new wxMemoryDC(*m_bitmap);
        m_context->SetDeviceOrigin(-r.x(), -r.y());
        m_rect = r;
#else
        m_context = host;
#endif
    }

    wxDC* context() { return m_context; }

    ~LocalDC()
    {
#ifndef __WXMAC__
        delete m_context;
        m_host->DrawBitmap(*m_bitmap, m_rect.x(), m_rect.y());
        delete m_bitmap;
#endif
    }

private:
    wxDC* m_host;
    wxDC* m_context;
    wxBitmap* m_bitmap;
    IntRect m_rect;
            
};

}

