/*
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RenderSVGResourcePaintServer;

enum class SVGPaintType : bool { Fill, Stroke };

// Cached fill and stroke paint server resolution, held by the renderers that paint fill and stroke
// (shapes and SVG text). The weak pointer clears itself if the paint server dies, so a live pointer
// is a hit and a null one re-resolves. invalidateSVGPaintServerCache() drops it when the fill or
// stroke style changes, and when the referenced paint server changes.
class SVGPaintServerCache final : public CanMakeCheckedPtr<SVGPaintServerCache, WTF::DefaultedOperatorEqual::No, WTF::CheckedPtrDeleteCheckException::Yes> {
    WTF_MAKE_TZONE_ALLOCATED(SVGPaintServerCache);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGPaintServerCache);
public:
    // Defined in SVGPaintServerCacheInlines.h, they need the complete RenderSVGResourcePaintServer type.
    inline RenderSVGResourcePaintServer* paintServer(SVGPaintType) const;
    inline void setPaintServer(SVGPaintType, RenderSVGResourcePaintServer&);

    void clear()
    {
        m_fillPaintServer = nullptr;
        m_strokePaintServer = nullptr;
    }

private:
    SingleThreadWeakPtr<RenderSVGResourcePaintServer> m_fillPaintServer;
    SingleThreadWeakPtr<RenderSVGResourcePaintServer> m_strokePaintServer;
};

} // namespace WebCore
