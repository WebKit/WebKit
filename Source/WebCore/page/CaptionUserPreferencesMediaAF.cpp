/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#if !USE(DIRECT2D)

#include "CaptionUserPreferencesMediaAF.h"

#include "AudioTrackList.h"
#include "FloatConversion.h"
#include "HTMLMediaElement.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "TextTrackList.h"
#include "UserStyleSheetTypes.h"
#include <algorithm>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/SoftLinking.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if PLATFORM(IOS_FAMILY)
#import "WebCoreThreadRun.h"
#endif

#if PLATFORM(WIN)
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/35bhkfb6.aspx
#pragma warning(disable: 4273)
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

#include <CoreText/CoreText.h>
#include <MediaAccessibility/MediaAccessibility.h>

#include "MediaAccessibilitySoftLink.h"

#if PLATFORM(WIN)

#ifdef DEBUG_ALL
#define SOFT_LINK_AVF_FRAMEWORK(Lib) SOFT_LINK_DEBUG_LIBRARY(Lib)
#else
#define SOFT_LINK_AVF_FRAMEWORK(Lib) SOFT_LINK_LIBRARY(Lib)
#endif

#define SOFT_LINK_AVF(Lib, Name, Type) SOFT_LINK_DLL_IMPORT(Lib, Name, Type)
#define SOFT_LINK_AVF_POINTER(Lib, Name, Type) SOFT_LINK_VARIABLE_DLL_IMPORT_OPTIONAL(Lib, Name, Type)
#define SOFT_LINK_AVF_FRAMEWORK_IMPORT(Lib, Fun, ReturnType, Arguments, Signature) SOFT_LINK_DLL_IMPORT(Lib, Fun, ReturnType, __cdecl, Arguments, Signature)
#define SOFT_LINK_AVF_FRAMEWORK_IMPORT_OPTIONAL(Lib, Fun, ReturnType, Arguments) SOFT_LINK_DLL_IMPORT_OPTIONAL(Lib, Fun, ReturnType, __cdecl, Arguments)

SOFT_LINK_AVF_FRAMEWORK(CoreMedia)
SOFT_LINK_AVF_FRAMEWORK_IMPORT_OPTIONAL(CoreMedia, MTEnableCaption2015Behavior, Boolean, ())

#else // PLATFORM(WIN)

SOFT_LINK_FRAMEWORK_OPTIONAL(MediaToolbox)
SOFT_LINK_OPTIONAL(MediaToolbox, MTEnableCaption2015Behavior, Boolean, (), ())

#endif // !PLATFORM(WIN)

#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

namespace WebCore {

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

static void userCaptionPreferencesChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void *, CFDictionaryRef)
{
#if !PLATFORM(IOS_FAMILY)
    static_cast<CaptionUserPreferencesMediaAF*>(observer)->captionPreferencesChanged();
#else
    WebThreadRun(^{
        static_cast<CaptionUserPreferencesMediaAF*>(observer)->captionPreferencesChanged();
    });
#endif
}

#endif

CaptionUserPreferencesMediaAF::CaptionUserPreferencesMediaAF(PageGroup& group)
    : CaptionUserPreferences(group)
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    , m_updateStyleSheetTimer(*this, &CaptionUserPreferencesMediaAF::updateTimerFired)
    , m_listeningForPreferenceChanges(false)
#endif
{
    static bool initialized;
    if (!initialized) {
        initialized = true;

#if !PLATFORM(WIN)
        if (!MediaToolboxLibrary())
            return;
#endif

        MTEnableCaption2015BehaviorPtrType function = MTEnableCaption2015BehaviorPtr();
        if (!function || !function())
            return;

        beginBlockingNotifications();
        CaptionUserPreferences::setCaptionDisplayMode(Manual);
        setUserPrefersCaptions(false);
        setUserPrefersSubtitles(false);
        setUserPrefersTextDescriptions(false);
        endBlockingNotifications();
    }
}

