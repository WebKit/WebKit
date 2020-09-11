/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006-2011, 2016 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "config.h"
#import "FontCascade.h"

#import "GraphicsContext.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/ios/CoreUISPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(CoreUI)
SOFT_LINK_CLASS(CoreUI, CUICatalog)
SOFT_LINK_CLASS(CoreUI, CUIStyleEffectConfiguration)
#endif

namespace WebCore {

bool FontCascade::canReturnFallbackFontsForComplexText()
{
    return true;
}

bool FontCascade::canExpandAroundIdeographsInComplexText()
{
    return true;
}

void showLetterpressedGlyphsWithAdvances(const FloatPoint& point, const Font& font, GraphicsContext& coreContext, const CGGlyph* glyphs, const CGSize* advances, unsigned count)
{
#if ENABLE(LETTERPRESS)
    if (!count)
        return;

    const FontPlatformData& platformData = font.platformData();
    if (platformData.orientation() == FontOrientation::Vertical) {
        // FIXME: Implement support for vertical text. See <rdar://problem/13737298>.
        return;
    }

    CGContextRef context = coreContext.platformContext();

    CGContextSetTextPosition(context, point.x(), point.y());
    Vector<CGPoint, 256> positions(count);
    fillVectorWithHorizontalGlyphPositions(positions, context, advances, count);

    CTFontRef ctFont = platformData.ctFont();
    CGContextSetFontSize(context, CTFontGetSize(ctFont));

    static CUICatalog *catalog = PAL::softLink_UIKit__UIKitGetTextEffectsCatalog();
    if (!catalog)
        return;

    static CUIStyleEffectConfiguration *styleConfiguration;
    if (!styleConfiguration) {
        styleConfiguration = [allocCUIStyleEffectConfigurationInstance() init];
        styleConfiguration.useSimplifiedEffect = YES;
    }

#if HAVE(OS_DARK_MODE_SUPPORT)
    styleConfiguration.appearanceName = coreContext.useDarkAppearance() ? @"UIAppearanceDark" : @"UIAppearanceLight";
#endif

    CGContextSetFont(context, adoptCF(CTFontCopyGraphicsFont(ctFont, nullptr)).get());
    CGContextSetFontSize(context, platformData.size());

    [catalog drawGlyphs:glyphs atPositions:positions.data() inContext:context withFont:ctFont count:count stylePresetName:@"_UIKitNewLetterpressStyle" styleConfiguration:styleConfiguration foregroundColor:CGContextGetFillColorAsColor(context)];

    CGContextSetFont(context, nullptr);
    CGContextSetFontSize(context, 0);
#else
    UNUSED_PARAM(point);
    UNUSED_PARAM(font);
    UNUSED_PARAM(coreContext);
    UNUSED_PARAM(glyphs);
    UNUSED_PARAM(advances);
    UNUSED_PARAM(count);
#endif
}

}
