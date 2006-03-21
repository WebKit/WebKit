/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "CDATASection.h"
#import "Comment.h"
#import "ContainerNode.h"
#import "DOMEventsInternal.h"
#import "DOMHTML.h"
#import "DOMImplementationFront.h"
#import "DOMInternal.h"
#import "DOMPrivate.h"
#import "DocumentFragment.h"
#import "Document.h"
#import "DocumentType.h"
#import "EventListener.h"
#import "FoundationExtras.h"
#import "HTMLElement.h"
#import "FrameMac.h"
#import "NodeList.h"
#import "PlatformString.h"
#import "StringImpl.h"
#import "css_stylesheetimpl.h"
#import "csshelper.h"
#import "Range.h"
#import "dom2_traversalimpl.h"
#import "AbstractView.h"
#import "dom_elementimpl.h"
#import "dom_xmlimpl.h"
#import "HTMLNames.h"
#import "RenderImage.h"
#import <JavaScriptCore/WebScriptObjectPrivate.h>
#import <kxmlcore/Assertions.h>
#import <kxmlcore/HashMap.h>
#import <objc/objc-class.h>

using WebCore::Attr;
using WebCore::CharacterData;
using WebCore::Document;
using WebCore::DocumentFragment;
using WebCore::DocumentType;
using WebCore::DOMImplementationFront;
using WebCore::Element;
using WebCore::Entity;
using WebCore::Event;
using WebCore::EventListener;
using WebCore::ExceptionCode;
using WebCore::HTMLElement;
using WebCore::FrameMac;
using WebCore::NamedNodeMap;
using WebCore::Node;
using WebCore::NodeFilter;
using WebCore::NodeFilterCondition;
using WebCore::NodeIterator;
using WebCore::NodeList;
using WebCore::Notation;
using WebCore::ProcessingInstruction;
using WebCore::Range;
using WebCore::RenderImage;
using WebCore::RenderObject;
using WebCore::String;
using WebCore::Text;
using WebCore::TreeWalker;

using namespace WebCore::HTMLNames;

@interface DOMAttr (WebCoreInternal)
+ (DOMAttr *)_attrWith:(Attr *)impl;
- (Attr *)_attr;
@end

@interface DOMDocumentType (WebCoreInternal)
- (DocumentType *)_documentType;
@end

@interface DOMImplementation (WebCoreInternal)
+ (DOMImplementation *)_DOMImplementationWith:(DOMImplementationFront *)impl;
- (DOMImplementationFront *)_DOMImplementation;
@end

@interface DOMNamedNodeMap (WebCoreInternal)
+ (DOMNamedNodeMap *)_namedNodeMapWith:(NamedNodeMap *)impl;
@end

class ObjCEventListener : public EventListener {
public:
    static ObjCEventListener *find(id <DOMEventListener>);
    static ObjCEventListener *create(id <DOMEventListener>);

private:
    ObjCEventListener(id <DOMEventListener>);
    virtual ~ObjCEventListener();

    virtual void handleEvent(Event *, bool isWindowEvent);

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
    [NSException raise:NSGenericException format:@"+[%@ init]: should never be used", NSStringFromClass([self class])];
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
        DOM_cast<Node *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<Node *>(_internal)->deref();
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
    return [self _node]->nodeName();
}

- (NSString *)nodeValue
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return [self _node]->nodeValue();
}

- (void)setNodeValue:(NSString *)string
{
    ASSERT(string);
    
    ExceptionCode ec = 0;
    [self _node]->setNodeValue(string, ec);
    raiseOnDOMError(ec);
}

- (unsigned short)nodeType
{
    return [self _node]->nodeType();
}

- (DOMNode *)parentNode
{
    return [DOMNode _nodeWith:[self _node]->parentNode()];
}

- (DOMNodeList *)childNodes
{
    return [DOMNodeList _nodeListWith:[self _node]->childNodes().get()];
}

- (DOMNode *)firstChild
{
    return [DOMNode _nodeWith:[self _node]->firstChild()];
}

- (DOMNode *)lastChild
{
    return [DOMNode _nodeWith:[self _node]->lastChild()];
}

- (DOMNode *)previousSibling
{
    return [DOMNode _nodeWith:[self _node]->previousSibling()];
}

- (DOMNode *)nextSibling
{
    return [DOMNode _nodeWith:[self _node]->nextSibling()];
}

- (DOMNamedNodeMap *)attributes
{
    // DOM level 2 core specification says: 
    // A NamedNodeMap containing the attributes of this node (if it is an Element) or null otherwise.
    return nil;
}

- (DOMDocument *)ownerDocument
{
    return [DOMDocument _documentWith:[self _node]->getDocument()];
}

- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild
{
    ASSERT(newChild);
    ASSERT(refChild);

    ExceptionCode ec = 0;
    if ([self _node]->insertBefore([newChild _node], [refChild _node], ec))
        return newChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild
{
    ASSERT(newChild);
    ASSERT(oldChild);

    ExceptionCode ec = 0;
    if ([self _node]->replaceChild([newChild _node], [oldChild _node], ec))
        return oldChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    ASSERT(oldChild);

    ExceptionCode ec = 0;
    if ([self _node]->removeChild([oldChild _node], ec))
        return oldChild;
    raiseOnDOMError(ec);
    return nil;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    ASSERT(newChild);

    ExceptionCode ec = 0;
    if ([self _node]->appendChild([newChild _node], ec))
        return newChild;
    raiseOnDOMError(ec);
    return nil;
}

- (BOOL)hasChildNodes
{
    return [self _node]->hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    return [DOMNode _nodeWith:[self _node]->cloneNode(deep).get()];
}

- (void)normalize
{
    [self _node]->normalize();
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return [self _node]->isSupported(feature, version);
}

- (NSString *)namespaceURI
{
    return [self _node]->namespaceURI();
}

- (NSString *)prefix
{
    return [self _node]->prefix();
}

- (void)setPrefix:(NSString *)prefix
{
    ASSERT(prefix);

    ExceptionCode ec = 0;
    String prefixStr(prefix);
    [self _node]->setPrefix(prefixStr.impl(), ec);
    raiseOnDOMError(ec);
}

- (NSString *)localName
{
    return [self _node]->localName();
}

- (BOOL)hasAttributes
{
    return [self _node]->hasAttributes();
}

- (BOOL)isSameNode:(DOMNode *)other
{
    return [self _node]->isSameNode([other _node]);
}

- (BOOL)isEqualNode:(DOMNode *)other
{
    return [self _node]->isEqualNode([other _node]);
}

- (BOOL)isDefaultNamespace:(NSString *)namespaceURI
{
    return [self _node]->isDefaultNamespace(namespaceURI);
}

- (NSString *)lookupPrefix:(NSString *)namespaceURI
{
    return [self _node]->lookupPrefix(namespaceURI);
}

- (NSString *)lookupNamespaceURI:(NSString *)prefix
{
    return [self _node]->lookupNamespaceURI(prefix);
}

- (NSString *)textContent
{
    return [self _node]->textContent();
}

- (void)setTextContent:(NSString *)text
{
    ExceptionCode ec = 0;
    [self _node]->setTextContent(text, ec);
    raiseOnDOMError(ec);
}

- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    if (![self _node]->isEventTargetNode())
        raiseDOMException(DOM_NOT_SUPPORTED_ERR);
    
    EventListener *wrapper = ObjCEventListener::create(listener);
    EventTargetNodeCast([self _node])->addEventListener(type, wrapper, useCapture);
    wrapper->deref();
}

- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture
{
    if (![self _node]->isEventTargetNode())
        raiseDOMException(DOM_NOT_SUPPORTED_ERR);

    if (EventListener *wrapper = ObjCEventListener::find(listener))
        EventTargetNodeCast([self _node])->removeEventListener(type, wrapper, useCapture);
}

- (BOOL)dispatchEvent:(DOMEvent *)event
{
    if (![self _node]->isEventTargetNode())
        raiseDOMException(DOM_NOT_SUPPORTED_ERR);

    ExceptionCode ec = 0;
    BOOL result = EventTargetNodeCast([self _node])->dispatchEvent([event _event], ec);
    raiseOnDOMError(ec);
    return result;
}

- (NSRect)boundingBox
{
    WebCore::RenderObject *renderer = [self _node]->renderer();
    if (renderer)
        return renderer->absoluteBoundingBoxRect();
    return NSZeroRect;
}

- (NSArray *)lineBoxRects
{
    WebCore::RenderObject *renderer = [self _node]->renderer();
    if (renderer) {
        NSMutableArray *results = [[NSMutableArray alloc] init];
        DeprecatedValueList<IntRect> rects = renderer->lineBoxRects();
        if (!rects.isEmpty()) {
            for (DeprecatedValueList<IntRect>::ConstIterator it = rects.begin(); it != rects.end(); ++it)
                [results addObject:[NSValue valueWithRect:*it]];
        }
        return [results autorelease];
    }
    return nil;
}

@end

@implementation DOMNode (WebCoreInternal)

- (id)_initWithNode:(Node *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNode *)_nodeWith:(Node *)impl
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
                HTMLElement* htmlElt = static_cast<HTMLElement*>(impl);
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
                else if (htmlElt->hasLocalName(preTag) || htmlElt->hasLocalName(listingTag))
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
            if (static_cast<Document *>(impl)->isHTMLDocument()) {
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
    return [[[wrapperClass alloc] _initWithNode:impl] autorelease];
}

- (Node *)_node
{
    return DOM_cast<Node *>(_internal);
}

- (BOOL)isContentEditable
{
    return [self _node]->isContentEditable();
}

- (const KJS::Bindings::RootObject *)_executionContext
{
    if (Node *n = [self _node])
        if (FrameMac *f = Mac(n->getDocument()->frame()))
            return f->executionContextForDOM();

    return 0;
}

@end

//------------------------------------------------------------------------------------------
// DOMNamedNodeMap

@implementation DOMNamedNodeMap

- (void)dealloc
{
    if (_internal) {
        DOM_cast<NamedNodeMap *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<NamedNodeMap *>(_internal)->deref();
    }
    [super finalize];
}

- (NamedNodeMap *)_namedNodeMap
{
    return DOM_cast<NamedNodeMap *>(_internal);
}

