/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>
#include <wtf/ScopedLambda.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class ImageEnvironmentSettings;
class SVGImage;
class SVGImageInstance;
struct SVGImageDocumentState;

class SVGImageInstanceCache {
    WTF_MAKE_TZONE_ALLOCATED(SVGImageInstanceCache);
    WTF_MAKE_NONCOPYABLE(SVGImageInstanceCache);
public:
    SVGImageInstanceCache() = default;
    ~SVGImageInstanceCache();

    void reset(RefPtr<SVGImageInstance>&& defaultInstance);

    SVGImageInstance* defaultInstance() const { return m_defaultInstance.get(); }
    bool isEmpty() const { return m_entries.isEmpty(); }

    struct Lookup {
        Ref<SVGImageInstance> instance;
        bool tookPlayingInstance { false };
    };
    Lookup instanceForDraw(SVGImage&, const ImageEnvironmentSettings*, const SVGImageDocumentState&);
    SVGImageInstance* matchingInstance(const ImageEnvironmentSettings*, const SVGImageDocumentState&) const;

    void removeInstances(size_t keepCount);
    void removeInstancesHeldByGoneTimelines();

    long long takeDecodedSizeChange() { return std::exchange(m_unreportedDecodedSizeChange, 0); }

    void forEach(NOESCAPE const ScopedLambda<void(SVGImageInstance&)>&) const;
    bool containsIf(NOESCAPE const ScopedLambda<bool(const SVGImageInstance&)>&) const;

private:
    std::optional<size_t> findMatching(const ImageEnvironmentSettings*, const SVGImageDocumentState&) const;
    std::optional<size_t> findPlayingToHold(const ImageEnvironmentSettings*, const SVGImageDocumentState&) const;
    std::optional<size_t> findToRestyle(const ImageEnvironmentSettings*) const;
    Ref<SVGImageInstance> create(SVGImage&, const ImageEnvironmentSettings*, const SVGImageDocumentState&);
    SVGImageInstance* animationTimesSource(const ImageEnvironmentSettings*) const;
    Ref<SVGImageInstance> moveToFront(size_t index);

    RefPtr<SVGImageInstance> m_defaultInstance;

    struct Entry {
        Ref<SVGImageInstance> instance;
        size_t decodedSize { 0 };
    };
    Vector<Entry> m_entries;

    long long m_unreportedDecodedSizeChange { 0 };
};

} // namespace WebCore
