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
#include "CaptionStyleSheetMediaAF.h"

#if ENABLE(VIDEO_TRACK)

#include "CSSPropertyNames.h"
#include "Color.h"
#include "Logging.h"
#include "SoftLinking.h"
#include "TextTrackCue.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include "CoreText/CoreText.h"
#include <MediaAccessibility/MediaAccessibility.h>
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

#if !PLATFORM(WIN)
#define SOFT_LINK_AVF_FRAMEWORK(Lib) SOFT_LINK_FRAMEWORK_OPTIONAL(Lib)
#define SOFT_LINK_AVF(Lib, Name, Type) SOFT_LINK(Lib, Name, Type)
#define SOFT_LINK_AVF_POINTER(Lib, Name, Type) SOFT_LINK_POINTER_OPTIONAL(Lib, Name, Type)
#define SOFT_LINK_AVF_FRAMEWORK_IMPORT(Lib, Fun, ReturnType, Arguments, Signature) SOFT_LINK(Lib, Fun, ReturnType, Arguments, Signature)
#else

#ifdef DEBUG_ALL
#define SOFT_LINK_AVF_FRAMEWORK(Lib) SOFT_LINK_DEBUG_LIBRARY(Lib)
#else
#define SOFT_LINK_AVF_FRAMEWORK(Lib) SOFT_LINK_LIBRARY(Lib)
#endif

#define SOFT_LINK_AVF(Lib, Name, Type) SOFT_LINK_DLL_IMPORT(Lib, Name, Type)
#define SOFT_LINK_AVF_POINTER(Lib, Name, Type) SOFT_LINK_VARIABLE_DLL_IMPORT_OPTIONAL(Lib, Name, Type)
#define SOFT_LINK_AVF_FRAMEWORK_IMPORT(Lib, Fun, ReturnType, Arguments, Signature) SOFT_LINK_DLL_IMPORT(Lib, Fun, ReturnType, __cdecl, Arguments, Signature)
#endif

SOFT_LINK_AVF_FRAMEWORK(MediaAccessibility)
SOFT_LINK_AVF_FRAMEWORK(CoreText)

SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceCopyForegroundColor, CGColorRef, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceCopyBackgroundColor, CGColorRef, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceCopyWindowColor, CGColorRef, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceGetForegroundOpacity, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceGetBackgroundOpacity, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceGetWindowOpacity, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceGetWindowRoundedCornerRadius, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceCopyFontDescriptorForStyle, CTFontDescriptorRef, (MACaptionAppearanceDomain domain,  MACaptionAppearanceBehavior *behavior, MACaptionAppearanceFontStyle fontStyle), (domain, behavior, fontStyle))
SOFT_LINK_AVF_FRAMEWORK_IMPORT(MediaAccessibility, MACaptionAppearanceGetTextEdgeStyle, MACaptionAppearanceTextEdgeStyle, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))

SOFT_LINK_AVF_FRAMEWORK_IMPORT(CoreText, CTFontDescriptorCopyAttribute,  CFTypeRef, (CTFontDescriptorRef descriptor, CFStringRef attribute), (descriptor, attribute));

#if PLATFORM(WIN)
// These are needed on Windows due to the way DLLs work. We do not need them on other platforms
#define MACaptionAppearanceCopyForegroundColor softLink_MACaptionAppearanceCopyForegroundColor
#define MACaptionAppearanceCopyBackgroundColor softLink_MACaptionAppearanceCopyBackgroundColor
#define MACaptionAppearanceCopyWindowColor softLink_MACaptionAppearanceCopyWindowColor
#define MACaptionAppearanceGetForegroundOpacity softLink_MACaptionAppearanceGetForegroundOpacity
#define MACaptionAppearanceGetBackgroundOpacity softLink_MACaptionAppearanceGetBackgroundOpacity
#define MACaptionAppearanceGetWindowOpacity softLink_MACaptionAppearanceGetWindowOpacity
#define MACaptionAppearanceGetWindowRoundedCornerRadius softLink_MACaptionAppearanceGetWindowRoundedCornerRadius
#define MACaptionAppearanceCopyFontDescriptorForStyle softLink_MACaptionAppearanceCopyFontDescriptorForStyle
#define MACaptionAppearanceGetTextEdgeStyle softLink_MACaptionAppearanceGetTextEdgeStyle
#define CTFontDescriptorCopyAttribute softLink_CTFontDescriptorCopyAttribute
#endif

