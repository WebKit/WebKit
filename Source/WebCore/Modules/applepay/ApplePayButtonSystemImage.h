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

#if ENABLE(APPLE_PAY)

#include "SystemImage.h"
#include <optional>
#include <wtf/ArgumentCoder.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class ApplePayButtonType : uint8_t {
    Plain,
    Buy,
    SetUp,
    Donate,
    CheckOut,
    Book,
    Subscribe,
#if ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
    Reload,
    AddMoney,
    TopUp,
    Order,
    Rent,
    Support,
    Contribute,
    Tip,
#endif // ENABLE(APPLE_PAY_NEW_BUTTON_TYPES)
};

enum class ApplePayButtonStyle : uint8_t {
    White,
    WhiteOutline,
    Black,
};

class WEBCORE_EXPORT ApplePayButtonSystemImage final : public SystemImage {
public:
    static Ref<ApplePayButtonSystemImage> create(ApplePayButtonType applePayButtonType, ApplePayButtonStyle applePayButtonStyle, const String& locale, float largestCornerRadius)
    {
        return adoptRef(*new ApplePayButtonSystemImage(applePayButtonType, applePayButtonStyle, locale, largestCornerRadius));
    }

    virtual ~ApplePayButtonSystemImage() = default;

    void draw(GraphicsContext&, const FloatRect&) const final;

private:
    friend struct IPC::ArgumentCoder<ApplePayButtonSystemImage, void>;
    ApplePayButtonSystemImage(ApplePayButtonType applePayButtonType, ApplePayButtonStyle applePayButtonStyle, const String& locale, float largestCornerRadius)
        : SystemImage(SystemImageType::ApplePayButton)
        , m_applePayButtonType(applePayButtonType)
        , m_applePayButtonStyle(applePayButtonStyle)
        , m_locale(locale)
        , m_largestCornerRadius(largestCornerRadius)
    {
    }

    ApplePayButtonType m_applePayButtonType;
    ApplePayButtonStyle m_applePayButtonStyle;
    String m_locale;
    float m_largestCornerRadius;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ApplePayButtonSystemImage)
    static bool isType(const WebCore::SystemImage& systemImage) { return systemImage.systemImageType() == WebCore::SystemImageType::ApplePayButton; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(APPLE_PAY)
