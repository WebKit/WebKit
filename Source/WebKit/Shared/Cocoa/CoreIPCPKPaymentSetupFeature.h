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
#include <wtf/OptionSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS PKPaymentSetupFeature;

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKPAYMENTSETUPFEATURE)

namespace WebKit {

enum class PKPaymentSetupFeatureType : uint8_t {
    ApplePay = 0,
    AppleCard = 1,
    AppleBalance = 2,
    Transit = 3,
};

enum class PKPaymentSetupFeatureState : uint8_t {
    Unsupported = 0,
    Supported = 1,
    SupplementarySupported = 2,
    Completed = 3,
};

enum class PKPaymentSetupFeatureSupportedOptions : uint8_t {
    None = 0,
    Installments = 1 << 0,
};

enum class PKPaymentSetupFeatureSupportedDevices : uint8_t {
    None = 0,
    Phone = 1 << 0,
    Watch = 1 << 1,
};

struct CoreIPCPKPaymentSetupFeatureData {
    std::optional<Vector<RetainPtr<NSString>>> identifiers;
    RetainPtr<NSString> localizedDisplayName;
    std::optional<PKPaymentSetupFeatureType> type;
    std::optional<PKPaymentSetupFeatureState> state;
    std::optional<OptionSet<PKPaymentSetupFeatureSupportedOptions>> supportedOptions;
    std::optional<OptionSet<PKPaymentSetupFeatureSupportedDevices>> supportedDevices;
    RetainPtr<NSString> productIdentifier;
    RetainPtr<NSString> partnerIdentifier;
    RetainPtr<NSNumber> featureIdentifier;
    RetainPtr<NSDate> lastUpdated;
    RetainPtr<NSDate> expiry;
    RetainPtr<NSNumber> productType;
    RetainPtr<NSNumber> productState;
    RetainPtr<NSString> notificationTitle;
    RetainPtr<NSString> notificationMessage;
    RetainPtr<NSString> discoveryCardIdentifier;
};

class CoreIPCPKPaymentSetupFeature {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCPKPaymentSetupFeature);
public:
    CoreIPCPKPaymentSetupFeature(PKPaymentSetupFeature *);
    CoreIPCPKPaymentSetupFeature(std::optional<CoreIPCPKPaymentSetupFeatureData>&&);
    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCPKPaymentSetupFeature>;

    std::optional<CoreIPCPKPaymentSetupFeatureData> m_data;
};

} // namespace WebKit

#endif
