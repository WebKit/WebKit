/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#import "DOMDocumentInternal.h"

#import <WebCore/Attr.h>
#import <WebCore/CDATASection.h>
#import <WebCore/CSSRuleList.h>
#import <WebCore/CSSStyleDeclaration.h>
#import <WebCore/Comment.h>
#import "DOMAbstractViewInternal.h"
#import "DOMAttrInternal.h"
#import "DOMCDATASectionInternal.h"
#import "DOMCSSRuleListInternal.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMCommentInternal.h"
#import "DOMCustomXPathNSResolver.h"
#import "DOMDocumentFragmentInternal.h"
#import "DOMDocumentTypeInternal.h"
#import "DOMElementInternal.h"
#import "DOMEventInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMHTMLHeadElementInternal.h"
#import "DOMHTMLScriptElementInternal.h"
#import "DOMImplementationInternal.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeIteratorInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMProcessingInstructionInternal.h"
#import "DOMRangeInternal.h"
#import "DOMStyleSheetListInternal.h"
#import "DOMTextInternal.h"
#import "DOMTreeWalkerInternal.h"
#import <WebCore/DOMWindow.h>
#import "DOMXPathExpressionInternal.h"
#import "DOMXPathResultInternal.h"
#import <WebCore/Document.h>
#import <WebCore/DocumentFragment.h>
#import <WebCore/DocumentType.h>
#import <WebCore/Element.h>
#import <WebCore/Event.h>
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLHeadElement.h>
#import <WebCore/HTMLScriptElement.h>
#import <WebCore/JSExecState.h>
#import <WebCore/NameNodeList.h>
#import <WebCore/NativeNodeFilter.h>
#import <WebCore/Node.h>
#import <WebCore/NodeIterator.h>
#import <WebCore/NodeList.h>
#import "ObjCNodeFilterCondition.h"
#import <WebCore/ProcessingInstruction.h>
#import <WebCore/Range.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/StyleSheetList.h>
#import <WebCore/Text.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/TreeWalker.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <WebCore/XPathExpression.h>
#import <WebCore/XPathNSResolver.h>
#import <WebCore/XPathResult.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL static_cast<WebCore::Document*>(reinterpret_cast<WebCore::Node*>(_internal))

@implementation DOMDocument

- (DOMDocumentType *)doctype
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->doctype()));
}

- (DOMImplementation *)implementation
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->implementation()));
}

- (DOMElement *)documentElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->documentElement()));
}

- (NSString *)inputEncoding
{
    WebCore::JSMainThreadNullState state;
    return IMPL->characterSetWithUTF8Fallback();
}

- (NSString *)xmlEncoding
{
    WebCore::JSMainThreadNullState state;
    return IMPL->xmlEncoding();
}

- (NSString *)xmlVersion
{
    WebCore::JSMainThreadNullState state;
    return IMPL->xmlVersion();
}

- (void)setXmlVersion:(NSString *)newXmlVersion
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setXMLVersion(newXmlVersion));
}

- (BOOL)xmlStandalone
{
    WebCore::JSMainThreadNullState state;
    return IMPL->xmlStandalone();
}

- (void)setXmlStandalone:(BOOL)newXmlStandalone
{
    WebCore::JSMainThreadNullState state;
    IMPL->setXMLStandalone(newXmlStandalone);
}

- (NSString *)documentURI
{
    WebCore::JSMainThreadNullState state;
    return IMPL->documentURI();
}

- (void)setDocumentURI:(NSString *)newDocumentURI
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDocumentURI(newDocumentURI);
}

- (DOMAbstractView *)defaultView
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->windowProxy()));
}

- (DOMStyleSheetList *)styleSheets
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->styleSheets()));
}

- (NSString *)contentType
{
    WebCore::JSMainThreadNullState state;
    return IMPL->contentType();
}

- (NSString *)title
{
    WebCore::JSMainThreadNullState state;
    return IMPL->title();
}

- (void)setTitle:(NSString *)newTitle
{
    WebCore::JSMainThreadNullState state;
    IMPL->setTitle(newTitle);
}

- (NSString *)dir
{
    WebCore::JSMainThreadNullState state;
    return IMPL->dir();
}

- (void)setDir:(NSString *)newDir
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDir(newDir);
}

- (NSString *)referrer
{
    WebCore::JSMainThreadNullState state;
    return IMPL->referrer();
}

- (NSString *)domain
{
    WebCore::JSMainThreadNullState state;
    return IMPL->domain();
}

- (NSString *)URL
{
    WebCore::JSMainThreadNullState state;
    return IMPL->urlForBindings();
}

