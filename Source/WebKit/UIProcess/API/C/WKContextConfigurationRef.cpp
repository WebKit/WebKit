/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
    auto configuration = API::ProcessPoolConfiguration::create();
    
    // FIXME: A context created like this shouldn't have a data store,
    // instead there should be a WKPageConfigurationRef object that specifies the data store.
    configuration->setShouldHaveLegacyDataStore(true);
    
    return toAPI(&configuration.leakRef());
}

WKContextConfigurationRef WKContextConfigurationCreateWithLegacyOptions()
{
    return toAPI(&API::ProcessPoolConfiguration::createWithLegacyOptions().leakRef());
}

WKStringRef WKContextConfigurationCopyDiskCacheDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->diskCacheDirectory());
}

void WKContextConfigurationSetDiskCacheDirectory(WKContextConfigurationRef configuration, WKStringRef diskCacheDirectory)
{
    toImpl(configuration)->setDiskCacheDirectory(toImpl(diskCacheDirectory)->string());
}

WKStringRef WKContextConfigurationCopyApplicationCacheDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->applicationCacheDirectory());
}

void WKContextConfigurationSetApplicationCacheDirectory(WKContextConfigurationRef configuration, WKStringRef applicationCacheDirectory)
{
    toImpl(configuration)->setApplicationCacheDirectory(toImpl(applicationCacheDirectory)->string());
}

WKStringRef WKContextConfigurationCopyIndexedDBDatabaseDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->indexedDBDatabaseDirectory());
}

void WKContextConfigurationSetIndexedDBDatabaseDirectory(WKContextConfigurationRef configuration, WKStringRef indexedDBDatabaseDirectory)
{
    toImpl(configuration)->setIndexedDBDatabaseDirectory(toImpl(indexedDBDatabaseDirectory)->string());
}

WKStringRef WKContextConfigurationCopyInjectedBundlePath(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->injectedBundlePath());
}

void WKContextConfigurationSetInjectedBundlePath(WKContextConfigurationRef configuration, WKStringRef injectedBundlePath)
{
    toImpl(configuration)->setInjectedBundlePath(toImpl(injectedBundlePath)->string());
}

WKStringRef WKContextConfigurationCopyLocalStorageDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->localStorageDirectory());
}

void WKContextConfigurationSetLocalStorageDirectory(WKContextConfigurationRef configuration, WKStringRef localStorageDirectory)
{
    toImpl(configuration)->setLocalStorageDirectory(toImpl(localStorageDirectory)->string());
}

WKStringRef WKContextConfigurationCopyWebSQLDatabaseDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->webSQLDatabaseDirectory());
}

void WKContextConfigurationSetWebSQLDatabaseDirectory(WKContextConfigurationRef configuration, WKStringRef webSQLDatabaseDirectory)
{
    toImpl(configuration)->setWebSQLDatabaseDirectory(toImpl(webSQLDatabaseDirectory)->string());
}

WKStringRef WKContextConfigurationCopyMediaKeysStorageDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->mediaKeysStorageDirectory());
}

void WKContextConfigurationSetMediaKeysStorageDirectory(WKContextConfigurationRef configuration, WKStringRef mediaKeysStorageDirectory)
{
    toImpl(configuration)->setMediaKeysStorageDirectory(toImpl(mediaKeysStorageDirectory)->string());
}

WKStringRef WKContextConfigurationCopyResourceLoadStatisticsDirectory(WKContextConfigurationRef configuration)
{
    return toCopiedAPI(toImpl(configuration)->resourceLoadStatisticsDirectory());
}

void WKContextConfigurationSetResourceLoadStatisticsDirectory(WKContextConfigurationRef configuration, WKStringRef resourceLoadStatisticsDirectory)
{
    toImpl(configuration)->setResourceLoadStatisticsDirectory(toImpl(resourceLoadStatisticsDirectory)->string());
}

bool WKContextConfigurationFullySynchronousModeIsAllowedForTesting(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->fullySynchronousModeIsAllowedForTesting();
}

void WKContextConfigurationSetFullySynchronousModeIsAllowedForTesting(WKContextConfigurationRef configuration, bool allowed)
{
    toImpl(configuration)->setFullySynchronousModeIsAllowedForTesting(allowed);
}

WKArrayRef WKContextConfigurationCopyOverrideLanguages(WKContextConfigurationRef configuration)
{
    return toAPI(&API::Array::createStringArray(toImpl(configuration)->overrideLanguages()).leakRef());
}

void WKContextConfigurationSetOverrideLanguages(WKContextConfigurationRef configuration, WKArrayRef overrideLanguages)
{
    toImpl(configuration)->setOverrideLanguages(toImpl(overrideLanguages)->toStringVector());
}

bool WKContextConfigurationShouldCaptureAudioInUIProcess(WKContextConfigurationRef configuration)
{
    return toImpl(configuration)->shouldCaptureAudioInUIProcess();
}

void WKContextConfigurationSetShouldCaptureAudioInUIProcess(WKContextConfigurationRef configuration, bool should)
{
    toImpl(configuration)->setShouldCaptureAudioInUIProcess(should);
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
    return toImpl(configuration)->diskCacheSizeOverride();
}

void WKContextConfigurationSetDiskCacheSizeOverride(WKContextConfigurationRef configuration, int64_t size)
{
    toImpl(configuration)->setDiskCacheSizeOverride(size);
}