- (DOMNode *)getNamedItem:(NSString *)name
{
    ASSERT(name);

    return [DOMNode _nodeWith:[self _namedNodeMap]->getNamedItem(name).get()];
}

- (DOMNode *)setNamedItem:(DOMNode *)arg
{
    ASSERT(arg);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _namedNodeMap]->setNamedItem([arg _node], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNode *)removeNamedItem:(NSString *)name
{
    ASSERT(name);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _namedNodeMap]->removeNamedItem(name, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNode *)item:(unsigned)index
{
    return [DOMNode _nodeWith:[self _namedNodeMap]->item(index).get()];
}

- (unsigned)length
{
    return [self _namedNodeMap]->length();
}

- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    if (!namespaceURI || !localName) {
        return nil;
    }

    return [DOMNode _nodeWith:[self _namedNodeMap]->getNamedItemNS(namespaceURI, localName).get()];
}

- (DOMNode *)setNamedItemNS:(DOMNode *)arg
{
    ASSERT(arg);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _namedNodeMap]->setNamedItemNS([arg _node], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    int exception = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _namedNodeMap]->removeNamedItemNS(namespaceURI, localName, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

@end

@implementation DOMNamedNodeMap (WebCoreInternal)

- (id)_initWithNamedNodeMap:(NamedNodeMap *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNamedNodeMap *)_namedNodeMapWith:(NamedNodeMap *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNamedNodeMap:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMNodeList

@implementation DOMNodeList

- (void)dealloc
{
    if (_internal) {
        DOM_cast<NodeList *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<NodeList *>(_internal)->deref();
    }
    [super finalize];
}

- (NodeList *)_nodeList
{
    return DOM_cast<NodeList *>(_internal);
}

- (DOMNode *)item:(unsigned)index
{
    return [DOMNode _nodeWith:[self _nodeList]->item(index)];
}

- (unsigned)length
{
    return [self _nodeList]->length();
}

@end

@implementation DOMNodeList (WebCoreInternal)

- (id)_initWithNodeList:(NodeList *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNodeList *)_nodeListWith:(NodeList *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNodeList:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMImplementation

@implementation DOMImplementation

- (void)dealloc
{
    if (_internal)
        DOM_cast<DOMImplementationFront *>(_internal)->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        DOM_cast<DOMImplementationFront *>(_internal)->deref();
    [super finalize];
}

- (BOOL)hasFeature:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return [self _DOMImplementation]->hasFeature(feature, version);
}

- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId
{
    ASSERT(qualifiedName);
    ASSERT(publicId);
    ASSERT(systemId);

    ExceptionCode ec = 0;
    RefPtr<DocumentType> impl = [self _DOMImplementation]->createDocumentType(qualifiedName, publicId, systemId, ec);
    raiseOnDOMError(ec);
    return static_cast<DOMDocumentType *>([DOMNode _nodeWith:impl.get()]);
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    ExceptionCode ec = 0;
    RefPtr<Document> impl = [self _DOMImplementation]->createDocument(namespaceURI, qualifiedName, [doctype _documentType], ec);
    raiseOnDOMError(ec);
    return static_cast<DOMDocument *>([DOMNode _nodeWith:impl.get()]);
}

@end

@implementation DOMImplementation (DOMImplementationCSS)

- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title :(NSString *)media
{
    ASSERT(title);
    ASSERT(media);

    ExceptionCode ec = 0;
    DOMCSSStyleSheet *result = [DOMCSSStyleSheet _CSSStyleSheetWith:[self _DOMImplementation]->createCSSStyleSheet(title, media, ec).get()];
    raiseOnDOMError(ec);
    return result;
}

@end
 
@implementation DOMImplementation (WebCoreInternal)

- (id)_initWithDOMImplementation:(DOMImplementationFront *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMImplementation *)_DOMImplementationWith:(DOMImplementationFront *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithDOMImplementation:impl] autorelease];
}

- (DOMImplementationFront *)_DOMImplementation
{
    return DOM_cast<DOMImplementationFront *>(_internal);
}

@end

//------------------------------------------------------------------------------------------
// DOMDocumentFragment

@implementation DOMDocumentFragment

@end

@implementation DOMDocumentFragment (WebCoreInternal)

+ (DOMDocumentFragment *)_documentFragmentWith:(DocumentFragment *)impl
{
    return static_cast<DOMDocumentFragment *>([DOMNode _nodeWith:impl]);
}

- (DocumentFragment *)_fragment
{
    return static_cast<DocumentFragment *>(DOM_cast<Node *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMDocument

@implementation DOMDocument

- (DOMNode *)adoptNode:(DOMNode *)source
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _document]->adoptNode([source _node], ec).get()];
    raiseOnDOMError(ec);
    return result;
}

- (DOMDocumentType *)doctype
{
    return static_cast<DOMDocumentType *>([DOMNode _nodeWith:[self _document]->doctype()]);
}

- (DOMImplementation *)implementation
{
    return [DOMImplementation _DOMImplementationWith:implementationFront([self _document])];
}

- (DOMElement *)documentElement
{
    return static_cast<DOMElement *>([DOMNode _nodeWith:[self _document]->documentElement()]);
}

