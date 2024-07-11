/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CaptionUserPreferencesMediaAF.h"

#if ENABLE(VIDEO)

#include "AudioTrackList.h"
#include "ColorSerialization.h"
#include "CommonAtomStrings.h"
#include "FloatConversion.h"
#include "FontCacheCoreText.h"
#include "HTMLMediaElement.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "TextTrackList.h"
#include "UserAgentParts.h"
#include "UserStyleSheetTypes.h"
#include <algorithm>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/SoftLinking.h>
#include <wtf/URL.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/cf/StringConcatenateCF.h>
#include <wtf/unicode/Collator.h>

#if PLATFORM(COCOA)
#include <pal/spi/cf/CFNotificationCenterSPI.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThreadRun.h"
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

#include <CoreText/CoreText.h>
#include <MediaAccessibility/MediaAccessibility.h>

#include "MediaAccessibilitySoftLink.h"

SOFT_LINK_FRAMEWORK_OPTIONAL(MediaToolbox)
SOFT_LINK_OPTIONAL(MediaToolbox, MTEnableCaption2015Behavior, Boolean, (), ())

#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

namespace WebCore {

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

static std::unique_ptr<CaptionPreferencesDelegate>& captionPreferencesDelegate()
{
    static NeverDestroyed<std::unique_ptr<CaptionPreferencesDelegate>> delegate;
    return delegate.get();
}

static std::optional<CaptionUserPreferencesMediaAF::CaptionDisplayMode>& cachedCaptionDisplayMode()
{
    static NeverDestroyed<std::optional<CaptionUserPreferencesMediaAF::CaptionDisplayMode>> captionDisplayMode;
    return captionDisplayMode;
}

static std::optional<Vector<String>>& cachedPreferredLanguages()
{
    static NeverDestroyed<std::optional<Vector<String>>> preferredLanguages;
    return preferredLanguages;
}

static void userCaptionPreferencesChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    RefPtr userPreferences = CaptionUserPreferencesMediaAF::extractCaptionUserPreferencesMediaAF(observer);
    if (userPreferences) {
#if !PLATFORM(IOS_FAMILY)
        userPreferences->captionPreferencesChanged();
#else
        WebThreadRun(^{
            userPreferences->captionPreferencesChanged();
        });
#endif
    }
}

#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

Ref<CaptionUserPreferencesMediaAF> CaptionUserPreferencesMediaAF::create(PageGroup& group)
{
    return adoptRef(*new CaptionUserPreferencesMediaAF(group));
}

CaptionUserPreferencesMediaAF::CaptionUserPreferencesMediaAF(PageGroup& group)
    : CaptionUserPreferences(group)
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    , m_updateStyleSheetTimer(*this, &CaptionUserPreferencesMediaAF::updateTimerFired)
#endif
{
    static bool initialized;
    if (!initialized) {
        initialized = true;

        if (!MediaToolboxLibrary())
            return;

        MTEnableCaption2015BehaviorPtrType function = MTEnableCaption2015BehaviorPtr();
        if (!function || !function())
            return;

        beginBlockingNotifications();
        CaptionUserPreferences::setCaptionDisplayMode(CaptionDisplayMode::Manual);
        setUserPrefersCaptions(false);
        setUserPrefersSubtitles(false);
        setUserPrefersTextDescriptions(false);
        endBlockingNotifications();
    }
}

CaptionUserPreferencesMediaAF::~CaptionUserPreferencesMediaAF()
{
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    if (RetainPtr observer = m_observer) {
        auto center = CFNotificationCenterGetLocalCenter();
        if (kMAXCaptionAppearanceSettingsChangedNotification)
            CFNotificationCenterRemoveObserver(center, observer.get(), kMAXCaptionAppearanceSettingsChangedNotification, 0);
        if (kMAAudibleMediaSettingsChangedNotification)
            CFNotificationCenterRemoveObserver(center, observer.get(), kMAAudibleMediaSettingsChangedNotification, 0);
    }
#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
}

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

CaptionUserPreferences::CaptionDisplayMode CaptionUserPreferencesMediaAF::captionDisplayMode() const
{
    CaptionDisplayMode internalMode = CaptionUserPreferences::captionDisplayMode();
    if (internalMode == CaptionDisplayMode::Manual || testingMode() || !MediaAccessibilityLibrary())
        return internalMode;

    if (cachedCaptionDisplayMode().has_value())
        return cachedCaptionDisplayMode().value();

    return platformCaptionDisplayMode();
}

