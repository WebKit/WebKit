/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#import "config.h"

#if ENABLE(VIDEO_TRACK)

#import "CaptionUserPreferencesMac.h"

#import "ColorMac.h"
#import "DOMWrapperWorld.h"
#import "FloatConversion.h"
#import "KURL.h"
#import "PageGroup.h"
#import "TextTrackCue.h"
#import "UserStyleSheetTypes.h"
#import "WebCoreSystemInterface.h"
#import <wtf/RetainPtr.h>
#import <wtf/text/StringBuilder.h>

using namespace std;

namespace WebCore {

static void userCaptionPreferencesChangedNotificationCallback(CFNotificationCenterRef, void* observer, CFStringRef, const void *, CFDictionaryRef)
{
    static_cast<CaptionUserPreferencesMac*>(observer)->captionPreferencesChanged();
}


CaptionUserPreferencesMac::CaptionUserPreferencesMac(PageGroup* group)
    : CaptionUserPreferences(group)
    , m_listeningForPreferenceChanges(false)
{
}

CaptionUserPreferencesMac::~CaptionUserPreferencesMac()
{
    if (wkCaptionAppearanceGetSettingsChangedNotification())
        CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), this, wkCaptionAppearanceGetSettingsChangedNotification(), NULL);
}

bool CaptionUserPreferencesMac::userHasCaptionPreferences() const
{
    return wkCaptionAppearanceHasUserPreferences();
}

bool CaptionUserPreferencesMac::userPrefersCaptions() const
{
    return wkCaptionAppearanceShowCaptionsWhenAvailable();
}

void CaptionUserPreferencesMac::registerForCaptionPreferencesChangedCallbacks(CaptionPreferencesChangedListener* listener)
{
    ASSERT(!m_captionPreferenceChangeListeners.contains(listener));

    if (!wkCaptionAppearanceGetSettingsChangedNotification())
        return;
    
    if (!m_listeningForPreferenceChanges) {
        m_listeningForPreferenceChanges = true;
        CFNotificationCenterAddObserver (CFNotificationCenterGetLocalCenter(), this, userCaptionPreferencesChangedNotificationCallback, wkCaptionAppearanceGetSettingsChangedNotification(), NULL, CFNotificationSuspensionBehaviorCoalesce);
        updateCaptionStyleSheetOveride();
    }
    
    m_captionPreferenceChangeListeners.add(listener);
}

void CaptionUserPreferencesMac::unregisterForCaptionPreferencesChangedCallbacks(CaptionPreferencesChangedListener* listener)
{
    if (wkCaptionAppearanceGetSettingsChangedNotification())
        m_captionPreferenceChangeListeners.remove(listener);
}

Color CaptionUserPreferencesMac::captionsWindowColor() const
{
    RetainPtr<CGColorRef> color(AdoptCF, wkCaptionAppearanceCopyWindowColor());
    Color windowColor(color.get());
    if (!windowColor.isValid())
        windowColor = Color::transparent;
    
    CGFloat opacity;
    if (wkCaptionAppearanceGetWindowOpacity(&opacity))
        return Color(windowColor.red(), windowColor.green(), windowColor.blue(), static_cast<int>(opacity * 255));
    
    if (!color)
        return Color();
    
    return windowColor;
}

Color CaptionUserPreferencesMac::captionsBackgroundColor() const
{
    // This default value must be the same as the one specified in mediaControls.css for -webkit-media-text-track-past-nodes
    // and webkit-media-text-track-future-nodes.
    DEFINE_STATIC_LOCAL(Color, defaultBackgroundColor, (Color(0, 0, 0, 0.8 * 255)));
    
    RetainPtr<CGColorRef> color(AdoptCF, wkCaptionAppearanceCopyBackgroundColor());
    Color backgroundColor(color.get());
    if (!backgroundColor.isValid()) {
        backgroundColor = defaultBackgroundColor;
    }
    
    CGFloat opacity;
    if (wkCaptionAppearanceGetBackgroundOpacity(&opacity))
        return Color(backgroundColor.red(), backgroundColor.green(), backgroundColor.blue(), static_cast<int>(opacity * 255));
    
    if (!color)
        return Color();
    
    return backgroundColor;
}

