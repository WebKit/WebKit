/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 James G. Speth (speth@end.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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
#import "CSSStyleSheet.h"
#import "Comment.h"
#import "DOMEventsInternal.h"
#import "DOMHTML.h"
#import "DOMImplementationFront.h"
#import "DOMInternal.h"
#import "DOMPrivate.h"
#import "DeprecatedValueList.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DocumentType.h"
#import "EntityReference.h"
#import "Event.h"
#import "EventListener.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "HTMLDocument.h"
#import "HTMLNames.h"
#import "HTMLPlugInElement.h"
#import "IntRect.h"
#import "NodeFilter.h"
#import "NodeFilterCondition.h"
#import "NodeIterator.h"
#import "NodeList.h"
#import "ProcessingInstruction.h"
#import "QualifiedName.h"
#import "Range.h"
#import "RenderImage.h"
#import "Text.h"
#import "TreeWalker.h"
#import "WebScriptObjectPrivate.h"
#import "csshelper.h"

// From old DOMCore.h
#import "DOMNode.h"
#import "DOMObject.h"

// Generated Objective-C Bindings
#import "DOMAttr.h"
#import "DOMCDATASection.h"
#import "DOMCharacterData.h"
#import "DOMComment.h"
#import "DOMDOMImplementation.h"
#import "DOMDocument.h"
#import "DOMDocumentFragment.h"
#import "DOMDocumentType.h"
#import "DOMElement.h"
#import "DOMEntity.h"
#import "DOMEntityReference.h"
#import "DOMNamedNodeMap.h"
#import "DOMNodeList.h"
#import "DOMNotation.h"
#import "DOMProcessingInstruction.h"
#import "DOMText.h"

// From old DOMHTML.h
#import "DOMHTMLAppletElement.h"
#import "DOMHTMLDocument.h"
#import "DOMHTMLOptionElement.h"

// Generated Objective-C Bindings
#import "DOMHTMLAnchorElement.h"
#import "DOMHTMLAreaElement.h"
#import "DOMHTMLBRElement.h"
#import "DOMHTMLBaseElement.h"
#import "DOMHTMLBaseFontElement.h"
#import "DOMHTMLBodyElement.h"
#import "DOMHTMLButtonElement.h"
#import "DOMHTMLCollection.h"
#import "DOMHTMLDListElement.h"
#import "DOMHTMLDirectoryElement.h"
#import "DOMHTMLDivElement.h"
#import "DOMHTMLElement.h"
#import "DOMHTMLFieldSetElement.h"
#import "DOMHTMLFontElement.h"
#import "DOMHTMLFormElement.h"
#import "DOMHTMLFrameElement.h"
#import "DOMHTMLFrameSetElement.h"
#import "DOMHTMLHRElement.h"
#import "DOMHTMLHeadElement.h"
#import "DOMHTMLHeadingElement.h"
#import "DOMHTMLHtmlElement.h"
#import "DOMHTMLIFrameElement.h"
#import "DOMHTMLImageElement.h"
#import "DOMHTMLInputElement.h"
#import "DOMHTMLIsIndexElement.h"
#import "DOMHTMLLIElement.h"
#import "DOMHTMLLabelElement.h"
#import "DOMHTMLLegendElement.h"
#import "DOMHTMLLinkElement.h"
#import "DOMHTMLMapElement.h"
#import "DOMHTMLMenuElement.h"
#import "DOMHTMLMetaElement.h"
#import "DOMHTMLModElement.h"
#import "DOMHTMLOListElement.h"
#import "DOMHTMLObjectElement.h"
#import "DOMHTMLOptGroupElement.h"
#import "DOMHTMLOptionsCollection.h"
#import "DOMHTMLParagraphElement.h"
#import "DOMHTMLParamElement.h"
#import "DOMHTMLPreElement.h"
#import "DOMHTMLQuoteElement.h"
#import "DOMHTMLScriptElement.h"
#import "DOMHTMLSelectElement.h"
#import "DOMHTMLStyleElement.h"
#import "DOMHTMLTableCaptionElement.h"
#import "DOMHTMLTableCellElement.h"
#import "DOMHTMLTableColElement.h"
#import "DOMHTMLTableElement.h"
#import "DOMHTMLTableRowElement.h"
#import "DOMHTMLTableSectionElement.h"
#import "DOMHTMLTextAreaElement.h"
#import "DOMHTMLTitleElement.h"
#import "DOMHTMLUListElement.h"

#import <objc/objc-class.h>

using WebCore::AtomicString;
using WebCore::AtomicStringImpl;
using WebCore::Attr;
using WebCore::Document;
using WebCore::DocumentFragment;
using WebCore::DOMImplementationFront;
using WebCore::Element;
using WebCore::Event;
using WebCore::EventListener;
using WebCore::ExceptionCode;
using WebCore::HTMLDocument;
using WebCore::HTMLElement;
using WebCore::FrameMac;
using WebCore::KURL;
using WebCore::NamedNodeMap;
using WebCore::Node;
using WebCore::NodeFilter;
using WebCore::NodeFilterCondition;
using WebCore::NodeIterator;
using WebCore::NodeList;
using WebCore::QualifiedName;
using WebCore::Range;
using WebCore::RenderImage;
using WebCore::RenderObject;
using WebCore::String;
using WebCore::TreeWalker;

using namespace WebCore::HTMLNames;

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
typedef HashMap<AtomicStringImpl*, Class> ObjCClassMap;

static ObjCClassMap* elementClassMap;
static ListenerMap* listenerMap;


//------------------------------------------------------------------------------------------
// DOMObject

@implementation DOMObject (WebCoreInternal)

- (id)_init
{
    return [super _init];
}

@end


//------------------------------------------------------------------------------------------
// DOMNode

static void addElementClass(const QualifiedName& tag, Class objCClass)
{
    elementClassMap->set(tag.localName().impl(), objCClass);
}

static void createHTMLElementClassMap()
{
    // Create the table.
    elementClassMap = new ObjCClassMap;
    
    // Populate it with HTML element classes.
    addElementClass(aTag, [DOMHTMLAnchorElement class]);
    addElementClass(appletTag, [DOMHTMLAppletElement class]);
    addElementClass(areaTag, [DOMHTMLAreaElement class]);
    addElementClass(baseTag, [DOMHTMLBaseElement class]);
    addElementClass(basefontTag, [DOMHTMLBaseFontElement class]);
    addElementClass(bodyTag, [DOMHTMLBodyElement class]);
    addElementClass(brTag, [DOMHTMLBRElement class]);
    addElementClass(buttonTag, [DOMHTMLButtonElement class]);
    addElementClass(canvasTag, [DOMHTMLImageElement class]);
    addElementClass(captionTag, [DOMHTMLTableCaptionElement class]);
    addElementClass(colTag, [DOMHTMLTableColElement class]);
    addElementClass(colgroupTag, [DOMHTMLTableColElement class]);
    addElementClass(dirTag, [DOMHTMLDirectoryElement class]);
    addElementClass(divTag, [DOMHTMLDivElement class]);
    addElementClass(dlTag, [DOMHTMLDListElement class]);
    addElementClass(fieldsetTag, [DOMHTMLFieldSetElement class]);
    addElementClass(fontTag, [DOMHTMLFontElement class]);
    addElementClass(formTag, [DOMHTMLFormElement class]);
    addElementClass(frameTag, [DOMHTMLFrameElement class]);
    addElementClass(framesetTag, [DOMHTMLFrameSetElement class]);
    addElementClass(h1Tag, [DOMHTMLHeadingElement class]);
    addElementClass(h2Tag, [DOMHTMLHeadingElement class]);
    addElementClass(h3Tag, [DOMHTMLHeadingElement class]);
    addElementClass(h4Tag, [DOMHTMLHeadingElement class]);
    addElementClass(h5Tag, [DOMHTMLHeadingElement class]);
    addElementClass(h6Tag, [DOMHTMLHeadingElement class]);
    addElementClass(headTag, [DOMHTMLHeadElement class]);
    addElementClass(hrTag, [DOMHTMLHRElement class]);
    addElementClass(htmlTag, [DOMHTMLHtmlElement class]);
    addElementClass(iframeTag, [DOMHTMLIFrameElement class]);
    addElementClass(imgTag, [DOMHTMLImageElement class]);
    addElementClass(inputTag, [DOMHTMLInputElement class]);
    addElementClass(isindexTag, [DOMHTMLIsIndexElement class]);
    addElementClass(labelTag, [DOMHTMLLabelElement class]);
    addElementClass(legendTag, [DOMHTMLLegendElement class]);
    addElementClass(liTag, [DOMHTMLLIElement class]);
    addElementClass(linkTag, [DOMHTMLLinkElement class]);
    addElementClass(listingTag, [DOMHTMLPreElement class]);
    addElementClass(mapTag, [DOMHTMLMapElement class]);
    addElementClass(menuTag, [DOMHTMLMenuElement class]);
    addElementClass(metaTag, [DOMHTMLMetaElement class]);
    addElementClass(objectTag, [DOMHTMLObjectElement class]);
    addElementClass(olTag, [DOMHTMLOListElement class]);
    addElementClass(optgroupTag, [DOMHTMLOptGroupElement class]);
    addElementClass(optionTag, [DOMHTMLOptionElement class]);
    addElementClass(pTag, [DOMHTMLParagraphElement class]);
    addElementClass(paramTag, [DOMHTMLParamElement class]);
    addElementClass(preTag, [DOMHTMLPreElement class]);
    addElementClass(qTag, [DOMHTMLQuoteElement class]);
    addElementClass(scriptTag, [DOMHTMLScriptElement class]);
    addElementClass(selectTag, [DOMHTMLSelectElement class]);
    addElementClass(styleTag, [DOMHTMLStyleElement class]);
    addElementClass(tableTag, [DOMHTMLTableElement class]);
    addElementClass(tbodyTag, [DOMHTMLTableSectionElement class]);
    addElementClass(tdTag, [DOMHTMLTableCellElement class]);
    addElementClass(textareaTag, [DOMHTMLTextAreaElement class]);
    addElementClass(tfootTag, [DOMHTMLTableSectionElement class]);
    addElementClass(theadTag, [DOMHTMLTableSectionElement class]);
    addElementClass(titleTag, [DOMHTMLTitleElement class]);
    addElementClass(trTag, [DOMHTMLTableRowElement class]);
    addElementClass(ulTag, [DOMHTMLUListElement class]);

    // FIXME: Reflect marquee once the API has been determined.
}

static Class elementClass(const AtomicString& tagName)
{
    if (!elementClassMap)
        createHTMLElementClassMap();
    Class objcClass = elementClassMap->get(tagName.impl());
    if (!objcClass)
        objcClass = [DOMHTMLElement class];
    return objcClass;
}

@implementation DOMNode (WebCoreInternal)

// FIXME: should this go in the main implementation?
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
            if (impl->isHTMLElement())
                wrapperClass = elementClass(static_cast<HTMLElement*>(impl)->localName());
            else
                wrapperClass = [DOMElement class];
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
            if (static_cast<Document*>(impl)->isHTMLDocument())
                wrapperClass = [DOMHTMLDocument class];
            else
                wrapperClass = [DOMDocument class];
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
        case Node::XPATH_NAMESPACE_NODE:
            // FIXME: Create an XPath objective C wrapper
            // See http://bugzilla.opendarwin.org/show_bug.cgi?id=8755
            return nil;
    }
    return [[[wrapperClass alloc] _initWithNode:impl] autorelease];
}