- (NSString *)cookie
{
    WebCore::JSMainThreadNullState state;
    return raiseOnDOMError(IMPL->cookie());
}

- (void)setCookie:(NSString *)newCookie
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setCookie(newCookie));
}

- (DOMHTMLElement *)body
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->bodyOrFrameset()));
}

- (void)setBody:(DOMHTMLElement *)newBody
{
    WebCore::JSMainThreadNullState state;
    raiseOnDOMError(IMPL->setBodyOrFrameset(core(newBody)));
}

- (DOMHTMLHeadElement *)head
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->head()));
}

- (DOMHTMLCollection *)images
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->images()));
}

- (DOMHTMLCollection *)applets
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->applets()));
}

- (DOMHTMLCollection *)links
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->links()));
}

- (DOMHTMLCollection *)forms
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->forms()));
}

- (DOMHTMLCollection *)anchors
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->anchors()));
}

- (NSString *)lastModified
{
    WebCore::JSMainThreadNullState state;
    return IMPL->lastModified();
}

- (NSString *)charset
{
    WebCore::JSMainThreadNullState state;
    return IMPL->charset();
}

- (void)setCharset:(NSString *)newCharset
{
    WebCore::JSMainThreadNullState state;
    IMPL->setCharset(newCharset);
}

- (NSString *)defaultCharset
{
    return IMPL->defaultCharsetForLegacyBindings();
}

- (NSString *)readyState
{
    WebCore::JSMainThreadNullState state;
    auto readyState = IMPL->readyState();
    switch (readyState) {
    case WebCore::Document::Loading:
        return @"loading";
    case WebCore::Document::Interactive:
        return @"interactive";
    case WebCore::Document::Complete:
        return @"complete";
    }
    ASSERT_NOT_REACHED();
    return @"complete";
}

- (NSString *)characterSet
{
    WebCore::JSMainThreadNullState state;
    return IMPL->characterSetWithUTF8Fallback();
}

- (NSString *)preferredStylesheetSet
{
    return nil;
}

- (NSString *)selectedStylesheetSet
{
    return nil;
}

- (void)setSelectedStylesheetSet:(NSString *)newSelectedStylesheetSet
{
}

- (DOMElement *)activeElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->activeElement()));
}

- (NSString *)compatMode
{
    WebCore::JSMainThreadNullState state;
    return IMPL->compatMode();
}

#if ENABLE(FULLSCREEN_API)

- (BOOL)webkitIsFullScreen
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitIsFullScreen();
}

- (BOOL)webkitFullScreenKeyboardInputAllowed
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitFullScreenKeyboardInputAllowed();
}

- (DOMElement *)webkitCurrentFullScreenElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->webkitCurrentFullScreenElementForBindings()));
}

- (BOOL)webkitFullscreenEnabled
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitFullscreenEnabled();
}

- (DOMElement *)webkitFullscreenElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->webkitFullscreenElementForBindings()));
}

#endif

- (NSString *)visibilityState
{
    WebCore::JSMainThreadNullState state;
    switch (IMPL->visibilityState()) {
    case WebCore::VisibilityState::Hidden:
        return @"hidden";
    case WebCore::VisibilityState::Visible:
        return @"visible";
    case WebCore::VisibilityState::Prerender:
        return @"prerender";
    }
}

- (BOOL)hidden
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hidden();
}

- (DOMHTMLScriptElement *)currentScript
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->currentScript()));
}

- (NSString *)origin
{
    WebCore::JSMainThreadNullState state;
    return IMPL->origin();
}

- (DOMElement *)scrollingElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->scrollingElementForAPI()));
}

- (DOMHTMLCollection *)children
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->children()));
}

- (DOMElement *)firstElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->firstElementChild()));
}

- (DOMElement *)lastElementChild
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->lastElementChild()));
}

- (unsigned)childElementCount
{
    WebCore::JSMainThreadNullState state;
    return IMPL->childElementCount();
}

- (DOMElement *)createElement:(NSString *)tagName
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createElementForBindings(tagName)).ptr());
}

- (DOMDocumentFragment *)createDocumentFragment
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createDocumentFragment()));
}

- (DOMText *)createTextNode:(NSString *)data
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createTextNode(data)));
}

- (DOMComment *)createComment:(NSString *)data
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createComment(data)));
}

- (DOMCDATASection *)createCDATASection:(NSString *)data
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createCDATASection(data)).ptr());
}

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target data:(NSString *)data
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createProcessingInstruction(target, data)).ptr());
}

- (DOMAttr *)createAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createAttribute(name)).ptr());
}

