/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CanvasPattern_h
#define CanvasPattern_h

#include "CachedResourceClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if PLATFORM(CG)
#include <wtf/RetainPtr.h>
#include <ApplicationServices/ApplicationServices.h>
#elif PLATFORM(CAIRO)
#include <cairo.h>
#endif

namespace WebCore {

    class CachedImage;
    class String;

    typedef int ExceptionCode;

    class CanvasPattern : public RefCounted<CanvasPattern>, CachedResourceClient {
    public:
        static void parseRepetitionType(const String&, bool& repeatX, bool& repeatY, ExceptionCode&);

#if PLATFORM(CG)
        static PassRefPtr<CanvasPattern> create(CGImageRef image, bool repeatX, bool repeatY, bool originClean)
        {
            return adoptRef(new CanvasPattern(image, repeatX, repeatY, originClean));
        }
#elif PLATFORM(CAIRO)
        static PassRefPtr<CanvasPattern> create(cairo_surface_t* surface, bool repeatX, bool repeatY, bool originClean)
        {
            return adoptRef(new CanvasPattern(surface, repeatX, repeatY, originClean));
        }
#endif
        static PassRefPtr<CanvasPattern> create(CachedImage* image, bool repeatX, bool repeatY, bool originClean)
        {
            return adoptRef(new CanvasPattern(image, repeatX, repeatY, originClean));
        }
        virtual ~CanvasPattern();

#if PLATFORM(CG)
        CGImageRef platformImage() const { return m_platformImage.get(); }
#elif PLATFORM(CAIRO)
        cairo_surface_t* platformImage() const { return m_platformImage; }
#endif
        CachedImage* cachedImage() const { return m_cachedImage; }

#if PLATFORM(CG)
        CGPatternRef createPattern(const CGAffineTransform&);
#elif PLATFORM(CAIRO)
        cairo_pattern_t* createPattern(const cairo_matrix_t&);
#endif

        bool originClean() const { return m_originClean; }

    private:
#if PLATFORM(CG)
        CanvasPattern(CGImageRef, bool repeatX, bool repeatY, bool originClean);
#elif PLATFORM(CAIRO)
        CanvasPattern(cairo_surface_t*, bool repeatX, bool repeatY, bool originClean);
#endif
        CanvasPattern(CachedImage*, bool repeatX, bool repeatY, bool originClean);

#if PLATFORM(CG)
        const RetainPtr<CGImageRef> m_platformImage;
#elif PLATFORM(CAIRO)
        cairo_surface_t* const m_platformImage;
#endif
        CachedImage* const m_cachedImage;
        const bool m_repeatX;
        const bool m_repeatY;
        bool m_originClean;
    };

} // namespace WebCore

#endif
