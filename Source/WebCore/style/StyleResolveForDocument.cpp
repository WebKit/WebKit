/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "StyleResolveForDocument.h"

#include "CSSFontSelector.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "HTMLIFrameElement.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocaleToScriptMapping.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+SettersInlines.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleAdjuster.h"
#include "StyleFontSizeFunctions.h"
#include "StyleResolver.h"

namespace WebCore {

namespace Style {

RenderStyle resolveForDocument(const Document& document)
{
    ASSERT(document.hasLivingRenderTree());

    RenderView& renderView = *document.renderView();

    auto documentStyle = RenderStyle::create();

    documentStyle.setDisplay(DisplayType::Block);
    documentStyle.setRTLOrdering(document.visuallyOrdered() ? WebCore::Order::Visual : WebCore::Order::Logical);
    documentStyle.setZoom(!document.printing() ? renderView.frame().pageZoomFactor() : 1);
    documentStyle.setPageScaleTransform(renderView.frame().frameScaleFactor());

    // This overrides any -webkit-user-modify inherited from the parent iframe.
    documentStyle.setUserModify(document.inDesignMode() ? UserModify::ReadWrite : UserModify::ReadOnly);
#if PLATFORM(IOS_FAMILY)
    if (document.inDesignMode())
        documentStyle.setTextSizeAdjust(CSS::Keyword::None { });
#endif

    Adjuster::adjustEventListenerRegionTypesForRootStyle(documentStyle, document);
    
    auto& pagination = renderView.frameView().pagination();
    if (pagination.mode != Pagination::Mode::Unpaginated) {
        documentStyle.setColumnStylesFromPaginationMode(pagination.mode);
        documentStyle.setColumnGap(GapGutter::Fixed { static_cast<float>(pagination.gap) });
        if (renderView.multiColumnFlow())
            renderView.updateColumnProgressionFromStyle(documentStyle);
    }

    auto fontDescription = [&]() {
        auto& settings = renderView.frame().settings();

        FontCascadeDescription fontDescription;
        fontDescription.setSpecifiedLocale(document.contentLanguage());
        fontDescription.setOneFamily(standardFamily);
        fontDescription.setShouldAllowUserInstalledFonts(settings.shouldAllowUserInstalledFonts() ? AllowUserInstalledFonts::Yes : AllowUserInstalledFonts::No);
        // FIXME: We need evaluationTimeZoomEnabled to be accessible from FontDescription, not only from RenderStyle. Would it be weird to move it to FontDescription (which is already accessible from RenderStyle)?
        fontDescription.setEvaluationTimeZoomEnabled(document.settings().evaluationTimeZoomEnabled());

        fontDescription.setKeywordSizeFromIdentifier(CSSValueMedium);
        int size = fontSizeForKeyword(CSSValueMedium, false, document);
        fontDescription.setSpecifiedSize(size);
        bool useSVGZoomRules = document.isSVGDocument();
        auto computedFontSize = computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), useSVGZoomRules, documentStyle.computedStyle(), document);
        fontDescription.setComputedSize(computedFontSize.size, computedFontSize.usedZoomFactor);

        auto [fontOrientation, glyphOrientation] = documentStyle.fontAndGlyphOrientation();
        fontDescription.setOrientation(fontOrientation);
        fontDescription.setNonCJKGlyphOrientation(glyphOrientation);
        return fontDescription;
    }();

    auto fontCascade = FontCascade { WTF::move(fontDescription), documentStyle.fontCascade() };

    // We don't just call setFontDescription() because we need to provide the fontSelector to the FontCascade.
    RefPtr fontSelector = document.fontSelector();
    fontCascade.update(WTF::move(fontSelector));
    documentStyle.setFontCascade(WTF::move(fontCascade));

    documentStyle.setEvaluationTimeZoomEnabled(document.settings().evaluationTimeZoomEnabled());

    documentStyle.setDeviceScaleFactor(document.deviceScaleFactor());

    return documentStyle;
}

}
}