void CaptionUserPreferencesMediaAF::platformSetCaptionDisplayMode(CaptionDisplayMode mode)
{
    MACaptionAppearanceDisplayType displayType = kMACaptionAppearanceDisplayTypeForcedOnly;
    switch (mode) {
    case CaptionDisplayMode::Automatic:
        displayType = kMACaptionAppearanceDisplayTypeAutomatic;
        break;
    case CaptionDisplayMode::ForcedOnly:
        displayType = kMACaptionAppearanceDisplayTypeForcedOnly;
        break;
    case CaptionDisplayMode::AlwaysOn:
        displayType = kMACaptionAppearanceDisplayTypeAlwaysOn;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    MACaptionAppearanceSetDisplayType(kMACaptionAppearanceDomainUser, displayType);
}

void CaptionUserPreferencesMediaAF::setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode mode)
{
    if (testingMode() || !MediaAccessibilityLibrary()) {
        CaptionUserPreferences::setCaptionDisplayMode(mode);
        return;
    }

    if (captionDisplayMode() == CaptionDisplayMode::Manual)
        return;

    if (captionPreferencesDelegate()) {
        captionPreferencesDelegate()->setDisplayMode(mode);
        return;
    }
    
    platformSetCaptionDisplayMode(mode);
}

CaptionUserPreferences::CaptionDisplayMode CaptionUserPreferencesMediaAF::platformCaptionDisplayMode()
{
    MACaptionAppearanceDisplayType displayType = MACaptionAppearanceGetDisplayType(kMACaptionAppearanceDomainUser);
    switch (displayType) {
    case kMACaptionAppearanceDisplayTypeForcedOnly:
        return CaptionDisplayMode::ForcedOnly;

    case kMACaptionAppearanceDisplayTypeAutomatic:
        return CaptionDisplayMode::Automatic;

    case kMACaptionAppearanceDisplayTypeAlwaysOn:
        return CaptionDisplayMode::AlwaysOn;
    }

    ASSERT_NOT_REACHED();
    return CaptionDisplayMode::ForcedOnly;
}

void CaptionUserPreferencesMediaAF::setCachedCaptionDisplayMode(CaptionDisplayMode captionDisplayMode)
{
    cachedCaptionDisplayMode() = captionDisplayMode;
}

bool CaptionUserPreferencesMediaAF::userPrefersCaptions() const
{
    bool captionSetting = CaptionUserPreferences::userPrefersCaptions();
    if (captionSetting || testingMode() || !MediaAccessibilityLibrary())
        return captionSetting;

    RetainPtr captioningMediaCharacteristics = adoptCF(MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics(kMACaptionAppearanceDomainUser));
    return captioningMediaCharacteristics && CFArrayGetCount(captioningMediaCharacteristics.get());
}

bool CaptionUserPreferencesMediaAF::userPrefersSubtitles() const
{
    bool subtitlesSetting = CaptionUserPreferences::userPrefersSubtitles();
    if (subtitlesSetting || testingMode() || !MediaAccessibilityLibrary())
        return subtitlesSetting;
    
    RetainPtr captioningMediaCharacteristics = adoptCF(MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics(kMACaptionAppearanceDomainUser));
    return !(captioningMediaCharacteristics && CFArrayGetCount(captioningMediaCharacteristics.get()));
}

bool CaptionUserPreferencesMediaAF::userPrefersTextDescriptions() const
{
    bool prefersTextDescriptions = CaptionUserPreferences::userPrefersTextDescriptions();
    if (prefersTextDescriptions || testingMode() || !MediaAccessibilityLibrary())
        return prefersTextDescriptions;

    RetainPtr preferDescriptiveVideo = adoptCF(MAAudibleMediaPrefCopyPreferDescriptiveVideo());
    return preferDescriptiveVideo && CFBooleanGetValue(preferDescriptiveVideo.get());
}

void CaptionUserPreferencesMediaAF::updateTimerFired()
{
    updateCaptionStyleSheetOverride();
}

void CaptionUserPreferencesMediaAF::setInterestedInCaptionPreferenceChanges()
{
    if (m_listeningForPreferenceChanges)
        return;

    if (!MediaAccessibilityLibrary())
        return;

    if (!kMAXCaptionAppearanceSettingsChangedNotification && !canLoad_MediaAccessibility_kMAAudibleMediaSettingsChangedNotification())
        return;

    m_listeningForPreferenceChanges = true;
    m_registeringForNotification = true;

    if (!m_observer)
        m_observer = createWeakObserver(this);
    RetainPtr observer = m_observer;
    auto suspensionBehavior = static_cast<CFNotificationSuspensionBehavior>(CFNotificationSuspensionBehaviorCoalesce | _CFNotificationObserverIsObjC);
    auto center = CFNotificationCenterGetLocalCenter();
    if (kMAXCaptionAppearanceSettingsChangedNotification)
        CFNotificationCenterAddObserver(center, observer.get(), userCaptionPreferencesChangedNotificationCallback, kMAXCaptionAppearanceSettingsChangedNotification, 0, suspensionBehavior);

    if (canLoad_MediaAccessibility_kMAAudibleMediaSettingsChangedNotification())
        CFNotificationCenterAddObserver(center, observer.get(), userCaptionPreferencesChangedNotificationCallback, kMAAudibleMediaSettingsChangedNotification, 0, suspensionBehavior);
    m_registeringForNotification = false;

    // Generating and registering the caption stylesheet can be expensive and this method is called indirectly when the parser creates an audio or
    // video element, so do it after a brief pause.
    m_updateStyleSheetTimer.startOneShot(0_s);
}

