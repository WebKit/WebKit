/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "BidiContext.h"

#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

namespace WebCore {

struct SameSizeAsBidiContext : public ThreadSafeRefCounted<SameSizeAsBidiContext> {
    uint32_t bitfields : 16;
    void* parent;
};

COMPILE_ASSERT(sizeof(BidiContext) == sizeof(SameSizeAsBidiContext), BidiContext_should_stay_small);

inline BidiContext::BidiContext(unsigned char level, UCharDirection direction, bool override, BidiEmbeddingSource source, BidiContext* parent)
    : m_level(level)
    , m_direction(direction)
    , m_override(override)
    , m_source(source)
    , m_parent(parent)
{
}

inline Ref<BidiContext> BidiContext::createUncached(unsigned char level, UCharDirection direction, bool override, BidiEmbeddingSource source, BidiContext* parent)
{
    return adoptRef(*new BidiContext(level, direction, override, source, parent));
}

Ref<BidiContext> BidiContext::create(unsigned char level, UCharDirection direction, bool override, BidiEmbeddingSource source, BidiContext* parent)
{
    ASSERT(direction == (level % 2 ? U_RIGHT_TO_LEFT : U_LEFT_TO_RIGHT));

    if (parent)
        return createUncached(level, direction, override, source, parent);

    ASSERT(level <= 1);
    if (!level) {
        if (!override) {
            static NeverDestroyed<RefPtr<BidiContext>> ltrContext;
            static std::once_flag ltrContextOnceFlag;
            std::call_once(ltrContextOnceFlag, [&]() {
                ltrContext.get() = createUncached(0, U_LEFT_TO_RIGHT, false, FromStyleOrDOM, 0);
            });
            return *ltrContext.get();
        }

        static NeverDestroyed<RefPtr<BidiContext>> ltrOverrideContext;
        static std::once_flag ltrOverrideContextOnceFlag;
        std::call_once(ltrOverrideContextOnceFlag, [&]() {
            ltrOverrideContext.get() = createUncached(0, U_LEFT_TO_RIGHT, true, FromStyleOrDOM, 0);
        });
        return *ltrOverrideContext.get();
    }

    if (!override) {
        static NeverDestroyed<RefPtr<BidiContext>> rtlContext;
        static std::once_flag rtlContextOnceFlag;
        std::call_once(rtlContextOnceFlag, [&]() {
            rtlContext.get() = createUncached(1, U_RIGHT_TO_LEFT, false, FromStyleOrDOM, 0);
        });
        return *rtlContext.get();
    }

    static NeverDestroyed<RefPtr<BidiContext>> rtlOverrideContext;
    static std::once_flag rtlOverrideContextOnceFlag;
    std::call_once(rtlOverrideContextOnceFlag, [&]() {
        rtlOverrideContext.get() = createUncached(1, U_RIGHT_TO_LEFT, true, FromStyleOrDOM, 0);
    });
    return *rtlOverrideContext.get();
}

static inline Ref<BidiContext> copyContextAndRebaselineLevel(BidiContext& context, BidiContext* parent)
{
    auto newLevel = parent ? parent->level() : 0;
    if (context.dir() == U_RIGHT_TO_LEFT)
        newLevel = nextGreaterOddLevel(newLevel);
    else if (parent)
        newLevel = nextGreaterEvenLevel(newLevel);
    return BidiContext::create(newLevel, context.dir(), context.override(), context.source(), parent);
}

// The BidiContext stack must be immutable -- they're re-used for re-layout after
// DOM modification/editing -- so we copy all the non-Unicode contexts, and
// recalculate their levels.
Ref<BidiContext> BidiContext::copyStackRemovingUnicodeEmbeddingContexts()
{
    Vector<BidiContext*, 64> contexts;
    for (auto* ancestor = this; ancestor; ancestor = ancestor->parent()) {
        if (ancestor->source() != FromUnicode)
            contexts.append(ancestor);
    }
    ASSERT(contexts.size());
    auto topContext = copyContextAndRebaselineLevel(*contexts.last(), nullptr);
    for (unsigned i = contexts.size() - 1; i; --i)
        topContext = copyContextAndRebaselineLevel(*contexts[i - 1], topContext.ptr());
    return topContext;
}

bool operator==(const BidiContext& c1, const BidiContext& c2)
{
    if (&c1 == &c2)
        return true;
    if (c1.level() != c2.level() || c1.override() != c2.override() || c1.dir() != c2.dir() || c1.source() != c2.source())
        return false;
    if (!c1.parent())
        return !c2.parent();
    return c2.parent() && *c1.parent() == *c2.parent();
}

} // namespace WebCore
