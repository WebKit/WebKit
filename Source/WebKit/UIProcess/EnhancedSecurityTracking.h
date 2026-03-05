/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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


#include "APINavigation.h"
#include "EnhancedSecurity.h"

#include <WebCore/RegistrableDomain.h>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>

namespace WebKit {

class EnhancedSecurityTracking final : public CanMakeWeakPtr<EnhancedSecurityTracking> {
public:
    void initializeWithWebsiteDataStore(WebsiteDataStore&);

    void trackNavigation(const API::Navigation&, bool hasOpenedPage);

    bool isEnhancedSecurityEnabled() const { return isEnhancedSecurityEnabledForState(enhancedSecurityState()); }
    EnhancedSecurity NODELETE enhancedSecurityState() const;
    EnhancedSecurityReason enhancedSecurityReason() const { return m_activeReason; }

    void initializeFrom(const EnhancedSecurityTracking&);

private:
    enum class ActivationState : uint8_t { None, Dormant, Active };

    void NODELETE reset();
    void NODELETE makeDormant();
    void NODELETE makeActive();

    void handleBackForwardNavigation(const API::Navigation&);

    void enableFor(EnhancedSecurityReason, const API::Navigation&);
    bool enableIfRequired(const API::Navigation&);

    void trackSameSiteNavigation(const API::Navigation&);
    void trackChangingSiteNavigation();

    ActivationState m_activeState { ActivationState::None };
    EnhancedSecurityReason m_activeReason { EnhancedSecurityReason::None };

    WebCore::RegistrableDomain m_initialProtectedDomain;
};

} // namespace WebKit
