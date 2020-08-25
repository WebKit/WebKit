/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "CacheModel.h"
#include "WebsiteDataStore.h"
#include <wtf/ProcessID.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace API {

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#define DEFAULT_CAPTURE_DISPLAY_IN_UI_PROCESS true
#else
#define DEFAULT_CAPTURE_DISPLAY_IN_UI_PROCESS false
#endif

class ProcessPoolConfiguration final : public ObjectImpl<Object::Type::ProcessPoolConfiguration> {
public:
    static Ref<ProcessPoolConfiguration> create();

    explicit ProcessPoolConfiguration();
    virtual ~ProcessPoolConfiguration();
    
    Ref<ProcessPoolConfiguration> copy();

    bool usesSingleWebProcess() const { return m_usesSingleWebProcess; }
    void setUsesSingleWebProcess(bool enabled) { m_usesSingleWebProcess = enabled; }

    bool isAutomaticProcessWarmingEnabled() const
    {
        return m_isAutomaticProcessWarmingEnabledByClient.valueOr(m_clientWouldBenefitFromAutomaticProcessPrewarming);
    }

    bool wasAutomaticProcessWarmingSetByClient() const { return !!m_isAutomaticProcessWarmingEnabledByClient; }
    void setIsAutomaticProcessWarmingEnabled(bool value) { m_isAutomaticProcessWarmingEnabledByClient = value; }

    void setUsesWebProcessCache(bool value) { m_usesWebProcessCache = value; }
    bool usesWebProcessCache() const { return m_usesWebProcessCache; }

    bool clientWouldBenefitFromAutomaticProcessPrewarming() const { return m_clientWouldBenefitFromAutomaticProcessPrewarming; }
    void setClientWouldBenefitFromAutomaticProcessPrewarming(bool value) { m_clientWouldBenefitFromAutomaticProcessPrewarming = value; }

    void setUsesBackForwardCache(bool value) { m_usesBackForwardCache = value; }
    bool usesBackForwardCache() const { return m_usesBackForwardCache; }

    const WTF::String& injectedBundlePath() const { return m_injectedBundlePath; }
    void setInjectedBundlePath(const WTF::String& injectedBundlePath) { m_injectedBundlePath = injectedBundlePath; }

    const Vector<WTF::String>& customClassesForParameterCoder() const { return m_customClassesForParameterCoder; }
    void setCustomClassesForParameterCoder(Vector<WTF::String>&& classesForCoder) { m_customClassesForParameterCoder = WTFMove(classesForCoder); }

    const Vector<WTF::String>& cachePartitionedURLSchemes() { return m_cachePartitionedURLSchemes; }
    void setCachePartitionedURLSchemes(Vector<WTF::String>&& cachePartitionedURLSchemes) { m_cachePartitionedURLSchemes = WTFMove(cachePartitionedURLSchemes); }

    const Vector<WTF::String>& alwaysRevalidatedURLSchemes() { return m_alwaysRevalidatedURLSchemes; }
    void setAlwaysRevalidatedURLSchemes(Vector<WTF::String>&& alwaysRevalidatedURLSchemes) { m_alwaysRevalidatedURLSchemes = WTFMove(alwaysRevalidatedURLSchemes); }

    const Vector<WTF::CString>& additionalReadAccessAllowedPaths() { return m_additionalReadAccessAllowedPaths; }
    void setAdditionalReadAccessAllowedPaths(Vector<WTF::CString>&& additionalReadAccessAllowedPaths) { m_additionalReadAccessAllowedPaths = additionalReadAccessAllowedPaths; }

    bool fullySynchronousModeIsAllowedForTesting() const { return m_fullySynchronousModeIsAllowedForTesting; }
    void setFullySynchronousModeIsAllowedForTesting(bool allowed) { m_fullySynchronousModeIsAllowedForTesting = allowed; }

    bool ignoreSynchronousMessagingTimeoutsForTesting() const { return m_ignoreSynchronousMessagingTimeoutsForTesting; }
    void setIgnoreSynchronousMessagingTimeoutsForTesting(bool allowed) { m_ignoreSynchronousMessagingTimeoutsForTesting = allowed; }

    bool attrStyleEnabled() const { return m_attrStyleEnabled; }
    void setAttrStyleEnabled(bool enabled) { m_attrStyleEnabled = enabled; }

    const Vector<WTF::String>& overrideLanguages() const { return m_overrideLanguages; }
    void setOverrideLanguages(Vector<WTF::String>&& languages) { m_overrideLanguages = WTFMove(languages); }
    
    bool alwaysRunsAtBackgroundPriority() const { return m_alwaysRunsAtBackgroundPriority; }
    void setAlwaysRunsAtBackgroundPriority(bool alwaysRunsAtBackgroundPriority) { m_alwaysRunsAtBackgroundPriority = alwaysRunsAtBackgroundPriority; }

    bool shouldTakeUIBackgroundAssertion() const { return m_shouldTakeUIBackgroundAssertion; }
    void setShouldTakeUIBackgroundAssertion(bool shouldTakeUIBackgroundAssertion) { m_shouldTakeUIBackgroundAssertion = shouldTakeUIBackgroundAssertion; }