Color CaptionUserPreferencesMac::captionsTextColor() const
{
    RetainPtr<CGColorRef> color(AdoptCF, wkCaptionAppearanceCopyForegroundColor());
    Color textColor(color.get());
    if (!textColor.isValid()) {
        // This default value must be the same as the one specified in mediaControls.css for -webkit-media-text-track-container.
        textColor = Color::white;
    }
    
    CGFloat opacity;
    if (wkCaptionAppearanceGetForegroundOpacity(&opacity))
        return Color(textColor.red(), textColor.green(), textColor.blue(), static_cast<int>(opacity * 255));
    
    if (!color)
        return Color();
    
    return textColor;
}

Color CaptionUserPreferencesMac::captionsEdgeColorForTextColor(const Color& textColor) const
{
    int distanceFromWhite = differenceSquared(textColor, Color::white);
    int distanceFromBlack = differenceSquared(textColor, Color::black);
    
    if (distanceFromWhite < distanceFromBlack)
        return textColor.dark();
    
    return textColor.light();
}

String CaptionUserPreferencesMac::cssPropertyWithTextEdgeColor(CSSPropertyID id, const String& value, const Color& textColor) const
{
    StringBuilder builder;
    
    builder.append(getPropertyNameString(id));
    builder.append(':');
    builder.append(value);
    builder.append(' ');
    builder.append(captionsEdgeColorForTextColor(textColor).serialized());
    builder.append(';');
    
    return builder.toString();
}

String CaptionUserPreferencesMac::cssColorProperty(CSSPropertyID id, const Color& color) const
{
    StringBuilder builder;
    
    builder.append(getPropertyNameString(id));
    builder.append(':');
    builder.append(color.serialized());
    builder.append(';');
    
    return builder.toString();
}