void CaptionUserPreferencesMediaAF::captionPreferencesChanged()
{
    if (m_registeringForNotification)
        return;

    if (m_listeningForPreferenceChanges)
        updateCaptionStyleSheetOverride();

    CaptionUserPreferences::captionPreferencesChanged();
}

void CaptionUserPreferencesMediaAF::setCaptionPreferencesDelegate(std::unique_ptr<CaptionPreferencesDelegate>&& delegate)
{
    captionPreferencesDelegate() = WTFMove(delegate);
}

String CaptionUserPreferencesMediaAF::captionsWindowCSS() const
{
    MACaptionAppearanceBehavior behavior;
    RetainPtr color = adoptCF(MACaptionAppearanceCopyWindowColor(kMACaptionAppearanceDomainUser, &behavior));

    Color windowColor(roundAndClampToSRGBALossy(color.get()));
    if (!windowColor.isValid())
        windowColor = Color::transparentBlack;

    bool important = behavior == kMACaptionAppearanceBehaviorUseValue;
    CGFloat opacity = MACaptionAppearanceGetWindowOpacity(kMACaptionAppearanceDomainUser, &behavior);
    if (!important)
        important = behavior == kMACaptionAppearanceBehaviorUseValue;
    String windowStyle = colorPropertyCSS(CSSPropertyBackgroundColor, windowColor.colorWithAlpha(opacity), important);

    if (!opacity)
        return windowStyle;

    return makeString(windowStyle, nameLiteral(CSSPropertyPadding), ": .4em !important;"_s);
}

String CaptionUserPreferencesMediaAF::captionsBackgroundCSS() const
{
    // This default value must be the same as the one specified in mediaControls.css for -webkit-media-text-track-past-nodes
    // and webkit-media-text-track-future-nodes.
    constexpr auto defaultBackgroundColor = Color::black.colorWithAlphaByte(204);

    MACaptionAppearanceBehavior behavior;

    RetainPtr color = adoptCF(MACaptionAppearanceCopyBackgroundColor(kMACaptionAppearanceDomainUser, &behavior));
    Color backgroundColor(roundAndClampToSRGBALossy(color.get()));
    if (!backgroundColor.isValid())
        backgroundColor = defaultBackgroundColor;

    bool important = behavior == kMACaptionAppearanceBehaviorUseValue;
    CGFloat opacity = MACaptionAppearanceGetBackgroundOpacity(kMACaptionAppearanceDomainUser, &behavior);
    if (!important)
        important = behavior == kMACaptionAppearanceBehaviorUseValue;
    return colorPropertyCSS(CSSPropertyBackgroundColor, backgroundColor.colorWithAlpha(opacity), important);
}

Color CaptionUserPreferencesMediaAF::captionsTextColor(bool& important) const
{
    MACaptionAppearanceBehavior behavior;
    RetainPtr color = adoptCF(MACaptionAppearanceCopyForegroundColor(kMACaptionAppearanceDomainUser, &behavior)).get();
    Color textColor(roundAndClampToSRGBALossy(color.get()));
    if (!textColor.isValid()) {
        // This default value must be the same as the one specified in mediaControls.css for -webkit-media-text-track-container.
        textColor = Color::white;
    }
    important = behavior == kMACaptionAppearanceBehaviorUseValue;
    CGFloat opacity = MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainUser, &behavior);
    if (!important)
        important = behavior == kMACaptionAppearanceBehaviorUseValue;
    return textColor.colorWithAlpha(opacity);
}

String CaptionUserPreferencesMediaAF::captionsTextColorCSS() const
{
    bool important;
    auto textColor = captionsTextColor(important);
    if (!textColor.isValid())
        return emptyString();
    return colorPropertyCSS(CSSPropertyColor, textColor, important);
}

template<typename... Types> void appendCSS(StringBuilder& builder, CSSPropertyID id, bool important, const Types&... values)
{
    builder.append(nameLiteral(id), ':', values..., important ? " !important;"_s : ";"_s);
}

String CaptionUserPreferencesMediaAF::windowRoundedCornerRadiusCSS() const
{
    MACaptionAppearanceBehavior behavior;
    CGFloat radius = MACaptionAppearanceGetWindowRoundedCornerRadius(kMACaptionAppearanceDomainUser, &behavior);
    if (!radius)
        return emptyString();

    StringBuilder builder;
    appendCSS(builder, CSSPropertyBorderRadius, behavior == kMACaptionAppearanceBehaviorUseValue, radius, "px"_s);
    return builder.toString();
}

