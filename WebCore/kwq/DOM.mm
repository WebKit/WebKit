/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "DOMInternal.h"

#import <dom/dom_doc.h>
#import <dom/dom_element.h>
#import <dom/dom_exception.h>
#import <dom/dom_node.h>
#import <dom/dom_string.h>
#import <dom/dom_text.h>
#import <dom/dom_xml.h>
#import <dom/dom2_range.h>
#import <xml/dom_docimpl.h>
#import <xml/dom_elementimpl.h>
#import <xml/dom_nodeimpl.h>
#import <xml/dom_stringimpl.h>
#import <xml/dom_textimpl.h>
#import <xml/dom_xmlimpl.h>
#import <xml/dom2_rangeimpl.h>

#import "KWQAssertions.h"

using DOM::Attr;
using DOM::AttrImpl;
using DOM::CharacterData;
using DOM::CharacterDataImpl;
using DOM::DocumentFragmentImpl;
using DOM::DocumentType;
using DOM::DocumentTypeImpl;
using DOM::Document;
using DOM::DocumentImpl;
using DOM::DOMException;
using DOM::DOMImplementationImpl;
using DOM::DOMString;
using DOM::DOMStringImpl;
using DOM::Element;
using DOM::ElementImpl;
using DOM::EntityImpl;
using DOM::NamedNodeMap;
using DOM::NamedNodeMapImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::NodeList;
using DOM::NodeListImpl;
using DOM::NotationImpl;
using DOM::ProcessingInstruction;
using DOM::ProcessingInstructionImpl;
using DOM::Range;
using DOM::RangeImpl;
using DOM::Text;
using DOM::TextImpl;

@interface DOMNamedNodeMap (WebCoreInternal)
+ (DOMNamedNodeMap *)namedNodeMapWithImpl:(NamedNodeMapImpl *)impl;
@end

@interface DOMNodeList (WebCoreInternal)
+ (DOMNodeList *)nodeListWithImpl:(NodeListImpl *)impl;
@end

@interface DOMImplementation (WebCoreInternal)
+ (DOMImplementation *)DOMImplementationWithImpl:(DOMImplementationImpl *)impl;
@end

@interface DOMDocumentFragment (WebCoreInternal)
+ (DOMDocumentFragment *)documentFragmentWithImpl:(DocumentFragmentImpl *)impl;
@end

@interface DOMAttr (WebCoreInternal)
+ (DOMAttr *)attrWithImpl:(AttrImpl *)impl;
- (AttrImpl *)attrImpl;
@end

@interface DOMRange (WebCoreInternal)
+ (DOMRange *)rangeWithImpl:(RangeImpl *)impl;
@end

//------------------------------------------------------------------------------------------
// Static functions and data

NSString * const DOMErrorDomain = @"DOMErrorDomain";

static CFMutableDictionaryRef wrapperCache = NULL;

static id wrapperForImpl(const void *impl)
{
    if (!wrapperCache)
        return nil;
    return (id)CFDictionaryGetValue(wrapperCache, impl);
}