- (DOMElement *)createElement:(NSString *)tagName
{
    ASSERT(tagName);

    ExceptionCode ec = 0;
    DOMElement *result = static_cast<DOMElement *>([DOMNode _nodeWith:[self _document]->createElement(tagName, ec).get()]);
    raiseOnDOMError(ec);
    return result;
}

- (DOMDocumentFragment *)createDocumentFragment
{
    return static_cast<DOMDocumentFragment *>([DOMNode _nodeWith:[self _document]->createDocumentFragment().get()]);
}

- (DOMText *)createTextNode:(NSString *)data
{
    ASSERT(data);
    return static_cast<DOMText *>([DOMNode _nodeWith:[self _document]->createTextNode(data).get()]);
}

- (DOMComment *)createComment:(NSString *)data
{
    ASSERT(data);
    return static_cast<DOMComment *>([DOMNode _nodeWith:[self _document]->createComment(data).get()]);
}

- (DOMCDATASection *)createCDATASection:(NSString *)data
{
    ASSERT(data);
    int exception = 0;
    DOMCDATASection *result = static_cast<DOMCDATASection *>([DOMNode _nodeWith:[self _document]->createCDATASection(data, exception).get()]);
    raiseOnDOMError(exception);
    return result;
}

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data
{
    ASSERT(target);
    ASSERT(data);
    int exception = 0;
    DOMProcessingInstruction *result = static_cast<DOMProcessingInstruction *>([DOMNode _nodeWith:[self _document]->createProcessingInstruction(target, data, exception).get()]);
    raiseOnDOMError(exception);
    return result;
}

- (DOMAttr *)createAttribute:(NSString *)name
{
    ASSERT(name);
    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWith:[self _document]->createAttribute(name, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMEntityReference *)createEntityReference:(NSString *)name
{
    ASSERT(name);
    int exception = 0;
    DOMEntityReference *result = static_cast<DOMEntityReference *>([DOMNode _nodeWith:[self _document]->createEntityReference(name, exception).get()]);
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagName:(NSString *)tagname
{
    ASSERT(tagname);
    return [DOMNodeList _nodeListWith:[self _document]->getElementsByTagName(tagname).get()];
}

- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _document]->importNode([importedNode _node], deep, ec).get()];
    raiseOnDOMError(ec);
    return result;
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _document]->createElementNS(namespaceURI, qualifiedName, ec).get()];
    raiseOnDOMError(ec);
    return static_cast<DOMElement *>(result);
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWith:[self _document]->createAttributeNS(namespaceURI, qualifiedName, exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList _nodeListWith:[self _document]->getElementsByTagNameNS(namespaceURI, localName).get()];
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    ASSERT(elementId);

    return static_cast<DOMElement *>([DOMNode _nodeWith:[self _document]->getElementById(elementId)]);
}

@end

@implementation DOMDocument (DOMDocumentRange)

- (DOMRange *)createRange
{
    return [DOMRange _rangeWith:[self _document]->createRange().get()];
}

@end

@implementation DOMDocument (DOMDocumentCSS)

- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)elt :(NSString *)pseudoElt
{
    Element *element = [elt _element];
    String pseudoEltString(pseudoElt);
    return [DOMCSSStyleDeclaration _styleDeclarationWith:[self _document]->getOverrideStyle(element, pseudoEltString.impl())];
}

@end

@implementation DOMDocument (DOMDocumentStyle)

- (DOMStyleSheetList *)styleSheets
{
    return [DOMStyleSheetList _styleSheetListWith:[self _document]->styleSheets()];
}

@end

@implementation DOMDocument (DOMDocumentExtensions)

- (DOMCSSStyleDeclaration *)createCSSStyleDeclaration
{
    return [DOMCSSStyleDeclaration _styleDeclarationWith:[self _document]->createCSSStyleDeclaration().get()];
}

@end

@implementation DOMDocument (WebCoreInternal)

+ (DOMDocument *)_documentWith:(Document *)impl
{
    return static_cast<DOMDocument *>([DOMNode _nodeWith:impl]);
}

- (Document *)_document
{
    return static_cast<Document *>(DOM_cast<Node *>(_internal));
}

- (DOMElement *)_ownerElement
{
    Element *element = [self _document]->ownerElement();
    return element ? [DOMElement _elementWith:element] : nil;
}

@end

//------------------------------------------------------------------------------------------
// DOMCharacterData

@implementation DOMCharacterData

- (CharacterData *)_characterData
{
    return static_cast<CharacterData *>(DOM_cast<Node *>(_internal));
}

- (NSString *)data
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return [self _characterData]->data();
}

- (void)setData:(NSString *)data
{
    ASSERT(data);
    
    ExceptionCode ec = 0;
    [self _characterData]->setData(data, ec);
    raiseOnDOMError(ec);
}

- (unsigned)length
{
    return [self _characterData]->length();
}

- (NSString *)substringData:(unsigned)offset :(unsigned)count
{
    ExceptionCode ec = 0;
    NSString *result = [self _characterData]->substringData(offset, count, ec);
    raiseOnDOMError(ec);
    return result;
}

