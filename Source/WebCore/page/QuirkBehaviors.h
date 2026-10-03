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

#include <WebCore/QuirkBehaviorID.h>
#include <WebCore/URLMatch.h>
#include <wtf/BitSet.h>
#include <wtf/OptionSet.h>
#include <wtf/text/ASCIILiteral.h>

namespace WebCore {

namespace BuildCondition {

constexpr bool always = true;

#if PLATFORM(COCOA)
constexpr bool cocoa = true;
#else
constexpr bool cocoa = false;
#endif
#if PLATFORM(MAC)
constexpr bool mac = true;
#else
constexpr bool mac = false;
#endif
#if PLATFORM(IOS)
constexpr bool iOS = true;
#else
constexpr bool iOS = false;
#endif
#if PLATFORM(IOS_FAMILY)
constexpr bool iOSFamily = true;
#else
constexpr bool iOSFamily = false;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
constexpr bool iOSTouchEvents = true;
#else
constexpr bool iOSTouchEvents = false;
#endif
#if PLATFORM(VISION)
constexpr bool vision = true;
#else
constexpr bool vision = false;
#endif
#if HAVE(APPKIT_GESTURES_SUPPORT)
constexpr bool appKitGestures = true;
#else
constexpr bool appKitGestures = false;
#endif
#if ENABLE(CONTENT_CHANGE_OBSERVER)
constexpr bool contentChangeObserver = true;
#else
constexpr bool contentChangeObserver = false;
#endif
#if ENABLE(DESKTOP_CONTENT_MODE_QUIRKS)
constexpr bool desktopContentModeQuirks = true;
#else
constexpr bool desktopContentModeQuirks = false;
#endif
#if ENABLE(FLIP_SCREEN_DIMENSIONS_QUIRKS)
constexpr bool flipScreenDimensionsQuirks = true;
#else
constexpr bool flipScreenDimensionsQuirks = false;
#endif
#if ENABLE(FULLSCREEN_API)
constexpr bool fullscreenAPI = true;
#else
constexpr bool fullscreenAPI = false;
#endif
#if ENABLE(VIDEO_PRESENTATION_MODE)
constexpr bool videoPresentationMode = true;
#else
constexpr bool videoPresentationMode = false;
#endif
#if ENABLE(MEDIA_RECORDER)
constexpr bool mediaRecorder = true;
#else
constexpr bool mediaRecorder = false;
#endif
#if ENABLE(COCOA_WEBM_PLAYER)
constexpr bool cocoaWebMPlayer = true;
#else
constexpr bool cocoaWebMPlayer = false;
#endif
#if ENABLE(MEDIA_SOURCE)
constexpr bool mediaSource = true;
#else
constexpr bool mediaSource = false;
#endif
#if ENABLE(MEDIA_STREAM)
constexpr bool mediaStream = true;
#else
constexpr bool mediaStream = false;
#endif
#if ENABLE(META_VIEWPORT)
constexpr bool metaViewport = true;
#else
constexpr bool metaViewport = false;
#endif
#if HAVE(PIP_SKIP_PREROLL)
constexpr bool pipSkipPreroll = true;
#else
constexpr bool pipSkipPreroll = false;
#endif
#if ENABLE(PICTURE_IN_PICTURE_API)
constexpr bool pictureInPictureAPI = true;
#else
constexpr bool pictureInPictureAPI = false;
#endif
#if ENABLE(THREADED_ANIMATIONS)
constexpr bool threadedAnimations = true;
#else
constexpr bool threadedAnimations = false;
#endif
#if ENABLE(TOUCH_EVENTS)
constexpr bool touchEvents = true;
#else
constexpr bool touchEvents = false;
#endif
#if ENABLE(TOUCH_EVENT_REGIONS)
constexpr bool touchEventRegions = true;
#else
constexpr bool touchEventRegions = false;
#endif
#if ENABLE(TWO_PHASE_CLICKS)
constexpr bool twoPhaseClicks = true;
#else
constexpr bool twoPhaseClicks = false;
#endif
#if ENABLE(WEB_RTC)
constexpr bool webRTC = true;
#else
constexpr bool webRTC = false;
#endif

} // namespace BuildCondition

struct QuirkParameters {
    ASCIILiteral script = ""_s;
    ASCIILiteral userAgent = ""_s;
    ASCIILiteral chromeCompatibilityVersion = ""_s;
    std::span<const ASCIILiteral> cookieNames { };

    static consteval QuirkParameters fromScript(ASCIILiteral script)
    {
        return QuirkParameters {
            .script = script
        };
    }

