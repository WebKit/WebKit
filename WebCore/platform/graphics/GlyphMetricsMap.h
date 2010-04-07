/*
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef GlyphMetricsMap_h
#define GlyphMetricsMap_h

#include "FloatRect.h"
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

typedef unsigned short Glyph;

const float cGlyphSizeUnknown = -1;

struct GlyphMetrics {
    float horizontalAdvance;
    FloatRect boundingBox;
};

class GlyphMetricsMap : public Noncopyable {
public:
    GlyphMetricsMap() : m_filledPrimaryPage(false) { }
    ~GlyphMetricsMap()
    { 
        if (m_pages)
            deleteAllValues(*m_pages);
    }

    GlyphMetrics metricsForGlyph(Glyph glyph)
    {
        return locatePage(glyph / GlyphMetricsPage::size)->metricsForGlyph(glyph);
    }

    float widthForGlyph(Glyph glyph)
    {
        return locatePage(glyph / GlyphMetricsPage::size)->metricsForGlyph(glyph).horizontalAdvance;
    }

    void setMetricsForGlyph(Glyph glyph, const GlyphMetrics& metrics)
    {
        locatePage(glyph / GlyphMetricsPage::size)->setMetricsForGlyph(glyph, metrics);
    }

private:
    struct GlyphMetricsPage {
        static const size_t size = 256; // Usually covers Latin-1 in a single page.
        GlyphMetrics m_metrics[size];

        GlyphMetrics metricsForGlyph(Glyph glyph) const { return m_metrics[glyph % size]; }
        void setMetricsForGlyph(Glyph glyph, const GlyphMetrics& metrics)
        {
            setMetricsForIndex(glyph % size, metrics);
        }
        void setMetricsForIndex(unsigned index, const GlyphMetrics& metrics)
        {
            m_metrics[index] = metrics;
        }
    };
    
    GlyphMetricsPage* locatePage(unsigned pageNumber)
    {
        if (!pageNumber && m_filledPrimaryPage)
            return &m_primaryPage;
        return locatePageSlowCase(pageNumber);
    }

    GlyphMetricsPage* locatePageSlowCase(unsigned pageNumber);
    
    bool m_filledPrimaryPage;
    GlyphMetricsPage m_primaryPage; // We optimize for the page that contains glyph indices 0-255.
    OwnPtr<HashMap<int, GlyphMetricsPage*> > m_pages;
};

} // namespace WebCore

#endif
