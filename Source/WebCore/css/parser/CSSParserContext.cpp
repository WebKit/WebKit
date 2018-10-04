/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "CSSParserContext.h"

#include "Document.h"
#include "Page.h"
#include "Settings.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

const CSSParserContext& strictCSSParserContext()
{
    static NeverDestroyed<CSSParserContext> strictContext(HTMLStandardMode);
    return strictContext;
}

CSSParserContext::CSSParserContext(CSSParserMode mode, const URL& baseURL)
    : baseURL(baseURL)
    , mode(mode)
{
#if PLATFORM(IOS)
    // FIXME: Force the site specific quirk below to work on iOS. Investigating other site specific quirks
    // to see if we can enable the preference all together is to be handled by:
    // <rdar://problem/8493309> Investigate Enabling Site Specific Quirks in MobileSafari and UIWebView
    needsSiteSpecificQuirks = true;
#endif
}

CSSParserContext::CSSParserContext(const Document& document, const URL& sheetBaseURL, const String& charset)
    : baseURL(sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL)
    , charset(charset)
    , mode(document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode)
    , isHTMLDocument(document.isHTMLDocument())
    , hasDocumentSecurityOrigin(sheetBaseURL.isNull() || document.securityOrigin().canRequest(baseURL))
{

    needsSiteSpecificQuirks = document.settings().needsSiteSpecificQuirks();
    enforcesCSSMIMETypeInNoQuirksMode = document.settings().enforceCSSMIMETypeInNoQuirksMode();
    useLegacyBackgroundSizeShorthandBehavior = document.settings().useLegacyBackgroundSizeShorthandBehavior();
#if ENABLE(TEXT_AUTOSIZING)
    textAutosizingEnabled = document.settings().textAutosizingEnabled();
#endif
    springTimingFunctionEnabled = document.settings().springTimingFunctionEnabled();
    constantPropertiesEnabled = document.settings().constantPropertiesEnabled();
    conicGradientsEnabled = document.settings().conicGradientsEnabled();
    colorFilterEnabled = document.settings().colorFilterEnabled();
    deferredCSSParserEnabled = document.settings().deferredCSSParserEnabled();
    useSystemAppearance = document.page() ? document.page()->useSystemAppearance() : false;

#if PLATFORM(IOS)
    // FIXME: Force the site specific quirk below to work on iOS. Investigating other site specific quirks
    // to see if we can enable the preference all together is to be handled by:
    // <rdar://problem/8493309> Investigate Enabling Site Specific Quirks in MobileSafari and UIWebView
    needsSiteSpecificQuirks = true;
#endif
}

bool operator==(const CSSParserContext& a, const CSSParserContext& b)
{
    return a.baseURL == b.baseURL
        && a.charset == b.charset
        && a.mode == b.mode
        && a.isHTMLDocument == b.isHTMLDocument
#if ENABLE(TEXT_AUTOSIZING)
        && a.textAutosizingEnabled == b.textAutosizingEnabled
#endif
        && a.needsSiteSpecificQuirks == b.needsSiteSpecificQuirks
        && a.enforcesCSSMIMETypeInNoQuirksMode == b.enforcesCSSMIMETypeInNoQuirksMode
        && a.useLegacyBackgroundSizeShorthandBehavior == b.useLegacyBackgroundSizeShorthandBehavior
        && a.springTimingFunctionEnabled == b.springTimingFunctionEnabled
        && a.constantPropertiesEnabled == b.constantPropertiesEnabled
        && a.conicGradientsEnabled == b.conicGradientsEnabled
        && a.colorFilterEnabled == b.colorFilterEnabled
        && a.deferredCSSParserEnabled == b.deferredCSSParserEnabled
        && a.hasDocumentSecurityOrigin == b.hasDocumentSecurityOrigin
        && a.useSystemAppearance == b.useSystemAppearance;
}

}