String CaptionUserPreferencesMediaAF::colorPropertyCSS(CSSPropertyID id, const Color& color, bool important) const
{
    StringBuilder builder;
    // FIXME: Seems like this should be using serializationForCSS instead?
    appendCSS(builder, id, important, serializationForHTML(color));
    return builder.toString();
}

bool CaptionUserPreferencesMediaAF::captionStrokeWidthForFont(float fontSize, const String& language, float& strokeWidth, bool& important) const
{
    if (!canLoad_MediaAccessibility_MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle())
        return false;
    
    MACaptionAppearanceBehavior behavior;
    auto trackLanguage = language.createCFString();
    CGFloat strokeWidthPt;
    
    RetainPtr fontDescriptor = adoptCF(MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle(kMACaptionAppearanceDomainUser, &behavior, kMACaptionAppearanceFontStyleDefault, trackLanguage.get(), fontSize, &strokeWidthPt));

    if (!fontDescriptor)
        return false;

    // Since only half of the stroke is visible because the stroke is drawn before the fill, we double the stroke width here.
    strokeWidth = strokeWidthPt * 2;
    important = behavior == kMACaptionAppearanceBehaviorUseValue;
    return true;
}

String CaptionUserPreferencesMediaAF::captionsTextEdgeCSS() const
{
    MACaptionAppearanceBehavior behavior;
    MACaptionAppearanceTextEdgeStyle textEdgeStyle = MACaptionAppearanceGetTextEdgeStyle(kMACaptionAppearanceDomainUser, &behavior);
    
    if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleUndefined || textEdgeStyle == kMACaptionAppearanceTextEdgeStyleNone)
        return emptyString();

    StringBuilder builder;
    bool important = behavior == kMACaptionAppearanceBehaviorUseValue;
    if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleRaised)
        appendCSS(builder, CSSPropertyTextShadow, important, "-.1em -.1em .16em black"_s);
    else if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleDepressed)
        appendCSS(builder, CSSPropertyTextShadow, important, ".1em .1em .16em black"_s);
    else if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleDropShadow)
        appendCSS(builder, CSSPropertyTextShadow, important, "0 .1em .16em black"_s);

    if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleDropShadow || textEdgeStyle == kMACaptionAppearanceTextEdgeStyleUniform) {
        appendCSS(builder, CSSPropertyStrokeColor, important, "black"_s);
        appendCSS(builder, CSSPropertyPaintOrder, important, nameLiteral(CSSValueStroke));
        appendCSS(builder, CSSPropertyStrokeLinejoin, important, nameLiteral(CSSValueRound));
        appendCSS(builder, CSSPropertyStrokeLinecap, important, nameLiteral(CSSValueRound));
    }
    
    return builder.toString();
}

String CaptionUserPreferencesMediaAF::captionsDefaultFontCSS() const
{
    MACaptionAppearanceBehavior behavior;
    
    RetainPtr font = adoptCF(MACaptionAppearanceCopyFontDescriptorForStyle(kMACaptionAppearanceDomainUser, &behavior, kMACaptionAppearanceFontStyleDefault));
    if (!font)
        return emptyString();

    RetainPtr name = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(font.get(), kCTFontNameAttribute)));
    if (!name)
        return emptyString();

    if (fontNameIsSystemFont(name.get())) {
        if (CFStringHasPrefix(CFSTR(".AppleSystemUIFontMonospaced"), name.get()))
            name = CFSTR("system-ui-monospaced");
        else if (CFStringHasPrefix(CFSTR(".AppleSystemUIFont"), name.get()))
            name = CFSTR("system-ui");
        else {
            // FIXME: Add more fallbacks for system font names
            // Default to "system-ui" for all other disallowed system fonts
            name = CFSTR("system-ui");
        }
    }

    StringBuilder builder;
    builder.append("font-family: \""_s, name.get(), '"');
    if (RetainPtr cascadeList = adoptCF(static_cast<CFArrayRef>(CTFontDescriptorCopyAttribute(font.get(), kCTFontCascadeListAttribute)))) {
        for (CFIndex i = 0; i < CFArrayGetCount(cascadeList.get()); i++) {
            auto fontCascade = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(cascadeList.get(), i));
            if (!fontCascade)
                continue;
            RetainPtr fontCascadeName = adoptCF(static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(fontCascade, kCTFontNameAttribute)));
            if (!fontCascadeName)
                continue;
            builder.append(", \""_s, fontCascadeName.get(), '"');
        }
    }
    builder.append(behavior == kMACaptionAppearanceBehaviorUseValue ? " !important;"_s : ";"_s);
    return builder.toString();
}