CaptionUserPreferencesMediaAF::~CaptionUserPreferencesMediaAF()
{
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    if (kMAXCaptionAppearanceSettingsChangedNotification)
        CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this, kMAXCaptionAppearanceSettingsChangedNotification, 0);
    if (kMAAudibleMediaSettingsChangedNotification)
        CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this, kMAAudibleMediaSettingsChangedNotification, 0);
#endif
}

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

CaptionUserPreferences::CaptionDisplayMode CaptionUserPreferencesMediaAF::captionDisplayMode() const
{
    CaptionDisplayMode internalMode = CaptionUserPreferences::captionDisplayMode();
    if (internalMode == Manual || testingMode() || !MediaAccessibilityLibrary())
        return internalMode;

    MACaptionAppearanceDisplayType displayType = MACaptionAppearanceGetDisplayType(kMACaptionAppearanceDomainUser);
    switch (displayType) {
    case kMACaptionAppearanceDisplayTypeForcedOnly:
        return ForcedOnly;

    case kMACaptionAppearanceDisplayTypeAutomatic:
        return Automatic;

    case kMACaptionAppearanceDisplayTypeAlwaysOn:
        return AlwaysOn;
    }

    ASSERT_NOT_REACHED();
    return ForcedOnly;
}
    
void CaptionUserPreferencesMediaAF::setCaptionDisplayMode(CaptionUserPreferences::CaptionDisplayMode mode)
{
    if (testingMode() || !MediaAccessibilityLibrary()) {
        CaptionUserPreferences::setCaptionDisplayMode(mode);
        return;
    }

    if (captionDisplayMode() == Manual)
        return;

    MACaptionAppearanceDisplayType displayType = kMACaptionAppearanceDisplayTypeForcedOnly;
    switch (mode) {
    case Automatic:
        displayType = kMACaptionAppearanceDisplayTypeAutomatic;
        break;
    case ForcedOnly:
        displayType = kMACaptionAppearanceDisplayTypeForcedOnly;
        break;
    case AlwaysOn:
        displayType = kMACaptionAppearanceDisplayTypeAlwaysOn;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    MACaptionAppearanceSetDisplayType(kMACaptionAppearanceDomainUser, displayType);
}

bool CaptionUserPreferencesMediaAF::userPrefersCaptions() const
{
    bool captionSetting = CaptionUserPreferences::userPrefersCaptions();
    if (captionSetting || testingMode() || !MediaAccessibilityLibrary())
        return captionSetting;
    
    RetainPtr<CFArrayRef> captioningMediaCharacteristics = adoptCF(MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics(kMACaptionAppearanceDomainUser));
    return captioningMediaCharacteristics && CFArrayGetCount(captioningMediaCharacteristics.get());
}

bool CaptionUserPreferencesMediaAF::userPrefersSubtitles() const
{
    bool subtitlesSetting = CaptionUserPreferences::userPrefersSubtitles();
    if (subtitlesSetting || testingMode() || !MediaAccessibilityLibrary())
        return subtitlesSetting;
    
    RetainPtr<CFArrayRef> captioningMediaCharacteristics = adoptCF(MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics(kMACaptionAppearanceDomainUser));
    return !(captioningMediaCharacteristics && CFArrayGetCount(captioningMediaCharacteristics.get()));
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

    if (kMAXCaptionAppearanceSettingsChangedNotification)
        CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, userCaptionPreferencesChangedNotificationCallback, kMAXCaptionAppearanceSettingsChangedNotification, 0, CFNotificationSuspensionBehaviorCoalesce);
    if (canLoad_MediaAccessibility_kMAAudibleMediaSettingsChangedNotification())
        CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), this, userCaptionPreferencesChangedNotificationCallback, kMAAudibleMediaSettingsChangedNotification, 0, CFNotificationSuspensionBehaviorCoalesce);
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

