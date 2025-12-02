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
#include "config.h"
#include "NavigatorUAData.h"

#include "JSUADataValues.h"
#include "NavigatorUABrandVersion.h"
#include "NotImplemented.h"
#include "UADataValues.h"
#include "UALowEntropyJSON.h"
#include "UserAgent.h"
#include "UserAgentStringData.h"
#include "page/UserAgentStringData.h"
#include <algorithm>
#include <random>
#include <wtf/NeverDestroyed.h>
#include <wtf/WeakRandomNumber.h>
#include <wtf/text/WTFString.h>

#if USE(GLIB)
#include <wtf/glib/ChassisType.h>
#endif

#if OS(LINUX)
#include "sys/utsname.h"
#include <wtf/StdLibExtras.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import <pal/system/ios/Device.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#endif

namespace WebCore {
NavigatorUAData::NavigatorUAData() = default;
NavigatorUAData::~NavigatorUAData() = default;

NavigatorUAData::NavigatorUAData(Ref<UserAgentStringData>&& userAgentStringData)
{
    overrideFromUserAgentString = true;
    mobileOverride = userAgentStringData->mobile;
    static std::once_flag onceKey;
    std::call_once(onceKey, [userAgentStringData] {
        Vector<NavigatorUABrandVersion> temp = {
            NavigatorUABrandVersion {
                .brand = userAgentStringData->browserName,
                .version = userAgentStringData->browserVersion },
            NavigatorUABrandVersion {
                .brand = createArbitraryBrand(),
                .version = createArbitraryVersion() }
        };

        auto rng = std::default_random_engine { };
        std::ranges::shuffle(temp, rng);
        NavigatorUAData::m_brands.construct(temp);
    });

    platformOverride = userAgentStringData->platform;
}

Ref<NavigatorUAData> NavigatorUAData::create()
{
    return adoptRef(*new NavigatorUAData());
}

Ref<NavigatorUAData> NavigatorUAData::create(Ref<UserAgentStringData>&& userAgentStringData)
{
    return adoptRef(*new NavigatorUAData(WTFMove(userAgentStringData)));
}

const Vector<NavigatorUABrandVersion>& NavigatorUAData::brands() const
{
    if (overrideFromUserAgentString)
        return NavigatorUAData::m_brands;

    static NeverDestroyed<Vector<NavigatorUABrandVersion>> brandVersion = [] {
        Vector<NavigatorUABrandVersion> temp = {
            NavigatorUABrandVersion {
                .brand = "AppleWebKit"_s,
                .version = "605.1.15"_s },
            NavigatorUABrandVersion {
                .brand = createArbitraryBrand(),
                .version = createArbitraryVersion() }
        };

        auto rng = std::default_random_engine { };
        std::ranges::shuffle(temp, rng);
        return temp;
    }();

    return brandVersion;
}

bool NavigatorUAData::mobile() const
{
    if (overrideFromUserAgentString)
        return mobileOverride;

#if PLATFORM(IOS_FAMILY)
    return !(PAL::currentUserInterfaceIdiomIsDesktop() || PAL::currentUserInterfaceIdiomIsVision());
#elif USE(GLIB)
    return chassisType() == WTF::ChassisType::Mobile;
#else
    return false;
#endif
}

String NavigatorUAData::platform() const
{
    if (overrideFromUserAgentString)
        return platformOverride;

#if OS(LINUX)
    static NeverDestroyed<String> platformName = [] {
        struct utsname osname;
        return uname(&osname) >= 0 ? makeString(unsafeSpan(osname.sysname)) : emptyString();
    }();
    return platformName->isolatedCopy();
#elif PLATFORM(IOS_FAMILY)
    return (PAL::currentUserInterfaceIdiomIsDesktop() || PAL::currentUserInterfaceIdiomIsVision()) ? "macOS"_s : "iOS"_s;
#elif OS(MACOS)
    return "macOS"_s;
#else
    return ""_s;
#endif
}

UALowEntropyJSON NavigatorUAData::toJSON() const
{
    return UALowEntropyJSON {
        brands(),
        mobile(),
        platform()
    };
}

void NavigatorUAData::getHighEntropyValues(const Vector<String>& hints, NavigatorUAData::ValuesPromise&& promise) const
{
    auto values = UADataValues::create(brands(), mobile(), platform());
    if (overrideFromUserAgentString) {
        // if the user agent string has been overridden, we should not expose high entropy values
        promise.resolve(values);
        return;
    }

    for (auto& hint : hints) {
        if (hint == "architecture")
            values->architecture = ""_s;
        else if (hint == "bitness")
            values->bitness = "64"_s;
        else if (hint == "formFactors")
            values->formFactors = Vector<String> { };
        else if (hint == "fullVersionList")
            values->fullVersionList = brands();
        else if (hint == "model")
            values->model = ""_s;
        else if (hint == "platformVersion") {
#if OS(LINUX)
            values->platformVersion = ""_s;
#elif PLATFORM(IOS_FAMILY)
            values->platformVersion = systemMarketingVersionForUserAgentString();
#elif OS(MACOS)
            values->platformVersion = "10.15.7"_s;
#else
            values->platformVersion = ""_s;
#endif
        } else if (hint == "uaFullVersion")
            values->uaFullVersion = "605.1.15"_s;
        else if (hint == "wow64")
            values->wow64 = false;
    }

    promise.resolve(WTFMove(values));
}

String NavigatorUAData::createArbitraryVersion()
{
    return makeString(weakRandomNumber<unsigned>() % 10000, '.', weakRandomNumber<unsigned>() % 10000, '.', weakRandomNumber<unsigned>() % 10000);
}

String NavigatorUAData::createArbitraryBrand()
{
    auto greasyChars = unsafeSpan(" ()-./:;=?_");
    return makeString("The"_s, greasyChars[weakRandomNumber<unsigned>() % greasyChars.size()], "Best"_s, greasyChars[weakRandomNumber<unsigned>() % greasyChars.size()], "Browser"_s);
}
}