- (Node *)_node
{
    return DOM_cast<Node *>(_internal);
}

- (const KJS::Bindings::RootObject *)_executionContext
{
    if (Node *n = [self _node]) {
        if (FrameMac *f = Mac(n->document()->frame()))
            return f->executionContextForDOM();
    }
    return 0;
}

@end

@implementation DOMNode (WebPrivate)

- (BOOL)isContentEditable
{
    return [self _node]->isContentEditable();
}

@end

// FIXME: this should be auto-genenerate in DOMNode.mm
@implementation DOMNode (DOMEventTarget)

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

@end


//------------------------------------------------------------------------------------------
// DOMNamedNodeMap

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

// FIXME: this should be auto-genenerate in DOMDocument.mm
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
    return [DOMElement _elementWith:[self _document]->ownerElement()];
}

@end


//------------------------------------------------------------------------------------------
// DOMAttr

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
// DOMDocumentType

@implementation DOMDocumentType (WebCoreInternal)

+ (DOMDocumentType *)_documentTypeWith:(WebCore::DocumentType *)impl;
{
    return static_cast<DOMDocumentType *>([DOMNode _nodeWith:impl]);
}

- (WebCore::DocumentType *)_documentType;
{
    return static_cast<WebCore::DocumentType *>(DOM_cast<WebCore::Node *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

// FIXME: this should be auto-genenerate in DOMElement.mm
@implementation DOMElement (DOMElementAppKitExtensions)

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
    return KURL(e->document()->completeURL(parseURL(e->getAttribute(name)).deprecatedString())).getNSURL();
}

- (void *)_NPObject
{
    Element* element = [self _element];
    if (element->hasTagName(appletTag) || element->hasTagName(embedTag) || element->hasTagName(objectTag))
        return static_cast<WebCore::HTMLPlugInElement*>(element)->getNPObject();
    else
        return 0;
}

- (BOOL)isFocused
{
    Element* impl = [self _element];
    if (impl->document()->focusNode() == impl)
        return YES;
    return NO;
}

@end


//------------------------------------------------------------------------------------------
// DOMText

@implementation DOMText (WebCoreInternal)

+ (DOMText *)_textWith:(WebCore::Text *)impl
{
    return static_cast<DOMText *>([DOMNode _nodeWith:impl]);
}

@end


//------------------------------------------------------------------------------------------
// DOMComment

@implementation DOMComment (WebCoreInternal)

+ (DOMComment *)_commentWith:(WebCore::Comment *)impl
{
    return static_cast<DOMComment *>([DOMNode _nodeWith:impl]);
}

@end


//------------------------------------------------------------------------------------------
// DOMCDATASection

@implementation DOMCDATASection (WebCoreInternal)

+ (DOMCDATASection *)_CDATASectionWith:(WebCore::CDATASection *)impl;
{
    return static_cast<DOMCDATASection *>([DOMNode _nodeWith:impl]);
}

@end


//------------------------------------------------------------------------------------------
// DOMProcessingInstruction

@implementation DOMProcessingInstruction (WebCoreInternal)

+ (DOMProcessingInstruction *)_processingInstructionWith:(WebCore::ProcessingInstruction *)impl;
{
    return static_cast<DOMProcessingInstruction *>([DOMNode _nodeWith:impl]);
}

@end


//------------------------------------------------------------------------------------------
// DOMProcessingInstruction

@implementation DOMEntityReference (WebCoreInternal)

+ (DOMEntityReference *)_entityReferenceWith:(WebCore::EntityReference *)impl;
{
    return static_cast<DOMEntityReference *>([DOMNode _nodeWith:impl]);
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

- (NSString *)text
{
    return [self _range]->text();
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
    return [self text];
}

@end


//------------------------------------------------------------------------------------------
// DOMNodeFilter

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


//------------------------------------------------------------------------------------------
// DOMNodeIterator

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
    [self _nodeIterator]->detach();
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


//------------------------------------------------------------------------------------------
// DOMTreeWalker

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


//------------------------------------------------------------------------------------------
// ObjCNodeFilterCondition

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


//------------------------------------------------------------------------------------------
// DOMDocument (DOMDocumentTraversal)

// FIXME: this should be auto-genenerate in DOMDocument.mm
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


//------------------------------------------------------------------------------------------
// ObjCEventListener

ObjCEventListener* ObjCEventListener::find(id <DOMEventListener> listener)
{
    if (ListenerMap* map = listenerMap)
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
    ListenerMap* map = listenerMap;
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
