/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS PKDateComponentsRange;
OBJC_CLASS PKShippingMethod;

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKSHIPPINGMETHOD)

namespace WebKit {

enum class PKPaymentSummaryItemType : uint8_t {
    Final = 0,
    Pending = 1,
};

struct CoreIPCPKShippingMethodData {
    RetainPtr<NSString> label;
    RetainPtr<NSNumber> amount;
    std::optional<PKPaymentSummaryItemType> type;
    RetainPtr<NSString> localizedTitle;
    RetainPtr<NSString> localizedAmount;
    std::optional<bool> useDarkColor;
    std::optional<bool> useLargeFont;
    RetainPtr<NSString> identifier;
    RetainPtr<NSString> detail;
    RetainPtr<PKDateComponentsRange> dateComponentsRange;
};

class CoreIPCPKShippingMethod {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCPKShippingMethod);
public:
    CoreIPCPKShippingMethod(PKShippingMethod *);
    CoreIPCPKShippingMethod(std::optional<CoreIPCPKShippingMethodData>&&);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCPKShippingMethod>;

    std::optional<CoreIPCPKShippingMethodData> m_data;
};

} // namespace WebKit

#endif