- (void)appendData:(NSString *)arg
{
    ASSERT(arg);
    
    ExceptionCode ec = 0;
    [self _characterData]->appendData(arg, ec);
    raiseOnDOMError(ec);
}

- (void)insertData:(unsigned)offset :(NSString *)arg
{
    ASSERT(arg);
    
    ExceptionCode ec = 0;
    [self _characterData]->insertData(offset, arg, ec);
    raiseOnDOMError(ec);
}

- (void)deleteData:(unsigned)offset :(unsigned) count
{
    ExceptionCode ec = 0;
    [self _characterData]->deleteData(offset, count, ec);
    raiseOnDOMError(ec);
}

- (void)replaceData:(unsigned)offset :(unsigned)count :(NSString *)arg
{
    ASSERT(arg);

    ExceptionCode ec = 0;
    [self _characterData]->replaceData(offset, count, arg, ec);
    raiseOnDOMError(ec);
}

@end

//------------------------------------------------------------------------------------------
// DOMAttr

@implementation DOMAttr

- (NSString *)name
{
    return [self _attr]->nodeName();
}

- (BOOL)specified
{
    return [self _attr]->specified();
}

- (NSString *)value
{
    return [self _attr]->nodeValue();
}

- (void)setValue:(NSString *)value
{
    ASSERT(value);

    ExceptionCode ec = 0;
    [self _attr]->setValue(value, ec);
    raiseOnDOMError(ec);
}

- (DOMElement *)ownerElement
{
    return [DOMElement _elementWith:[self _attr]->ownerElement()];
}

- (DOMCSSStyleDeclaration *)style
{
    return [DOMCSSStyleDeclaration _styleDeclarationWith: [self _attr]->style()];
}

@end

@implementation DOMAttr (WebCoreInternal)

+ (DOMAttr *)_attrWith:(Attr *)impl
{
    return static_cast<DOMAttr *>([DOMNode _nodeWith:impl]);
}

- (Attr *)_attr
{
    return static_cast<Attr *>(DOM_cast<Node *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

@implementation DOMElement

- (NSString *)tagName
{
    return [self _element]->nodeName();
}

- (DOMNamedNodeMap *)attributes
{
    return [DOMNamedNodeMap _namedNodeMapWith:[self _element]->attributes()];
}

- (NSString *)getAttribute:(NSString *)name
{
    ASSERT(name);
    return [self _element]->getAttribute(name);
}

- (void)setAttribute:(NSString *)name :(NSString *)value
{
    ASSERT(name);
    ASSERT(value);

    int exception = 0;
    [self _element]->setAttribute(name, value, exception);
    raiseOnDOMError(exception);
}

- (void)removeAttribute:(NSString *)name
{
    ASSERT(name);

    int exception = 0;
    [self _element]->removeAttribute(name, exception);
    raiseOnDOMError(exception);
}

- (DOMAttr *)getAttributeNode:(NSString *)name
{
    ASSERT(name);

    return [DOMAttr _attrWith:[self _element]->getAttributeNode(name).get()];
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr
{
    ASSERT(newAttr);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWith:[self _element]->setAttributeNode([newAttr _attr], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr
{
    ASSERT(oldAttr);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWith:[self _element]->removeAttributeNode([oldAttr _attr], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    ASSERT(name);

    return [DOMNodeList _nodeListWith:[self _element]->getElementsByTagName(name).get()];
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [self _element]->getAttributeNS(namespaceURI, localName);
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);
    ASSERT(value);

    int exception = 0;
    [self _element]->setAttributeNS(namespaceURI, qualifiedName, value, exception);
    raiseOnDOMError(exception);
}

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    int exception = 0;
    [self _element]->removeAttributeNS(namespaceURI, localName, exception);
    raiseOnDOMError(exception);
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMAttr _attrWith:[self _element]->getAttributeNodeNS(namespaceURI, localName).get()];
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr
{
    ASSERT(newAttr);

    int exception = 0;
    DOMAttr *result = [DOMAttr _attrWith:[self _element]->setAttributeNodeNS([newAttr _attr], exception).get()];
    raiseOnDOMError(exception);
    return result;
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList _nodeListWith:[self _element]->getElementsByTagNameNS(namespaceURI, localName).get()];
}

- (BOOL)hasAttribute:(NSString *)name
{
    ASSERT(name);

    return [self _element]->hasAttribute(name);
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [self _element]->hasAttributeNS(namespaceURI, localName);
}

@end

@implementation DOMElement (DOMElementCSSInlineStyle)

- (DOMCSSStyleDeclaration *)style
{
    return [DOMCSSStyleDeclaration _styleDeclarationWith:[self _element]->style()];
}

@end

@implementation DOMElement (DOMElementExtensions)

- (NSImage*)image
{
    RenderObject* renderer = [self _element]->renderer();
    if (renderer && renderer->isImage()) {
        RenderImage* img = static_cast<RenderImage*>(renderer);
        if (img->cachedImage() && !img->cachedImage()->isErrorImage())
            return img->cachedImage()->image()->getNSImage();
    }
    return nil;
}

- (void)focus
{
    [self _element]->focus();
}

- (void)blur
{
    [self _element]->blur();
}

- (void)scrollIntoView:(BOOL)alignTop
{
    [self _element]->scrollIntoView(alignTop);
}

- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded
{
    [self _element]->scrollIntoViewIfNeeded(centerIfNeeded);
}

@end

@implementation DOMElement (WebCoreInternal)

+ (DOMElement *)_elementWith:(Element *)impl
{
    return static_cast<DOMElement *>([DOMNode _nodeWith:impl]);
}

- (Element *)_element
{
    return static_cast<Element *>(DOM_cast<Node *>(_internal));
}

@end

@implementation DOMElement (WebPrivate)

- (NSFont *)_font
{
    RenderObject *renderer = [self _element]->renderer();
    if (renderer) {
        return renderer->style()->font().getNSFont();
    }
    return nil;
}

- (NSData*)_imageTIFFRepresentation
{
    RenderObject *renderer = [self _element]->renderer();
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
    Element *e = [self _element];
    ASSERT(e);
    return KURL(e->getDocument()->completeURL(parseURL(e->getAttribute(name)).deprecatedString())).getNSURL();
}

@end

//------------------------------------------------------------------------------------------
// DOMText

@implementation DOMText

- (Text *)_text
{
    return static_cast<Text *>(DOM_cast<Node *>(_internal));
}

- (DOMText *)splitText:(unsigned)offset
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _text]->splitText(offset, ec)];
    raiseOnDOMError(ec);
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
    return [self _documentType]->publicId();
}