String CaptionUserPreferencesMediaAF::captionsWindowCSS() const
{
    MACaptionAppearanceBehavior behavior;
    RetainPtr<CGColorRef> color = adoptCF(MACaptionAppearanceCopyWindowColor(kMACaptionAppearanceDomainUser, &behavior));

    Color windowColor(color.get());
    if (!windowColor.isValid())
        windowColor = Color::transparent;

    bool important = behavior == kMACaptionAppearanceBehaviorUseValue;
    CGFloat opacity = MACaptionAppearanceGetWindowOpacity(kMACaptionAppearanceDomainUser, &behavior);
    if (!important)
        important = behavior == kMACaptionAppearanceBehaviorUseValue;
    String windowStyle = colorPropertyCSS(CSSPropertyBackgroundColor, windowColor.colorWithAlpha(opacity), important);

    if (!opacity)
        return windowStyle;

    return makeString(windowStyle, getPropertyNameString(CSSPropertyPadding), ": .4em !important;");
}

String CaptionUserPreferencesMediaAF::captionsBackgroundCSS() const
{
    // This default value must be the same as the one specified in mediaControls.css for -webkit-media-text-track-past-nodes
    // and webkit-media-text-track-future-nodes.
    constexpr auto defaultBackgroundColor = makeSimpleColor(0, 0, 0, 0.8 * 255);

    MACaptionAppearanceBehavior behavior;

    RetainPtr<CGColorRef> color = adoptCF(MACaptionAppearanceCopyBackgroundColor(kMACaptionAppearanceDomainUser, &behavior));
    Color backgroundColor(color.get());
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
    RetainPtr<CGColorRef> color = adoptCF(MACaptionAppearanceCopyForegroundColor(kMACaptionAppearanceDomainUser, &behavior));
    Color textColor(color.get());
    if (!textColor.isValid())
        // This default value must be the same as the one specified in mediaControls.css for -webkit-media-text-track-container.
        textColor = Color::white;
    
    important = behavior == kMACaptionAppearanceBehaviorUseValue;
    CGFloat opacity = MACaptionAppearanceGetForegroundOpacity(kMACaptionAppearanceDomainUser, &behavior);
    if (!important)
        important = behavior == kMACaptionAppearanceBehaviorUseValue;
    return textColor.colorWithAlpha(opacity);
}
    
String CaptionUserPreferencesMediaAF::captionsTextColorCSS() const
{
    bool important;
    Color textColor = captionsTextColor(important);

    if (!textColor.isValid())
        return emptyString();

    return colorPropertyCSS(CSSPropertyColor, textColor, important);
}

static void appendCSS(StringBuilder& builder, CSSPropertyID id, const String& value, bool important)
{
    builder.append(getPropertyNameString(id));
    builder.append(':');
    builder.append(value);
    if (important)
        builder.appendLiteral(" !important");
    builder.append(';');
}
    
String CaptionUserPreferencesMediaAF::windowRoundedCornerRadiusCSS() const
{
    MACaptionAppearanceBehavior behavior;
    CGFloat radius = MACaptionAppearanceGetWindowRoundedCornerRadius(kMACaptionAppearanceDomainUser, &behavior);
    if (!radius)
        return emptyString();

    StringBuilder builder;
    appendCSS(builder, CSSPropertyBorderRadius, makeString(radius, "px"), behavior == kMACaptionAppearanceBehaviorUseValue);
    return builder.toString();
}

String CaptionUserPreferencesMediaAF::colorPropertyCSS(CSSPropertyID id, const Color& color, bool important) const
{
    StringBuilder builder;
    appendCSS(builder, id, color.serialized(), important);
    return builder.toString();
}

bool CaptionUserPreferencesMediaAF::captionStrokeWidthForFont(float fontSize, const String& language, float& strokeWidth, bool& important) const
{
    if (!canLoad_MediaAccessibility_MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle())
        return false;
    
    MACaptionAppearanceBehavior behavior;
    auto trackLanguage = language.createCFString();
    CGFloat strokeWidthPt;
    
    auto fontDescriptor = adoptCF(MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle(kMACaptionAppearanceDomainUser, &behavior, kMACaptionAppearanceFontStyleDefault, trackLanguage.get(), fontSize, &strokeWidthPt));

    if (!fontDescriptor)
        return false;

    // Since only half of the stroke is visible because the stroke is drawn before the fill, we double the stroke width here.
    strokeWidth = strokeWidthPt * 2;
    important = behavior == kMACaptionAppearanceBehaviorUseValue;
    
    return true;
}