float CaptionUserPreferencesMediaAF::captionFontSizeScaleAndImportance(bool& important) const
{
    if (testingMode() || !MediaAccessibilityLibrary())
        return CaptionUserPreferences::captionFontSizeScaleAndImportance(important);

    MACaptionAppearanceBehavior behavior;
    CGFloat characterScale = CaptionUserPreferences::captionFontSizeScaleAndImportance(important);
    CGFloat scaleAdjustment = MACaptionAppearanceGetRelativeCharacterSize(kMACaptionAppearanceDomainUser, &behavior);

    if (!scaleAdjustment)
        return characterScale;

    important = behavior == kMACaptionAppearanceBehaviorUseValue;
#if defined(__LP64__) && __LP64__
    return narrowPrecisionToFloat(scaleAdjustment * characterScale);
#else
    return scaleAdjustment * characterScale;
#endif
}

void CaptionUserPreferencesMediaAF::platformSetPreferredLanguage(const String& language)
{
    MACaptionAppearanceAddSelectedLanguage(kMACaptionAppearanceDomainUser, language.createCFString().get());
}

void CaptionUserPreferencesMediaAF::setPreferredLanguage(const String& language)
{
    if (CaptionUserPreferences::captionDisplayMode() == CaptionDisplayMode::Manual)
        return;

    if (testingMode() || !MediaAccessibilityLibrary()) {
        CaptionUserPreferences::setPreferredLanguage(language);
        return;
    }

    if (captionPreferencesDelegate()) {
        captionPreferencesDelegate()->setPreferredLanguage(language);
        return;
    }

    platformSetPreferredLanguage(language);
}

Vector<String> CaptionUserPreferencesMediaAF::preferredLanguages() const
{
    auto preferredLanguages = CaptionUserPreferences::preferredLanguages();
    if (testingMode() || !MediaAccessibilityLibrary())
        return preferredLanguages;

    if (cachedPreferredLanguages().has_value())
        return cachedPreferredLanguages().value();

    auto captionLanguages = platformPreferredLanguages();
    if (!captionLanguages.size())
        return preferredLanguages;

    Vector<String> captionAndPreferredLanguages;
    captionAndPreferredLanguages.reserveInitialCapacity(captionLanguages.size() + preferredLanguages.size());
    captionAndPreferredLanguages.appendVector(WTFMove(captionLanguages));
    captionAndPreferredLanguages.appendVector(WTFMove(preferredLanguages));
    return captionAndPreferredLanguages;
}

Vector<String> CaptionUserPreferencesMediaAF::platformPreferredLanguages()
{
    RetainPtr captionLanguages = adoptCF(MACaptionAppearanceCopySelectedLanguages(kMACaptionAppearanceDomainUser));
    return captionLanguages ? makeVector<String>(captionLanguages.get()) : Vector<String> { };
}

void CaptionUserPreferencesMediaAF::setCachedPreferredLanguages(const Vector<String>& preferredLanguages)
{
    cachedPreferredLanguages() = preferredLanguages;
}

void CaptionUserPreferencesMediaAF::setPreferredAudioCharacteristic(const String& characteristic)
{
    if (testingMode() || !MediaAccessibilityLibrary())
        CaptionUserPreferences::setPreferredAudioCharacteristic(characteristic);
}

Vector<String> CaptionUserPreferencesMediaAF::preferredAudioCharacteristics() const
{
    if (testingMode() || !MediaAccessibilityLibrary() || !canLoad_MediaAccessibility_MAAudibleMediaCopyPreferredCharacteristics())
        return CaptionUserPreferences::preferredAudioCharacteristics();

    CFIndex characteristicCount = 0;
    RetainPtr characteristics = adoptCF(MAAudibleMediaCopyPreferredCharacteristics());
    if (characteristics)
        characteristicCount = CFArrayGetCount(characteristics.get());

    if (!characteristicCount)
        return CaptionUserPreferences::preferredAudioCharacteristics();

    return makeVector<String>(characteristics.get());
}
#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

String CaptionUserPreferencesMediaAF::captionsStyleSheetOverride() const
{
    if (testingMode())
        return CaptionUserPreferences::captionsStyleSheetOverride();
    
    StringBuilder captionsOverrideStyleSheet;

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    if (!MediaAccessibilityLibrary())
        return CaptionUserPreferences::captionsStyleSheetOverride();

    String captionsColor = captionsTextColorCSS();
    String edgeStyle = captionsTextEdgeCSS();
    String fontName = captionsDefaultFontCSS();
    String background = captionsBackgroundCSS();
    if (!background.isEmpty() || !captionsColor.isEmpty() || !edgeStyle.isEmpty() || !fontName.isEmpty()) {
        captionsOverrideStyleSheet.append(" ::"_s, UserAgentParts::cue(), '{', background, captionsColor, edgeStyle, fontName, '}');
        captionsOverrideStyleSheet.append(" ::"_s, UserAgentParts::cue(), "(rt) {"_s, background, captionsColor, edgeStyle, fontName, '}');
    }
    String windowColor = captionsWindowCSS();
    String windowCornerRadius = windowRoundedCornerRadiusCSS();
    if (!windowColor.isEmpty() || !windowCornerRadius.isEmpty())
        captionsOverrideStyleSheet.append(" ::"_s, UserAgentParts::webkitMediaTextTrackDisplayBackdrop(), '{', windowColor, windowCornerRadius, '}');
#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

    LOG(Media, "CaptionUserPreferencesMediaAF::captionsStyleSheetOverrideSetting style to:\n%s", captionsOverrideStyleSheet.toString().utf8().data());

    return captionsOverrideStyleSheet.toString();
}

