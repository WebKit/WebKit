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

#include <objc/objc-class.h>

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
using DOM::CharacterDataImpl;
using DOM::DocumentFragmentImpl;
using DOM::DocumentType;
using DOM::DocumentTypeImpl;
using DOM::Document;
using DOM::DocumentImpl;
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
using DOM::NodeListImpl;
using DOM::NotationImpl;
using DOM::ProcessingInstructionImpl;
using DOM::Range;
using DOM::RangeException;
using DOM::RangeImpl;
using DOM::TextImpl;

@interface DOMAttr (WebCoreInternal)
+ (DOMAttr *)_attrWithImpl:(AttrImpl *)impl;
- (AttrImpl *)_attrImpl;
@end

@interface DOMDocumentFragment (WebCoreInternal)
+ (DOMDocumentFragment *)_documentFragmentWithImpl:(DocumentFragmentImpl *)impl;
@end

@interface DOMImplementation (WebCoreInternal)
+ (DOMImplementation *)_DOMImplementationWithImpl:(DOMImplementationImpl *)impl;
@end

@interface DOMNamedNodeMap (WebCoreInternal)
+ (DOMNamedNodeMap *)_namedNodeMapWithImpl:(NamedNodeMapImpl *)impl;
@end

@interface DOMNodeList (WebCoreInternal)
+ (DOMNodeList *)_nodeListWithImpl:(NodeListImpl *)impl;
@end

@interface DOMObject (WebCoreInternal)
- (id)_init;
@end

@interface DOMRange (WebCoreInternal)
+ (DOMRange *)_rangeWithImpl:(RangeImpl *)impl;
@end

//------------------------------------------------------------------------------------------
// Static functions and data

NSString * const DOMException = @"DOMException";
NSString * const DOMRangeException = @"DOMRangeException";

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

static void raiseDOMException(int code)
{
    ASSERT(code);
    
    NSString *name;
    if (code >= RangeException::_EXCEPTION_OFFSET) {
        name = DOMRangeException;
        code -= RangeException::_EXCEPTION_OFFSET;
    }
    else {
        name = DOMException;
    }
    NSString *reason = [NSString stringWithFormat:@"*** Exception received from DOM API: %d", code];
    NSException *exception = [NSException exceptionWithName:name reason:reason
        userInfo:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:code] forKey:name]];
    [exception raise];
}

static inline void raiseOnError(int code) 
{
    if (code) 
        raiseDOMException(code);
}

//------------------------------------------------------------------------------------------
// DOMString/NSString bridging

DOMString::operator NSString *() const
{
    return [NSString stringWithCharacters:reinterpret_cast<const unichar *>(unicode()) length:length()];
}

DOMString::DOMString(NSString *str)
{
    ASSERT(str);

    CFIndex size = CFStringGetLength(reinterpret_cast<CFStringRef>(str));
    if (size == 0)
        impl = DOMStringImpl::empty();
    else {
        UniChar fixedSizeBuffer[1024];
        UniChar *buffer;
        if (size > static_cast<CFIndex>(sizeof(fixedSizeBuffer) / sizeof(UniChar))) {
            buffer = static_cast<UniChar *>(malloc(size * sizeof(UniChar)));
        } else {
            buffer = fixedSizeBuffer;
        }
        CFStringGetCharacters(reinterpret_cast<CFStringRef>(str), CFRangeMake(0, size), buffer);
        impl = new DOMStringImpl(reinterpret_cast<const QChar *>(buffer), (uint)size);
        if (buffer != fixedSizeBuffer) {
            free(buffer);
        }
    }
    impl->ref();
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
        removeWrapperForImpl(_internal);
    }
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

@implementation DOMObject (WebCoreInternal)

- (id)_init
{
    return [super init];
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
    
    int code = 0;
    [self _nodeImpl]->setNodeValue(string, code);
    raiseOnError(code);
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
    return [DOMNodeList _nodeListWithImpl:[self _nodeImpl]->childNodes()];
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

    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _nodeImpl]->insertBefore([newChild _nodeImpl], [refChild _nodeImpl], code)];
    raiseOnError(code);
    return result;
}

- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild
{
    ASSERT(newChild);
    ASSERT(oldChild);

    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _nodeImpl]->replaceChild([newChild _nodeImpl], [oldChild _nodeImpl], code)];
    raiseOnError(code);
    return result;
}

- (DOMNode *)removeChild:(DOMNode *)oldChild
{
    ASSERT(oldChild);

    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _nodeImpl]->removeChild([oldChild _nodeImpl], code)];
    raiseOnError(code);
    return result;
}

- (DOMNode *)appendChild:(DOMNode *)newChild
{
    ASSERT(newChild);

    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _nodeImpl]->appendChild([newChild _nodeImpl], code)];
    raiseOnError(code);
    return result;
}

- (BOOL)hasChildNodes
{
    return [self _nodeImpl]->hasChildNodes();
}

- (DOMNode *)cloneNode:(BOOL)deep
{
    return [DOMNode _nodeWithImpl:[self _nodeImpl]->cloneNode(deep)];
}

- (void)normalize
{
    [self _nodeImpl]->normalize();
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    // Method not reflected in DOM::NodeImpl interface
    return Node([self _nodeImpl]).isSupported(feature, version);
}

- (NSString *)namespaceURI
{
    // Method not reflected in DOM::NodeImpl interface
    return Node([self _nodeImpl]).namespaceURI();
}

- (NSString *)prefix
{
    return [self _nodeImpl]->prefix();
}

- (void)setPrefix:(NSString *)prefix
{
    ASSERT(prefix);

    int code = 0;
    [self _nodeImpl]->setPrefix(prefix, code);
    raiseOnError(code);
}

- (NSString *)localName
{
    return [self _nodeImpl]->localName();
}

- (BOOL)hasAttributes
{
    // Method not reflected in DOM::NodeImpl interface
    return Node([self _nodeImpl]).hasAttributes();
}

- (NSString *)HTMLString
{
    return [self _nodeImpl]->recursive_toHTML(true).getNSString();
}

@end

@implementation DOMNode (WebCoreInternal)

- (id)_initWithNodeImpl:(NodeImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMNode *)_nodeWithImpl:(NodeImpl *)impl
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
    return [[[wrapperClass alloc] _initWithNodeImpl:impl] autorelease];
}

- (NodeImpl *)_nodeImpl
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

- (NamedNodeMapImpl *)_namedNodeMapImpl
{
    return reinterpret_cast<NamedNodeMapImpl *>(_internal);
}

- (DOMNode *)getNamedItem:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    NamedNodeMap map = NamedNodeMapImpl::createInstance([self _namedNodeMapImpl]);
    Node result(map.getNamedItem(name));
    return [DOMNode _nodeWithImpl:result.handle()];
}

- (DOMNode *)setNamedItem:(DOMNode *)arg
{
    ASSERT(arg);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self _namedNodeMapImpl]);
        Node result(map.setNamedItem([arg _nodeImpl]));
        return [DOMNode _nodeWithImpl:result.handle()];
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMNode *)removeNamedItem:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self _namedNodeMapImpl]);
        Node result(map.removeNamedItem(name));
        return [DOMNode _nodeWithImpl:result.handle()];
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMNode *)item:(unsigned long)index
{
    return [DOMNode _nodeWithImpl:[self _namedNodeMapImpl]->item(index)];
}

- (unsigned long)length
{
    return [self _namedNodeMapImpl]->length();
}

- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    if (!namespaceURI || !localName) {
        return nil;
    }

    // Method not reflected in DOM::NamedNodeMapImpl interface
    NamedNodeMap map = NamedNodeMapImpl::createInstance([self _namedNodeMapImpl]);
    Node result(map.getNamedItemNS(namespaceURI, localName));
    return [DOMNode _nodeWithImpl:result.handle()];
}

