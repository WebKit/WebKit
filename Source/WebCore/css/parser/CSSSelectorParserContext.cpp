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
#include "Document.h"
#include <wtf/Hasher.h>

namespace WebCore {

CSSSelectorParserContext::CSSSelectorParserContext(const CSSParserContext& context)
    : mode(context.mode)
    , cssNestingEnabled(context.cssNestingEnabled)
    , customStateSetEnabled(context.customStateSetEnabled)
    , grammarAndSpellingPseudoElementsEnabled(context.grammarAndSpellingPseudoElementsEnabled)
    , highlightAPIEnabled(context.highlightAPIEnabled)
#if ENABLE(SERVICE_CONTROLS)
    , imageControlsEnabled(context.imageControlsEnabled)
#endif
    , popoverAttributeEnabled(context.popoverAttributeEnabled)
    , targetTextPseudoElementEnabled(context.targetTextPseudoElementEnabled)
    , thumbAndTrackPseudoElementsEnabled(context.thumbAndTrackPseudoElementsEnabled)
    , viewTransitionsEnabled(context.propertySettings.viewTransitionsEnabled)
    , viewTransitionClassesEnabled(viewTransitionsEnabled && context.propertySettings.viewTransitionClassesEnabled)
    , viewTransitionTypesEnabled(viewTransitionsEnabled && context.viewTransitionTypesEnabled)
{
}

CSSSelectorParserContext::CSSSelectorParserContext(const Document& document)
    : mode(document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode)
    , cssNestingEnabled(document.settings().cssNestingEnabled())
    , customStateSetEnabled(document.settings().customStateSetEnabled())
    , grammarAndSpellingPseudoElementsEnabled(document.settings().grammarAndSpellingPseudoElementsEnabled())
    , highlightAPIEnabled(document.settings().highlightAPIEnabled())
#if ENABLE(SERVICE_CONTROLS)
    , imageControlsEnabled(document.settings().imageControlsEnabled())
#endif
    , popoverAttributeEnabled(document.settings().popoverAttributeEnabled())
    , targetTextPseudoElementEnabled(document.settings().targetTextPseudoElementEnabled())
    , thumbAndTrackPseudoElementsEnabled(document.settings().thumbAndTrackPseudoElementsEnabled())
    , viewTransitionsEnabled(document.settings().viewTransitionsEnabled())
    , viewTransitionClassesEnabled(viewTransitionsEnabled && document.settings().viewTransitionClassesEnabled())
    , viewTransitionTypesEnabled(viewTransitionsEnabled && document.settings().viewTransitionTypesEnabled())
{
}

void add(Hasher& hasher, const CSSSelectorParserContext& context)
{
    add(hasher,
        context.mode,
        context.cssNestingEnabled,
        context.customStateSetEnabled,
        context.grammarAndSpellingPseudoElementsEnabled,
        context.highlightAPIEnabled,
#if ENABLE(SERVICE_CONTROLS)
        context.imageControlsEnabled,
#endif
        context.popoverAttributeEnabled,
        context.targetTextPseudoElementEnabled,
        context.thumbAndTrackPseudoElementsEnabled,
        context.viewTransitionsEnabled,
        context.viewTransitionClassesEnabled,
        context.viewTransitionTypesEnabled
    );
}

} // namespace WebCore