- (DOMEntityReference *)createEntityReference:(NSString *)name
{
    UNUSED_PARAM(name);
    raiseNotSupportedErrorException();
}

- (DOMNodeList *)getElementsByTagName:(NSString *)tagname
{
    if (!tagname)
        return nullptr;

    WebCore::JSMainThreadNullState state;
    return kit(static_cast<WebCore::NodeList*>(WTF::getPtr(IMPL->getElementsByTagName(tagname))));
}

- (DOMNode *)importNode:(DOMNode *)importedNode deep:(BOOL)deep
{
    WebCore::JSMainThreadNullState state;
    if (!importedNode)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(IMPL->importNode(*core(importedNode), deep)).ptr());
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createElementNS(namespaceURI, qualifiedName)).ptr());
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createAttributeNS(namespaceURI, qualifiedName)).ptr());
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName
{
    if (!localName)
        return nullptr;
    WebCore::JSMainThreadNullState state;
    return kit(static_cast<WebCore::NodeList*>(WTF::getPtr(IMPL->getElementsByTagNameNS(namespaceURI, localName))));
}

- (DOMNode *)adoptNode:(DOMNode *)source
{
    WebCore::JSMainThreadNullState state;
    if (!source)
        raiseTypeErrorException();
    return kit(raiseOnDOMError(IMPL->adoptNode(*core(source))).ptr());
}

- (DOMEvent *)createEvent:(NSString *)eventType
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createEvent(eventType)).ptr());
}

- (DOMRange *)createRange
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createRange()));
}

- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root whatToShow:(unsigned)whatToShow filter:(id <DOMNodeFilter>)filter expandEntityReferences:(BOOL)expandEntityReferences
{
    WebCore::JSMainThreadNullState state;
    if (!root)
        raiseTypeErrorException();
    RefPtr<WebCore::NodeFilter> nativeNodeFilter;
    if (filter)
        nativeNodeFilter = WebCore::NativeNodeFilter::create(IMPL, WebCore::ObjCNodeFilterCondition::create(filter));
    return kit(WTF::getPtr(IMPL->createNodeIterator(*core(root), whatToShow, WTF::getPtr(nativeNodeFilter), expandEntityReferences)));
}

- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root whatToShow:(unsigned)whatToShow filter:(id <DOMNodeFilter>)filter expandEntityReferences:(BOOL)expandEntityReferences
{
    WebCore::JSMainThreadNullState state;
    if (!root)
        raiseTypeErrorException();
    RefPtr<WebCore::NodeFilter> nativeNodeFilter;
    if (filter)
        nativeNodeFilter = WebCore::NativeNodeFilter::create(IMPL, WebCore::ObjCNodeFilterCondition::create(filter));
    return kit(WTF::getPtr(IMPL->createTreeWalker(*core(root), whatToShow, WTF::getPtr(nativeNodeFilter), expandEntityReferences)));
}

- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement
{
    return nil;
}

static RefPtr<WebCore::XPathNSResolver> wrap(id <DOMXPathNSResolver> resolver)
{
    if (!resolver)
        return nullptr;
    if ([resolver isMemberOfClass:[DOMNativeXPathNSResolver class]])
        return core(static_cast<DOMNativeXPathNSResolver *>(resolver));
    return DOMCustomXPathNSResolver::create(resolver);
}

- (DOMXPathExpression *)createExpression:(NSString *)expression resolver:(id <DOMXPathNSResolver>)resolver
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->createExpression(expression, wrap(resolver))).ptr());
}

- (id <DOMXPathNSResolver>)createNSResolver:(DOMNode *)nodeResolver
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createNSResolver(core(nodeResolver))));
}

- (DOMXPathResult *)evaluate:(NSString *)expression contextNode:(DOMNode *)contextNode resolver:(id <DOMXPathNSResolver>)resolver type:(unsigned short)type inResult:(DOMXPathResult *)inResult
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->evaluate(expression, core(contextNode), wrap(resolver), type, core(inResult))).ptr());
}

- (BOOL)execCommand:(NSString *)command userInterface:(BOOL)userInterface value:(NSString *)value
{
    WebCore::JSMainThreadNullState state;
    return IMPL->execCommand(command, userInterface, value);
}

- (BOOL)execCommand:(NSString *)command userInterface:(BOOL)userInterface
{
    return [self execCommand:command userInterface:userInterface value:nullptr];
}

- (BOOL)execCommand:(NSString *)command
{
    return [self execCommand:command userInterface:NO value:nullptr];
}

- (BOOL)queryCommandEnabled:(NSString *)command
{
    WebCore::JSMainThreadNullState state;
    return IMPL->queryCommandEnabled(command);
}