- (DOMNode *)setNamedItemNS:(DOMNode *)arg
{
    ASSERT(arg);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self _namedNodeMapImpl]);
        Node result(map.setNamedItemNS([arg _nodeImpl]));
        return [DOMNode _nodeWithImpl:result.handle()];
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self _namedNodeMapImpl]);
        Node result(map.removeNamedItemNS(namespaceURI, localName));
        return [DOMNode _nodeWithImpl:result.handle()];
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

@end

@implementation DOMNamedNodeMap (WebCoreInternal)

- (id)_initWithNamedNodeMapImpl:(NamedNodeMapImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMNamedNodeMap *)_namedNodeMapWithImpl:(NamedNodeMapImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
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
        reinterpret_cast<NodeListImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (NodeListImpl *)_nodeListImpl
{
    return reinterpret_cast<NodeListImpl *>(_internal);
}

- (DOMNode *)item:(unsigned long)index
{
    return [DOMNode _nodeWithImpl:[self _nodeListImpl]->item(index)];
}

- (unsigned long)length
{
    return [self _nodeListImpl]->length();
}

@end

@implementation DOMNodeList (WebCoreInternal)

- (id)_initWithNodeListImpl:(NodeListImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMNodeList *)_nodeListWithImpl:(NodeListImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
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
        reinterpret_cast<DOMImplementationImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (DOMImplementationImpl *)_DOMImplementationImpl
{
    return reinterpret_cast<DOMImplementationImpl *>(_internal);
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

    int code = 0;
    DocumentTypeImpl *impl = [self _DOMImplementationImpl]->createDocumentType(qualifiedName, publicId, systemId, code);
    raiseOnError(code);
    return static_cast<DOMDocumentType *>([DOMNode _nodeWithImpl:impl]);
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code = 0;
    DocumentType dt = DocumentTypeImpl::createInstance(static_cast<DocumentTypeImpl *>([doctype _nodeImpl]));
    DocumentImpl *impl = [self _DOMImplementationImpl]->createDocument(namespaceURI, qualifiedName, dt, code);
    raiseOnError(code);
    return static_cast<DOMDocument *>([DOMNode _nodeWithImpl:impl]);
}

@end
 
@implementation DOMImplementation (WebInternal)

- (id)_initWithDOMImplementationImpl:(DOMImplementationImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMImplementation *)_DOMImplementationWithImpl:(DOMImplementationImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithDOMImplementationImpl:impl] autorelease];
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

@end

//------------------------------------------------------------------------------------------
// DOMDocument

@implementation DOMDocument

- (DocumentImpl *)_documentImpl
{
    return static_cast<DocumentImpl *>(reinterpret_cast<NodeImpl *>(_internal));
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

    int code = 0;
    DOMElement *result = static_cast<DOMElement *>([DOMNode _nodeWithImpl:[self _documentImpl]->createElement(tagName, code)]);
    raiseOnError(code);
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

    // Documentation says we can raise a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report that error up to us.
    return static_cast<DOMCDATASection *>([DOMNode _nodeWithImpl:[self _documentImpl]->createCDATASection(data)]);
}

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data
{
    ASSERT(target);
    ASSERT(data);

    // Documentation says we can raise a INVALID_CHARACTER_ERR or a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report these errors up to us.
    return static_cast<DOMProcessingInstruction *>([DOMNode _nodeWithImpl:[self _documentImpl]->createProcessingInstruction(target, data)]);
}

- (DOMAttr *)createAttribute:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self _documentImpl]));
        Attr result(doc.createAttribute(name));
        return static_cast<DOMAttr *>([DOMNode _nodeWithImpl:result.handle()]);
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMEntityReference *)createEntityReference:(NSString *)name
{
    ASSERT(name);

    // Documentation says we can raise a INVALID_CHARACTER_ERR or a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report these errors up to us.
    return static_cast<DOMEntityReference *>([DOMNode _nodeWithImpl:[self _documentImpl]->createEntityReference(name)]);
}

- (DOMNodeList *)getElementsByTagName:(NSString *)tagname
{
    ASSERT(tagname);
    return [DOMNodeList _nodeListWithImpl:[self _documentImpl]->getElementsByTagNameNS(0, DOMString(tagname).implementation())];
}

- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep
{
    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _documentImpl]->importNode([importedNode _nodeImpl], deep, code)];
    raiseOnError(code);
    return result;
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _documentImpl]->createElementNS(namespaceURI, qualifiedName, code)];
    raiseOnError(code);
    return static_cast<DOMElement *>(result);
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self _documentImpl]));
        Attr result(doc.createAttributeNS(namespaceURI, qualifiedName));
        return static_cast<DOMAttr *>([DOMNode _nodeWithImpl:result.handle()]);
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList _nodeListWithImpl:[self _documentImpl]->getElementsByTagNameNS(DOMString(namespaceURI).implementation(), DOMString(localName).implementation())];
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    ASSERT(elementId);

    return static_cast<DOMElement *>([DOMNode _nodeWithImpl:[self _documentImpl]->getElementById(elementId)]);
}

@end

@implementation DOMDocument (WebCoreInternal)

+ (DOMDocument *)_documentWithImpl:(DocumentImpl *)impl
{
    return static_cast<DOMDocument *>([DOMNode _nodeWithImpl:impl]);
}

@end

//------------------------------------------------------------------------------------------
// DOMCharacterData

@implementation DOMCharacterData

- (CharacterDataImpl *)_characterDataImpl
{
    return static_cast<CharacterDataImpl *>(reinterpret_cast<NodeImpl *>(_internal));
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
    
    int code = 0;
    [self _characterDataImpl]->setData(data, code);
    raiseOnError(code);
}

- (unsigned long)length
{
    return [self _characterDataImpl]->length();
}

- (NSString *)substringData:(unsigned long)offset :(unsigned long)count
{
    int code = 0;
    NSString *result = [self _characterDataImpl]->substringData(offset, count, code);
    raiseOnError(code);
    return result;
}

- (void)appendData:(NSString *)arg
{
    ASSERT(arg);
    
    int code = 0;
    [self _characterDataImpl]->appendData(arg, code);
    raiseOnError(code);
}

- (void)insertData:(unsigned long)offset :(NSString *)arg
{
    ASSERT(arg);
    
    int code = 0;
    [self _characterDataImpl]->insertData(offset, arg, code);
    raiseOnError(code);
}

- (void)deleteData:(unsigned long)offset :(unsigned long) count;
{
    int code = 0;
    [self _characterDataImpl]->deleteData(offset, count, code);
    raiseOnError(code);
}

- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg
{
    ASSERT(arg);

    int code = 0;
    [self _characterDataImpl]->replaceData(offset, count, arg, code);
    raiseOnError(code);
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

    int code = 0;
    [self _attrImpl]->setValue(value, code);
    raiseOnError(code);
}

- (DOMElement *)ownerElement
{
    return [DOMElement _elementWithImpl:[self _attrImpl]->ownerElement()];
}

@end

@implementation DOMAttr (WebCoreInternal)

+ (DOMAttr *)_attrWithImpl:(AttrImpl *)impl
{
    return static_cast<DOMAttr *>([DOMNode _nodeWithImpl:impl]);
}

- (AttrImpl *)_attrImpl
{
    return static_cast<AttrImpl *>(reinterpret_cast<NodeImpl *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

@implementation DOMElement

- (NSString *)tagName
{
    return [self _elementImpl]->tagName();
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

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self _elementImpl]));
        element.setAttribute(name, value);
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
    }
}

- (void)removeAttribute:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self _elementImpl]));
        element.removeAttribute(name);
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
    }
}