static String languageIdentifier(const String& languageCode)
{
    if (languageCode.isEmpty())
        return languageCode;

    String lowercaseLanguageCode;
    // Need 2U here to disambiguate String::operator[] from operator(NSString*, int)[] in a production build.
    if (languageCode.length() >= 3 && (languageCode[2U] == '_' || languageCode[2U] == '-'))
        lowercaseLanguageCode = StringView(languageCode).left(2).convertToASCIILowercase();
    else
        lowercaseLanguageCode = languageCode.convertToASCIILowercase();

    return lowercaseLanguageCode;
}

static String addTextTrackKindDisplayNameIfNeeded(const TextTrack& track, const String& text)
{
    String result;

    if (track.isClosedCaptions()) {
        if (!text.contains(textTrackKindClosedCaptionsDisplayName()))
            result = addTextTrackKindClosedCaptionsSuffix(text);
    } else {
        switch (track.kind()) {
        case TextTrack::Kind::Subtitles:
            // Subtitle text tracks are only shown in a dedicated "Subtitle" grouping, meaning it
            // would look odd to add it as a suffix (e.g. "Subtitles > English Subtitles").
            break;

        case TextTrack::Kind::Captions:
            if (!text.contains(textTrackKindCaptionsDisplayName()))
                result = addTextTrackKindCaptionsSuffix(text);
            break;

        case TextTrack::Kind::Descriptions:
            if (!text.contains(textTrackKindDescriptionsDisplayName()))
                result = addTextTrackKindDescriptionsSuffix(text);
            break;

        case TextTrack::Kind::Chapters:
            if (!text.contains(textTrackKindChaptersDisplayName()))
                result = addTextTrackKindChaptersSuffix(text);
            break;

        case TextTrack::Kind::Metadata:
            if (!text.contains(textTrackKindMetadataDisplayName()))
                result = addTextTrackKindMetadataSuffix(text);
            break;

        case TextTrack::Kind::Forced:
            // Handled below.
            break;
        }
    }

    if (result.isEmpty())
        result = text;

    if (track.isSDH() && !text.contains(textTrackKindSDHDisplayName()))
        result = addTextTrackKindSDHSuffix(result);

    if (track.isEasyToRead() && !text.contains(textTrackKindEasyReaderDisplayName()))
        result = addTextTrackKindEasyReaderSuffix(result);

    if ((track.containsOnlyForcedSubtitles() || track.kind() == TextTrack::Kind::Forced) && !text.contains(textTrackKindForcedDisplayName()))
        result = addTextTrackKindForcedSuffix(result);

    return result;
}

static String addAudioTrackKindDisplayNameIfNeeded(const AudioTrack& track, const String& text)
{
    // Audio tracks are only shown in a dedicated "Languages" grouping, meaning it would look odd to
    // add it as a suffix (e.g. "Languages > English Language").

    if ((track.kind() == AudioTrack::descriptionKeyword() || track.kind() == AudioTrack::mainDescKeyword()) && !text.contains(audioTrackKindDescriptionsDisplayName()))
        return addAudioTrackKindDescriptionsSuffix(text);

    if (track.kind() == commentaryAtom() && !text.contains(audioTrackKindCommentaryDisplayName()))
        return addAudioTrackKindCommentarySuffix(text);

    return text;
}

static String addTrackKindDisplayNameIfNeeded(const TrackBase& track, const String& text)
{
    switch (track.type()) {
    case TrackBase::Type::TextTrack:
        return addTextTrackKindDisplayNameIfNeeded(downcast<TextTrack>(track), text);

    case TrackBase::Type::AudioTrack:
        return addAudioTrackKindDisplayNameIfNeeded(downcast<AudioTrack>(track), text);

    case TrackBase::Type::VideoTrack:
    case TrackBase::Type::BaseTrack:
        ASSERT_NOT_REACHED();
        break;
    }

    return text;
}

