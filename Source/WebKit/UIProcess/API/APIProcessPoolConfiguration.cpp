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

#include "config.h"
#include "APIProcessPoolConfiguration.h"

#include "WebProcessPool.h"
#include "WebsiteDataStore.h"

namespace API {

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::create()
{
    return adoptRef(*new ProcessPoolConfiguration);
}

ProcessPoolConfiguration::ProcessPoolConfiguration() = default;

ProcessPoolConfiguration::~ProcessPoolConfiguration() = default;

Ref<ProcessPoolConfiguration> ProcessPoolConfiguration::copy()
{
    auto copy = this->create();

    copy->m_injectedBundlePath = this->m_injectedBundlePath;
    copy->m_customClassesForParameterCoder = this->m_customClassesForParameterCoder;
    copy->m_cachePartitionedURLSchemes = this->m_cachePartitionedURLSchemes;
    copy->m_alwaysRevalidatedURLSchemes = this->m_alwaysRevalidatedURLSchemes;
    copy->m_additionalReadAccessAllowedPaths = this->m_additionalReadAccessAllowedPaths;
    copy->m_fullySynchronousModeIsAllowedForTesting = this->m_fullySynchronousModeIsAllowedForTesting;
    copy->m_ignoreSynchronousMessagingTimeoutsForTesting = this->m_ignoreSynchronousMessagingTimeoutsForTesting;
    copy->m_attrStyleEnabled = this->m_attrStyleEnabled;
    copy->m_overrideLanguages = this->m_overrideLanguages;
    copy->m_alwaysRunsAtBackgroundPriority = this->m_alwaysRunsAtBackgroundPriority;
    copy->m_shouldTakeUIBackgroundAssertion = this->m_shouldTakeUIBackgroundAssertion;
    copy->m_shouldCaptureAudioInUIProcess = this->m_shouldCaptureAudioInUIProcess;
    copy->m_shouldCaptureDisplayInUIProcess = this->m_shouldCaptureDisplayInUIProcess;
    copy->m_shouldConfigureJSCForTesting = this->m_shouldConfigureJSCForTesting;
    copy->m_isJITEnabled = this->m_isJITEnabled;
    copy->m_presentingApplicationPID = this->m_presentingApplicationPID;
    copy->m_processSwapsOnNavigationFromClient = this->m_processSwapsOnNavigationFromClient;
    copy->m_processSwapsOnNavigationFromExperimentalFeatures = this->m_processSwapsOnNavigationFromExperimentalFeatures;
    copy->m_alwaysKeepAndReuseSwappedProcesses = this->m_alwaysKeepAndReuseSwappedProcesses;
    copy->m_processSwapsOnWindowOpenWithOpener = this->m_processSwapsOnWindowOpenWithOpener;
    copy->m_isAutomaticProcessWarmingEnabledByClient = this->m_isAutomaticProcessWarmingEnabledByClient;
    copy->m_usesWebProcessCache = this->m_usesWebProcessCache;
    copy->m_usesBackForwardCache = this->m_usesBackForwardCache;
#if PLATFORM(COCOA)
    copy->m_suppressesConnectionTerminationOnSystemChange = this->m_suppressesConnectionTerminationOnSystemChange;
#endif
    copy->m_customWebContentServiceBundleIdentifier = this->m_customWebContentServiceBundleIdentifier;
    copy->m_usesSingleWebProcess = m_usesSingleWebProcess;

    return copy;
}

} // namespace API