- (DOMAttr *)getAttributeNode:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self _elementImpl]));
    Attr result(element.getAttributeNode(name));
    return [DOMAttr _attrWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr
{
    ASSERT(newAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self _elementImpl]));
        Attr attr(AttrImpl::createInstance([newAttr _attrImpl]));
        Attr result(element.setAttributeNode(attr));
        return [DOMAttr _attrWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr
{
    ASSERT(oldAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self _elementImpl]));
        Attr attr(AttrImpl::createInstance([oldAttr _attrImpl]));
        Attr result(element.removeAttributeNode(attr));
        return [DOMAttr _attrWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    ASSERT(name);

    return [DOMNodeList _nodeListWithImpl:[self _elementImpl]->getElementsByTagNameNS(0, DOMString(name).implementation())];
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    Element element(ElementImpl::createInstance([self _elementImpl]));
    return element.getAttributeNS(namespaceURI, localName);
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);
    ASSERT(value);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self _elementImpl]));
        element.setAttributeNS(namespaceURI, qualifiedName, value);
    }
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
    }
}

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self _elementImpl]));
        element.removeAttributeNS(namespaceURI, localName);
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
    }
}

- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self _elementImpl]));
    Attr result(element.getAttributeNodeNS(namespaceURI, localName));
    return [DOMAttr _attrWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr
{
    ASSERT(newAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self _elementImpl]));
        Attr attr(AttrImpl::createInstance([newAttr _attrImpl]));
        Attr result(element.setAttributeNodeNS(attr));
        return [DOMAttr _attrWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOM::DOMException &e) {
        raiseOnError(e.code);
        return nil;
    }
}

- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [DOMNodeList _nodeListWithImpl:[self _elementImpl]->getElementsByTagNameNS(DOMString(namespaceURI).implementation(), DOMString(localName).implementation())];
}

- (BOOL)hasAttribute:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self _elementImpl]));
    return element.hasAttribute(name);
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self _elementImpl]));
    return element.hasAttributeNS(namespaceURI, localName);
}

@end

@implementation DOMElement (WebCoreInternal)

+ (DOMElement *)_elementWithImpl:(ElementImpl *)impl
{
    return static_cast<DOMElement *>([DOMNode _nodeWithImpl:impl]);
}

- (ElementImpl *)_elementImpl
{
    return static_cast<ElementImpl *>(reinterpret_cast<NodeImpl *>(_internal));
}

@end

//------------------------------------------------------------------------------------------
// DOMText

@implementation DOMText

- (TextImpl *)_textImpl
{
    return static_cast<TextImpl *>(reinterpret_cast<NodeImpl *>(_internal));
}

- (DOMText *)splitText:(unsigned long)offset
{
    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _textImpl]->splitText(offset, code)];
    raiseOnError(code);
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

- (DocumentTypeImpl *)_documentTypeImpl
{
    return static_cast<DocumentTypeImpl *>(reinterpret_cast<NodeImpl *>(_internal));
}

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

//------------------------------------------------------------------------------------------
// DOMNotation

@implementation DOMNotation

- (NotationImpl *)_notationImpl
{
    return static_cast<NotationImpl *>(reinterpret_cast<NodeImpl *>(_internal));
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
    return static_cast<EntityImpl *>(reinterpret_cast<NodeImpl *>(_internal));
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
    return static_cast<ProcessingInstructionImpl *>(reinterpret_cast<NodeImpl *>(_internal));
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

    int code = 0;
    [self _processingInstructionImpl]->setData(data, code);
    raiseOnError(code);
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

- (RangeImpl *)_rangeImpl
{
    return reinterpret_cast<RangeImpl *>(_internal);
}

- (DOMNode *)startContainer
{
    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _rangeImpl]->startContainer(code)];
    raiseOnError(code);
    return result;
}

- (long)startOffset
{
    int code = 0;
    long result = [self _rangeImpl]->startOffset(code);
    raiseOnError(code);
    return result;
}

- (DOMNode *)endContainer
{
    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _rangeImpl]->endContainer(code)];
    raiseOnError(code);
    return result;
}

- (long)endOffset
{
    int code = 0;
    long result = [self _rangeImpl]->endOffset(code);
    raiseOnError(code);
    return result;
}

- (BOOL)collapsed
{
    int code = 0;
    BOOL result = [self _rangeImpl]->collapsed(code);
    raiseOnError(code);
    return result;
}

