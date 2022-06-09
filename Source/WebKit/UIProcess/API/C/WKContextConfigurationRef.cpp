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
#include "WKContextConfigurationRef.h"

#include "APIArray.h"
#include "APIProcessPoolConfiguration.h"
#include "WKAPICast.h"

using namespace WebKit;

WKContextConfigurationRef WKContextConfigurationCreate()
{
    return toAPI(&API::ProcessPoolConfiguration::create().leakRef());
}

WKContextConfigurationRef WKContextConfigurationCreateWithLegacyOptions()
{
    return WKContextConfigurationCreate();
}

WKStringRef WKContextConfigurationCopyDiskCacheDirectory(WKContextConfigurationRef)
{
    return nullptr;
}

void WKContextConfigurationSetDiskCacheDirectory(WKContextConfigurationRef, WKStringRef)
{
}

WKStringRef WKContextConfigurationCopyApplicationCacheDirectory(WKContextConfigurationRef)
{
    return nullptr;
}

void WKContextConfigurationSetApplicationCacheDirectory(WKContextConfigurationRef, WKStringRef)
{
}

WKStringRef WKContextConfigurationCopyIndexedDBDatabaseDirectory(WKContextConfigurationRef)
{
    return nullptr;
}

void WKContextConfigurationSetIndexedDBDatabaseDirectory(WKContextConfigurationRef, WKStringRef)
{
}

WKStringRef WKContextConfigurationCopyInjectedBundlePath(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->injectedBundlePath());
}

void WKContextConfigurationSetInjectedBundlePath(WKContextConfigurationRef configuration, WKStringRef injectedBundlePath)
{
    toImpl(configuration)->setInjectedBundlePath(toImpl(injectedBundlePath)->string());
}

WKArrayRef WKContextConfigurationCopyCustomClassesForParameterCoder(WKContextConfigurationRef configuration)
{
    return toAPI(&API::Array::createStringArray(toImpl(configuration)->customClassesForParameterCoder()).leakRef());
}

void WKContextConfigurationSetCustomClassesForParameterCoder(WKContextConfigurationRef configuration, WKArrayRef classesForCoder)
{
    toImpl(configuration)->setCustomClassesForParameterCoder(toImpl(classesForCoder)->toStringVector());
}

WKStringRef WKContextConfigurationCopyLocalStorageDirectory(WKContextConfigurationRef)
{
    return nullptr;
}

void WKContextConfigurationSetLocalStorageDirectory(WKContextConfigurationRef, WKStringRef)
{
}

WKStringRef WKContextConfigurationCopyWebSQLDatabaseDirectory(WKContextConfigurationRef)
{
    return nullptr;
}

void WKContextConfigurationSetWebSQLDatabaseDirectory(WKContextConfigurationRef, WKStringRef)
{
}

WKStringRef WKContextConfigurationCopyMediaKeysStorageDirectory(WKContextConfigurationRef)
{
    return nullptr;
}

void WKContextConfigurationSetMediaKeysStorageDirectory(WKContextConfigurationRef, WKStringRef)
{
}

WKStringRef WKContextConfigurationCopyResourceLoadStatisticsDirectory(WKContextConfigurationRef)
{
    return nullptr;
}

void WKContextConfigurationSetResourceLoadStatisticsDirectory(WKContextConfigurationRef, WKStringRef)
{
}

bool WKContextConfigurationFullySynchronousModeIsAllowedForTesting(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->fullySynchronousModeIsAllowedForTesting();
}

void WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(WKContextConfigurationRef configuration, bool allowed)
{
    toImpl(configuration)->setFullySynchronousModeIsAllowedForTesting(allowed);
}

bool WKContextConfigurationIgnoreSynchronousMessagingTimeoutsForTesting(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->ignoreSynchronousMessagingTimeoutsForTesting();
}

void WKContextConfigurationSetIgnoreSynchronousMessagingTimeoutsForTesting(WKContextConfigurationRef configuration, bool ignore)
{
    toImpl(configuration)->setIgnoreSynchronousMessagingTimeoutsForTesting(ignore);
}

WKArrayRef WKContextConfigurationCopyOverrideLanguages(WKContextConfigurationRef configuration)
{
    return toAPI(&API::Array::createStringArray(toImpl(configuration)->overrideLanguages()).leakRef());
}

void WKContextConfigurationSetOverrideLanguages(WKContextConfigurationRef configuration, WKArrayRef overrideLanguages)
{
    toImpl(configuration)->setOverrideLanguages(toImpl(overrideLanguages)->toStringVector());
}

bool WKContextConfigurationProcessSwapsOnNavigation(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->processSwapsOnNavigation();
}

void WKContextConfigurationSetProcessSwapsOnNavigation(WKContextConfigurationRef configuration, bool swaps)
{
    toImpl(configuration)->setProcessSwapsOnNavigation(swaps);
}

bool WKContextConfigurationPrewarmsProcessesAutomatically(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->isAutomaticProcessWarmingEnabled();
}

void WKContextConfigurationSetPrewarmsProcessesAutomatically(WKContextConfigurationRef configuration, bool prewarms)
{
    toImpl(configuration)->setIsAutomaticProcessWarmingEnabled(prewarms);
}

bool WKContextConfigurationUsesWebProcessCache(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->usesWebProcessCache();
}

void WKContextConfigurationSetUsesWebProcessCache(WKContextConfigurationRef configuration, bool uses)
{
    toImpl(configuration)->setUsesWebProcessCache(uses);
}

bool WKContextConfigurationAlwaysKeepAndReuseSwappedProcesses(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->alwaysKeepAndReuseSwappedProcesses();
}

void WKContextConfigurationSetAlwaysKeepAndReuseSwappedProcesses(WKContextConfigurationRef configuration, bool keepAndReuse)
{
    toImpl(configuration)->setAlwaysKeepAndReuseSwappedProcesses(keepAndReuse);
}

bool WKContextConfigurationProcessSwapsOnWindowOpenWithOpener(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->processSwapsOnWindowOpenWithOpener();
}

void WKContextConfigurationSetProcessSwapsOnWindowOpenWithOpener(WKContextConfigurationRef configuration, bool swaps)
{
    toImpl(configuration)->setProcessSwapsOnWindowOpenWithOpener(swaps);
}

int64_t WKContextConfigurationDiskCacheSizeOverride(WKContextConfigurationRef configuration)
{
    return 0;
}

void WKContextConfigurationSetDiskCacheSizeOverride(WKContextConfigurationRef configuration, int64_t size)
{
}

void WKContextConfigurationSetShouldCaptureAudioInUIProcess(WKContextConfigurationRef, bool)
{
}

void WKContextConfigurationSetShouldConfigureJSCForTesting(WKContextConfigurationRef configuration, bool value)
{
    toImpl(configuration)->setShouldConfigureJSCForTesting(value);
}
