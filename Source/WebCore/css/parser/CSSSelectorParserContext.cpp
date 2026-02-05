/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSSelectorParserContext.h"

#include "CSSParserContext.h"
#include "DocumentQuirks.h"
#include "Settings.h"
#include <wtf/Hasher.h>

namespace WebCore {

CSSSelectorParserContext::CSSSelectorParserContext(const CSSParserContext& context)
    : mode(context.mode)
#if ENABLE(SERVICE_CONTROLS)
    , imageControlsEnabled(context.imageControlsEnabled)
#endif
    , popoverAttributeEnabled(context.popoverAttributeEnabled)
    , htmlEnhancedSelectPseudoElementsEnabled(context.htmlEnhancedSelectPseudoElementsEnabled)
    , targetTextPseudoElementEnabled(context.targetTextPseudoElementEnabled)
    , cssAppearanceBaseEnabled(context.cssAppearanceBaseEnabled)
    , viewTransitionsEnabled(context.propertySettings.viewTransitionsEnabled)
    , webkitMediaTextTrackDisplayQuirkEnabled(context.webkitMediaTextTrackDisplayQuirkEnabled)
    , openPseudoClassEnabled(context.openPseudoClassEnabled)
{
}

CSSSelectorParserContext::CSSSelectorParserContext(const Document& document)
    : mode(document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode)
#if ENABLE(SERVICE_CONTROLS)
    , imageControlsEnabled(document.settings().imageControlsEnabled())
#endif
    , popoverAttributeEnabled(document.settings().popoverAttributeEnabled())
    , htmlEnhancedSelectPseudoElementsEnabled(document.settings().htmlEnhancedSelectPseudoElementsEnabled())
    , targetTextPseudoElementEnabled(document.settings().targetTextPseudoElementEnabled())
    , cssAppearanceBaseEnabled(document.settings().cssAppearanceBaseEnabled())
    , viewTransitionsEnabled(document.settings().viewTransitionsEnabled())
    , webkitMediaTextTrackDisplayQuirkEnabled(document.quirks().needsWebKitMediaTextTrackDisplayQuirk())
    , openPseudoClassEnabled(document.settings().openPseudoClassEnabled())
{
}

void add(Hasher& hasher, const CSSSelectorParserContext& context)
{
    auto bits = WTF::packBools(
#if ENABLE(SERVICE_CONTROLS)
        context.imageControlsEnabled,
#endif
        context.popoverAttributeEnabled,
        context.htmlEnhancedSelectPseudoElementsEnabled,
        context.targetTextPseudoElementEnabled,
        context.cssAppearanceBaseEnabled,
        context.viewTransitionsEnabled,
        context.webkitMediaTextTrackDisplayQuirkEnabled,
        context.openPseudoClassEnabled
    );
    add(hasher, context.mode, bits);
}

} // namespace WebCore