String CaptionUserPreferencesMac::captionsTextEdgeStyle() const
{
    DEFINE_STATIC_LOCAL(const String, edgeStyleRaised, (" -.05em -.05em 0 ", String::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const String, edgeStyleDepressed, (" .05em .05em 0 ", String::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const String, edgeStyleDropShadow, (" .075em .075em 0 ", String::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const String, edgeStyleUniform, (" .03em ", String::ConstructFromLiteral));
    
    Color color = captionsTextColor();
    if (!color.isValid())
        color.setNamedColor("black");
    color = captionsEdgeColorForTextColor(color);
    
    wkCaptionTextEdgeStyle textEdgeStyle = static_cast<wkCaptionTextEdgeStyle>(wkCaptionAppearanceGetTextEdgeStyle());
    switch (textEdgeStyle) {
        case wkCaptionTextEdgeStyleUndefined:
        case wkCaptionTextEdgeStyleNone:
            return emptyString();
            
        case wkCaptionTextEdgeStyleRaised:
            return cssPropertyWithTextEdgeColor(CSSPropertyTextShadow, edgeStyleRaised, color);
        case wkCaptionTextEdgeStyleDepressed:
            return cssPropertyWithTextEdgeColor(CSSPropertyTextShadow, edgeStyleDepressed, color);
        case wkCaptionTextEdgeStyleDropShadow:
            return cssPropertyWithTextEdgeColor(CSSPropertyTextShadow, edgeStyleDropShadow, color);
        case wkCaptionTextEdgeStyleUniform:
            return cssPropertyWithTextEdgeColor(CSSPropertyWebkitTextStroke, edgeStyleUniform, color);
            
        case wkCaptionTextEdgeStyleMax:
            ASSERT_NOT_REACHED();
        default:
            ASSERT_NOT_REACHED();
            break;
    }
    
    return emptyString();
}

String CaptionUserPreferencesMac::captionsDefaultFont() const
{
    RetainPtr<CGFontRef> font(AdoptCF, wkCaptionAppearanceCopyFontForStyle(wkCaptionFontStyleDefault));
    if (!font)
        return emptyString();
    
    RetainPtr<CFStringRef> name(AdoptCF, CGFontCopyPostScriptName(font.get()));
    if (!name)
        return emptyString();
    
    StringBuilder builder;
    
    builder.append(getPropertyNameString(CSSPropertyFontFamily));
    builder.append(": \"");
    builder.append(name.get());
    builder.append("\";");
    
    return builder.toString();
}

String CaptionUserPreferencesMac::captionsStyleSheetOverride() const
{
    StringBuilder captionsOverrideStyleSheet;
    
    Color color = captionsBackgroundColor();
    if (color.isValid()) {
        captionsOverrideStyleSheet.append(" video::");
        captionsOverrideStyleSheet.append(TextTrackCue::allNodesShadowPseudoId());
        captionsOverrideStyleSheet.append('{');
        captionsOverrideStyleSheet.append(cssColorProperty(CSSPropertyBackgroundColor, color));
        captionsOverrideStyleSheet.append('}');
    }
    
    color = captionsWindowColor();
    if (color.isValid()) {
        captionsOverrideStyleSheet.append(" video::");
        captionsOverrideStyleSheet.append(TextTrackCueBox::textTrackCueBoxShadowPseudoId());
        captionsOverrideStyleSheet.append('{');
        captionsOverrideStyleSheet.append(cssColorProperty(CSSPropertyBackgroundColor, color));
        captionsOverrideStyleSheet.append('}');
    }
    
    color = captionsTextColor();
    String edgeStyle = captionsTextEdgeStyle();
    String fontName = captionsDefaultFont();
    if (color.isValid() || !edgeStyle.isEmpty() || !fontName.isEmpty()) {
        captionsOverrideStyleSheet.append(" video::");
        captionsOverrideStyleSheet.append(TextTrackCueBox::textTrackCueBoxShadowPseudoId());
        captionsOverrideStyleSheet.append('{');
        
        if (color.isValid())
            captionsOverrideStyleSheet.append(cssColorProperty(CSSPropertyColor, color));
        if (!edgeStyle.isEmpty())
            captionsOverrideStyleSheet.append(edgeStyle);
        if (!fontName.isEmpty())
            captionsOverrideStyleSheet.append(fontName);
        
        captionsOverrideStyleSheet.append('}');
    }
    
    return captionsOverrideStyleSheet.toString();
}

float CaptionUserPreferencesMac::captionFontSizeScale() const
{
    CGFloat characterScale = CaptionUserPreferences::captionFontSizeScale();
    CGFloat scaleAdjustment;
    
    if (!wkCaptionAppearanceGetRelativeCharacterSize(&scaleAdjustment))
        return characterScale;
    
#if defined(__LP64__) && __LP64__
    return narrowPrecisionToFloat(scaleAdjustment * characterScale);
#else
    return scaleAdjustment * characterScale;
#endif
}

void CaptionUserPreferencesMac::captionPreferencesChanged()
{
    if (m_captionPreferenceChangeListeners.isEmpty())
        return;

    updateCaptionStyleSheetOveride();

    for (HashSet<CaptionPreferencesChangedListener*>::iterator i = m_captionPreferenceChangeListeners.begin(); i != m_captionPreferenceChangeListeners.end(); ++i)
        (*i)->captionPreferencesChanged();
}

void CaptionUserPreferencesMac::updateCaptionStyleSheetOveride()
{
    // Identify our override style sheet with a unique URL - a new scheme and a UUID.
    DEFINE_STATIC_LOCAL(KURL, captionsStyleSheetURL, (ParsedURLString, "user-captions-override:01F6AF12-C3B0-4F70-AF5E-A3E00234DC23"));
    
    pageGroup()->removeUserStyleSheetFromWorld(mainThreadNormalWorld(), captionsStyleSheetURL);
    
    if (!userHasCaptionPreferences())
        return;
    
    String captionsOverrideStyleSheet = captionsStyleSheetOverride();
    if (captionsOverrideStyleSheet.isEmpty())
        return;
    
    pageGroup()->addUserStyleSheetToWorld(mainThreadNormalWorld(), captionsOverrideStyleSheet, captionsStyleSheetURL, Vector<String>(),
             Vector<String>(), InjectInAllFrames, UserStyleAuthorLevel, InjectInExistingDocuments);
}

}

#endif // ENABLE(VIDEO_TRACK)
