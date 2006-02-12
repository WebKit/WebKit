/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source exceptionCode must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "DOM.h"

#import "ContainerNodeImpl.h"
#import "DOMEventsInternal.h"
#import "DOMHTML.h"
#import "DOMImplementationImpl.h"
#import "DOMInternal.h"
#import "DOMPrivate.h"
#import "DocumentFragmentImpl.h"
#import "DocumentImpl.h"
#import "DocumentTypeImpl.h"
#import "KWQFoundationExtras.h"
#import "MacFrame.h"
#import "NodeListImpl.h"
#import "csshelper.h"
#import "dom2_events.h"
#import "dom2_range.h"
#import "dom2_rangeimpl.h"
#import "dom2_traversal.h"
#import "dom2_viewsimpl.h"
#import "dom_elementimpl.h"
#import "dom_exception.h"
#import "dom_node.h"
#import "PlatformString.h"
#import "StringImpl.h"
#import "CDATASectionImpl.h"
#import "CommentImpl.h"
#import "dom_xmlimpl.h"
#import "HTMLElementImpl.h"
#import "htmlnames.h"
#import "render_image.h"
#import <JavaScriptCore/WebScriptObjectPrivate.h>
#import <kxmlcore/Assertions.h>
#import <kxmlcore/HashMap.h>
#include <objc/objc-class.h>

using namespace WebCore;
using namespace HTMLNames;

@interface DOMAttr (WebCoreInternal)
+ (DOMAttr *)_attrWithImpl:(AttrImpl *)impl;
- (AttrImpl *)_attrImpl;
@end

@interface DOMDocumentType (WebCoreInternal)
- (DocumentTypeImpl *)_documentTypeImpl;
@end

@interface DOMImplementation (WebCoreInternal)
+ (DOMImplementation *)_DOMImplementationWithImpl:(DOMImplementationImpl *)impl;
- (DOMImplementationImpl *)_DOMImplementationImpl;
@end

@interface DOMNamedNodeMap (WebCoreInternal)
+ (DOMNamedNodeMap *)_namedNodeMapWithImpl:(NamedNodeMapImpl *)impl;
@end

class ObjCEventListener : public EventListener {
public:
    static ObjCEventListener *find(id <DOMEventListener>);
    static ObjCEventListener *create(id <DOMEventListener>);

private:
    ObjCEventListener(id <DOMEventListener>);
    virtual ~ObjCEventListener();

    virtual void handleEvent(EventImpl *, bool isWindowEvent);

    id <DOMEventListener> m_listener;
};

typedef HashMap<id, ObjCEventListener*> ListenerMap;

static ListenerMap *listenerMap;

//------------------------------------------------------------------------------------------
// DOMObject

@implementation DOMObject

// Prevent creation of DOM objects by clients who just "[[xxx alloc] init]".
- (id)init
{
    [NSException raise:NSGenericException format:@"+[%s init]: should never be used", [self class]->name];
    [self release];
    return nil;
}

