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

#import "config.h"
#import "DOMDocumentInternal.h"

#import "Attr.h"
#import "CDATASection.h"
#import "CSSRuleList.h"
#import "CSSStyleDeclaration.h"
#import "Comment.h"
#import "DOMAbstractViewInternal.h"
#import "DOMAttrInternal.h"
#import "DOMCDATASectionInternal.h"
#import "DOMCSSRuleListInternal.h"
#import "DOMCSSStyleDeclarationInternal.h"
#import "DOMCommentInternal.h"
#import "DOMCustomXPathNSResolver.h"
#import "DOMDOMImplementationInternal.h"
#import "DOMDocumentFragmentInternal.h"
#import "DOMDocumentTypeInternal.h"
#import "DOMElementInternal.h"
#import "DOMEventInternal.h"
#import "DOMHTMLCollectionInternal.h"
#import "DOMHTMLElementInternal.h"
#import "DOMHTMLHeadElementInternal.h"
#import "DOMHTMLScriptElementInternal.h"
#import "DOMImplementation.h"
#import "DOMInternal.h"
#import "DOMNodeInternal.h"
#import "DOMNodeIteratorInternal.h"
#import "DOMNodeListInternal.h"
#import "DOMProcessingInstructionInternal.h"
#import "DOMRangeInternal.h"
#import "DOMStyleSheetListInternal.h"
#import "DOMTextInternal.h"
#import "DOMTreeWalkerInternal.h"
#import "DOMWindow.h"
#import "DOMXPathExpressionInternal.h"
#import "DOMXPathResultInternal.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentType.h"
#import "Element.h"
#import "Event.h"
#import "ExceptionHandlers.h"
#import "HTMLCollection.h"
#import "HTMLElement.h"
#import "HTMLHeadElement.h"
#import "HTMLScriptElement.h"
#import "JSMainThreadExecState.h"
#import "NameNodeList.h"
#import "NativeNodeFilter.h"
#import "Node.h"
#import "NodeIterator.h"
#import "NodeList.h"
#import "ObjCNodeFilterCondition.h"
#import "ProcessingInstruction.h"
#import "Range.h"
#import "StyleProperties.h"
#import "StyleSheetList.h"
#import "Text.h"
#import "ThreadCheck.h"
#import "TreeWalker.h"
#import "URL.h"
#import "WebScriptObjectPrivate.h"
#import "XPathExpression.h"
#import "XPathNSResolver.h"
#import "XPathResult.h"
#import <wtf/GetPtr.h>

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
    WebCore::ExceptionCode ec = 0;
    IMPL->setXMLVersion(newXmlVersion, ec);
    WebCore::raiseOnDOMError(ec);
}

- (BOOL)xmlStandalone
{
    WebCore::JSMainThreadNullState state;
    return IMPL->xmlStandalone();
}

- (void)setXmlStandalone:(BOOL)newXmlStandalone
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setXMLStandalone(newXmlStandalone, ec);
    WebCore::raiseOnDOMError(ec);
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
    return kit(WTF::getPtr(IMPL->defaultView()));
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
    WebCore::ExceptionCode ec = 0;
    IMPL->setTitle(newTitle, ec);
    WebCore::raiseOnDOMError(ec);
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
    WebCore::ExceptionCode ec = 0;
    NSString *result = IMPL->cookie(ec);
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (void)setCookie:(NSString *)newCookie
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    IMPL->setCookie(newCookie, ec);
    WebCore::raiseOnDOMError(ec);
}

- (DOMHTMLElement *)body
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->bodyOrFrameset()));
}

- (void)setBody:(DOMHTMLElement *)newBody
{
    WebCore::JSMainThreadNullState state;
    ASSERT(newBody);

    WebCore::ExceptionCode ec = 0;
    IMPL->setBodyOrFrameset(core(newBody), ec);
    WebCore::raiseOnDOMError(ec);
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
    WebCore::JSMainThreadNullState state;
    return IMPL->defaultCharsetForBindings();
}

- (NSString *)readyState
{
    WebCore::JSMainThreadNullState state;
    return IMPL->readyState();
}

- (NSString *)characterSet
{
    WebCore::JSMainThreadNullState state;
    return IMPL->characterSetWithUTF8Fallback();
}

- (NSString *)preferredStylesheetSet
{
    WebCore::JSMainThreadNullState state;
    return IMPL->preferredStylesheetSet();
}

- (NSString *)selectedStylesheetSet
{
    WebCore::JSMainThreadNullState state;
    return IMPL->selectedStylesheetSet();
}

- (void)setSelectedStylesheetSet:(NSString *)newSelectedStylesheetSet
{
    WebCore::JSMainThreadNullState state;
    IMPL->setSelectedStylesheetSet(newSelectedStylesheetSet);
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
    return kit(WTF::getPtr(IMPL->webkitCurrentFullScreenElement()));
}

- (BOOL)webkitFullscreenEnabled
{
    WebCore::JSMainThreadNullState state;
    return IMPL->webkitFullscreenEnabled();
}

- (DOMElement *)webkitFullscreenElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->webkitFullscreenElement()));
}
#endif

#if ENABLE(POINTER_LOCK)
- (DOMElement *)pointerLockElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->pointerLockElement()));
}
#endif

