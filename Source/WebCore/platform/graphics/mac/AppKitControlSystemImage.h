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

#if USE(APPKIT)

#include "SystemImage.h"
#include <optional>
#include <wtf/Forward.h>
#include <wtf/Ref.h>

namespace WebCore {

class Color;

enum class AppKitControlSystemImageType : uint8_t {
    ScrollbarTrackCorner,
};

class WEBCORE_EXPORT AppKitControlSystemImage : public SystemImage {
public:
    virtual ~AppKitControlSystemImage() = default;

    void draw(GraphicsContext&, const FloatRect&) const final;

    virtual void drawControl(GraphicsContext&, const FloatRect&) const { }

    AppKitControlSystemImageType controlType() const { return m_controlType; }

    Color tintColor() const { return m_tintColor; }
    void setTintColor(const Color& tintColor) { m_tintColor = tintColor; }

    bool useDarkAppearance() const { return m_useDarkAppearance; }
    void setUseDarkAppearance(bool useDarkAppearance) { m_useDarkAppearance = useDarkAppearance; }

protected:
    AppKitControlSystemImage(AppKitControlSystemImageType);

private:
    AppKitControlSystemImageType m_controlType;

    Color m_tintColor;
    bool m_useDarkAppearance { false };
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::AppKitControlSystemImageType> {
    using values = EnumValues<
        WebCore::AppKitControlSystemImageType,
        WebCore::AppKitControlSystemImageType::ScrollbarTrackCorner
    >;
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AppKitControlSystemImage)
    static bool isType(const WebCore::SystemImage& systemImage) { return systemImage.systemImageType() == WebCore::SystemImageType::AppKitControl; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(APPKIT)
