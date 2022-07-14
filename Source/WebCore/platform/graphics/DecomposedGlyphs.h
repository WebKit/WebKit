/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FloatRect.h"
#include "PositionedGlyphs.h"
#include "RenderingResourceIdentifier.h"
#include <wtf/HashSet.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class DecomposedGlyphs : public ThreadSafeRefCounted<DecomposedGlyphs, WTF::DestructionThread::Main>, public CanMakeWeakPtr<DecomposedGlyphs> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void releaseDecomposedGlyphs(RenderingResourceIdentifier) = 0;
    protected:
        Observer() = default;
    };

    static WEBCORE_EXPORT Ref<DecomposedGlyphs> create(const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());
    static WEBCORE_EXPORT Ref<DecomposedGlyphs> create(PositionedGlyphs&&, RenderingResourceIdentifier);
    WEBCORE_EXPORT ~DecomposedGlyphs();

    const PositionedGlyphs& positionedGlyphs() const { return m_positionedGlyphs; }

    void addObserver(Observer& observer) { m_observers.add(&observer); }
    void removeObserver(Observer& observer) { m_observers.remove(&observer); }

    RenderingResourceIdentifier renderingResourceIdentifier() const { return m_renderingResourceIdentifier; }

private:
    DecomposedGlyphs(const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode, RenderingResourceIdentifier);
    DecomposedGlyphs(PositionedGlyphs&&, RenderingResourceIdentifier);

    PositionedGlyphs m_positionedGlyphs;
    HashSet<Observer*> m_observers;
    RenderingResourceIdentifier m_renderingResourceIdentifier;
};

} // namespace WebCore