- (NSString *)visibilityState
{
    WebCore::JSMainThreadNullState state;
    return IMPL->visibilityState();
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
    return kit(WTF::getPtr(IMPL->scrollingElement()));
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
    WebCore::ExceptionCode ec = 0;
    DOMElement *result = kit(WTF::getPtr(IMPL->createElementForBindings(tagName, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
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
    WebCore::ExceptionCode ec = 0;
    DOMCDATASection *result = kit(WTF::getPtr(IMPL->createCDATASection(data, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target data:(NSString *)data
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMProcessingInstruction *result = kit(WTF::getPtr(IMPL->createProcessingInstruction(target, data, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMAttr *)createAttribute:(NSString *)name
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMAttr *result = kit(WTF::getPtr(IMPL->createAttribute(name, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMEntityReference *)createEntityReference:(NSString *)name
{
    UNUSED_PARAM(name);

    WebCore::raiseOnDOMError(WebCore::NOT_SUPPORTED_ERR);
    return nil;
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
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->importNode(*core(importedNode), deep, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMElement *result = kit(WTF::getPtr(IMPL->createElementNS(namespaceURI, qualifiedName, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMAttr *result = kit(WTF::getPtr(IMPL->createAttributeNS(namespaceURI, qualifiedName, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
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
        WebCore::raiseTypeErrorException();
    WebCore::ExceptionCode ec = 0;
    DOMNode *result = kit(WTF::getPtr(IMPL->adoptNode(*core(source), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMEvent *)createEvent:(NSString *)eventType
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMEvent *result = kit(WTF::getPtr(IMPL->createEvent(eventType, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
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
        WebCore::raiseTypeErrorException();
    RefPtr<WebCore::NodeFilter> nativeNodeFilter;
    if (filter)
        nativeNodeFilter = WebCore::NativeNodeFilter::create(WebCore::ObjCNodeFilterCondition::create(filter));
    return kit(WTF::getPtr(IMPL->createNodeIterator(*core(root), whatToShow, WTF::getPtr(nativeNodeFilter), expandEntityReferences)));
}

- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root whatToShow:(unsigned)whatToShow filter:(id <DOMNodeFilter>)filter expandEntityReferences:(BOOL)expandEntityReferences
{
    WebCore::JSMainThreadNullState state;
    if (!root)
        WebCore::raiseTypeErrorException();
    RefPtr<WebCore::NodeFilter> nativeNodeFilter;
    if (filter)
        nativeNodeFilter = WebCore::NativeNodeFilter::create(WebCore::ObjCNodeFilterCondition::create(filter));
    return kit(WTF::getPtr(IMPL->createTreeWalker(*core(root), whatToShow, WTF::getPtr(nativeNodeFilter), expandEntityReferences)));
}

- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->getOverrideStyle(core(element), pseudoElement)));
}

- (DOMXPathExpression *)createExpression:(NSString *)expression resolver:(id <DOMXPathNSResolver>)resolver
{
    WebCore::JSMainThreadNullState state;
    WebCore::XPathNSResolver* nativeResolver = 0;
    RefPtr<WebCore::XPathNSResolver> customResolver;
    if (resolver) {
        if ([resolver isMemberOfClass:[DOMNativeXPathNSResolver class]])
            nativeResolver = core(static_cast<DOMNativeXPathNSResolver *>(resolver));
        else {
            customResolver = WebCore::DOMCustomXPathNSResolver::create(resolver);
            nativeResolver = WTF::getPtr(customResolver);
        }
    }
    WebCore::ExceptionCode ec = 0;
    DOMXPathExpression *result = kit(WTF::getPtr(IMPL->createExpression(expression, WTF::getPtr(nativeResolver), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (id <DOMXPathNSResolver>)createNSResolver:(DOMNode *)nodeResolver
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->createNSResolver(core(nodeResolver))));
}

- (DOMXPathResult *)evaluate:(NSString *)expression contextNode:(DOMNode *)contextNode resolver:(id <DOMXPathNSResolver>)resolver type:(unsigned short)type inResult:(DOMXPathResult *)inResult
{
    WebCore::JSMainThreadNullState state;
    WebCore::XPathNSResolver* nativeResolver = 0;
    RefPtr<WebCore::XPathNSResolver> customResolver;
    if (resolver) {
        if ([resolver isMemberOfClass:[DOMNativeXPathNSResolver class]])
            nativeResolver = core(static_cast<DOMNativeXPathNSResolver *>(resolver));
        else {
            customResolver = WebCore::DOMCustomXPathNSResolver::create(resolver);
            nativeResolver = WTF::getPtr(customResolver);
        }
    }
    WebCore::ExceptionCode ec = 0;
    DOMXPathResult *result = kit(WTF::getPtr(IMPL->evaluate(expression, core(contextNode), WTF::getPtr(nativeResolver), type, core(inResult), ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
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
        WebCore::raiseTypeErrorException();
    WebCore::DOMWindow* dv = IMPL->defaultView();
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
    WebCore::DOMWindow* dv = IMPL->defaultView();
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

#if ENABLE(POINTER_LOCK)
- (void)exitPointerLock
{
    WebCore::JSMainThreadNullState state;
    IMPL->exitPointerLock();
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
    WebCore::ExceptionCode ec = 0;
    DOMElement *result = kit(WTF::getPtr(IMPL->querySelector(selectors, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
}

- (DOMNodeList *)querySelectorAll:(NSString *)selectors
{
    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    DOMNodeList *result = kit(WTF::getPtr(IMPL->querySelectorAll(selectors, ec)));
    WebCore::raiseOnDOMError(ec);
    return result;
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
    return [self getOverrideStyle:element pseudoElement:pseudoElement];
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
