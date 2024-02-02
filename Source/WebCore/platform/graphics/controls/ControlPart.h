/*
 * Copyright (C) 2022 Apple Inc. All Rights Reserved.
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

#include "Color.h"
#include "ControlAppearance.h"
#include "StyleAppearance.h"

#include <wtf/RefCounted.h>

namespace WebCore {

class FloatRect;
class FloatRoundedRect;
class GraphicsContext;
class ControlFactory;
class PlatformControl;
struct ControlStyle;

class ControlPart : public RefCounted<ControlPart> {
public:
    WEBCORE_EXPORT static Ref<ControlPart> create(const ControlAppearance&);
    virtual ~ControlPart() = default;

    StyleAppearance type() const;
    WEBCORE_EXPORT ControlAppearance& appearance() const;

    template <typename AppearanceType>
    const AppearanceType& get() const
    {
        ASSERT(AppearanceType::appearance == type());
        return std::get<AppearanceType>(m_appearance);
    }

    WEBCORE_EXPORT ControlFactory& controlFactory() const;
    void setControlFactory(ControlFactory* controlFactory) { m_controlFactory = controlFactory; }

    FloatSize sizeForBounds(const FloatRect& bounds, const ControlStyle&);
    FloatRect rectForBounds(const FloatRect& bounds, const ControlStyle&);
    void draw(GraphicsContext&, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle&) const;

protected:
    ControlPart(const ControlAppearance&);

    PlatformControl* platformControl() const;
    std::unique_ptr<PlatformControl> createPlatformControl();

    ControlAppearance m_appearance;
    mutable std::unique_ptr<PlatformControl> m_platformControl;
    ControlFactory* m_controlFactory { nullptr };
};

} // namespace WebCore