String CaptionUserPreferencesMediaAF::captionsTextEdgeCSS() const
{
    static NeverDestroyed<const String> edgeStyleRaised(MAKE_STATIC_STRING_IMPL(" -.1em -.1em .16em "));
    static NeverDestroyed<const String> edgeStyleDepressed(MAKE_STATIC_STRING_IMPL(" .1em .1em .16em "));
    static NeverDestroyed<const String> edgeStyleDropShadow(MAKE_STATIC_STRING_IMPL(" 0 .1em .16em "));

    MACaptionAppearanceBehavior behavior;
    MACaptionAppearanceTextEdgeStyle textEdgeStyle = MACaptionAppearanceGetTextEdgeStyle(kMACaptionAppearanceDomainUser, &behavior);
    
    if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleUndefined || textEdgeStyle == kMACaptionAppearanceTextEdgeStyleNone)
        return emptyString();

    StringBuilder builder;
    bool important = behavior == kMACaptionAppearanceBehaviorUseValue;
    if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleRaised)
        appendCSS(builder, CSSPropertyTextShadow, makeString(edgeStyleRaised.get(), " black"), important);
    else if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleDepressed)
        appendCSS(builder, CSSPropertyTextShadow, makeString(edgeStyleDepressed.get(), " black"), important);
    else if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleDropShadow)
        appendCSS(builder, CSSPropertyTextShadow, makeString(edgeStyleDropShadow.get(), " black"), important);

    if (textEdgeStyle == kMACaptionAppearanceTextEdgeStyleDropShadow || textEdgeStyle == kMACaptionAppearanceTextEdgeStyleUniform) {
        appendCSS(builder, CSSPropertyStrokeColor, "black", important);
        appendCSS(builder, CSSPropertyPaintOrder, getValueName(CSSValueStroke), important);
        appendCSS(builder, CSSPropertyStrokeLinejoin, getValueName(CSSValueRound), important);
        appendCSS(builder, CSSPropertyStrokeLinecap, getValueName(CSSValueRound), important);
    }
    
    return builder.toString();
}

String CaptionUserPreferencesMediaAF::captionsDefaultFontCSS() const
{
    MACaptionAppearanceBehavior behavior;
    
    RetainPtr<CTFontDescriptorRef> font = adoptCF(MACaptionAppearanceCopyFontDescriptorForStyle(kMACaptionAppearanceDomainUser, &behavior, kMACaptionAppearanceFontStyleDefault));
    if (!font)
        return emptyString();

    RetainPtr<CFTypeRef> name = adoptCF(CTFontDescriptorCopyAttribute(font.get(), kCTFontNameAttribute));
    if (!name)
        return emptyString();

    StringBuilder builder;
    
    builder.append(getPropertyNameString(CSSPropertyFontFamily));
    builder.appendLiteral(": \"");
    builder.append(static_cast<CFStringRef>(name.get()));
    builder.append('"');

    auto cascadeList = adoptCF(static_cast<CFArrayRef>(CTFontDescriptorCopyAttribute(font.get(), kCTFontCascadeListAttribute)));

    if (cascadeList) {
        for (CFIndex i = 0; i < CFArrayGetCount(cascadeList.get()); i++) {
            auto fontCascade = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(cascadeList.get(), i));
            if (!fontCascade)
                continue;
            auto fontCascadeName = adoptCF(CTFontDescriptorCopyAttribute(fontCascade, kCTFontNameAttribute));
            if (!fontCascadeName)
                continue;
            builder.append(", \"");
            builder.append(static_cast<CFStringRef>(fontCascadeName.get()));
            builder.append('"');
        }
    }
    
    if (behavior == kMACaptionAppearanceBehaviorUseValue)
        builder.appendLiteral(" !important");
    builder.append(';');
    
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

void CaptionUserPreferencesMediaAF::setPreferredLanguage(const String& language)
{
    if (CaptionUserPreferences::captionDisplayMode() == Manual)
        return;

    if (testingMode() || !MediaAccessibilityLibrary()) {
        CaptionUserPreferences::setPreferredLanguage(language);
        return;
    }

    MACaptionAppearanceAddSelectedLanguage(kMACaptionAppearanceDomainUser, language.createCFString().get());
}