SOFT_LINK_AVF_POINTER(CoreText, kCTFontNameAttribute, CFStringRef)
#define kCTFontNameAttribute getkCTFontNameAttribute()
#endif

namespace WebCore {

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

static String captionsWindowCSS();
static String captionsBackgroundCSS();
static String captionsTextColorCSS();
static Color captionsTextColor(bool&);
static String captionsDefaultFontCSS();
static Color captionsEdgeColorForTextColor(const Color&);
static String windowRoundedCornerRadiusCSS();
static String captionsTextEdgeCSS();
static String cssPropertyWithTextEdgeColor(CSSPropertyID, const String&, const Color&, bool);
static String colorPropertyCSS(CSSPropertyID, const Color&, bool);

#endif

String defaultCaptionsStyleSheet()
{
    StringBuilder captionsOverrideStyleSheet;

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
    if (!MediaAccessibilityLibrary())
        return String();
    
    String captionsColor = captionsTextColorCSS();
    String edgeStyle = captionsTextEdgeCSS();
    String fontName = captionsDefaultFontCSS();
    String background = captionsBackgroundCSS();
    if (!background.isEmpty() || !captionsColor.isEmpty() || !edgeStyle.isEmpty() || !fontName.isEmpty()) {
        captionsOverrideStyleSheet.append(" video::");
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
        captionsOverrideStyleSheet.append(" video::");
        captionsOverrideStyleSheet.append(TextTrackCueBox::textTrackCueBoxShadowPseudoId());
        captionsOverrideStyleSheet.append('{');
        
        if (!windowColor.isEmpty())
            captionsOverrideStyleSheet.append(windowColor);
        if (!windowCornerRadius.isEmpty())
            captionsOverrideStyleSheet.append(windowCornerRadius);
        
        captionsOverrideStyleSheet.append('}');
    }
#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

    LOG(Media, "CaptionUserPreferencesMediaAF::captionsStyleSheetOverrideSetting sytle to:\n%s", captionsOverrideStyleSheet.toString().utf8().data());

    return captionsOverrideStyleSheet.toString();
}

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

String captionsWindowCSS()
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
    String windowStyle = colorPropertyCSS(CSSPropertyBackgroundColor, Color(windowColor.red(), windowColor.green(), windowColor.blue(), static_cast<int>(opacity * 255)), important);

    if (!opacity)
        return windowStyle;

    StringBuilder builder;
    builder.append(windowStyle);
    builder.append(getPropertyNameString(CSSPropertyPadding));
    builder.append(": .4em !important;");

    return builder.toString();
}

String captionsBackgroundCSS()
{
    // This default value must be the same as the one specified in mediaControls.css for -webkit-media-text-track-past-nodes
    // and webkit-media-text-track-future-nodes.
    DEFINE_STATIC_LOCAL(Color, defaultBackgroundColor, (Color(0, 0, 0, 0.8 * 255)));

    MACaptionAppearanceBehavior behavior;

    RetainPtr<CGColorRef> color = adoptCF(MACaptionAppearanceCopyBackgroundColor(kMACaptionAppearanceDomainUser, &behavior));
    Color backgroundColor(color.get());
    if (!backgroundColor.isValid())
        backgroundColor = defaultBackgroundColor;

    bool important = behavior == kMACaptionAppearanceBehaviorUseValue;
    CGFloat opacity = MACaptionAppearanceGetBackgroundOpacity(kMACaptionAppearanceDomainUser, &behavior);
    if (!important)
        important = behavior == kMACaptionAppearanceBehaviorUseValue;
    return colorPropertyCSS(CSSPropertyBackgroundColor, Color(backgroundColor.red(), backgroundColor.green(), backgroundColor.blue(), static_cast<int>(opacity * 255)), important);
}

Color captionsTextColor(bool& important)
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
    return Color(textColor.red(), textColor.green(), textColor.blue(), static_cast<int>(opacity * 255));
}
    