static String trackDisplayName(const TrackBase& track)
{
    if (&track == &TextTrack::captionMenuOffItem())
        return textTrackOffMenuItemText();
    if (&track == &TextTrack::captionMenuAutomaticItem())
        return textTrackAutomaticMenuItemText();

    String result;

    String label = track.label();
    String trackLanguageIdentifier = track.validBCP47Language();

    auto preferredLanguages = userPreferredLanguages(ShouldMinimizeLanguages::No);
    auto defaultLanguage = !preferredLanguages.isEmpty() ? preferredLanguages[0] : emptyString(); // This matches `defaultLanguage`.
    RetainPtr currentLocale = adoptCF(CFLocaleCreate(kCFAllocatorDefault, defaultLanguage.createCFString().get()));
    RetainPtr localeIdentifier = adoptCF(CFLocaleCreateCanonicalLocaleIdentifierFromString(kCFAllocatorDefault, trackLanguageIdentifier.createCFString().get()));
    String languageDisplayName = adoptCF(CFLocaleCopyDisplayNameForPropertyValue(currentLocale.get(), kCFLocaleLanguageCode, localeIdentifier.get())).get();

    bool exactMatch;
    bool matchesDefaultLanguage = !indexOfBestMatchingLanguageInList(trackLanguageIdentifier, { defaultLanguage }, exactMatch);

    // Only add the language if the track isn't for the default language or the track's label doesn't already contain the language.
    if (!label.isEmpty() && (matchesDefaultLanguage || (!languageDisplayName.isEmpty() && label.contains(languageDisplayName))))
        result = addTrackKindDisplayNameIfNeeded(track, label);
    else {
        String languageAndLocale = adoptCF(CFLocaleCopyDisplayNameForPropertyValue(currentLocale.get(), kCFLocaleIdentifier, trackLanguageIdentifier.createCFString().get())).get();
        if (!languageAndLocale.isEmpty())
            result = languageAndLocale;
        else if (!languageDisplayName.isEmpty())
            result = languageDisplayName;
        else
            result = localeIdentifier.get();

        result = addTrackKindDisplayNameIfNeeded(track, result);

        if (!label.isEmpty() && !result.contains(label))
            result = addTrackLabelAsSuffix(result, label);
    }

    if (result.isEmpty())
        return trackNoLabelText();

    return result;
}

String CaptionUserPreferencesMediaAF::displayNameForTrack(AudioTrack* track) const
{
    return trackDisplayName(*track);
}

String CaptionUserPreferencesMediaAF::displayNameForTrack(TextTrack* track) const
{
    return trackDisplayName(*track);
}

static bool textTrackCompare(const RefPtr<TextTrack>& a, const RefPtr<TextTrack>& b)
{
    auto preferredLanguages = userPreferredLanguages(ShouldMinimizeLanguages::No);

    bool aExactMatch;
    auto aUserLanguageIndex = indexOfBestMatchingLanguageInList(a->validBCP47Language(), preferredLanguages, aExactMatch);

    bool bExactMatch;
    auto bUserLanguageIndex = indexOfBestMatchingLanguageInList(b->validBCP47Language(), preferredLanguages, bExactMatch);

    if (aUserLanguageIndex != bUserLanguageIndex)
        return aUserLanguageIndex < bUserLanguageIndex;
    if (aExactMatch != bExactMatch)
        return aExactMatch;

    String aLanguageDisplayName = displayNameForLanguageLocale(languageIdentifier(a->validBCP47Language()));
    String bLanguageDisplayName = displayNameForLanguageLocale(languageIdentifier(b->validBCP47Language()));

    Collator collator;

    // Tracks not in the user's preferred language sort first by language ...
    if (auto languageDisplayNameComparison = collator.collate(aLanguageDisplayName, bLanguageDisplayName))
        return languageDisplayNameComparison < 0;

    // ... but when tracks have the same language, main program content sorts next highest ...
    bool aIsMainContent = a->isMainProgramContent();
    bool bIsMainContent = b->isMainProgramContent();
    if (aIsMainContent != bIsMainContent)
        return aIsMainContent;

    // ... and main program tracks sort higher than CC tracks ...
    bool aIsCC = a->isClosedCaptions();
    bool bIsCC = b->isClosedCaptions();
    if (aIsCC != bIsCC)
        return aIsCC;

    // ... and tracks of the same type and language sort by the menu item text.
    if (auto trackDisplayComparison = collator.collate(trackDisplayName(*a), trackDisplayName(*b)))
        return trackDisplayComparison < 0;

    // ... and if the menu item text is the same, compare the unique IDs
    return a->uniqueId() < b->uniqueId();
}

Vector<RefPtr<AudioTrack>> CaptionUserPreferencesMediaAF::sortedTrackListForMenu(AudioTrackList* trackList)
{
    ASSERT(trackList);
    
    Vector<RefPtr<AudioTrack>> tracksForMenu;
    
    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        AudioTrack* track = trackList->item(i);
        String language = displayNameForLanguageLocale(track->validBCP47Language());
        tracksForMenu.append(track);
    }

    Collator collator;

    std::sort(tracksForMenu.begin(), tracksForMenu.end(), [&] (auto& a, auto& b) {
        if (auto trackDisplayComparison = collator.collate(trackDisplayName(*a), trackDisplayName(*b)))
            return trackDisplayComparison < 0;

        return a->uniqueId() < b->uniqueId();
    });
    
    return tracksForMenu;
}