    static consteval QuirkParameters fromUserAgent(ASCIILiteral userAgent)
    {
        return QuirkParameters {
            .userAgent = userAgent
        };
    }

    static consteval QuirkParameters fromChromeCompatibilityVersion(ASCIILiteral chromeCompatibilityVersion)
    {
        return QuirkParameters {
            .chromeCompatibilityVersion = chromeCompatibilityVersion
        };
    }

    static consteval QuirkParameters fromCookieNames(std::span<const ASCIILiteral> cookieNames)
    {
        return QuirkParameters {
            .cookieNames = cookieNames
        };
    }

    friend bool operator==(const QuirkParameters& a, const QuirkParameters& b)
    {
        return a.script == b.script
            && a.userAgent == b.userAgent
            && a.chromeCompatibilityVersion == b.chromeCompatibilityVersion
            && std::ranges::equal(a.cookieNames, b.cookieNames);
    }
};

enum class QuirkParametersNeeded : uint8_t {
    NeedsScript = 1 << 0,
    NeedsUserAgent = 1 << 1,
    NeedsChromeCompatibilityVersion = 1 << 2,
    NeedsCookieNames = 1 << 3,
};

enum class QuirkConditionsSupported : uint8_t {
    ElementSelector = 1 << 0,
    SecondaryURL = 1 << 1,
    DocumentSelector = 1 << 2,
};

namespace QuirkBehaviorConditions {
struct ElementMatchesSelector {
    ASCIILiteral selector;
};

struct SecondaryURLMatches {
    URLMatch match;
};

struct DocumentHasElementMatching {
    ASCIILiteral selector;
};

constexpr ElementMatchesSelector elementMatchesSelector(ASCIILiteral selector)
{
    return ElementMatchesSelector { selector };
}

constexpr SecondaryURLMatches secondaryURLMatches(URLMatch match)
{
    return SecondaryURLMatches { match };
}

constexpr DocumentHasElementMatching documentHasElementMatching(ASCIILiteral selector)
{
    return DocumentHasElementMatching { selector };
}

} // namespace QuirkBehaviorConditions

struct QuirkConditions {
    std::optional<ASCIILiteral> elementSelector { std::nullopt };
    std::optional<URLMatch> secondaryURL { std::nullopt };
    std::optional<ASCIILiteral> documentSelector { std::nullopt };

    friend bool operator==(const QuirkConditions&, const QuirkConditions&) = default;
};

struct QuirkBehavior {
    QuirkBehaviorID id;
    bool isAvailable { false };
    OptionSet<QuirkParametersNeeded> quirkParametersNeeded { };
    OptionSet<QuirkConditionsSupported> quirkConditionsSupported { };
    OptionSet<QuirkConditionsSupported> quirkConditionsNeeded { };
    QuirkConditions conditions { };
    std::optional<QuirkParameters> parameters { std::nullopt };

    bool secondaryURLConditionMatches(const URLMatchContext& context) const
    {
        return !conditions.secondaryURL || conditions.secondaryURL->matches(context);
    }

    friend bool operator==(const QuirkBehavior&, const QuirkBehavior&) = default;

    consteval QuirkBehavior operator()(QuirkParameters params) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!quirkParametersNeeded.isEmpty());
        auto copy = *this;
        copy.parameters = params;
        return copy;
    }

    template<typename... Conditions> consteval QuirkBehavior when(Conditions... conditions) const
    {
        static_assert(sizeof...(conditions), "when() must name at least one condition");
        auto copy = *this;
        (applyCondition(copy, conditions), ...);
        return copy;
    }

    consteval void applyCondition(QuirkBehavior& behavior, QuirkBehaviorConditions::ElementMatchesSelector elementMatchesSelector) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::ElementSelector));
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!behavior.conditions.elementSelector);
        behavior.conditions.elementSelector = elementMatchesSelector.selector;
    }

    consteval void applyCondition(QuirkBehavior& behavior, QuirkBehaviorConditions::SecondaryURLMatches secondaryURLMatches) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::SecondaryURL));
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!behavior.conditions.secondaryURL);
        behavior.conditions.secondaryURL = secondaryURLMatches.match;
    }

    consteval void applyCondition(QuirkBehavior& behavior, QuirkBehaviorConditions::DocumentHasElementMatching documentHasElementMatching) const
    {
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(behavior.quirkConditionsSupported.contains(QuirkConditionsSupported::DocumentSelector));
        RELEASE_ASSERT_UNDER_CONSTEXPR_CONTEXT(!behavior.conditions.documentSelector);
        behavior.conditions.documentSelector = documentHasElementMatching.selector;
    }
};

} // namespace WebCore