- (DOMNamedNodeMap *)entities
{
    return [DOMNamedNodeMap _namedNodeMapWith:[self _documentType]->entities()];
}

- (DOMNamedNodeMap *)notations
{
    return [DOMNamedNodeMap _namedNodeMapWith:[self _documentType]->notations()];
}

- (NSString *)publicId
{
    return [self _documentType]->publicId();
}

- (NSString *)systemId
{
    return [self _documentType]->systemId();
}

- (NSString *)internalSubset
{
    return [self _documentType]->internalSubset();
}

@end

@implementation DOMDocumentType (WebCoreInternal)

- (DocumentType *)_documentType
{
    return static_cast<DocumentType *>(DOM_cast<Node *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMNotation

@implementation DOMNotation

- (Notation *)_notation
{
    return static_cast<Notation *>(DOM_cast<Node *>(_internal));
}

- (NSString *)publicId
{
    return [self _notation]->publicId();
}

- (NSString *)systemId
{
    return [self _notation]->systemId();
}

@end

//------------------------------------------------------------------------------------------
// DOMEntity

@implementation DOMEntity

- (Entity *)_entity
{
    return static_cast<Entity *>(DOM_cast<Node *>(_internal));
}

- (NSString *)publicId
{
    return [self _entity]->publicId();
}

- (NSString *)systemId
{
    return [self _entity]->systemId();
}

- (NSString *)notationName
{
    return [self _entity]->notationName();
}

@end

//------------------------------------------------------------------------------------------
// DOMEntityReference

@implementation DOMEntityReference

@end

//------------------------------------------------------------------------------------------
// DOMProcessingInstruction

@implementation DOMProcessingInstruction

- (ProcessingInstruction *)_processingInstruction
{
    return static_cast<ProcessingInstruction *>(DOM_cast<Node *>(_internal));
}

- (NSString *)target
{
    return [self _processingInstruction]->target();
}

- (NSString *)data
{
    return [self _processingInstruction]->data();
}

- (void)setData:(NSString *)data
{
    ASSERT(data);

    ExceptionCode ec = 0;
    [self _processingInstruction]->setData(data, ec);
    raiseOnDOMError(ec);
}

@end

//------------------------------------------------------------------------------------------
// DOMRange

@implementation DOMRange

- (void)dealloc
{
    if (_internal) {
        DOM_cast<Range *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<Range *>(_internal)->deref();
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
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _range]->startContainer(ec)];
    raiseOnDOMError(ec);
    return result;
}

- (int)startOffset
{
    ExceptionCode ec = 0;
    int result = [self _range]->startOffset(ec);
    raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)endContainer
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _range]->endContainer(ec)];
    raiseOnDOMError(ec);
    return result;
}

- (int)endOffset
{
    ExceptionCode ec = 0;
    int result = [self _range]->endOffset(ec);
    raiseOnDOMError(ec);
    return result;
}

- (BOOL)collapsed
{
    ExceptionCode ec = 0;
    BOOL result = [self _range]->collapsed(ec);
    raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)commonAncestorContainer
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _range]->commonAncestorContainer(ec)];
    raiseOnDOMError(ec);
    return result;
}

- (void)setStart:(DOMNode *)refNode :(int)offset
{
    ExceptionCode ec = 0;
    [self _range]->setStart([refNode _node], offset, ec);
    raiseOnDOMError(ec);
}

- (void)setEnd:(DOMNode *)refNode :(int)offset
{
    ExceptionCode ec = 0;
    [self _range]->setEnd([refNode _node], offset, ec);
    raiseOnDOMError(ec);
}

- (void)setStartBefore:(DOMNode *)refNode
{
    ExceptionCode ec = 0;
    [self _range]->setStartBefore([refNode _node], ec);
    raiseOnDOMError(ec);
}