Vector<RefPtr<TextTrack>> CaptionUserPreferencesMediaAF::sortedTrackListForMenu(TextTrackList* trackList, HashSet<TextTrack::Kind> kinds)
{
    ASSERT(trackList);

    Vector<RefPtr<TextTrack>> tracksForMenu;
    HashSet<String> languagesIncluded;
    CaptionDisplayMode displayMode = captionDisplayMode();
    bool prefersAccessibilityTracks = userPrefersCaptions();
    bool filterTrackList = shouldFilterTrackMenu();
    bool requestingCaptionsOrDescriptionsOrSubtitles = kinds.contains(TextTrack::Kind::Subtitles) || kinds.contains(TextTrack::Kind::Captions) || kinds.contains(TextTrack::Kind::Descriptions);

    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        TextTrack* track = trackList->item(i);
        if (!kinds.contains(track->kind()))
            continue;

        String language = displayNameForLanguageLocale(track->validBCP47Language());

        if (displayMode == CaptionDisplayMode::Manual) {
            LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - adding '%s' track with language '%s' because selection mode is 'manual'", track->kindKeyword().string().utf8().data(), language.utf8().data());
            tracksForMenu.append(track);
            continue;
        }

        if (requestingCaptionsOrDescriptionsOrSubtitles) {
            if (track->containsOnlyForcedSubtitles()) {
                LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - skipping '%s' track with language '%s' because it contains only forced subtitles", track->kindKeyword().string().utf8().data(), language.utf8().data());
                continue;
            }

            if (track->isEasyToRead()) {
                LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - adding '%s' track with language '%s' because it is 'easy to read'", track->kindKeyword().string().utf8().data(), language.utf8().data());
                if (!language.isEmpty())
                    languagesIncluded.add(language);
                tracksForMenu.append(track);
                continue;
            }

            if (track->mode() == TextTrack::Mode::Showing) {
                LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - adding '%s' track with language '%s' because it is already visible", track->kindKeyword().string().utf8().data(), language.utf8().data());
                if (!language.isEmpty())
                    languagesIncluded.add(language);
                tracksForMenu.append(track);
                continue;
            }

            if (!language.isEmpty() && track->isMainProgramContent()) {
                bool isAccessibilityTrack = track->kind() == TextTrack::Kind::Captions;
                if (prefersAccessibilityTracks) {
                    // In the first pass, include only caption tracks if the user prefers accessibility tracks.
                    if (!isAccessibilityTrack && filterTrackList) {
                        LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - skipping '%s' track with language '%s' because it is NOT an accessibility track", track->kindKeyword().string().utf8().data(), language.utf8().data());
                        continue;
                    }
                } else {
                    // In the first pass, only include the first non-CC or SDH track with each language if the user prefers translation tracks.
                    if (isAccessibilityTrack && filterTrackList) {
                        LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - skipping '%s' track with language '%s' because it is an accessibility track", track->kindKeyword().string().utf8().data(), language.utf8().data());
                        continue;
                    }
                    if (languagesIncluded.contains(language) && filterTrackList) {
                        LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - skipping '%s' track with language '%s' because it is not the first with this language", track->kindKeyword().string().utf8().data(), language.utf8().data());
                        continue;
                    }
                }
            }

            if (!language.isEmpty())
                languagesIncluded.add(language);
        }

        tracksForMenu.append(track);

        LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - adding '%s' track with language '%s', is%s main program content", track->kindKeyword().string().utf8().data(), language.utf8().data(), track->isMainProgramContent() ? "" : " NOT");
    }

    if (requestingCaptionsOrDescriptionsOrSubtitles) {
        // Now that we have filtered for the user's accessibility/translation preference, add  all tracks with a unique language without regard to track type.
        for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
            TextTrack* track = trackList->item(i);
            String language = displayNameForLanguageLocale(track->language());

            if (tracksForMenu.contains(track))
                continue;

            if (!kinds.contains(track->kind()))
                continue;

            // All candidates with no languge were added the first time through.
            if (language.isEmpty())
                continue;

            if (track->containsOnlyForcedSubtitles())
                continue;

            if (!languagesIncluded.contains(language) && track->isMainProgramContent()) {
                languagesIncluded.add(language);
                tracksForMenu.append(track);
                LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - adding '%s' track with language '%s' because it is the only track with this language", track->kindKeyword().string().utf8().data(), language.utf8().data());
            }
        }
    }

    if (tracksForMenu.isEmpty())
        return tracksForMenu;

    std::sort(tracksForMenu.begin(), tracksForMenu.end(), textTrackCompare);

    if (requestingCaptionsOrDescriptionsOrSubtitles) {
        tracksForMenu.insert(0, &TextTrack::captionMenuOffItem());
        tracksForMenu.insert(1, &TextTrack::captionMenuAutomaticItem());
    }

    return tracksForMenu;
}
    
}

#endif // ENABLE(VIDEO)