static void setWrapperForImpl(id wrapper, const void *impl)
{
    if (!wrapperCache) {
        // No need to retain/free either impl key, or id value.  Items will be removed
        // from the cache in dealloc methods.
        wrapperCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    CFDictionarySetValue(wrapperCache, impl, wrapper);
}

static void removeWrapperForImpl(const void *impl)
{
    if (!wrapperCache)
        return;
    CFDictionaryRemoveValue(wrapperCache, impl);
}

static NSString *DOMStringToNSString(const DOMString &aString)
{
    return [NSString stringWithCharacters:reinterpret_cast<const unichar *>(aString.unicode()) length:aString.length()];
}

static DOMString NSStringToDOMString(NSString *aString)
{
    ASSERT(aString);

    CFIndex size = CFStringGetLength(reinterpret_cast<CFStringRef>(aString));
    UniChar fixedSizeBuffer[1024];
    UniChar *buffer;
    if (size > static_cast<CFIndex>(sizeof(fixedSizeBuffer) / sizeof(UniChar))) {
        buffer = static_cast<UniChar *>(malloc(size * sizeof(UniChar)));
    } else {
        buffer = fixedSizeBuffer;
    }
    CFStringGetCharacters(reinterpret_cast<CFStringRef>(aString), CFRangeMake(0, size), buffer);
    DOMString ret(reinterpret_cast<const QChar *>(buffer), (uint)size);
    if (buffer != fixedSizeBuffer) {
        free(buffer);
    }
    return ret;
}

static void fillInError(NSError **error, int code)
{
    if (!error)
        return;
    if (!code)
        *error = nil;
    else
        *error = [NSError errorWithDomain:DOMErrorDomain code:code userInfo:nil];
}

//------------------------------------------------------------------------------------------
// Factory methods

inline NamedNodeMap NamedNodeMapImpl::createInstance(NamedNodeMapImpl *impl)
{
    return NamedNodeMap(impl);
}

inline Attr AttrImpl::createInstance(AttrImpl *impl)
{
    return Attr(impl);
}

inline Element ElementImpl::createInstance(ElementImpl *impl)
{
    return Element(impl);
}

inline DocumentType DocumentTypeImpl::createInstance(DocumentTypeImpl *impl)
{
    return DocumentType(impl);
}

inline Document DocumentImpl::createInstance(DocumentImpl *impl)
{
    return Document(impl);
}

//------------------------------------------------------------------------------------------
// DOMObject

@implementation DOMObject

- (void)dealloc
{
    if (_internal) {
        removeWrapperForImpl(_internal);
    }
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

//------------------------------------------------------------------------------------------
// DOMNode

@implementation DOMNode

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<NodeImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (NSString *)nodeName
{
    return DOMStringToNSString([self nodeImpl]->nodeName());
}

- (NSString *)nodeValue
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return DOMStringToNSString([self nodeImpl]->nodeValue());
}

- (void)setNodeValue:(NSString *)string error:(NSError **)error
{
    ASSERT(string);
    
    int code;
    [self nodeImpl]->setNodeValue(NSStringToDOMString(string), code);
    fillInError(error, code);
}

- (unsigned short)nodeType
{
    return [self nodeImpl]->nodeType();
}

- (DOMNode *)parentNode
{
    return [[self class] nodeWithImpl:[self nodeImpl]->parentNode()];
}

- (DOMNodeList *)childNodes
{
    return [[self class] nodeListWithImpl:[self nodeImpl]->childNodes()];
}

- (DOMNode *)firstChild
{
    return [[self class] nodeWithImpl:[self nodeImpl]->firstChild()];
}

- (DOMNode *)lastChild
{
    return [[self class] nodeWithImpl:[self nodeImpl]->lastChild()];
}

- (DOMNode *)previousSibling
{
    return [[self class] nodeWithImpl:[self nodeImpl]->previousSibling()];
}

- (DOMNode *)nextSibling
{
    return [[self class] nodeWithImpl:[self nodeImpl]->nextSibling()];
}

- (DOMNamedNodeMap *)attributes
{
    // DOM level 2 core specification says: 
    // A NamedNodeMap containing the attributes of this node (if it is an Element) or null otherwise.
    return nil;
}

- (DOMDocument *)ownerDocument
{
    return [DOMDocument documentWithImpl:[self nodeImpl]->getDocument()];
}

- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild error:(NSError **)error
{
    ASSERT(newChild);
    ASSERT(refChild);

    int code;
    DOMNode *result = [[self class] nodeWithImpl:[self nodeImpl]->insertBefore([newChild nodeImpl], [refChild nodeImpl], code)];
    fillInError(error, code);
    return result;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild error:(NSError **)error
{
    ASSERT(newChild);
    ASSERT(oldChild);

    int code;
    DOMNode *result = [[self class] nodeWithImpl:[self nodeImpl]->replaceChild([newChild nodeImpl], [oldChild nodeImpl], code)];
    fillInError(error, code);
    return result;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild error:(NSError **)error
{
    ASSERT(oldChild);

    int code;
    DOMNode *result = [[self class] nodeWithImpl:[self nodeImpl]->removeChild([oldChild nodeImpl], code)];
    fillInError(error, code);
    return result;
}

- (DOMNode *)appendChild:(DOMNode *)newChild error:(NSError **)error
{
    ASSERT(newChild);

    int code;
    DOMNode *result = [[self class] nodeWithImpl:[self nodeImpl]->appendChild([newChild nodeImpl], code)];
    fillInError(error, code);
    return result;
}

- (BOOL)hasChildNodes
{
    return [self nodeImpl]->hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    return [[self class] nodeWithImpl:[self nodeImpl]->cloneNode(deep)];
}

- (void)normalize
{
    [self nodeImpl]->normalize();
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    // Method not reflected in DOM::NodeImpl interface
    return Node([self nodeImpl]).isSupported(NSStringToDOMString(feature), NSStringToDOMString(version));
}

- (NSString *)namespaceURI
{
    // Method not reflected in DOM::NodeImpl interface
    return DOMStringToNSString(Node([self nodeImpl]).namespaceURI());
}

- (NSString *)prefix
{
    return DOMStringToNSString([self nodeImpl]->prefix());
}

- (void)setPrefix:(NSString *)prefix error:(NSError **)error
{
    ASSERT(prefix);

    int code;
    [self nodeImpl]->setPrefix(NSStringToDOMString(prefix), code);
    fillInError(error, code);
}

- (NSString *)localName
{
    return DOMStringToNSString([self nodeImpl]->localName());
}

- (BOOL)hasAttributes
{
    // Method not reflected in DOM::NodeImpl interface
    return Node([self nodeImpl]).hasAttributes();
}

- (NSString *)HTMLString
{
    return [self nodeImpl]->recursive_toHTML(true).getNSString();
}

@end

@implementation DOMNode (WebCoreInternal)

- (id)initWithNodeImpl:(NodeImpl *)impl
{
    ASSERT(impl);

    [super init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMNode *)nodeWithImpl:(NodeImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    Class wrapperClass = nil;
    switch (impl->nodeType()) {
    case Node::ELEMENT_NODE:
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
    }
    return [[[wrapperClass alloc] initWithNodeImpl:impl] autorelease];
}

- (NodeImpl *)nodeImpl
{
    return reinterpret_cast<NodeImpl *>(_internal);
}

@end

//------------------------------------------------------------------------------------------
// DOMNamedNodeMap

@implementation DOMNamedNodeMap

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<NamedNodeMapImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (NamedNodeMapImpl *)namedNodeMapImpl
{
    return reinterpret_cast<NamedNodeMapImpl *>(_internal);
}

- (DOMNode *)getNamedItem:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
    Node result(map.getNamedItem(NSStringToDOMString(name)));
    return [DOMNode nodeWithImpl:result.handle()];
}

- (DOMNode *)setNamedItem:(DOMNode *)arg error:(NSError **)error
{
    ASSERT(arg);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
        Node result(map.setNamedItem([arg nodeImpl]));
        if (error) {
            *error = nil;
        }
        return [DOMNode nodeWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNode *)removeNamedItem:(NSString *)name error:(NSError **)error
{
    ASSERT(name);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
        Node result(map.removeNamedItem(NSStringToDOMString(name)));
        if (error) {
            *error = nil;
        }
        return [DOMNode nodeWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNode *)item:(unsigned long)index
{
    return [DOMNode nodeWithImpl:[self namedNodeMapImpl]->item(index)];
}

- (unsigned long)length
{
    return [self namedNodeMapImpl]->length();
}

- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    if (!namespaceURI || !localName) {
        return nil;
    }

    // Method not reflected in DOM::NamedNodeMapImpl interface
    NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
    Node result(map.getNamedItemNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
    return [DOMNode nodeWithImpl:result.handle()];
}

- (DOMNode *)setNamedItemNS:(DOMNode *)arg error:(NSError **)error
{
    ASSERT(arg);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
        Node result(map.setNamedItemNS([arg nodeImpl]));
        if (error) {
            *error = nil;
        }
        return [DOMNode nodeWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
        Node result(map.removeNamedItemNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
        if (error) {
            *error = nil;
        }
        return [DOMNode nodeWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

@end

@implementation DOMNamedNodeMap (WebCoreInternal)

- (id)initWithNamedNodeMapImpl:(NamedNodeMapImpl *)impl
{
    ASSERT(impl);

    [super init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMNamedNodeMap *)namedNodeMapWithImpl:(NamedNodeMapImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithNamedNodeMapImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMNodeList

@implementation DOMNodeList

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<NodeListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (NodeListImpl *)nodeListImpl
{
    return reinterpret_cast<NodeListImpl *>(_internal);
}

- (DOMNode *)item:(unsigned long)index
{
    return [DOMNode nodeWithImpl:[self nodeListImpl]->item(index)];
}

- (unsigned long)length
{
    return [self nodeListImpl]->length();
}

@end

@implementation DOMNodeList (WebCoreInternal)

- (id)initWithNodeListImpl:(NodeListImpl *)impl
{
    ASSERT(impl);

    [super init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMNodeList *)nodeListWithImpl:(NodeListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithNodeListImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMImplementation

@implementation DOMImplementation

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<DOMImplementationImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (DOMImplementationImpl *)DOMImplementationImpl
{
    return reinterpret_cast<DOMImplementationImpl *>(_internal);
}

- (BOOL)hasFeature:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return [self DOMImplementationImpl]->hasFeature(NSStringToDOMString(feature), NSStringToDOMString(version));
}

- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId error:(NSError **)error
{
    ASSERT(qualifiedName);
    ASSERT(publicId);
    ASSERT(systemId);

    int code;
    DocumentTypeImpl *impl = [self DOMImplementationImpl]->createDocumentType(NSStringToDOMString(qualifiedName), NSStringToDOMString(publicId), NSStringToDOMString(systemId), code);
    fillInError(error, code);
    return static_cast<DOMDocumentType *>([DOMNode nodeWithImpl:impl]);
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code;
    DocumentType dt = DocumentTypeImpl::createInstance(static_cast<DocumentTypeImpl *>([doctype nodeImpl]));
    DocumentImpl *impl = [self DOMImplementationImpl]->createDocument(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName), dt, code);
    fillInError(error, code);
    return static_cast<DOMDocument *>([DOMNode nodeWithImpl:impl]);
}

@end
 
@implementation DOMImplementation (WebInternal)

- (id)initWithDOMImplementationImpl:(DOMImplementationImpl *)impl
{
    ASSERT(impl);

    [super init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMImplementation *)DOMImplementationWithImpl:(DOMImplementationImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithDOMImplementationImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
// DOMDocumentFragment

@implementation DOMDocumentFragment

@end

@implementation DOMDocumentFragment (WebCoreInternal)

+ (DOMDocumentFragment *)documentFragmentWithImpl:(DocumentFragmentImpl *)impl
{
    return static_cast<DOMDocumentFragment *>([DOMNode nodeWithImpl:impl]);
}

@end

//------------------------------------------------------------------------------------------
// DOMDocument

@implementation DOMDocument

- (DocumentImpl *)documentImpl
{
    return reinterpret_cast<DocumentImpl *>(_internal);
}

- (DOMDocumentType *)doctype
{
    return static_cast<DOMDocumentType *>([DOMNode nodeWithImpl:[self documentImpl]->doctype()]);
}

- (DOMImplementation *)implementation
{
    return [DOMImplementation DOMImplementationWithImpl:[self documentImpl]->implementation()];
}

- (DOMElement *)documentElement
{
    return static_cast<DOMElement *>([DOMNode nodeWithImpl:[self documentImpl]->documentElement()]);
}

- (DOMElement *)createElement:(NSString *)tagName error:(NSError **)error
{
    ASSERT(tagName);

    int code;
    DOMElement *result = static_cast<DOMElement *>([DOMNode nodeWithImpl:[self documentImpl]->createElement(NSStringToDOMString(tagName), code)]);
    fillInError(error, code);
    return result;
}

- (DOMDocumentFragment *)createDocumentFragment
{
    return static_cast<DOMDocumentFragment *>([DOMNode nodeWithImpl:[self documentImpl]->createDocumentFragment()]);
}

- (DOMText *)createTextNode:(NSString *)data
{
    ASSERT(data);
    return static_cast<DOMText *>([DOMNode nodeWithImpl:[self documentImpl]->createTextNode(NSStringToDOMString(data))]);
}

- (DOMComment *)createComment:(NSString *)data
{
    ASSERT(data);
    return static_cast<DOMComment *>([DOMNode nodeWithImpl:[self documentImpl]->createComment(NSStringToDOMString(data))]);
}

- (DOMCDATASection *)createCDATASection:(NSString *)data error:(NSError **)error
{
    ASSERT(data);

    // Documentation says we can raise a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report that error up to us.
    if (error) {
        *error = nil;
    }
    return static_cast<DOMCDATASection *>([DOMNode nodeWithImpl:[self documentImpl]->createCDATASection(NSStringToDOMString(data))]);
}

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data error:(NSError **)error
{
    ASSERT(target);
    ASSERT(data);

    // Documentation says we can raise a INVALID_CHARACTER_ERR or a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report these errors up to us.
    if (error) {
        *error = nil;
    }
    return static_cast<DOMProcessingInstruction *>([DOMNode nodeWithImpl:[self documentImpl]->createProcessingInstruction(NSStringToDOMString(target), NSStringToDOMString(data))]);
}

- (DOMAttr *)createAttribute:(NSString *)name error:(NSError **)error
{
    ASSERT(name);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self documentImpl]));
        Attr result(doc.createAttribute(NSStringToDOMString(name)));
        if (error) {
            *error = nil;
        }
        return static_cast<DOMAttr *>([DOMNode nodeWithImpl:result.handle()]);
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMEntityReference *)createEntityReference:(NSString *)name error:(NSError **)error
{
    ASSERT(name);

    // Documentation says we can raise a INVALID_CHARACTER_ERR or a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report these errors up to us.
    if (error) {
        *error = nil;
    }
    return static_cast<DOMEntityReference *>([DOMNode nodeWithImpl:[self documentImpl]->createEntityReference(NSStringToDOMString(name))]);
}

- (DOMNodeList *)getElementsByTagName:(NSString *)tagname
{
    ASSERT(tagname);
    return [DOMNodeList nodeListWithImpl:[self documentImpl]->getElementsByTagNameNS(0, NSStringToDOMString(tagname).implementation())];
}

- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep error:(NSError **)error
{
    int code;
    DOMNode *result = [DOMNode nodeWithImpl:[self documentImpl]->importNode([importedNode nodeImpl], deep, code)];
    fillInError(error, code);
    return result;
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code;
    DOMNode *result = [DOMNode nodeWithImpl:[self documentImpl]->createElementNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName), code)];
    fillInError(error, code);
    return static_cast<DOMElement *>(result);
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self documentImpl]));
        Attr result(doc.createAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName)));
        if (error) {
            *error = nil;
        }
        return static_cast<DOMAttr *>([DOMNode nodeWithImpl:result.handle()]);
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList nodeListWithImpl:[self documentImpl]->getElementsByTagNameNS(NSStringToDOMString(namespaceURI).implementation(), NSStringToDOMString(localName).implementation())];
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    ASSERT(elementId);

    return static_cast<DOMElement *>([DOMNode nodeWithImpl:[self documentImpl]->getElementById(NSStringToDOMString(elementId))]);
}

@end

@implementation DOMDocument (WebCoreInternal)

+ (DOMDocument *)documentWithImpl:(DocumentImpl *)impl
{
    return static_cast<DOMDocument *>([DOMNode nodeWithImpl:impl]);
}

@end

//------------------------------------------------------------------------------------------
// DOMCharacterData

@implementation DOMCharacterData

- (CharacterDataImpl *)characterDataImpl
{
    return reinterpret_cast<CharacterDataImpl *>(_internal);
}

- (NSString *)data
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return DOMStringToNSString([self characterDataImpl]->data());
}

- (void)setData:(NSString *)data error:(NSError **)error
{
    ASSERT(data);
    
    int code;
    [self characterDataImpl]->setData(NSStringToDOMString(data), code);
    fillInError(error, code);
}

- (unsigned long)length
{
    return [self characterDataImpl]->length();
}

- (NSString *)substringData:(unsigned long)offset :(unsigned long)count error:(NSError **)error
{
    int code;
    NSString *result = DOMStringToNSString([self characterDataImpl]->substringData(offset, count, code));
    fillInError(error, code);
    return result;
}

- (void)appendData:(NSString *)arg error:(NSError **)error
{
    ASSERT(arg);
    
    int code;
    [self characterDataImpl]->appendData(NSStringToDOMString(arg), code);
    fillInError(error, code);
}

- (void)insertData:(unsigned long)offset :(NSString *)arg error:(NSError **)error
{
    ASSERT(arg);
    
    int code;
    [self characterDataImpl]->insertData(offset, NSStringToDOMString(arg), code);
    fillInError(error, code);
}

- (void)deleteData:(unsigned long)offset :(unsigned long) count error:(NSError **)error;
{
    int code;
    [self characterDataImpl]->deleteData(offset, count, code);
    fillInError(error, code);
}

- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg error:(NSError **)error
{
    ASSERT(arg);

    int code;
    [self characterDataImpl]->replaceData(offset, count, NSStringToDOMString(arg), code);
    fillInError(error, code);
}

@end

//------------------------------------------------------------------------------------------
// DOMAttr

@implementation DOMAttr

- (NSString *)name
{
    return DOMStringToNSString([self attrImpl]->nodeName());
}

- (BOOL)specified
{
    return [self attrImpl]->specified();
}

- (NSString *)value
{
    return DOMStringToNSString([self attrImpl]->nodeValue());
}

- (void)setValue:(NSString *)value error:(NSError **)error
{
    ASSERT(value);

    int code;
    [self attrImpl]->setValue(NSStringToDOMString(value), code);
    fillInError(error, code);
}

- (DOMElement *)ownerElement
{
    return [DOMElement elementWithImpl:[self attrImpl]->ownerElement()];
}

@end

@implementation DOMAttr (WebCoreInternal)

+ (DOMAttr *)attrWithImpl:(AttrImpl *)impl
{
    return static_cast<DOMAttr *>([DOMNode nodeWithImpl:impl]);
}

- (AttrImpl *)attrImpl
{
    return reinterpret_cast<AttrImpl *>(_internal);
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

@implementation DOMElement

- (NSString *)tagName
{
    return DOMStringToNSString([self elementImpl]->tagName());
}

- (DOMNamedNodeMap *)attributes
{
    return [DOMNamedNodeMap namedNodeMapWithImpl:[self elementImpl]->attributes()];
}

- (NSString *)getAttribute:(NSString *)name
{
    ASSERT(name);

    return DOMStringToNSString([self elementImpl]->getAttribute(NSStringToDOMString(name)));
}

- (void)setAttribute:(NSString *)name :(NSString *)value error:(NSError **)error
{
    ASSERT(name);
    ASSERT(value);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        element.setAttribute(NSStringToDOMString(name), NSStringToDOMString(value));
        if (error) {
            *error = nil;
        }
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
    }
}

- (void)removeAttribute:(NSString *)name error:(NSError **)error
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        element.removeAttribute(NSStringToDOMString(name));
        if (error) {
            *error = nil;
        }
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
    }
}

