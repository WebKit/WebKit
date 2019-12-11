/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
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

#import "DOMAbstractViewInternal.h"
#import "DOMCSSRuleInternal.h"
#import "DOMCSSRuleListInternal.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMCSSValueInternal.h"
#import "DOMCounterInternal.h"
#import "DOMEventInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMImplementationInternal.h"
#import "DOMInternal.h"
#import "DOMMediaListInternal.h"
#import "DOMNamedNodeMapInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeIteratorInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMRGBColorInternal.h"
#import "DOMRangeInternal.h"
#import "DOMRectInternal.h"
#import "DOMStyleSheetInternal.h"
#import "DOMStyleSheetListInternal.h"
#import "DOMTreeWalkerInternal.h"
#import "DOMXPathExpressionInternal.h"
#import "DOMXPathResultInternal.h"
#import <WebCore/JSCSSRule.h>
#import <WebCore/JSCSSRuleList.h>
#import <WebCore/JSCSSStyleDeclaration.h>
#import <WebCore/JSDOMImplementation.h>
#import <WebCore/JSDeprecatedCSSOMCounter.h>
#import <WebCore/JSDeprecatedCSSOMRGBColor.h>
#import <WebCore/JSDeprecatedCSSOMRect.h>
#import <WebCore/JSDeprecatedCSSOMValue.h>
#import <WebCore/JSEvent.h>
#import <WebCore/JSHTMLCollection.h>
#import <WebCore/JSHTMLOptionsCollection.h>
#import <WebCore/JSMediaList.h>
#import <WebCore/JSNamedNodeMap.h>
#import <WebCore/JSNode.h>
#import <WebCore/JSNodeIterator.h>
#import <WebCore/JSNodeList.h>
#import <WebCore/JSRange.h>
#import <WebCore/JSStyleSheet.h>
#import <WebCore/JSStyleSheetList.h>
#import <WebCore/JSTreeWalker.h>
#import <WebCore/JSWindowProxy.h>
#import <WebCore/JSXPathExpression.h>
#import <WebCore/JSXPathResult.h>
#import <WebCore/WebScriptObjectPrivate.h>

static WebScriptObject *createDOMWrapper(JSC::JSObject& jsWrapper)
{
    JSC::VM& vm = jsWrapper.vm();
    #define WRAP(className) \
        if (auto* wrapped = WebCore::JS##className::toWrapped(vm, &jsWrapper)) \
            return kit(wrapped);

    WRAP(CSSRule)
    WRAP(CSSRuleList)
    WRAP(CSSStyleDeclaration)
    WRAP(DeprecatedCSSOMValue)
    WRAP(DeprecatedCSSOMCounter)
    WRAP(DOMImplementation)
    WRAP(Event)
    WRAP(HTMLOptionsCollection)
    WRAP(MediaList)
    WRAP(NamedNodeMap)
    WRAP(Node)
    WRAP(NodeIterator)
    WRAP(NodeList)
    WRAP(DeprecatedCSSOMRGBColor)
    WRAP(Range)
    WRAP(DeprecatedCSSOMRect)
    WRAP(StyleSheet)
    WRAP(StyleSheetList)
    WRAP(TreeWalker)
    WRAP(WindowProxy)
    WRAP(XPathExpression)
    WRAP(XPathResult)

    // This must be after HTMLOptionsCollection, because JSHTMLCollection is a base class of
    // JSHTMLOptionsCollection, and HTMLCollection is *not* a base class of HTMLOptionsCollection.
    WRAP(HTMLCollection)

    #undef WRAP

    return nil;
}

static void disconnectWindowWrapper(WebScriptObject *windowWrapper)
{
    ASSERT([windowWrapper isKindOfClass:[DOMAbstractView class]]);
    [(DOMAbstractView *)windowWrapper _disconnectFrame];
}

void initializeDOMWrapperHooks()
{
    WebCore::initializeDOMWrapperHooks(createDOMWrapper, disconnectWindowWrapper);
}
