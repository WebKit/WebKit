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

#include <wtf/RefCounted.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;

enum class SystemImageType : uint8_t {
#if ENABLE(APPLE_PAY)
    ApplePayButton,
    ApplePayLogo,
#endif
#if USE(SYSTEM_PREVIEW)
    ARKitBadge,
#endif
#if USE(APPKIT)
    AppKitControl,
#endif
};

class WEBCORE_EXPORT SystemImage : public RefCounted<SystemImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SystemImage() = default;

    virtual void draw(GraphicsContext&, const FloatRect&) const { }

    SystemImageType systemImageType() const { return m_systemImageType; }

protected:
    SystemImage(SystemImageType systemImageType)
        : m_systemImageType(systemImageType)
    {
    }

private:
    SystemImageType m_systemImageType;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::SystemImageType> {
    using values = EnumValues<
        WebCore::SystemImageType
#if ENABLE(APPLE_PAY)
        , WebCore::SystemImageType::ApplePayButton,
        WebCore::SystemImageType::ApplePayLogo
#endif
#if USE(SYSTEM_PREVIEW)
        , WebCore::SystemImageType::ARKitBadge
#endif
#if USE(APPKIT)
        , WebCore::SystemImageType::AppKitControl
#endif
    >;
};

} // namespace WTF