- (void)setStartAfter:(DOMNode *)refNode
{
    ExceptionCode ec = 0;
    [self _range]->setStartAfter([refNode _node], ec);
    raiseOnDOMError(ec);
}

- (void)setEndBefore:(DOMNode *)refNode
{
    ExceptionCode ec = 0;
    [self _range]->setEndBefore([refNode _node], ec);
    raiseOnDOMError(ec);
}

- (void)setEndAfter:(DOMNode *)refNode
{
    ExceptionCode ec = 0;
    [self _range]->setEndAfter([refNode _node], ec);
    raiseOnDOMError(ec);
}

- (void)collapse:(BOOL)toStart
{
    ExceptionCode ec = 0;
    [self _range]->collapse(toStart, ec);
    raiseOnDOMError(ec);
}

- (void)selectNode:(DOMNode *)refNode
{
    ExceptionCode ec = 0;
    [self _range]->selectNode([refNode _node], ec);
    raiseOnDOMError(ec);
}

- (void)selectNodeContents:(DOMNode *)refNode
{
    ExceptionCode ec = 0;
    [self _range]->selectNodeContents([refNode _node], ec);
    raiseOnDOMError(ec);
}

- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange
{
    ExceptionCode ec = 0;
    short result = [self _range]->compareBoundaryPoints(static_cast<Range::CompareHow>(how), [sourceRange _range], ec);
    raiseOnDOMError(ec);
    return result;
}

- (void)deleteContents
{
    ExceptionCode ec = 0;
    [self _range]->deleteContents(ec);
    raiseOnDOMError(ec);
}

- (DOMDocumentFragment *)extractContents
{
    ExceptionCode ec = 0;
    DOMDocumentFragment *result = [DOMDocumentFragment _documentFragmentWith:[self _range]->extractContents(ec).get()];
    raiseOnDOMError(ec);
    return result;
}

- (DOMDocumentFragment *)cloneContents
{
    ExceptionCode ec = 0;
    DOMDocumentFragment *result = [DOMDocumentFragment _documentFragmentWith:[self _range]->cloneContents(ec).get()];
    raiseOnDOMError(ec);
    return result;
}

- (void)insertNode:(DOMNode *)newNode
{
    ExceptionCode ec = 0;
    [self _range]->insertNode([newNode _node], ec);
    raiseOnDOMError(ec);
}

- (void)surroundContents:(DOMNode *)newParent
{
    ExceptionCode ec = 0;
    [self _range]->surroundContents([newParent _node], ec);
    raiseOnDOMError(ec);
}

- (DOMRange *)cloneRange
{
    ExceptionCode ec = 0;
    DOMRange *result = [DOMRange _rangeWith:[self _range]->cloneRange(ec).get()];
    raiseOnDOMError(ec);
    return result;
}

- (NSString *)toString
{
    ExceptionCode ec = 0;
    NSString *result = [self _range]->toString(ec);
    raiseOnDOMError(ec);
    return result;
}

- (void)detach
{
    ExceptionCode ec = 0;
    [self _range]->detach(ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMRange (WebCoreInternal)

- (id)_initWithRange:(Range *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMRange *)_rangeWith:(Range *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithRange:impl] autorelease];
}

- (Range *)_range
{
    return DOM_cast<Range *>(_internal);
}

@end

@implementation DOMRange (WebPrivate)

- (NSString *)_text
{
    return [self _range]->text();
}

@end

//------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------

@implementation DOMNodeFilter

- (id)_initWithNodeFilter:(NodeFilter *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMNodeFilter *)_nodeFilterWith:(NodeFilter *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNodeFilter:impl] autorelease];
}

- (NodeFilter *)_nodeFilter
{
    return DOM_cast<NodeFilter *>(_internal);
}

- (void)dealloc
{
    if (_internal)
        DOM_cast<NodeFilter *>(_internal)->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        DOM_cast<NodeFilter *>(_internal)->deref();
    [super finalize];
}

- (short)acceptNode:(DOMNode *)node
{
    return [self _nodeFilter]->acceptNode([node _node]);
}

@end


@implementation DOMNodeIterator

- (id)_initWithNodeIterator:(NodeIterator *)impl filter:(id <DOMNodeFilter>)filter
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    m_filter = [filter retain];
    return self;
}

- (NodeIterator *)_nodeIterator
{
    return DOM_cast<NodeIterator *>(_internal);
}

- (void)dealloc
{
    [m_filter release];
    if (_internal) {
        [self detach];
        DOM_cast<NodeIterator *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        [self detach];
        DOM_cast<NodeIterator *>(_internal)->deref();
    }
    [super finalize];
}

- (DOMNode *)root
{
    return [DOMNode _nodeWith:[self _nodeIterator]->root()];
}

- (unsigned)whatToShow
{
    return [self _nodeIterator]->whatToShow();
}

- (id <DOMNodeFilter>)filter
{
    if (m_filter)
        // This node iterator was created from the objc side
        return [[m_filter retain] autorelease];

    // This node iterator was created from the c++ side
    return [DOMNodeFilter _nodeFilterWith:[self _nodeIterator]->filter()];
}

