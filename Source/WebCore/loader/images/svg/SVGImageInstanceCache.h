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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGImageInstance.h"
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class SVGImageInstanceCache {
    WTF_MAKE_TZONE_ALLOCATED(SVGImageInstanceCache);
    WTF_MAKE_NONCOPYABLE(SVGImageInstanceCache);
public:
    SVGImageInstanceCache() = default;
    ~SVGImageInstanceCache();

    void reset(RefPtr<SVGImageInstance>&& defaultInstance);

    SVGImageInstance* defaultInstance() const { return m_defaultInstance.get(); }
    SVGImageInstance* find(const SVGImageInstanceKey&) const;
    void add(Ref<SVGImageInstance>&&);

    void forEach(NOESCAPE const auto& function) const
    {
        if (RefPtr defaultInstance = m_defaultInstance)
            function(*defaultInstance);
        for (auto& instance : m_otherInstances)
            function(instance.get());
    }

    bool anyOf(NOESCAPE const auto& predicate) const
    {
        if (RefPtr defaultInstance = m_defaultInstance; defaultInstance && predicate(*defaultInstance))
            return true;
        return m_otherInstances.containsIf([&](auto& instance) {
            return predicate(instance.get());
        });
    }

private:
    void detachAll();

    RefPtr<SVGImageInstance> m_defaultInstance;
    Vector<Ref<SVGImageInstance>> m_otherInstances;
};

} // namespace WebCore