Vector<String> CaptionUserPreferencesMediaAF::preferredLanguages() const
{
    if (testingMode() || !MediaAccessibilityLibrary())
        return CaptionUserPreferences::preferredLanguages();

    Vector<String> platformLanguages = platformUserPreferredLanguages();
    Vector<String> override = userPreferredLanguagesOverride();
    if (!override.isEmpty()) {
        if (platformLanguages.size() != override.size())
            return override;
        for (size_t i = 0; i < override.size(); i++) {
            if (override[i] != platformLanguages[i])
                return override;
        }
    }

    CFIndex languageCount = 0;
    RetainPtr<CFArrayRef> languages = adoptCF(MACaptionAppearanceCopySelectedLanguages(kMACaptionAppearanceDomainUser));
    if (languages)
        languageCount = CFArrayGetCount(languages.get());

    if (!languageCount)
        return CaptionUserPreferences::preferredLanguages();

    Vector<String> userPreferredLanguages;
    userPreferredLanguages.reserveCapacity(languageCount + platformLanguages.size());
    for (CFIndex i = 0; i < languageCount; i++)
        userPreferredLanguages.append(static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages.get(), i)));

    userPreferredLanguages.appendVector(platformLanguages);

    return userPreferredLanguages;
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
    RetainPtr<CFArrayRef> characteristics = adoptCF(MAAudibleMediaCopyPreferredCharacteristics());
    if (characteristics)
        characteristicCount = CFArrayGetCount(characteristics.get());

    if (!characteristicCount)
        return CaptionUserPreferences::preferredAudioCharacteristics();

    Vector<String> userPreferredAudioCharacteristics;
    userPreferredAudioCharacteristics.reserveCapacity(characteristicCount);
    for (CFIndex i = 0; i < characteristicCount; i++)
        userPreferredAudioCharacteristics.append(static_cast<CFStringRef>(CFArrayGetValueAtIndex(characteristics.get(), i)));

    return userPreferredAudioCharacteristics;
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
        captionsOverrideStyleSheet.appendLiteral(" ::");
        captionsOverrideStyleSheet.append(TextTrackCue::cueShadowPseudoId());
        captionsOverrideStyleSheet.append('{');
        
        if (!background.isEmpty())
            captionsOverrideStyleSheet.append(background);
        if (!captionsColor.isEmpty())
            captionsOverrideStyleSheet.append(captionsColor);
        if (!edgeStyle.isEmpty())
            captionsOverrideStyleSheet.append(edgeStyle);
        if (!fontName.isEmpty())
            captionsOverrideStyleSheet.append(fontName);
        
        captionsOverrideStyleSheet.append('}');
    }
    
    String windowColor = captionsWindowCSS();
    String windowCornerRadius = windowRoundedCornerRadiusCSS();
    if (!windowColor.isEmpty() || !windowCornerRadius.isEmpty()) {
        captionsOverrideStyleSheet.appendLiteral(" ::");
        captionsOverrideStyleSheet.append(TextTrackCue::cueBackdropShadowPseudoId());
        captionsOverrideStyleSheet.append('{');
        
        if (!windowColor.isEmpty())
            captionsOverrideStyleSheet.append(windowColor);
        if (!windowCornerRadius.isEmpty()) {
            captionsOverrideStyleSheet.append(windowCornerRadius);
        }
        
        captionsOverrideStyleSheet.append('}');
    }
#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

    LOG(Media, "CaptionUserPreferencesMediaAF::captionsStyleSheetOverrideSetting style to:\n%s", captionsOverrideStyleSheet.toString().utf8().data());

    return captionsOverrideStyleSheet.toString();
}

static String languageIdentifier(const String& languageCode)
{
    if (languageCode.isEmpty())
        return languageCode;

    String lowercaseLanguageCode = languageCode.convertToASCIILowercase();

    // Need 2U here to disambiguate String::operator[] from operator(NSString*, int)[] in a production build.
    if (lowercaseLanguageCode.length() >= 3 && (lowercaseLanguageCode[2U] == '_' || lowercaseLanguageCode[2U] == '-'))
        lowercaseLanguageCode.truncate(2);

    return lowercaseLanguageCode;
}

