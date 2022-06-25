/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include <wtf/EnumTraits.h>
#include <wtf/IsoMalloc.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
using PlatformNativeValue = id;
using PlatformNativeValueSmartPtr = RetainPtr<PlatformNativeValue>;
#else
using PlatformNativeValue = void*;
using PlatformNativeValueSmartPtr = PlatformNativeValue;
#endif

namespace WebCore {

class SerializedPlatformDataCueValue {
    WTF_MAKE_ISO_ALLOCATED(SerializedPlatformDataCueValue);
public:
    enum class PlatformType : bool {
        None,
        ObjC,
    };

    SerializedPlatformDataCueValue(PlatformType platformType, PlatformNativeValue nativeValue)
        : m_nativeValue(nativeValue)
        , m_type(platformType)
    {
    }
    SerializedPlatformDataCueValue() = default;
    ~SerializedPlatformDataCueValue() = default;

    PlatformType platformType() const { return m_type; }

    PlatformNativeValueSmartPtr nativeValue() const { return m_nativeValue; }

    bool encodingRequiresPlatformData() const { return m_type == PlatformType::ObjC; }

protected:
    PlatformNativeValueSmartPtr m_nativeValue { nullptr };
    PlatformType m_type { PlatformType::None };
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
