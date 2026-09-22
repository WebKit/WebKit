/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2026 Igalia S.L.
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
 */

#pragma once

#if USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
#include "AcceleratedAnimation.h"

namespace WebCore {

class AcceleratedAnimations {
public:
    AcceleratedAnimations() = default;
    ~AcceleratedAnimations() = default;

    struct Entry {
        void apply(AcceleratedAnimation::ApplyResult& applyResult, MonotonicTime time) const
        {
            animation->apply(applyResult, playback, time);
        }

        Ref<const AcceleratedAnimation> animation;
        AcceleratedAnimation::Playback playback;
    };

    struct Transforms {
        TransformationMatrix translate;
        TransformationMatrix rotate;
        TransformationMatrix scale;
        TransformationMatrix transform;
    };
    void setTransforms(Transforms&& transforms) { m_transforms = WTF::move(transforms); }

    void add(Ref<const AcceleratedAnimation>&&, MonotonicTime);
    void remove(const String&);
    void pause(const String&, Seconds);

    void apply(AcceleratedAnimation::ApplyResult&, MonotonicTime) const;

    bool isEmpty() const { return m_animations.isEmpty(); }
    const Vector<Entry>& animations() const LIFETIME_BOUND { return m_animations; }

    bool hasActiveAnimationsOfType(AnimatedProperty) const;
    bool hasRunningTransformAnimations() const;

private:
    Transforms m_transforms;
    Vector<Entry> m_animations;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && !USE(TEXTURE_MAPPER)