- (void)dealloc
{
    if (_internal) {
        removeDOMWrapper(_internal);
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        removeDOMWrapper(_internal);
    }
    [super finalize];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

@implementation DOMObject (WebCoreInternal)

- (id)_init
{
    return [super _init];
}

@end

//------------------------------------------------------------------------------------------
// DOMNode

@implementation DOMNode

- (void)dealloc
{
    if (_internal) {
        DOM_cast<NodeImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<NodeImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (NSString *)description
{
    if (!_internal) {
        return [NSString stringWithFormat:@"<%@: null>",
            [[self class] description], self];
    }
    NSString *value = [self nodeValue];
    if (value) {
        return [NSString stringWithFormat:@"<%@ [%@]: %p '%@'>",
            [[self class] description], [self nodeName], _internal, value];
    }
    return [NSString stringWithFormat:@"<%@ [%@]: %p>",
        [[self class] description], [self nodeName], _internal];
}

- (NSString *)nodeName
{
    return [self _nodeImpl]->nodeName();
}

- (NSString *)nodeValue
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return [self _nodeImpl]->nodeValue();
}

- (void)setNodeValue:(NSString *)string
{
    ASSERT(string);
    
    int exceptionCode = 0;
    [self _nodeImpl]->setNodeValue(string, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (unsigned short)nodeType
{
    return [self _nodeImpl]->nodeType();
}

- (DOMNode *)parentNode
{
    return [DOMNode _nodeWithImpl:[self _nodeImpl]->parentNode()];
}

- (DOMNodeList *)childNodes
{
    return [DOMNodeList _nodeListWithImpl:[self _nodeImpl]->childNodes().get()];
}

- (DOMNode *)firstChild
{
    return [DOMNode _nodeWithImpl:[self _nodeImpl]->firstChild()];
}

- (DOMNode *)lastChild
{
    return [DOMNode _nodeWithImpl:[self _nodeImpl]->lastChild()];
}

- (DOMNode *)previousSibling
{
    return [DOMNode _nodeWithImpl:[self _nodeImpl]->previousSibling()];
}

- (DOMNode *)nextSibling
{
    return [DOMNode _nodeWithImpl:[self _nodeImpl]->nextSibling()];
}

- (DOMNamedNodeMap *)attributes
{
    // DOM level 2 core specification says: 
    // A NamedNodeMap containing the attributes of this node (if it is an Element) or null otherwise.
    return nil;
}

- (DOMDocument *)ownerDocument
{
    return [DOMDocument _documentWithImpl:[self _nodeImpl]->getDocument()];
}

- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild
{
    ASSERT(newChild);
    ASSERT(refChild);

    int exceptionCode = 0;
    if ([self _nodeImpl]->insertBefore([newChild _nodeImpl], [refChild _nodeImpl], exceptionCode))
        return newChild;
    raiseOnDOMError(exceptionCode);
    return nil;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild
{
    ASSERT(newChild);
    ASSERT(oldChild);

    int exceptionCode = 0;
    if ([self _nodeImpl]->replaceChild([newChild _nodeImpl], [oldChild _nodeImpl], exceptionCode))
        return oldChild;
    raiseOnDOMError(exceptionCode);
    return nil;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    ASSERT(oldChild);

    int exceptionCode = 0;
    if ([self _nodeImpl]->removeChild([oldChild _nodeImpl], exceptionCode))
        return oldChild;
    raiseOnDOMError(exceptionCode);
    return nil;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    ASSERT(newChild);

    int exceptionCode = 0;
    if ([self _nodeImpl]->appendChild([newChild _nodeImpl], exceptionCode))
        return newChild;
    raiseOnDOMError(exceptionCode);
    return nil;
}

- (BOOL)hasChildNodes
{
    return [self _nodeImpl]->hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    return [DOMNode _nodeWithImpl:[self _nodeImpl]->cloneNode(deep).get()];
}

- (void)normalize
{
    [self _nodeImpl]->normalize();
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return [self _nodeImpl]->isSupported(feature, version);
}

- (NSString *)namespaceURI
{
    return [self _nodeImpl]->namespaceURI();
}

- (NSString *)prefix
{
    return [self _nodeImpl]->prefix();
}

- (void)setPrefix:(NSString *)prefix
{
    ASSERT(prefix);

    int exceptionCode = 0;
    DOMString prefixStr(prefix);
    [self _nodeImpl]->setPrefix(prefixStr.impl(), exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (NSString *)localName
{
    return [self _nodeImpl]->localName();
}

- (BOOL)hasAttributes
{
    return [self _nodeImpl]->hasAttributes();
}

- (BOOL)isSameNode:(DOMNode *)other
{
    return [self _nodeImpl]->isSameNode([other _nodeImpl]);
}

- (BOOL)isEqualNode:(DOMNode *)other
{
    return [self _nodeImpl]->isEqualNode([other _nodeImpl]);
}

- (BOOL)isDefaultNamespace:(NSString *)namespaceURI
{
    return [self _nodeImpl]->isDefaultNamespace(namespaceURI);
}

- (NSString *)lookupPrefix:(NSString *)namespaceURI
{
    return [self _nodeImpl]->lookupPrefix(namespaceURI);
}

- (NSString *)lookupNamespaceURI:(NSString *)prefix
{
    return [self _nodeImpl]->lookupNamespaceURI(prefix);
}

- (NSString *)textContent
{
    return [self _nodeImpl]->textContent();
}

- (void)setTextContent:(NSString *)text
{
    int exceptionCode = 0;
    [self _nodeImpl]->setTextContent(text, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    EventListener *wrapper = ObjCEventListener::create(listener);
    [self _nodeImpl]->addEventListener(type, wrapper, useCapture);
    wrapper->deref();
}

- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    if (EventListener *wrapper = ObjCEventListener::find(listener))
        [self _nodeImpl]->removeEventListener(type, wrapper, useCapture);
}

- (BOOL)dispatchEvent:(DOMEvent *)event
{
    int exceptionCode = 0;
    BOOL result = [self _nodeImpl]->dispatchEvent([event _eventImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (NSRect)boundingBox
{
    khtml::RenderObject *renderer = [self _nodeImpl]->renderer();
    if (renderer)
        return renderer->absoluteBoundingBoxRect();
    return NSZeroRect;
}

- (NSArray *)lineBoxRects
{
    khtml::RenderObject *renderer = [self _nodeImpl]->renderer();
    if (renderer) {
        NSMutableArray *results = [[NSMutableArray alloc] init];
        QValueList<IntRect> rects = renderer->lineBoxRects();
        if (!rects.isEmpty()) {
            for (QValueList<IntRect>::ConstIterator it = rects.begin(); it != rects.end(); ++it)
                [results addObject:[NSValue valueWithRect:*it]];
        }
        return [results autorelease];
    }
    return nil;
}

@end

@implementation DOMNode (WebCoreInternal)

- (id)_initWithNodeImpl:(NodeImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNode *)_nodeWithImpl:(NodeImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    Class wrapperClass = nil;
    switch (impl->nodeType()) {
        case Node::ELEMENT_NODE:
            if (impl->isHTMLElement()) {
                // FIXME: Reflect marquee once the API has been determined.
                // FIXME: We could make the HTML classes hand back their class names and then use that to make
                // the appropriate Obj-C class from the string.
                HTMLElementImpl* htmlElt = static_cast<HTMLElementImpl*>(impl);
                if (htmlElt->hasLocalName(htmlTag))
                    wrapperClass = [DOMHTMLHtmlElement class];
                else if (htmlElt->hasLocalName(headTag))
                    wrapperClass = [DOMHTMLHeadElement class];
                else if (htmlElt->hasLocalName(linkTag))
                    wrapperClass = [DOMHTMLLinkElement class];
                else if (htmlElt->hasLocalName(titleTag))
                    wrapperClass = [DOMHTMLTitleElement class];
                else if (htmlElt->hasLocalName(metaTag))
                    wrapperClass = [DOMHTMLMetaElement class];
                else if (htmlElt->hasLocalName(baseTag))
                    wrapperClass = [DOMHTMLBaseElement class];
                else if (htmlElt->hasLocalName(isindexTag))
                    wrapperClass = [DOMHTMLIsIndexElement class];
                else if (htmlElt->hasLocalName(styleTag))
                    wrapperClass = [DOMHTMLStyleElement class];
                else if (htmlElt->hasLocalName(bodyTag))
                    wrapperClass = [DOMHTMLBodyElement class];
                else if (htmlElt->hasLocalName(formTag))
                    wrapperClass = [DOMHTMLFormElement class];
                else if (htmlElt->hasLocalName(selectTag))
                    wrapperClass = [DOMHTMLSelectElement class];
                else if (htmlElt->hasLocalName(optgroupTag))
                    wrapperClass = [DOMHTMLOptGroupElement class];
                else if (htmlElt->hasLocalName(optionTag))
                    wrapperClass = [DOMHTMLOptionElement class];
                else if (htmlElt->hasLocalName(inputTag))
                    wrapperClass = [DOMHTMLInputElement class];
                else if (htmlElt->hasLocalName(textareaTag))
                    wrapperClass = [DOMHTMLTextAreaElement class];
                else if (htmlElt->hasLocalName(buttonTag))
                    wrapperClass = [DOMHTMLButtonElement class];
                else if (htmlElt->hasLocalName(labelTag))
                    wrapperClass = [DOMHTMLLabelElement class];
                else if (htmlElt->hasLocalName(fieldsetTag))
                    wrapperClass = [DOMHTMLFieldSetElement class];
                else if (htmlElt->hasLocalName(legendTag))
                    wrapperClass = [DOMHTMLLegendElement class];
                else if (htmlElt->hasLocalName(ulTag))
                    wrapperClass = [DOMHTMLUListElement class];
                else if (htmlElt->hasLocalName(olTag))                       
                    wrapperClass = [DOMHTMLOListElement class];
                else if (htmlElt->hasLocalName(dlTag))
                    wrapperClass = [DOMHTMLDListElement class];
                else if (htmlElt->hasLocalName(dirTag))
                    wrapperClass = [DOMHTMLDirectoryElement class];
                else if (htmlElt->hasLocalName(menuTag))
                    wrapperClass = [DOMHTMLMenuElement class];
                else if (htmlElt->hasLocalName(liTag))
                    wrapperClass = [DOMHTMLLIElement class];
                else if (htmlElt->hasLocalName(divTag))
                    wrapperClass = [DOMHTMLDivElement class];
                else if (htmlElt->hasLocalName(pTag))
                    wrapperClass = [DOMHTMLParagraphElement class];
                else if (htmlElt->hasLocalName(h1Tag) ||
                         htmlElt->hasLocalName(h2Tag) ||
                         htmlElt->hasLocalName(h3Tag) ||
                         htmlElt->hasLocalName(h4Tag) ||
                         htmlElt->hasLocalName(h5Tag) ||
                         htmlElt->hasLocalName(h6Tag))
                    wrapperClass = [DOMHTMLHeadingElement class];
                else if (htmlElt->hasLocalName(qTag))
                    wrapperClass = [DOMHTMLQuoteElement class];
                else if (htmlElt->hasLocalName(preTag))
                    wrapperClass = [DOMHTMLPreElement class];
                else if (htmlElt->hasLocalName(brTag))
                    wrapperClass = [DOMHTMLBRElement class];
                else if (htmlElt->hasLocalName(basefontTag))
                    wrapperClass = [DOMHTMLBaseFontElement class];
                else if (htmlElt->hasLocalName(fontTag))
                    wrapperClass = [DOMHTMLFontElement class];
                else if (htmlElt->hasLocalName(hrTag))
                    wrapperClass = [DOMHTMLHRElement class];
                else if (htmlElt->hasLocalName(aTag))
                    wrapperClass = [DOMHTMLAnchorElement class];
                else if (htmlElt->hasLocalName(imgTag) ||
                         htmlElt->hasLocalName(canvasTag))
                    wrapperClass = [DOMHTMLImageElement class];
                else if (htmlElt->hasLocalName(objectTag))
                    wrapperClass = [DOMHTMLObjectElement class];
                else if (htmlElt->hasLocalName(paramTag))
                    wrapperClass = [DOMHTMLParamElement class];
                else if (htmlElt->hasLocalName(appletTag))
                    wrapperClass = [DOMHTMLAppletElement class];
                else if (htmlElt->hasLocalName(mapTag))
                    wrapperClass = [DOMHTMLMapElement class];
                else if (htmlElt->hasLocalName(areaTag))
                    wrapperClass = [DOMHTMLAreaElement class];
                else if (htmlElt->hasLocalName(scriptTag))
                    wrapperClass = [DOMHTMLScriptElement class];
                else if (htmlElt->hasLocalName(tableTag))
                    wrapperClass = [DOMHTMLTableElement class];
                else if (htmlElt->hasLocalName(theadTag) ||
                         htmlElt->hasLocalName(tbodyTag) ||
                         htmlElt->hasLocalName(tfootTag))
                    wrapperClass = [DOMHTMLTableSectionElement class];
                else if (htmlElt->hasLocalName(tdTag))
                    wrapperClass = [DOMHTMLTableCellElement class];
                else if (htmlElt->hasLocalName(trTag))
                    wrapperClass = [DOMHTMLTableRowElement class];
                else if (htmlElt->hasLocalName(colTag) ||
                         htmlElt->hasLocalName(colgroupTag))
                    wrapperClass = [DOMHTMLTableColElement class];
                else if (htmlElt->hasLocalName(captionTag))
                    wrapperClass = [DOMHTMLTableCaptionElement class];
                else if (htmlElt->hasLocalName(framesetTag))
                    wrapperClass = [DOMHTMLFrameSetElement class];
                else if (htmlElt->hasLocalName(frameTag))
                    wrapperClass = [DOMHTMLFrameElement class];
                else if (htmlElt->hasLocalName(iframeTag))
                    wrapperClass = [DOMHTMLIFrameElement class];
                else
                    wrapperClass = [DOMHTMLElement class];
            } else {
                wrapperClass = [DOMElement class];
            }
            break;
        case Node::ATTRIBUTE_NODE:
            wrapperClass = [DOMAttr class];
            break;
        case Node::TEXT_NODE:
            wrapperClass = [DOMText class];
            break;
        case Node::CDATA_SECTION_NODE:
            wrapperClass = [DOMCDATASection class];
            break;
        case Node::ENTITY_REFERENCE_NODE:
            wrapperClass = [DOMEntityReference class];
            break;
        case Node::ENTITY_NODE:
            wrapperClass = [DOMEntity class];
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
            wrapperClass = [DOMProcessingInstruction class];
            break;
        case Node::COMMENT_NODE:
            wrapperClass = [DOMComment class];
            break;
        case Node::DOCUMENT_NODE:
            if (static_cast<DocumentImpl *>(impl)->isHTMLDocument()) {
                wrapperClass = [DOMHTMLDocument class];
            } else {
                wrapperClass = [DOMDocument class];
            }
            break;
        case Node::DOCUMENT_TYPE_NODE:
            wrapperClass = [DOMDocumentType class];
            break;
        case Node::DOCUMENT_FRAGMENT_NODE:
            wrapperClass = [DOMDocumentFragment class];
            break;
        case Node::NOTATION_NODE:
            wrapperClass = [DOMNotation class];
            break;
    }
    return [[[wrapperClass alloc] _initWithNodeImpl:impl] autorelease];
}

- (NodeImpl *)_nodeImpl
{
    return DOM_cast<NodeImpl *>(_internal);
}

- (BOOL)isContentEditable
{
    return [self _nodeImpl]->isContentEditable();
}

- (const KJS::Bindings::RootObject *)_executionContext
{
    NodeImpl *n = [self _nodeImpl];
    if (!n)
        return 0;
    
    DocumentImpl *doc = n->getDocument();
    if (!doc)
        return 0;
    
    MacFrame *p = Mac(doc->frame());
    if (!p)
        return 0;
        
    return p->executionContextForDOM();
}

@end

//------------------------------------------------------------------------------------------
// DOMNamedNodeMap

@implementation DOMNamedNodeMap

- (void)dealloc
{
    if (_internal) {
        DOM_cast<NamedNodeMapImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<NamedNodeMapImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (NamedNodeMapImpl *)_namedNodeMapImpl
{
    return DOM_cast<NamedNodeMapImpl *>(_internal);
}

- (DOMNode *)getNamedItem:(NSString *)name
{
    ASSERT(name);

    return [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->getNamedItem(name).get()];
}

- (DOMNode *)setNamedItem:(DOMNode *)arg
{
    ASSERT(arg);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->setNamedItem([arg _nodeImpl], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNode *)removeNamedItem:(NSString *)name
{
    ASSERT(name);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->removeNamedItem(name, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNode *)item:(unsigned)index
{
    return [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->item(index).get()];
}

- (unsigned)length
{
    return [self _namedNodeMapImpl]->length();
}

- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    if (!namespaceURI || !localName) {
        return nil;
    }

    return [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->getNamedItemNS(namespaceURI, localName).get()];
}

- (DOMNode *)setNamedItemNS:(DOMNode *)arg
{
    ASSERT(arg);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->setNamedItemNS([arg _nodeImpl], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->removeNamedItemNS(namespaceURI, localName, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

@end

@implementation DOMNamedNodeMap (WebCoreInternal)

- (id)_initWithNamedNodeMapImpl:(NamedNodeMapImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNamedNodeMap *)_namedNodeMapWithImpl:(NamedNodeMapImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNamedNodeMapImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMNodeList

@implementation DOMNodeList

- (void)dealloc
{
    if (_internal) {
        DOM_cast<NodeListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<NodeListImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (NodeListImpl *)_nodeListImpl
{
    return DOM_cast<NodeListImpl *>(_internal);
}

- (DOMNode *)item:(unsigned)index
{
    return [DOMNode _nodeWithImpl:[self _nodeListImpl]->item(index)];
}

- (unsigned)length
{
    return [self _nodeListImpl]->length();
}

@end

@implementation DOMNodeList (WebCoreInternal)

- (id)_initWithNodeListImpl:(NodeListImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNodeList *)_nodeListWithImpl:(NodeListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNodeListImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMImplementation

@implementation DOMImplementation

- (void)dealloc
{
    if (_internal) {
        DOM_cast<DOMImplementationImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<DOMImplementationImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (BOOL)hasFeature:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return [self _DOMImplementationImpl]->hasFeature(feature, version);
}

- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId
{
    ASSERT(qualifiedName);
    ASSERT(publicId);
    ASSERT(systemId);

    int exceptionCode = 0;
    DocumentTypeImpl *impl = [self _DOMImplementationImpl]->createDocumentType(qualifiedName, publicId, systemId, exceptionCode);
    raiseOnDOMError(exceptionCode);
    return static_cast<DOMDocumentType *>([DOMNode _nodeWithImpl:impl]);
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int exceptionCode = 0;
    DocumentImpl *impl = [self _DOMImplementationImpl]->createDocument(namespaceURI, qualifiedName, [doctype _documentTypeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
    return static_cast<DOMDocument *>([DOMNode _nodeWithImpl:impl]);
}

@end

@implementation DOMImplementation (DOMImplementationCSS)

- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title :(NSString *)media
{
    ASSERT(title);
    ASSERT(media);

    int exceptionCode = 0;
    DOMCSSStyleSheet *result = [DOMCSSStyleSheet _CSSStyleSheetWithImpl:[self _DOMImplementationImpl]->createCSSStyleSheet(title, media, exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

@end
 
@implementation DOMImplementation (WebCoreInternal)

- (id)_initWithDOMImplementationImpl:(DOMImplementationImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMImplementation *)_DOMImplementationWithImpl:(DOMImplementationImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithDOMImplementationImpl:impl] autorelease];
}

- (DOMImplementationImpl *)_DOMImplementationImpl
{
    return DOM_cast<DOMImplementationImpl *>(_internal);
}

@end

//------------------------------------------------------------------------------------------
// DOMDocumentFragment

@implementation DOMDocumentFragment

@end

@implementation DOMDocumentFragment (WebCoreInternal)

+ (DOMDocumentFragment *)_documentFragmentWithImpl:(DocumentFragmentImpl *)impl
{
    return static_cast<DOMDocumentFragment *>([DOMNode _nodeWithImpl:impl]);
}

- (DocumentFragmentImpl *)_fragmentImpl
{
    return static_cast<DocumentFragmentImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMDocument

@implementation DOMDocument

- (DOMNode *)adoptNode:(DOMNode *)source
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _documentImpl]->adoptNode([source _nodeImpl], exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (DOMDocumentType *)doctype
{
    return static_cast<DOMDocumentType *>([DOMNode _nodeWithImpl:[self _documentImpl]->doctype()]);
}

- (DOMImplementation *)implementation
{
    return [DOMImplementation _DOMImplementationWithImpl:[self _documentImpl]->implementation()];
}

- (DOMElement *)documentElement
{
    return static_cast<DOMElement *>([DOMNode _nodeWithImpl:[self _documentImpl]->documentElement()]);
}

- (DOMElement *)createElement:(NSString *)tagName
{
    ASSERT(tagName);

    int exceptionCode = 0;
    DOMElement *result = static_cast<DOMElement *>([DOMNode _nodeWithImpl:[self _documentImpl]->createElement(tagName, exceptionCode)]);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (DOMDocumentFragment *)createDocumentFragment
{
    return static_cast<DOMDocumentFragment *>([DOMNode _nodeWithImpl:[self _documentImpl]->createDocumentFragment()]);
}

- (DOMText *)createTextNode:(NSString *)data
{
    ASSERT(data);
    return static_cast<DOMText *>([DOMNode _nodeWithImpl:[self _documentImpl]->createTextNode(data)]);
}

- (DOMComment *)createComment:(NSString *)data
{
    ASSERT(data);
    return static_cast<DOMComment *>([DOMNode _nodeWithImpl:[self _documentImpl]->createComment(data)]);
}

- (DOMCDATASection *)createCDATASection:(NSString *)data
{
    ASSERT(data);
    int exception = 0;
    DOMCDATASection *result = static_cast<DOMCDATASection *>([DOMNode _nodeWithImpl:[self _documentImpl]->createCDATASection(data, exception)]);
    raiseOnDOMError(exception);
    return result;
}

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data
{
    ASSERT(target);
    ASSERT(data);
    int exception = 0;
    DOMProcessingInstruction *result = static_cast<DOMProcessingInstruction *>([DOMNode _nodeWithImpl:[self _documentImpl]->createProcessingInstruction(target, data, exception)]);
    raiseOnDOMError(exception);
    return result;
}

- (DOMAttr *)createAttribute:(NSString *)name
{
    ASSERT(name);
    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWithImpl:[self _documentImpl]->createAttribute(name, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMEntityReference *)createEntityReference:(NSString *)name
{
    ASSERT(name);
    int exception = 0;
    DOMEntityReference *result = static_cast<DOMEntityReference *>([DOMNode _nodeWithImpl:[self _documentImpl]->createEntityReference(name, exception)]);
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagName:(NSString *)tagname
{
    ASSERT(tagname);
    return [DOMNodeList _nodeListWithImpl:[self _documentImpl]->getElementsByTagName(tagname).get()];
}

- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _documentImpl]->importNode([importedNode _nodeImpl], deep, exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _documentImpl]->createElementNS(namespaceURI, qualifiedName, exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return static_cast<DOMElement *>(result);
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWithImpl:[self _documentImpl]->createAttributeNS(namespaceURI, qualifiedName, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList _nodeListWithImpl:[self _documentImpl]->getElementsByTagNameNS(namespaceURI, localName).get()];
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    ASSERT(elementId);

    return static_cast<DOMElement *>([DOMNode _nodeWithImpl:[self _documentImpl]->getElementById(elementId)]);
}

@end

@implementation DOMDocument (DOMDocumentRange)

- (DOMRange *)createRange
{
    return [DOMRange _rangeWithImpl:[self _documentImpl]->createRange()];
}

@end

@implementation DOMDocument (DOMDocumentCSS)

- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)elt :(NSString *)pseudoElt
{
    ElementImpl *elementImpl = [elt _elementImpl];
    DOMString pseudoEltString(pseudoElt);
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:[self _documentImpl]->getOverrideStyle(elementImpl, pseudoEltString.impl())];
}

@end

@implementation DOMDocument (DOMDocumentStyle)

- (DOMStyleSheetList *)styleSheets
{
    return [DOMStyleSheetList _styleSheetListWithImpl:[self _documentImpl]->styleSheets()];
}

@end

@implementation DOMDocument (DOMDocumentExtensions)

- (DOMCSSStyleDeclaration *)createCSSStyleDeclaration
{
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:[self _documentImpl]->createCSSStyleDeclaration()];
}

@end

@implementation DOMDocument (WebCoreInternal)

+ (DOMDocument *)_documentWithImpl:(DocumentImpl *)impl
{
    return static_cast<DOMDocument *>([DOMNode _nodeWithImpl:impl]);
}

- (DocumentImpl *)_documentImpl
{
    return static_cast<DocumentImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMElement *)_ownerElement
{
    ElementImpl *element = [self _documentImpl]->ownerElement();
    return element ? [DOMElement _elementWithImpl:element] : nil;
}

@end

//------------------------------------------------------------------------------------------
// DOMCharacterData

@implementation DOMCharacterData

- (CharacterDataImpl *)_characterDataImpl
{
    return static_cast<CharacterDataImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)data
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return [self _characterDataImpl]->data();
}

- (void)setData:(NSString *)data
{
    ASSERT(data);
    
    int exceptionCode = 0;
    [self _characterDataImpl]->setData(data, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (unsigned)length
{
    return [self _characterDataImpl]->length();
}

- (NSString *)substringData:(unsigned)offset :(unsigned)count
{
    int exceptionCode = 0;
    NSString *result = [self _characterDataImpl]->substringData(offset, count, exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)appendData:(NSString *)arg
{
    ASSERT(arg);
    
    int exceptionCode = 0;
    [self _characterDataImpl]->appendData(arg, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)insertData:(unsigned)offset :(NSString *)arg
{
    ASSERT(arg);
    
    int exceptionCode = 0;
    [self _characterDataImpl]->insertData(offset, arg, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)deleteData:(unsigned)offset :(unsigned) count
{
    int exceptionCode = 0;
    [self _characterDataImpl]->deleteData(offset, count, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)replaceData:(unsigned)offset :(unsigned)count :(NSString *)arg
{
    ASSERT(arg);

    int exceptionCode = 0;
    [self _characterDataImpl]->replaceData(offset, count, arg, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

@end

//------------------------------------------------------------------------------------------
// DOMAttr

@implementation DOMAttr

- (NSString *)name
{
    return [self _attrImpl]->nodeName();
}

- (BOOL)specified
{
    return [self _attrImpl]->specified();
}

- (NSString *)value
{
    return [self _attrImpl]->nodeValue();
}

- (void)setValue:(NSString *)value
{
    ASSERT(value);

    int exceptionCode = 0;
    [self _attrImpl]->setValue(value, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (DOMElement *)ownerElement
{
    return [DOMElement _elementWithImpl:[self _attrImpl]->ownerElement()];
}

- (DOMCSSStyleDeclaration *)style
{
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl: [self _attrImpl]->style()];
}

@end

@implementation DOMAttr (WebCoreInternal)

+ (DOMAttr *)_attrWithImpl:(AttrImpl *)impl
{
    return static_cast<DOMAttr *>([DOMNode _nodeWithImpl:impl]);
}

- (AttrImpl *)_attrImpl
{
    return static_cast<AttrImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

@implementation DOMElement

- (NSString *)tagName
{
    return [self _elementImpl]->nodeName();
}

- (DOMNamedNodeMap *)attributes
{
    return [DOMNamedNodeMap _namedNodeMapWithImpl:[self _elementImpl]->attributes()];
}

- (NSString *)getAttribute:(NSString *)name
{
    ASSERT(name);
    return [self _elementImpl]->getAttribute(name);
}

- (void)setAttribute:(NSString *)name :(NSString *)value
{
    ASSERT(name);
    ASSERT(value);

    int exception = 0;
    [self _elementImpl]->setAttribute(name, value, exception);
    raiseOnDOMError(exception);
}

- (void)removeAttribute:(NSString *)name
{
    ASSERT(name);

    int exception = 0;
    [self _elementImpl]->removeAttribute(name, exception);
    raiseOnDOMError(exception);
}

- (DOMAttr *)getAttributeNode:(NSString *)name
{
    ASSERT(name);

    return [DOMAttr _attrWithImpl:[self _elementImpl]->getAttributeNode(name).get()];
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr
{
    ASSERT(newAttr);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWithImpl:[self _elementImpl]->setAttributeNode([newAttr _attrImpl], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr
{
    ASSERT(oldAttr);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWithImpl:[self _elementImpl]->removeAttributeNode([oldAttr _attrImpl], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    ASSERT(name);

    return [DOMNodeList _nodeListWithImpl:[self _elementImpl]->getElementsByTagName(name).get()];
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [self _elementImpl]->getAttributeNS(namespaceURI, localName);
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);
    ASSERT(value);

    int exception = 0;
    [self _elementImpl]->setAttributeNS(namespaceURI, qualifiedName, value, exception);
    raiseOnDOMError(exception);
}

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    int exception = 0;
    [self _elementImpl]->removeAttributeNS(namespaceURI, localName, exception);
    raiseOnDOMError(exception);
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMAttr _attrWithImpl:[self _elementImpl]->getAttributeNodeNS(namespaceURI, localName).get()];
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr
{
    ASSERT(newAttr);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWithImpl:[self _elementImpl]->setAttributeNodeNS([newAttr _attrImpl], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList _nodeListWithImpl:[self _elementImpl]->getElementsByTagNameNS(namespaceURI, localName).get()];
}

- (BOOL)hasAttribute:(NSString *)name
{
    ASSERT(name);

    return [self _elementImpl]->hasAttribute(name);
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [self _elementImpl]->hasAttributeNS(namespaceURI, localName);
}

@end

@implementation DOMElement (DOMElementCSSInlineStyle)

- (DOMCSSStyleDeclaration *)style
{
    return [DOMCSSStyleDeclaration _styleDeclarationWithImpl:[self _elementImpl]->style()];
}

@end

@implementation DOMElement (DOMElementExtensions)

- (void)focus
{
    [self _elementImpl]->focus();
}

- (void)blur
{
    [self _elementImpl]->blur();
}

- (void)scrollIntoView:(BOOL)alignTop
{
    [self _elementImpl]->scrollIntoView(alignTop);
}

- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded
{
    [self _elementImpl]->scrollIntoViewIfNeeded(centerIfNeeded);
}

@end

@implementation DOMElement (WebCoreInternal)

+ (DOMElement *)_elementWithImpl:(ElementImpl *)impl
{
    return static_cast<DOMElement *>([DOMNode _nodeWithImpl:impl]);
}

- (ElementImpl *)_elementImpl
{
    return static_cast<ElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

@implementation DOMElement (WebPrivate)

- (NSFont *)_font
{
    RenderObject *renderer = [self _elementImpl]->renderer();
    if (renderer) {
        return renderer->style()->font().getNSFont();
    }
    return nil;
}

- (NSImage*)_image
{
    RenderObject *renderer = [self _elementImpl]->renderer();
    if (renderer && renderer->isImage()) {
        RenderImage* img = static_cast<RenderImage*>(renderer);
        if (img->cachedImage() && !img->cachedImage()->isErrorImage())
            return img->cachedImage()->image()->getNSImage();
    }
    return nil;
}

- (NSData*)_imageTIFFRepresentation
{
    RenderObject *renderer = [self _elementImpl]->renderer();
    if (renderer && renderer->isImage()) {
        RenderImage* img = static_cast<RenderImage*>(renderer);
        if (img->cachedImage() && !img->cachedImage()->isErrorImage())
            return (NSData*)(img->cachedImage()->image()->getTIFFRepresentation());
    }
    return nil;
}

- (NSURL *)_getURLAttribute:(NSString *)name
{
    ASSERT(name);
    ElementImpl *e = [self _elementImpl];
    ASSERT(e);
    return KURL(e->getDocument()->completeURL(parseURL(e->getAttribute(name)).qstring())).getNSURL();
}

@end

//------------------------------------------------------------------------------------------
// DOMText

@implementation DOMText

- (TextImpl *)_textImpl
{
    return static_cast<TextImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMText *)splitText:(unsigned)offset
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _textImpl]->splitText(offset, exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return static_cast<DOMText *>(result);
}

@end

//------------------------------------------------------------------------------------------
// DOMComment

@implementation DOMComment

@end

//------------------------------------------------------------------------------------------
// DOMCDATASection

@implementation DOMCDATASection

@end

//------------------------------------------------------------------------------------------
// DOMDocumentType

@implementation DOMDocumentType

- (NSString *)name
{
    return [self _documentTypeImpl]->publicId();
}

- (DOMNamedNodeMap *)entities
{
    return [DOMNamedNodeMap _namedNodeMapWithImpl:[self _documentTypeImpl]->entities()];
}

- (DOMNamedNodeMap *)notations
{
    return [DOMNamedNodeMap _namedNodeMapWithImpl:[self _documentTypeImpl]->notations()];
}

- (NSString *)publicId
{
    return [self _documentTypeImpl]->publicId();
}

- (NSString *)systemId
{
    return [self _documentTypeImpl]->systemId();
}

- (NSString *)internalSubset
{
    return [self _documentTypeImpl]->internalSubset();
}

@end

@implementation DOMDocumentType (WebCoreInternal)

- (DocumentTypeImpl *)_documentTypeImpl
{
    return static_cast<DocumentTypeImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMNotation

@implementation DOMNotation

- (NotationImpl *)_notationImpl
{
    return static_cast<NotationImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)publicId
{
    return [self _notationImpl]->publicId();
}

- (NSString *)systemId
{
    return [self _notationImpl]->systemId();
}

@end

//------------------------------------------------------------------------------------------
// DOMEntity

@implementation DOMEntity

- (EntityImpl *)_entityImpl
{
    return static_cast<EntityImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)publicId
{
    return [self _entityImpl]->publicId();
}

- (NSString *)systemId
{
    return [self _entityImpl]->systemId();
}

- (NSString *)notationName
{
    return [self _entityImpl]->notationName();
}

@end

//------------------------------------------------------------------------------------------
// DOMEntityReference

@implementation DOMEntityReference

@end

//------------------------------------------------------------------------------------------
// DOMProcessingInstruction

@implementation DOMProcessingInstruction

- (ProcessingInstructionImpl *)_processingInstructionImpl
{
    return static_cast<ProcessingInstructionImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)target
{
    return [self _processingInstructionImpl]->target();
}

- (NSString *)data
{
    return [self _processingInstructionImpl]->data();
}

- (void)setData:(NSString *)data
{
    ASSERT(data);

    int exceptionCode = 0;
    [self _processingInstructionImpl]->setData(data, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

@end

//------------------------------------------------------------------------------------------
// DOMRange

@implementation DOMRange

- (void)dealloc
{
    if (_internal) {
        DOM_cast<RangeImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<RangeImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (NSString *)description
{
    if (!_internal)
        return @"<DOMRange: null>";
    return [NSString stringWithFormat:@"<DOMRange: %@ %d %@ %d>",
        [self startContainer], [self startOffset],
        [self endContainer], [self endOffset]];
}

- (DOMNode *)startContainer
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _rangeImpl]->startContainer(exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (int)startOffset
{
    int exceptionCode = 0;
    int result = [self _rangeImpl]->startOffset(exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (DOMNode *)endContainer
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _rangeImpl]->endContainer(exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (int)endOffset
{
    int exceptionCode = 0;
    int result = [self _rangeImpl]->endOffset(exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (BOOL)collapsed
{
    int exceptionCode = 0;
    BOOL result = [self _rangeImpl]->collapsed(exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (DOMNode *)commonAncestorContainer
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _rangeImpl]->commonAncestorContainer(exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)setStart:(DOMNode *)refNode :(int)offset
{
    int exceptionCode = 0;
    [self _rangeImpl]->setStart([refNode _nodeImpl], offset, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)setEnd:(DOMNode *)refNode :(int)offset
{
    int exceptionCode = 0;
    [self _rangeImpl]->setEnd([refNode _nodeImpl], offset, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)setStartBefore:(DOMNode *)refNode
{
    int exceptionCode = 0;
    [self _rangeImpl]->setStartBefore([refNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)setStartAfter:(DOMNode *)refNode
{
    int exceptionCode = 0;
    [self _rangeImpl]->setStartAfter([refNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)setEndBefore:(DOMNode *)refNode
{
    int exceptionCode = 0;
    [self _rangeImpl]->setEndBefore([refNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)setEndAfter:(DOMNode *)refNode
{
    int exceptionCode = 0;
    [self _rangeImpl]->setEndAfter([refNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)collapse:(BOOL)toStart
{
    int exceptionCode = 0;
    [self _rangeImpl]->collapse(toStart, exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)selectNode:(DOMNode *)refNode
{
    int exceptionCode = 0;
    [self _rangeImpl]->selectNode([refNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)selectNodeContents:(DOMNode *)refNode
{
    int exceptionCode = 0;
    [self _rangeImpl]->selectNodeContents([refNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange
{
    int exceptionCode = 0;
    short result = [self _rangeImpl]->compareBoundaryPoints(static_cast<Range::CompareHow>(how), [sourceRange _rangeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)deleteContents
{
    int exceptionCode = 0;
    [self _rangeImpl]->deleteContents(exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (DOMDocumentFragment *)extractContents
{
    int exceptionCode = 0;
    DOMDocumentFragment *result = [DOMDocumentFragment _documentFragmentWithImpl:[self _rangeImpl]->extractContents(exceptionCode).get()];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (DOMDocumentFragment *)cloneContents
{
    int exceptionCode = 0;
    DOMDocumentFragment *result = [DOMDocumentFragment _documentFragmentWithImpl:[self _rangeImpl]->cloneContents(exceptionCode).get()];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)insertNode:(DOMNode *)newNode
{
    int exceptionCode = 0;
    [self _rangeImpl]->insertNode([newNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)surroundContents:(DOMNode *)newParent
{
    int exceptionCode = 0;
    [self _rangeImpl]->surroundContents([newParent _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (DOMRange *)cloneRange
{
    int exceptionCode = 0;
    DOMRange *result = [DOMRange _rangeWithImpl:[self _rangeImpl]->cloneRange(exceptionCode).get()];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (NSString *)toString
{
    int exceptionCode = 0;
    NSString *result = [self _rangeImpl]->toString(exceptionCode);
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)detach
{
    int exceptionCode = 0;
    [self _rangeImpl]->detach(exceptionCode);
    raiseOnDOMError(exceptionCode);
}

@end

@implementation DOMRange (WebCoreInternal)

- (id)_initWithRangeImpl:(RangeImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMRange *)_rangeWithImpl:(RangeImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithRangeImpl:impl] autorelease];
}

- (RangeImpl *)_rangeImpl
{
    return DOM_cast<RangeImpl *>(_internal);
}

@end

@implementation DOMRange (WebPrivate)

- (NSString *)_text
{
    return [self _rangeImpl]->text().qstring().getNSString();
}

@end

//------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------

@implementation DOMNodeFilter

- (id)_initWithNodeFilterImpl:(NodeFilterImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNodeFilter *)_nodeFilterWithImpl:(NodeFilterImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNodeFilterImpl:impl] autorelease];
}

- (NodeFilterImpl *)_nodeFilterImpl
{
    return DOM_cast<NodeFilterImpl *>(_internal);
}

- (void)dealloc
{
    if (_internal)
        DOM_cast<NodeFilterImpl *>(_internal)->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        DOM_cast<NodeFilterImpl *>(_internal)->deref();
    [super finalize];
}

- (short)acceptNode:(DOMNode *)node
{
    return [self _nodeFilterImpl]->acceptNode([node _nodeImpl]);
}

@end


@implementation DOMNodeIterator

- (id)_initWithNodeIteratorImpl:(NodeIteratorImpl *)impl filter:(id <DOMNodeFilter>)filter
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    m_filter = [filter retain];
    return self;
}

- (NodeIteratorImpl *)_nodeIteratorImpl
{
    return DOM_cast<NodeIteratorImpl *>(_internal);
}

- (void)dealloc
{
    [m_filter release];
    if (_internal) {
        [self detach];
        DOM_cast<NodeIteratorImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        [self detach];
        DOM_cast<NodeIteratorImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (DOMNode *)root
{
    return [DOMNode _nodeWithImpl:[self _nodeIteratorImpl]->root()];
}

- (unsigned)whatToShow
{
    return [self _nodeIteratorImpl]->whatToShow();
}

- (id <DOMNodeFilter>)filter
{
    if (m_filter)
        // This node iterator was created from the objc side
        return [[m_filter retain] autorelease];

    // This node iterator was created from the c++ side
    return [DOMNodeFilter _nodeFilterWithImpl:[self _nodeIteratorImpl]->filter()];
}

- (BOOL)expandEntityReferences
{
    return [self _nodeIteratorImpl]->expandEntityReferences();
}

- (DOMNode *)nextNode
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _nodeIteratorImpl]->nextNode(exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (DOMNode *)previousNode
{
    int exceptionCode = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _nodeIteratorImpl]->previousNode(exceptionCode)];
    raiseOnDOMError(exceptionCode);
    return result;
}

- (void)detach
{
    int exceptionCode = 0;
    [self _nodeIteratorImpl]->detach(exceptionCode);
    raiseOnDOMError(exceptionCode);
}

@end

@implementation DOMNodeIterator(WebCoreInternal)

+ (DOMNodeIterator *)_nodeIteratorWithImpl:(NodeIteratorImpl *)impl filter:(id <DOMNodeFilter>)filter
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNodeIteratorImpl:impl filter:filter] autorelease];
}

@end

@implementation DOMTreeWalker

- (id)_initWithTreeWalkerImpl:(TreeWalkerImpl *)impl filter:(id <DOMNodeFilter>)filter
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    m_filter = [filter retain];
    return self;
}

- (TreeWalkerImpl *)_treeWalkerImpl
{
    return DOM_cast<TreeWalkerImpl *>(_internal);
}

- (void)dealloc
{
    if (m_filter)
        [m_filter release];
    if (_internal) {
        DOM_cast<TreeWalkerImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<TreeWalkerImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (DOMNode *)root
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->root()];
}

- (unsigned)whatToShow
{
    return [self _treeWalkerImpl]->whatToShow();
}

- (id <DOMNodeFilter>)filter
{
    if (m_filter)
        // This tree walker was created from the objc side
        return [[m_filter retain] autorelease];

    // This tree walker was created from the c++ side
    return [DOMNodeFilter _nodeFilterWithImpl:[self _treeWalkerImpl]->filter()];
}

- (BOOL)expandEntityReferences
{
    return [self _treeWalkerImpl]->expandEntityReferences();
}

- (DOMNode *)currentNode
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->currentNode()];
}

- (void)setCurrentNode:(DOMNode *)currentNode
{
    int exceptionCode = 0;
    [self _treeWalkerImpl]->setCurrentNode([currentNode _nodeImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (DOMNode *)parentNode
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->parentNode()];
}

- (DOMNode *)firstChild
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->firstChild()];
}

- (DOMNode *)lastChild
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->lastChild()];
}

- (DOMNode *)previousSibling
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->previousSibling()];
}

- (DOMNode *)nextSibling
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->nextSibling()];
}

- (DOMNode *)previousNode
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->previousNode()];
}

- (DOMNode *)nextNode
{
    return [DOMNode _nodeWithImpl:[self _treeWalkerImpl]->nextNode()];
}

@end

@implementation DOMTreeWalker (WebCoreInternal)

+ (DOMTreeWalker *)_treeWalkerWithImpl:(TreeWalkerImpl *)impl filter:(id <DOMNodeFilter>)filter
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithTreeWalkerImpl:impl filter:filter] autorelease];
}

@end

class ObjCNodeFilterCondition : public NodeFilterCondition 
{
public:
    ObjCNodeFilterCondition(id <DOMNodeFilter>);
    virtual ~ObjCNodeFilterCondition();
    virtual short acceptNode(NodeImpl*) const;

private:
    ObjCNodeFilterCondition(const ObjCNodeFilterCondition &);
    ObjCNodeFilterCondition &operator=(const ObjCNodeFilterCondition &);

    id <DOMNodeFilter> m_filter;
};

ObjCNodeFilterCondition::ObjCNodeFilterCondition(id <DOMNodeFilter> filter)
    : m_filter(filter)
{
    ASSERT(m_filter);
    CFRetain(m_filter);
}

ObjCNodeFilterCondition::~ObjCNodeFilterCondition()
{
    CFRelease(m_filter);
}

short ObjCNodeFilterCondition::acceptNode(NodeImpl* node) const
{
    if (!node)
        return NodeFilter::FILTER_REJECT;
    return [m_filter acceptNode:[DOMNode _nodeWithImpl:node]];
}

@implementation DOMDocument (DOMDocumentTraversal)

- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    NodeFilterImpl *cppFilter = 0;
    if (filter) {
        cppFilter = new NodeFilterImpl(new ObjCNodeFilterCondition(filter));
        cppFilter->ref();
    }
    int exceptionCode = 0;
    NodeIteratorImpl *impl = [self _documentImpl]->createNodeIterator([root _nodeImpl], whatToShow, cppFilter, expandEntityReferences, exceptionCode);
    if (cppFilter) {
        cppFilter->deref();
    }
    raiseOnDOMError(exceptionCode);
    return [DOMNodeIterator _nodeIteratorWithImpl:impl filter:filter];
}

- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    NodeFilterImpl *cppFilter = 0;
    if (filter) {
        cppFilter = new NodeFilterImpl(new ObjCNodeFilterCondition(filter));
        cppFilter->ref();
    }
    int exceptionCode = 0;
    TreeWalkerImpl *impl = [self _documentImpl]->createTreeWalker([root _nodeImpl], whatToShow, cppFilter, expandEntityReferences, exceptionCode);
    if (cppFilter) {
        cppFilter->deref();
    }
    raiseOnDOMError(exceptionCode);
    return [DOMTreeWalker _treeWalkerWithImpl:impl filter:filter];
}

@end

ObjCEventListener *ObjCEventListener::find(id <DOMEventListener> listener)
{
    if (ListenerMap *map = listenerMap)
        return map->get(listener);
    return NULL;
}

ObjCEventListener *ObjCEventListener::create(id <DOMEventListener> listener)
{
    ObjCEventListener *wrapper = find(listener);
    if (!wrapper)
        wrapper = new ObjCEventListener(listener);
    wrapper->ref();
    return wrapper;
}

ObjCEventListener::ObjCEventListener(id <DOMEventListener> listener)
    : m_listener([listener retain])
{
    ListenerMap *map = listenerMap;
    if (!map) {
        map = new ListenerMap;
        listenerMap = map;
    }
    map->set(listener, this);
}

ObjCEventListener::~ObjCEventListener()
{
    listenerMap->remove(m_listener);
    [m_listener release];
}

void ObjCEventListener::handleEvent(EventImpl *event, bool)
{
    [m_listener handleEvent:[DOMEvent _eventWithImpl:event]];
}