String captionsTextColorCSS()
{
    bool important;
    Color textColor = captionsTextColor(important);

    if (!textColor.isValid())
        return emptyString();

    return colorPropertyCSS(CSSPropertyColor, textColor, important);
}
    
String windowRoundedCornerRadiusCSS()
{
    MACaptionAppearanceBehavior behavior;
    CGFloat radius = MACaptionAppearanceGetWindowRoundedCornerRadius(kMACaptionAppearanceDomainUser, &behavior);
    if (!radius)
        return emptyString();

    StringBuilder builder;
    builder.append(getPropertyNameString(CSSPropertyBorderRadius));
    builder.append(String::format(":%.02fpx", radius));
    if (behavior == kMACaptionAppearanceBehaviorUseValue)
        builder.append(" !important");
    builder.append(';');

    return builder.toString();
}
    
Color captionsEdgeColorForTextColor(const Color& textColor)
{
    int distanceFromWhite = differenceSquared(textColor, Color::white);
    int distanceFromBlack = differenceSquared(textColor, Color::black);
    
    if (distanceFromWhite < distanceFromBlack)
        return textColor.dark();
    
    return textColor.light();
}

String cssPropertyWithTextEdgeColor(CSSPropertyID id, const String& value, const Color& textColor, bool important)
{
    StringBuilder builder;
    
    builder.append(getPropertyNameString(id));
    builder.append(':');
    builder.append(value);
    builder.append(' ');
    builder.append(captionsEdgeColorForTextColor(textColor).serialized());
    if (important)
        builder.append(" !important");
    builder.append(';');
    
    return builder.toString();
}

String colorPropertyCSS(CSSPropertyID id, const Color& color, bool important)
{
    StringBuilder builder;
    
    builder.append(getPropertyNameString(id));
    builder.append(':');
    builder.append(color.serialized());
    if (important)
        builder.append(" !important");
    builder.append(';');
    
    return builder.toString();
}

String captionsTextEdgeCSS()
{
    DEFINE_STATIC_LOCAL(const String, edgeStyleRaised, (" -.05em -.05em 0 ", String::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const String, edgeStyleDepressed, (" .05em .05em 0 ", String::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const String, edgeStyleDropShadow, (" .075em .075em 0 ", String::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const String, edgeStyleUniform, (" .03em ", String::ConstructFromLiteral));

    bool unused;
    Color color = captionsTextColor(unused);
    if (!color.isValid())
        color.setNamedColor("black");
    color = captionsEdgeColorForTextColor(color);

    MACaptionAppearanceBehavior behavior;
    MACaptionAppearanceTextEdgeStyle textEdgeStyle = MACaptionAppearanceGetTextEdgeStyle(kMACaptionAppearanceDomainUser, &behavior);
    switch (textEdgeStyle) {
    case kMACaptionAppearanceTextEdgeStyleUndefined:
    case kMACaptionAppearanceTextEdgeStyleNone:
        return emptyString();
            
    case kMACaptionAppearanceTextEdgeStyleRaised:
        return cssPropertyWithTextEdgeColor(CSSPropertyTextShadow, edgeStyleRaised, color, behavior == kMACaptionAppearanceBehaviorUseValue);
    case kMACaptionAppearanceTextEdgeStyleDepressed:
        return cssPropertyWithTextEdgeColor(CSSPropertyTextShadow, edgeStyleDepressed, color, behavior == kMACaptionAppearanceBehaviorUseValue);
    case kMACaptionAppearanceTextEdgeStyleDropShadow:
        return cssPropertyWithTextEdgeColor(CSSPropertyTextShadow, edgeStyleDropShadow, color, behavior == kMACaptionAppearanceBehaviorUseValue);
    case kMACaptionAppearanceTextEdgeStyleUniform:
        return cssPropertyWithTextEdgeColor(CSSPropertyWebkitTextStroke, edgeStyleUniform, color, behavior == kMACaptionAppearanceBehaviorUseValue);
            
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    
    return emptyString();
}

String captionsDefaultFontCSS()
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
    builder.append(": \"");
    builder.append(static_cast<CFStringRef>(name.get()));
    builder.append('"');
    if (behavior == kMACaptionAppearanceBehaviorUseValue)
        builder.append(" !important");
    builder.append(';');
    
    return builder.toString();
}

#endif

} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK)