static void buildDisplayStringForTrackBase(StringBuilder& displayName, const TrackBase& track)
{
    String label = track.label();
    String trackLanguageIdentifier = track.validBCP47Language();

    RetainPtr<CFLocaleRef> currentLocale = adoptCF(CFLocaleCreate(kCFAllocatorDefault, defaultLanguage().createCFString().get()));
    RetainPtr<CFStringRef> localeIdentifier = adoptCF(CFLocaleCreateCanonicalLocaleIdentifierFromString(kCFAllocatorDefault, trackLanguageIdentifier.createCFString().get()));
    RetainPtr<CFStringRef> languageCF = adoptCF(CFLocaleCopyDisplayNameForPropertyValue(currentLocale.get(), kCFLocaleLanguageCode, localeIdentifier.get()));
    String language = languageCF.get();

    if (!label.isEmpty()) {
        if (language.isEmpty() || label.contains(language))
            displayName.append(label);
        else {
            RetainPtr<CFDictionaryRef> localeDict = adoptCF(CFLocaleCreateComponentsFromLocaleIdentifier(kCFAllocatorDefault, localeIdentifier.get()));
            if (localeDict) {
                CFStringRef countryCode = 0;
                String countryName;
                
                CFDictionaryGetValueIfPresent(localeDict.get(), kCFLocaleCountryCode, (const void **)&countryCode);
                if (countryCode) {
                    RetainPtr<CFStringRef> countryNameCF = adoptCF(CFLocaleCopyDisplayNameForPropertyValue(currentLocale.get(), kCFLocaleCountryCode, countryCode));
                    countryName = countryNameCF.get();
                }
                
                if (!countryName.isEmpty())
                    displayName.append(textTrackCountryAndLanguageMenuItemText(label, countryName, language));
                else
                    displayName.append(textTrackLanguageMenuItemText(label, language));
            }
        }
    } else {
        String languageAndLocale = adoptCF(CFLocaleCopyDisplayNameForPropertyValue(currentLocale.get(), kCFLocaleIdentifier, trackLanguageIdentifier.createCFString().get())).get();
        if (!languageAndLocale.isEmpty())
            displayName.append(languageAndLocale);
        else if (!language.isEmpty())
            displayName.append(language);
        else
            displayName.append(localeIdentifier.get());
    }
}

static String trackDisplayName(AudioTrack* track)
{
    StringBuilder displayName;
    buildDisplayStringForTrackBase(displayName, *track);
    
    if (displayName.isEmpty())
        displayName.append(audioTrackNoLabelText());

    if (track->kind() != AudioTrack::descriptionKeyword())
        return displayName.toString();

    return audioDescriptionTrackSuffixText(displayName.toString());
}

String CaptionUserPreferencesMediaAF::displayNameForTrack(AudioTrack* track) const
{
    return trackDisplayName(track);
}

static String trackDisplayName(TextTrack* track)
{
    if (track == &TextTrack::captionMenuOffItem())
        return textTrackOffMenuItemText();
    if (track == &TextTrack::captionMenuAutomaticItem())
        return textTrackAutomaticMenuItemText();

    StringBuilder displayNameBuilder;
    buildDisplayStringForTrackBase(displayNameBuilder, *track);

    if (displayNameBuilder.isEmpty())
        displayNameBuilder.append(textTrackNoLabelText());

    String displayName = displayNameBuilder.toString();

    if (track->isClosedCaptions()) {
        displayName = closedCaptionTrackMenuItemText(displayName);
        if (track->isEasyToRead())
            displayName = easyReaderTrackMenuItemText(displayName);

        return displayName;
    }

    if (track->isSDH())
        displayName = sdhTrackMenuItemText(displayName);

    if (track->containsOnlyForcedSubtitles())
        displayName = forcedTrackMenuItemText(displayName);

    if (track->isEasyToRead())
        displayName = easyReaderTrackMenuItemText(displayName);

    return displayName;
}