    bool shouldCaptureDisplayInUIProcess() const { return m_shouldCaptureDisplayInUIProcess; }
    void setShouldCaptureDisplayInUIProcess(bool shouldCaptureDisplayInUIProcess) { m_shouldCaptureDisplayInUIProcess = shouldCaptureDisplayInUIProcess; }

    bool shouldConfigureJSCForTesting() const { return m_shouldConfigureJSCForTesting; }
    void setShouldConfigureJSCForTesting(bool value) { m_shouldConfigureJSCForTesting = value; }
    bool isJITEnabled() const { return m_isJITEnabled; }
    void setJITEnabled(bool enabled) { m_isJITEnabled = enabled; }

    ProcessID presentingApplicationPID() const { return m_presentingApplicationPID; }
    void setPresentingApplicationPID(ProcessID pid) { m_presentingApplicationPID = pid; }

    bool processSwapsOnNavigation() const
    {
        return m_processSwapsOnNavigationFromClient.valueOr(m_processSwapsOnNavigationFromExperimentalFeatures);
    }
    void setProcessSwapsOnNavigation(bool swaps) { m_processSwapsOnNavigationFromClient = swaps; }
    void setProcessSwapsOnNavigationFromExperimentalFeatures(bool swaps) { m_processSwapsOnNavigationFromExperimentalFeatures = swaps; }

    bool alwaysKeepAndReuseSwappedProcesses() const { return m_alwaysKeepAndReuseSwappedProcesses; }
    void setAlwaysKeepAndReuseSwappedProcesses(bool keepAndReuse) { m_alwaysKeepAndReuseSwappedProcesses = keepAndReuse; }

    bool processSwapsOnWindowOpenWithOpener() const { return m_processSwapsOnWindowOpenWithOpener; }
    void setProcessSwapsOnWindowOpenWithOpener(bool swaps) { m_processSwapsOnWindowOpenWithOpener = swaps; }

    const WTF::String& customWebContentServiceBundleIdentifier() const { return m_customWebContentServiceBundleIdentifier; }
    void setCustomWebContentServiceBundleIdentifier(const WTF::String& customWebContentServiceBundleIdentifier) { m_customWebContentServiceBundleIdentifier = customWebContentServiceBundleIdentifier; }

#if PLATFORM(GTK) && !USE(GTK4)
    bool useSystemAppearanceForScrollbars() const { return m_useSystemAppearanceForScrollbars; }
    void setUseSystemAppearanceForScrollbars(bool useSystemAppearanceForScrollbars) { m_useSystemAppearanceForScrollbars = useSystemAppearanceForScrollbars; }
#endif

#if PLATFORM(PLAYSTATION)
    const WTF::String& webProcessPath() const { return m_webProcessPath; }
    void setWebProcessPath(const WTF::String& webProcessPath) { m_webProcessPath = webProcessPath; }

    const WTF::String& networkProcessPath() const { return m_networkProcessPath; }
    void setNetworkProcessPath(const WTF::String& networkProcessPath) { m_networkProcessPath = networkProcessPath; }

    int32_t userId() const { return m_userId; }
    void setUserId(const int32_t userId) { m_userId = userId; }
#endif

private:
    WTF::String m_injectedBundlePath;
    Vector<WTF::String> m_customClassesForParameterCoder;
    Vector<WTF::String> m_cachePartitionedURLSchemes;
    Vector<WTF::String> m_alwaysRevalidatedURLSchemes;
    Vector<WTF::CString> m_additionalReadAccessAllowedPaths;
    bool m_fullySynchronousModeIsAllowedForTesting { false };
    bool m_ignoreSynchronousMessagingTimeoutsForTesting { false };
    bool m_attrStyleEnabled { false };
    Vector<WTF::String> m_overrideLanguages;
    bool m_alwaysRunsAtBackgroundPriority { false };
    bool m_shouldTakeUIBackgroundAssertion { true };
    bool m_shouldCaptureDisplayInUIProcess { DEFAULT_CAPTURE_DISPLAY_IN_UI_PROCESS };
    ProcessID m_presentingApplicationPID { getCurrentProcessID() };
    Optional<bool> m_processSwapsOnNavigationFromClient;
    bool m_processSwapsOnNavigationFromExperimentalFeatures { false };
    bool m_alwaysKeepAndReuseSwappedProcesses { false };
    bool m_processSwapsOnWindowOpenWithOpener { false };
    Optional<bool> m_isAutomaticProcessWarmingEnabledByClient;
    bool m_usesWebProcessCache { false };
    bool m_usesBackForwardCache { true };
    bool m_clientWouldBenefitFromAutomaticProcessPrewarming { false };
    WTF::String m_customWebContentServiceBundleIdentifier;
    bool m_shouldConfigureJSCForTesting { false };
    bool m_isJITEnabled { true };
    bool m_usesSingleWebProcess { false };
#if PLATFORM(GTK) && !USE(GTK4)
    bool m_useSystemAppearanceForScrollbars { false };
#endif
#if PLATFORM(PLAYSTATION)
    WTF::String m_webProcessPath;
    WTF::String m_networkProcessPath;
    int32_t m_userId { -1 };
#endif
};

} // namespace API