- (BOOL)queryCommandIndeterm:(NSString *)command
{
    WebCore::JSMainThreadNullState state;
    return IMPL->queryCommandIndeterm(command);
}

- (BOOL)queryCommandState:(NSString *)command
{
    WebCore::JSMainThreadNullState state;
    return IMPL->queryCommandState(command);
}

- (BOOL)queryCommandSupported:(NSString *)command
{
    WebCore::JSMainThreadNullState state;
    return IMPL->queryCommandSupported(command);
}

- (NSString *)queryCommandValue:(NSString *)command
{
    WebCore::JSMainThreadNullState state;
    return IMPL->queryCommandValue(command);
}

- (DOMNodeList *)getElementsByName:(NSString *)elementName
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getElementsByName(elementName)));
}

- (DOMElement *)elementFromPoint:(int)x y:(int)y
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->elementFromPoint(x, y)));
}

- (DOMRange *)caretRangeFromPoint:(int)x y:(int)y
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->caretRangeFromPoint(x, y)));
}

- (DOMCSSStyleDeclaration *)createCSSStyleDeclaration
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createCSSStyleDeclaration()));
}

- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement
{
    WebCore::JSMainThreadNullState state;
    if (!element)
        raiseTypeErrorException();
    WebCore::DOMWindow* dv = IMPL->domWindow();
    if (!dv)
        return nil;
    return kit(WTF::getPtr(dv->getComputedStyle(*core(element), pseudoElement)));
}

- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement
{
    return [self getMatchedCSSRules:element pseudoElement:pseudoElement authorOnly:YES];
}

- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement authorOnly:(BOOL)authorOnly
{
    WebCore::JSMainThreadNullState state;
    WebCore::DOMWindow* dv = IMPL->domWindow();
    if (!dv)
        return nil;
    return kit(WTF::getPtr(dv->getMatchedCSSRules(core(element), pseudoElement, authorOnly)));
}

- (DOMNodeList *)getElementsByClassName:(NSString *)classNames
{
    WebCore::JSMainThreadNullState state;
    return kit(static_cast<WebCore::NodeList*>(WTF::getPtr(IMPL->getElementsByClassName(classNames))));
}

- (BOOL)hasFocus
{
    WebCore::JSMainThreadNullState state;
    return IMPL->hasFocus();
}

#if ENABLE(FULLSCREEN_API)

- (void)webkitCancelFullScreen
{
    WebCore::JSMainThreadNullState state;
    IMPL->webkitCancelFullScreen();
}

- (void)webkitExitFullscreen
{
    WebCore::JSMainThreadNullState state;
    IMPL->webkitExitFullscreen();
}

#endif

- (DOMElement *)getElementById:(NSString *)elementId
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getElementById(AtomicString(elementId))));
}

- (DOMElement *)querySelector:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->querySelector(selectors)));
}

- (DOMNodeList *)querySelectorAll:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    return kit(raiseOnDOMError(IMPL->querySelectorAll(selectors)).ptr());
}

@end

@implementation DOMDocument (DOMDocumentDeprecated)

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data
{
    return [self createProcessingInstruction:target data:data];
}

- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep
{
    return [self importNode:importedNode deep:deep];
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    return [self createElementNS:namespaceURI qualifiedName:qualifiedName];
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    return [self createAttributeNS:namespaceURI qualifiedName:qualifiedName];
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self getElementsByTagNameNS:namespaceURI localName:localName];
}

- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    return [self createNodeIterator:root whatToShow:whatToShow filter:filter expandEntityReferences:expandEntityReferences];
}

- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    return [self createTreeWalker:root whatToShow:whatToShow filter:filter expandEntityReferences:expandEntityReferences];
}

- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)element :(NSString *)pseudoElement
{
    return nil;
}

- (DOMXPathExpression *)createExpression:(NSString *)expression :(id <DOMXPathNSResolver>)resolver
{
    return [self createExpression:expression resolver:resolver];
}

- (DOMXPathResult *)evaluate:(NSString *)expression :(DOMNode *)contextNode :(id <DOMXPathNSResolver>)resolver :(unsigned short)type :(DOMXPathResult *)inResult
{
    return [self evaluate:expression contextNode:contextNode resolver:resolver type:type inResult:inResult];
}

- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)element :(NSString *)pseudoElement
{
    return [self getComputedStyle:element pseudoElement:pseudoElement];
}

@end

WebCore::Document* core(DOMDocument *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::Document*>(wrapper->_internal) : 0;
}

DOMDocument *kit(WebCore::Document* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMDocument*>(kit(static_cast<WebCore::Node*>(value)));
}