- (DOMAttr *)getAttributeNode:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self elementImpl]));
    Attr result(element.getAttributeNode(NSStringToDOMString(name)));
    return [DOMAttr attrWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr error:(NSError **)error
{
    ASSERT(newAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        Attr attr(AttrImpl::createInstance([newAttr attrImpl]));
        Attr result(element.setAttributeNode(attr));
        if (error) {
            *error = nil;
        }
        return [DOMAttr attrWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr error:(NSError **)error
{
    ASSERT(oldAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        Attr attr(AttrImpl::createInstance([oldAttr attrImpl]));
        Attr result(element.removeAttributeNode(attr));
        if (error) {
            *error = nil;
        }
        return [DOMAttr attrWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    ASSERT(name);

    return [DOMNodeList nodeListWithImpl:[self elementImpl]->getElementsByTagNameNS(0, NSStringToDOMString(name).implementation())];
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    Element element(ElementImpl::createInstance([self elementImpl]));
    return DOMStringToNSString(element.getAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);
    ASSERT(value);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        element.setAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName), NSStringToDOMString(value));
        if (error) {
            *error = nil;
        }
    }
    catch (const DOMException &e) {
        fillInError(error, e.code);
    }
}

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        element.removeAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName));
        if (error) {
            *error = nil;
        }
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
    }
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self elementImpl]));
    Attr result(element.getAttributeNodeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
    return [DOMAttr attrWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr error:(NSError **)error
{
    ASSERT(newAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        Attr attr(AttrImpl::createInstance([newAttr attrImpl]));
        Attr result(element.setAttributeNodeNS(attr));
        if (error) {
            *error = nil;
        }
        return [DOMAttr attrWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList nodeListWithImpl:[self elementImpl]->getElementsByTagNameNS(NSStringToDOMString(namespaceURI).implementation(), NSStringToDOMString(localName).implementation())];
}

- (BOOL)hasAttribute:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self elementImpl]));
    return element.hasAttribute(NSStringToDOMString(name));
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self elementImpl]));
    return element.hasAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName));
}

