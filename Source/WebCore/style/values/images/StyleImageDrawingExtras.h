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

#include <WebCore/ImageAnimationTimeline.h>
#include <WebCore/ImageDrawingExtras.h>
#include <WebCore/ImageEnvironmentSettings.h>
#include <WebCore/StyleLinkParameters.h>
#include <wtf/TypeCasts.h>
#include <wtf/URL.h>

namespace WebCore {
namespace Style {

class WEBCORE_EXPORT ImageDrawingExtras final : public WebCore::ImageDrawingExtras {
public:
    ImageDrawingExtras(WTF::URL fragmentURL = { }, LinkParameters linkParameters = { CSS::Keyword::None { } }, std::optional<bool> useDarkAppearance = std::nullopt, std::optional<bool> useElevatedUserInterfaceLevel = std::nullopt, RefPtr<const ImageEnvironmentSettings> environmentSettings = nullptr, RefPtr<ImageAnimationTimeline> animationTimeline = nullptr)
        : WebCore::ImageDrawingExtras(Type::Style)
        , m_fragmentURL(WTF::move(fragmentURL))
        , m_linkParameters(WTF::move(linkParameters))
        , m_useDarkAppearance(useDarkAppearance)
        , m_useElevatedUserInterfaceLevel(useElevatedUserInterfaceLevel)
        , m_environmentSettings(WTF::move(environmentSettings))
        , m_animationTimeline(WTF::move(animationTimeline))
    {
    }

    const WTF::URL& fragmentURL() const { return m_fragmentURL; }
    const LinkParameters& linkParameters() const { return m_linkParameters; }
    std::optional<bool> useDarkAppearance() const { return m_useDarkAppearance; }
    std::optional<bool> useElevatedUserInterfaceLevel() const { return m_useElevatedUserInterfaceLevel; }

    const ImageEnvironmentSettings* environmentSettings() const { return m_environmentSettings.get(); }

    ImageAnimationTimeline* animationTimeline() const final { return m_animationTimeline.get(); }

    bool hasSameEnvironment(const ImageDrawingExtras&) const;

    std::unique_ptr<WebCore::ImageDrawingExtras> copy() const final;

private:
    bool equals(const WebCore::ImageDrawingExtras&) const final;

    WTF::URL m_fragmentURL;
    LinkParameters m_linkParameters;
    std::optional<bool> m_useDarkAppearance;
    std::optional<bool> m_useElevatedUserInterfaceLevel;
    RefPtr<const ImageEnvironmentSettings> m_environmentSettings;
    RefPtr<ImageAnimationTimeline> m_animationTimeline;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Style::ImageDrawingExtras)
    static bool isType(const WebCore::ImageDrawingExtras& extras) { return extras.type() == WebCore::ImageDrawingExtras::Type::Style; }
SPECIALIZE_TYPE_TRAITS_END()