- (DOMNode *)commonAncestorContainer
{
    int code = 0;
    DOMNode *result = [DOMNode _nodeWithImpl:[self _rangeImpl]->commonAncestorContainer(code)];
    raiseOnError(code);
    return result;
}

- (void)setStart:(DOMNode *)refNode :(long)offset
{
    int code = 0;
    [self _rangeImpl]->setStart([refNode _nodeImpl], offset, code);
    raiseOnError(code);
}

- (void)setEnd:(DOMNode *)refNode :(long)offset
{
    int code = 0;
    [self _rangeImpl]->setEnd([refNode _nodeImpl], offset, code);
    raiseOnError(code);
}

- (void)setStartBefore:(DOMNode *)refNode
{
    int code = 0;
    [self _rangeImpl]->setStartBefore([refNode _nodeImpl], code);
    raiseOnError(code);
}

- (void)setStartAfter:(DOMNode *)refNode
{
    int code = 0;
    [self _rangeImpl]->setStartAfter([refNode _nodeImpl], code);
    raiseOnError(code);
}

- (void)setEndBefore:(DOMNode *)refNode
{
    int code = 0;
    [self _rangeImpl]->setEndBefore([refNode _nodeImpl], code);
    raiseOnError(code);
}

- (void)setEndAfter:(DOMNode *)refNode
{
    int code = 0;
    [self _rangeImpl]->setEndAfter([refNode _nodeImpl], code);
    raiseOnError(code);
}

- (void)collapse:(BOOL)toStart
{
    int code = 0;
    [self _rangeImpl]->collapse(toStart, code);
    raiseOnError(code);
}

- (void)selectNode:(DOMNode *)refNode
{
    int code = 0;
    [self _rangeImpl]->selectNode([refNode _nodeImpl], code);
    raiseOnError(code);
}

- (void)selectNodeContents:(DOMNode *)refNode
{
    int code = 0;
    [self _rangeImpl]->selectNodeContents([refNode _nodeImpl], code);
    raiseOnError(code);
}

- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange
{
    int code = 0;
    short result = [self _rangeImpl]->compareBoundaryPoints(static_cast<Range::CompareHow>(how), [sourceRange _rangeImpl], code);
    raiseOnError(code);
    return result;
}

- (void)deleteContents
{
    int code = 0;
    [self _rangeImpl]->deleteContents(code);
    raiseOnError(code);
}

- (DOMDocumentFragment *)extractContents
{
    int code = 0;
    DOMDocumentFragment *result = [DOMDocumentFragment _documentFragmentWithImpl:[self _rangeImpl]->extractContents(code)];
    raiseOnError(code);
    return result;
}

- (DOMDocumentFragment *)cloneContents
{
    int code = 0;
    DOMDocumentFragment *result = [DOMDocumentFragment _documentFragmentWithImpl:[self _rangeImpl]->cloneContents(code)];
    raiseOnError(code);
    return result;
}

- (void)insertNode:(DOMNode *)newNode
{
    int code = 0;
    [self _rangeImpl]->insertNode([newNode _nodeImpl], code);
    raiseOnError(code);
}

- (void)surroundContents:(DOMNode *)newParent
{
    int code = 0;
    [self _rangeImpl]->surroundContents([newParent _nodeImpl], code);
    raiseOnError(code);
}

- (DOMRange *)cloneRange
{
    int code = 0;
    DOMRange *result = [DOMRange _rangeWithImpl:[self _rangeImpl]->cloneRange(code)];
    raiseOnError(code);
    return result;
}

- (NSString *)toString
{
    int code = 0;
    NSString *result = [self _rangeImpl]->toString(code);
    raiseOnError(code);
    return result;
}

- (void)detach
{
    int code = 0;
    [self _rangeImpl]->detach(code);
    raiseOnError(code);
}

@end

@implementation DOMRange (WebCoreInternal)

- (id)_initWithRangeImpl:(RangeImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setWrapperForImpl(self, impl);
    return self;
}

+ (DOMRange *)_rangeWithImpl:(RangeImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithRangeImpl:impl] autorelease];
}

@end

//------------------------------------------------------------------------------------------