String CaptionUserPreferencesMediaAF::displayNameForTrack(TextTrack* track) const
{
    return trackDisplayName(track);
}

static bool textTrackCompare(const RefPtr<TextTrack>& a, const RefPtr<TextTrack>& b)
{
    String preferredLanguageDisplayName = displayNameForLanguageLocale(languageIdentifier(defaultLanguage()));
    String aLanguageDisplayName = displayNameForLanguageLocale(languageIdentifier(a->validBCP47Language()));
    String bLanguageDisplayName = displayNameForLanguageLocale(languageIdentifier(b->validBCP47Language()));

    // Tracks in the user's preferred language are always at the top of the menu.
    bool aIsPreferredLanguage = !codePointCompare(aLanguageDisplayName, preferredLanguageDisplayName);
    bool bIsPreferredLanguage = !codePointCompare(bLanguageDisplayName, preferredLanguageDisplayName);
    if (aIsPreferredLanguage != bIsPreferredLanguage)
        return aIsPreferredLanguage;

    // Tracks not in the user's preferred language sort first by language ...
    if (auto languageDisplayNameComparison = codePointCompare(aLanguageDisplayName, bLanguageDisplayName))
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
    if (auto trackDisplayComparison = codePointCompare(trackDisplayName(a.get()), trackDisplayName(b.get())))
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
    
    std::sort(tracksForMenu.begin(), tracksForMenu.end(), [](auto& a, auto& b) {
        auto trackDisplayComparison = codePointCompare(trackDisplayName(a.get()), trackDisplayName(b.get()));
        if (trackDisplayComparison)
            return trackDisplayComparison < 0;

        return a->uniqueId() < b->uniqueId();
    });
    
    return tracksForMenu;
}

Vector<RefPtr<TextTrack>> CaptionUserPreferencesMediaAF::sortedTrackListForMenu(TextTrackList* trackList)
{
    ASSERT(trackList);

    Vector<RefPtr<TextTrack>> tracksForMenu;
    HashSet<String> languagesIncluded;
    CaptionDisplayMode displayMode = captionDisplayMode();
    bool prefersAccessibilityTracks = userPrefersCaptions();
    bool filterTrackList = shouldFilterTrackMenu();

    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        TextTrack* track = trackList->item(i);
        String language = displayNameForLanguageLocale(track->validBCP47Language());

        if (displayMode == Manual) {
            LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - adding '%s' track with language '%s' because selection mode is 'manual'", track->kindKeyword().string().utf8().data(), language.utf8().data());
            tracksForMenu.append(track);
            continue;
        }

        auto kind = track->kind();
        if (kind != TextTrack::Kind::Captions && kind != TextTrack::Kind::Descriptions && kind != TextTrack::Kind::Subtitles)
            continue;

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
        tracksForMenu.append(track);

        LOG(Media, "CaptionUserPreferencesMediaAF::sortedTrackListForMenu - adding '%s' track with language '%s', is%s main program content", track->kindKeyword().string().utf8().data(), language.utf8().data(), track->isMainProgramContent() ? "" : " NOT");
    }

    // Now that we have filtered for the user's accessibility/translation preference, add  all tracks with a unique language without regard to track type.
    for (unsigned i = 0, length = trackList->length(); i < length; ++i) {
        TextTrack* track = trackList->item(i);
        String language = displayNameForLanguageLocale(track->language());

        if (tracksForMenu.contains(track))
            continue;

        auto kind = track->kind();
        if (kind != TextTrack::Kind::Captions && kind != TextTrack::Kind::Descriptions && kind != TextTrack::Kind::Subtitles)
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

    if (tracksForMenu.isEmpty())
        return tracksForMenu;

    std::sort(tracksForMenu.begin(), tracksForMenu.end(), textTrackCompare);

    tracksForMenu.insert(0, &TextTrack::captionMenuOffItem());
    tracksForMenu.insert(1, &TextTrack::captionMenuAutomaticItem());

    return tracksForMenu;
}
    
}

#endif

#endif // ENABLE(VIDEO_TRACK)
