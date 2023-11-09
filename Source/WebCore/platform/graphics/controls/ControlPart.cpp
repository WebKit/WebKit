/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "ControlPart.h"

#include "ControlAppearance.h"
#include "ControlFactory.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"

namespace WebCore {

Ref<ControlPart> ControlPart::create(const ControlAppearance& appearance)
{
    return adoptRef(*new ControlPart(appearance));
}

ControlPart::ControlPart(const ControlAppearance& appearance)
    : m_appearance(appearance)
{
}

ControlFactory& ControlPart::controlFactory() const
{
    return m_controlFactory ? *m_controlFactory : ControlFactory::sharedControlFactory();
}

std::unique_ptr<PlatformControl> ControlPart::createPlatformControl()
{
    return WTF::switchOn(m_appearance, [&](auto& appearance) {
        return appearance.createPlatformControl(*this, controlFactory());
    });
}

PlatformControl* ControlPart::platformControl() const
{
    if (!m_platformControl)
        m_platformControl = const_cast<ControlPart&>(*this).createPlatformControl();
    return m_platformControl.get();
}

ControlAppearance& ControlPart::appearance() const
{
    return const_cast<ControlAppearance&>(m_appearance);
}

StyleAppearance ControlPart::type() const
{
    return WTF::switchOn(m_appearance, [&](const auto& appearance) {
        return std::decay_t<decltype(appearance)>::appearance;
    });
}

FloatSize ControlPart::sizeForBounds(const FloatRect& bounds, const ControlStyle& style)
{
    auto platformControl = this->platformControl();
    if (!platformControl)
        return bounds.size();

    platformControl->updateCellStates(bounds, style);
    return platformControl->sizeForBounds(bounds, style);
}

FloatRect ControlPart::rectForBounds(const FloatRect& bounds, const ControlStyle& style)
{
    auto platformControl = this->platformControl();
    if (!platformControl)
        return bounds;

    platformControl->updateCellStates(bounds, style);
    return platformControl->rectForBounds(bounds, style);
}

void ControlPart::draw(GraphicsContext& context, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style) const
{
    auto platformControl = this->platformControl();
    if (!platformControl)
        return;

    // It's important to get the clip from the context, because it may be significantly
    // smaller than the layer bounds (e.g. tiled layers)
    platformControl->setFocusRingClipRect(context.clipBounds());

    platformControl->updateCellStates(borderRect.rect(), style);
    platformControl->draw(context, borderRect, deviceScaleFactor, style);

    platformControl->setFocusRingClipRect({ });
}

} // namespace WebCore