- (BOOL)expandEntityReferences
{
    return [self _nodeIterator]->expandEntityReferences();
}

- (DOMNode *)nextNode
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _nodeIterator]->nextNode(ec)];
    raiseOnDOMError(ec);
    return result;
}

- (DOMNode *)previousNode
{
    ExceptionCode ec = 0;
    DOMNode *result = [DOMNode _nodeWith:[self _nodeIterator]->previousNode(ec)];
    raiseOnDOMError(ec);
    return result;
}

- (void)detach
{
    ExceptionCode ec = 0;
    [self _nodeIterator]->detach(ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMNodeIterator(WebCoreInternal)

+ (DOMNodeIterator *)_nodeIteratorWith:(NodeIterator *)impl filter:(id <DOMNodeFilter>)filter
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithNodeIterator:impl filter:filter] autorelease];
}

@end

@implementation DOMTreeWalker

- (id)_initWithTreeWalker:(TreeWalker *)impl filter:(id <DOMNodeFilter>)filter
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    m_filter = [filter retain];
    return self;
}

- (TreeWalker *)_treeWalker
{
    return DOM_cast<TreeWalker *>(_internal);
}

- (void)dealloc
{
    if (m_filter)
        [m_filter release];
    if (_internal) {
        DOM_cast<TreeWalker *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<TreeWalker *>(_internal)->deref();
    }
    [super finalize];
}

- (DOMNode *)root
{
    return [DOMNode _nodeWith:[self _treeWalker]->root()];
}

- (unsigned)whatToShow
{
    return [self _treeWalker]->whatToShow();
}

- (id <DOMNodeFilter>)filter
{
    if (m_filter)
        // This tree walker was created from the objc side
        return [[m_filter retain] autorelease];

    // This tree walker was created from the c++ side
    return [DOMNodeFilter _nodeFilterWith:[self _treeWalker]->filter()];
}

- (BOOL)expandEntityReferences
{
    return [self _treeWalker]->expandEntityReferences();
}

- (DOMNode *)currentNode
{
    return [DOMNode _nodeWith:[self _treeWalker]->currentNode()];
}

- (void)setCurrentNode:(DOMNode *)currentNode
{
    ExceptionCode ec = 0;
    [self _treeWalker]->setCurrentNode([currentNode _node], ec);
    raiseOnDOMError(ec);
}

- (DOMNode *)parentNode
{
    return [DOMNode _nodeWith:[self _treeWalker]->parentNode()];
}

- (DOMNode *)firstChild
{
    return [DOMNode _nodeWith:[self _treeWalker]->firstChild()];
}

- (DOMNode *)lastChild
{
    return [DOMNode _nodeWith:[self _treeWalker]->lastChild()];
}

- (DOMNode *)previousSibling
{
    return [DOMNode _nodeWith:[self _treeWalker]->previousSibling()];
}

- (DOMNode *)nextSibling
{
    return [DOMNode _nodeWith:[self _treeWalker]->nextSibling()];
}

- (DOMNode *)previousNode
{
    return [DOMNode _nodeWith:[self _treeWalker]->previousNode()];
}

- (DOMNode *)nextNode
{
    return [DOMNode _nodeWith:[self _treeWalker]->nextNode()];
}

@end

@implementation DOMTreeWalker (WebCoreInternal)

+ (DOMTreeWalker *)_treeWalkerWith:(TreeWalker *)impl filter:(id <DOMNodeFilter>)filter
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithTreeWalker:impl filter:filter] autorelease];
}

@end

class ObjCNodeFilterCondition : public NodeFilterCondition 
{
public:
    ObjCNodeFilterCondition(id <DOMNodeFilter>);
    virtual ~ObjCNodeFilterCondition();
    virtual short acceptNode(Node*) const;

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

short ObjCNodeFilterCondition::acceptNode(Node* node) const
{
    if (!node)
        return NodeFilter::FILTER_REJECT;
    return [m_filter acceptNode:[DOMNode _nodeWith:node]];
}

@implementation DOMDocument (DOMDocumentTraversal)

- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    RefPtr<NodeFilter> cppFilter;
    if (filter)
        cppFilter = new NodeFilter(new ObjCNodeFilterCondition(filter));
    ExceptionCode ec = 0;
    RefPtr<NodeIterator> impl = [self _document]->createNodeIterator([root _node], whatToShow, cppFilter, expandEntityReferences, ec);
    raiseOnDOMError(ec);
    return [DOMNodeIterator _nodeIteratorWith:impl.get() filter:filter];
}

- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences
{
    RefPtr<NodeFilter> cppFilter;
    if (filter)
        cppFilter = new NodeFilter(new ObjCNodeFilterCondition(filter));
    ExceptionCode ec = 0;
    RefPtr<TreeWalker> impl = [self _document]->createTreeWalker([root _node], whatToShow, cppFilter, expandEntityReferences, ec);
    raiseOnDOMError(ec);
    return [DOMTreeWalker _treeWalkerWith:impl.get() filter:filter];
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

void ObjCEventListener::handleEvent(Event *event, bool)
{
    [m_listener handleEvent:[DOMEvent _eventWith:event]];
}