@end

@implementation DOMElement (WebCoreInternal)

+ (DOMElement *)elementWithImpl:(ElementImpl *)impl
{
    return static_cast<DOMElement *>([DOMNode nodeWithImpl:impl]);
}

- (ElementImpl *)elementImpl
{
    return reinterpret_cast<ElementImpl *>(_internal);
}

@end

//------------------------------------------------------------------------------------------
// DOMText

@implementation DOMText

- (TextImpl *)textImpl
{
    return reinterpret_cast<TextImpl *>(_internal);
}

- (DOMText *)splitText:(unsigned long)offset error:(NSError **)error
{
    int code;
    DOMNode *result = [DOMNode nodeWithImpl:[self textImpl]->splitText(offset, code)];
    fillInError(error, code);
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

- (DocumentTypeImpl *)documentTypeImpl
{
    return reinterpret_cast<DocumentTypeImpl *>(_internal);
}

- (NSString *)name
{
    return DOMStringToNSString([self documentTypeImpl]->publicId());
}

- (DOMNamedNodeMap *)entities
{
    return [DOMNamedNodeMap namedNodeMapWithImpl:[self documentTypeImpl]->entities()];
}

- (DOMNamedNodeMap *)notations
{
    return [DOMNamedNodeMap namedNodeMapWithImpl:[self documentTypeImpl]->notations()];
}

- (NSString *)publicId
{
    return DOMStringToNSString([self documentTypeImpl]->publicId());
}

- (NSString *)systemId
{
    return DOMStringToNSString([self documentTypeImpl]->systemId());
}

- (NSString *)internalSubset
{
    return DOMStringToNSString([self documentTypeImpl]->internalSubset());
}

@end

//------------------------------------------------------------------------------------------
// DOMNotation

@implementation DOMNotation

- (NotationImpl *)notationImpl
{
    return reinterpret_cast<NotationImpl *>(_internal);
}

- (NSString *)publicId
{
    return DOMStringToNSString([self notationImpl]->publicId());
}

- (NSString *)systemId
{
    return DOMStringToNSString([self notationImpl]->systemId());
}

@end

//------------------------------------------------------------------------------------------
// DOMEntity

@implementation DOMEntity

- (EntityImpl *)entityImpl
{
    return reinterpret_cast<EntityImpl *>(_internal);
}

- (NSString *)publicId
{
    return DOMStringToNSString([self entityImpl]->publicId());
}

- (NSString *)systemId
{
    return DOMStringToNSString([self entityImpl]->systemId());
}

- (NSString *)notationName
{
    return DOMStringToNSString([self entityImpl]->notationName());
}

@end

//------------------------------------------------------------------------------------------
// DOMEntityReference

@implementation DOMEntityReference

@end

//------------------------------------------------------------------------------------------
// DOMProcessingInstruction

@implementation DOMProcessingInstruction

- (ProcessingInstructionImpl *)processingInstructionImpl
{
    return reinterpret_cast<ProcessingInstructionImpl *>(_internal);
}

- (NSString *)target
{
    return DOMStringToNSString([self processingInstructionImpl]->target());
}

- (NSString *)data
{
    return DOMStringToNSString([self processingInstructionImpl]->data());
}

- (void)setData:(NSString *)data error:(NSError **)error
{
    ASSERT(data);

    int code;
    [self processingInstructionImpl]->setData(NSStringToDOMString(data), code);
    fillInError(error, code);
}

@end

//------------------------------------------------------------------------------------------
// DOMRange

@implementation DOMRange

- (void)dealloc
{
    if (_internal) {
        reinterpret_cast<RangeImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (RangeImpl *)rangeImpl
{
    return reinterpret_cast<RangeImpl *>(_internal);
}

- (DOMNode *)startContainer:(NSError **)error
{
    int code;
    DOMNode *result = [DOMNode nodeWithImpl:[self rangeImpl]->startContainer(code)];
    fillInError(error, code);
    return result;
}

- (long)startOffset:(NSError **)error
{
    int code;
    long result = [self rangeImpl]->startOffset(code);
    fillInError(error, code);
    return result;
}

- (DOMNode *)endContainer:(NSError **)error
{
    int code;
    DOMNode *result = [DOMNode nodeWithImpl:[self rangeImpl]->endContainer(code)];
    fillInError(error, code);
    return result;
}

- (long)endOffset:(NSError **)error
{
    int code;
    long result = [self rangeImpl]->endOffset(code);
    fillInError(error, code);
    return result;
}

- (BOOL)collapsed:(NSError **)error
{
    int code;
    BOOL result = [self rangeImpl]->collapsed(code);
    fillInError(error, code);
    return result;
}

- (DOMNode *)commonAncestorContainer:(NSError **)error
{
    int code;
    DOMNode *result = [DOMNode nodeWithImpl:[self rangeImpl]->commonAncestorContainer(code)];
    fillInError(error, code);
    return result;
}

- (void)setStart:(DOMNode *)refNode :(long)offset error:(NSError **)error
{
    int code;
    [self rangeImpl]->setStart([refNode nodeImpl], offset, code);
    fillInError(error, code);
}

- (void)setEnd:(DOMNode *)refNode :(long)offset error:(NSError **)error
{
    int code;
    [self rangeImpl]->setEnd([refNode nodeImpl], offset, code);
    fillInError(error, code);
}

- (void)setStartBefore:(DOMNode *)refNode error:(NSError **)error
{
    int code;
    [self rangeImpl]->setStartBefore([refNode nodeImpl], code);
    fillInError(error, code);
}

- (void)setStartAfter:(DOMNode *)refNode error:(NSError **)error
{
    int code;
    [self rangeImpl]->setStartAfter([refNode nodeImpl], code);
    fillInError(error, code);
}

- (void)setEndBefore:(DOMNode *)refNode error:(NSError **)error
{
    int code;
    [self rangeImpl]->setEndBefore([refNode nodeImpl], code);
    fillInError(error, code);
}

- (void)setEndAfter:(DOMNode *)refNode error:(NSError **)error
{
    int code;
    [self rangeImpl]->setEndAfter([refNode nodeImpl], code);
    fillInError(error, code);
}

- (void)collapse:(BOOL)toStart error:(NSError **)error
{
    int code;
    [self rangeImpl]->collapse(toStart, code);
    fillInError(error, code);
}

- (void)selectNode:(DOMNode *)refNode error:(NSError **)error
{
    int code;
    [self rangeImpl]->selectNode([refNode nodeImpl], code);
    fillInError(error, code);
}

- (void)selectNodeContents:(DOMNode *)refNode error:(NSError **)error
{
    int code;
    [self rangeImpl]->selectNodeContents([refNode nodeImpl], code);
    fillInError(error, code);
}

- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange error:(NSError **)error
{
    int code;
    short result = [self rangeImpl]->compareBoundaryPoints(static_cast<Range::CompareHow>(how), [sourceRange rangeImpl], code);
    fillInError(error, code);
    return result;
}

- (void)deleteContents:(NSError **)error
{
    int code;
    [self rangeImpl]->deleteContents(code);
    fillInError(error, code);
}

- (DOMDocumentFragment *)extractContents:(NSError **)error
{
    int code;
    DOMDocumentFragment *result = [DOMDocumentFragment documentFragmentWithImpl:[self rangeImpl]->extractContents(code)];
    fillInError(error, code);
    return result;
}

- (DOMDocumentFragment *)cloneContents:(NSError **)error
{
    int code;
    DOMDocumentFragment *result = [DOMDocumentFragment documentFragmentWithImpl:[self rangeImpl]->cloneContents(code)];
    fillInError(error, code);
    return result;
}

- (void)insertNode:(DOMNode *)newNode error:(NSError **)error
{
    int code;
    [self rangeImpl]->insertNode([newNode nodeImpl], code);
    fillInError(error, code);
}

- (void)surroundContents:(DOMNode *)newParent error:(NSError **)error
{
    int code;
    [self rangeImpl]->surroundContents([newParent nodeImpl], code);
    fillInError(error, code);
}

- (DOMRange *)cloneRange:(NSError **)error
{
    int code;
    DOMRange *result = [DOMRange rangeWithImpl:[self rangeImpl]->cloneRange(code)];
    fillInError(error, code);
    return result;
}

- (NSString *)toString:(NSError **)error
{
    int code;
    NSString *result = DOMStringToNSString([self rangeImpl]->toString(code));
    fillInError(error, code);
    return result;
}

- (void)detach:(NSError **)error
{
    int code;
    [self rangeImpl]->detach(code);
    fillInError(error, code);
}

@end

@implementation DOMRange (WebCoreInternal)

- (id)initWithRangeImpl:(RangeImpl *)impl
{
    ASSERT(impl);

    [super init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMRange *)rangeWithImpl:(RangeImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithRangeImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
