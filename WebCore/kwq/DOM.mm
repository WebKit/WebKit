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

#import "DOM.h"
#import "DOMInternal.h"

#import <Foundation/Foundation.h>

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
#import "KWQLogging.h"

using DOM::Attr;
using DOM::AttrImpl;
using DOM::CDATASectionImpl;
using DOM::CharacterData;
using DOM::CharacterDataImpl;
using DOM::CommentImpl;
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
using DOM::EntityReferenceImpl;
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

@class WebCoreDOMAttr;
@class WebCoreDOMCDATASection;
@class WebCoreDOMCharacterData;
@class WebCoreDOMComment;
@class WebCoreDOMDocumentFragment;
@class WebCoreDOMDocumentType;
@class WebCoreDOMDocument;
@class WebCoreDOMImplementation;
@class WebCoreDOMElement;
@class WebCoreDOMEntity;
@class WebCoreDOMEntityReference;
@class WebCoreDOMNamedNodeMap;
@class WebCoreDOMNode;
@class WebCoreDOMNodeList;
@class WebCoreDOMNotation;
@class WebCoreDOMProcessingInstruction;
@class WebCoreDOMRange;
@class WebCoreDOMText;

//------------------------------------------------------------------------------------------
// Static functions and data

NSString * const DOMErrorDomain = @"DOMErrorDomain";

static CFMutableDictionaryRef wrapperCache(void)
{
    static CFMutableDictionaryRef wrapperCache = NULL;
    if (!wrapperCache) {
        // No need to retain/free either impl key, or id value.  Items will be removed
        // from the cache in WebCoreDOMNode's dealloc method.
        wrapperCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    return wrapperCache;
}

static id wrapperForImpl(const void *impl)
{
    return (id)CFDictionaryGetValue(wrapperCache(), impl);
}

static void setWrapperForImpl(id wrapper, const void *impl)
{
    CFDictionarySetValue(wrapperCache(), impl, wrapper);
}

static void removeWrapperForImpl(const void *impl)
{
    CFDictionaryRemoveValue(wrapperCache(), impl);
}

static NSString *DOMStringToNSString(const DOMString &aString)
{
    return [NSString stringWithCharacters:(unichar *)aString.unicode() length:aString.length()];
}

static DOMString NSStringToDOMString(NSString *aString)
{
    QChar *chars = (QChar *)malloc([aString length] * sizeof(QChar));
    [aString getCharacters:(unichar *)chars];
    DOMString ret(chars, [aString length]);
    free(chars);
    return ret;
}

static void fillInError(NSError **error, int code)
{
    if (!error || !code)
        return;
        
    *error = [NSError errorWithDomain:DOMErrorDomain code:code userInfo:nil];
}

#define AbstractMethodCalled(absClass) do { \
	if ([self class] == absClass) \
		[NSException raise:NSInvalidArgumentException format:@"*** -%s cannot be sent to an abstract object of class %s: You must create a concrete instance.", sel_getName(_cmd), absClass->name]; \
	else \
		[NSException raise:NSInvalidArgumentException format:@"*** -%s only defined for abstract class. You must define -[%s %s]", sel_getName(_cmd), object_getClassName(self), sel_getName(_cmd)]; \
	} while (0)

//------------------------------------------------------------------------------------------
// Factory methods

NodeList NodeListImpl::createInstance(NodeListImpl *impl)
{
    return NodeList(impl);
}

NamedNodeMap NamedNodeMapImpl::createInstance(NamedNodeMapImpl *impl)
{
    return NamedNodeMap(impl);
}

Attr AttrImpl::createInstance(AttrImpl *impl)
{
    return Attr(impl);
}

Element ElementImpl::createInstance(ElementImpl *impl)
{
    return Element(impl);
}

CharacterData CharacterDataImpl::createInstance(CharacterDataImpl *impl)
{
    return CharacterData(impl);
}

Text TextImpl::createInstance(TextImpl *impl)
{
    return Text(impl);
}

ProcessingInstruction ProcessingInstructionImpl::createInstance(ProcessingInstructionImpl *impl)
{
    return ProcessingInstruction(impl);
}

DocumentType DocumentTypeImpl::createInstance(DocumentTypeImpl *impl)
{
    return DocumentType(impl);
}

Document DocumentImpl::createInstance(DocumentImpl *impl)
{
    return Document(impl);
}

Range RangeImpl::createInstance(RangeImpl *impl)
{
    return Range(impl);
}

//------------------------------------------------------------------------------------------
// DOMNode

@implementation DOMNode

- (NodeImpl *)nodeImpl
{
	AbstractMethodCalled([DOMNode class]);
    return nil;
}

- (Class)classForDOMDocument
{
	AbstractMethodCalled([DOMNode class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
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
    return [[self classForDOMDocument] documentWithImpl:[self nodeImpl]->getDocument()];
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
    Node node([self nodeImpl]);
    return node.isSupported(NSStringToDOMString(feature), NSStringToDOMString(version));
}

- (NSString *)namespaceURI
{
    // Method not reflected in DOM::NodeImpl interface
    Node node([self nodeImpl]);
    return DOMStringToNSString(node.namespaceURI());
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
    Node node([self nodeImpl]);
    return node.hasAttributes();
}

- (NSString *)HTMLString
{
    return [self nodeImpl]->recursive_toHTML(true).getNSString();
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMNode

@implementation WebCoreDOMNode

- (id)initWithNodeImpl:(NodeImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
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
    
    return [[[self alloc] initWithNodeImpl:impl checkCache:NO] autorelease];
}

- (id)initWithNodeImpl:(NodeImpl *)impl
{
    return [self initWithNodeImpl:impl checkCache:YES];
}

- (NodeImpl *)nodeImpl
{
	return m_impl;
}

- (Class)classForDOMDocument
{
	return [WebCoreDOMDocument class];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMNamedNodeMap

@implementation DOMNamedNodeMap

- (NamedNodeMapImpl *)namedNodeMapImpl
{
	AbstractMethodCalled([DOMNamedNodeMap class]);
    return nil;
}

- (Class)classForDOMNode
{
	AbstractMethodCalled([DOMNamedNodeMap class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (DOMNode *)getNamedItem:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
    Node result(map.getNamedItem(NSStringToDOMString(name)));
    return [[self classForDOMNode] nodeWithImpl:result.handle()];
}

- (DOMNode *)setNamedItem:(DOMNode *)arg error:(NSError **)error
{
    ASSERT(arg);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
        Node result(map.setNamedItem([arg nodeImpl]));
        return [[self classForDOMNode] nodeWithImpl:result.handle()];
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
        return [[self classForDOMNode] nodeWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNode *)item:(unsigned long)index
{
    return [[self classForDOMNode] nodeWithImpl:[self namedNodeMapImpl]->item(index)];
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
    return [[self classForDOMNode] nodeWithImpl:result.handle()];
}

- (DOMNode *)setNamedItemNS:(DOMNode *)arg error:(NSError **)error
{
    ASSERT(arg);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self namedNodeMapImpl]);
        Node result(map.setNamedItemNS([arg nodeImpl]));
        return [[self classForDOMNode] nodeWithImpl:result.handle()];
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
        return [[self classForDOMNode] nodeWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMNamedNodeMap

@implementation WebCoreDOMNamedNodeMap

- (id)initWithNamedNodeMapImpl:(NamedNodeMapImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
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
    
    return [[[self alloc] initWithNamedNodeMapImpl:impl checkCache:NO] autorelease];
}

- (id)initWithNamedNodeMapImpl:(NamedNodeMapImpl *)impl
{
    return [self initWithNamedNodeMapImpl:impl checkCache:YES];
}

- (NamedNodeMapImpl *)namedNodeMapImpl
{
	return m_impl;
}

- (Class)classForDOMNode
{
	return [WebCoreDOMNode class];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMNodeList

@implementation DOMNodeList

- (NodeListImpl *)nodeListImpl
{
	AbstractMethodCalled([DOMNodeList class]);
    return nil;
}

- (Class)classForDOMNode
{
	AbstractMethodCalled([DOMNodeList class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (DOMNode *)item:(unsigned long)index
{
    return [[self classForDOMNode] nodeWithImpl:[self nodeListImpl]->item(index)];
}

- (unsigned long)length
{
    return [self nodeListImpl]->length();
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMNodeList

@implementation WebCoreDOMNodeList

- (id)initWithNodeListImpl:(NodeListImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
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
    
    return [[[self alloc] initWithNodeListImpl:impl checkCache:NO] autorelease];
}

- (id)initWithNodeListImpl:(NodeListImpl *)impl
{
    return [self initWithNodeListImpl:impl checkCache:YES];
}

- (NodeListImpl *)nodeListImpl
{
	return m_impl;
}

- (Class)classForDOMNode
{
	return [WebCoreDOMNode class];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMImplementation

@implementation DOMImplementation

- (DOMImplementationImpl *)DOMImplementationImpl
{
	AbstractMethodCalled([DOMImplementation class]);
    return nil;
}

- (Class)classForDOMDocumentType
{
	AbstractMethodCalled([DOMImplementation class]);
    return nil;
}

- (Class)classForDOMDocument
{
	AbstractMethodCalled([DOMImplementation class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
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
    DOMDocumentType * result = [[self classForDOMDocumentType] documentTypeWithImpl:impl];
    fillInError(error, code);
    return result;
}

- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code;
    DocumentType dt = DocumentTypeImpl::createInstance([doctype documentTypeImpl]);
    DocumentImpl *impl = [self DOMImplementationImpl]->createDocument(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName), dt, code);
    DOMDocument * result = [[self classForDOMDocument] documentWithImpl:impl];
    fillInError(error, code);
    return result;
}

@end
 
//------------------------------------------------------------------------------------------
// WebCoreDOMImplementation

@implementation WebCoreDOMImplementation

- (id)initWithDOMImplementationImpl:(DOMImplementationImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
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
    
    return [[[self alloc] initWithDOMImplementationImpl:impl checkCache:NO] autorelease];
}

- (id)initWithDOMImplementationImpl:(DOMImplementationImpl *)impl
{
    return [self initWithDOMImplementationImpl:impl checkCache:YES];
}

- (DOMImplementationImpl *)DOMImplementationImpl
{
	return m_impl;
}

- (Class)classForDOMDocumentType
{
	return [WebCoreDOMDocumentType class];
}

- (Class)classForDOMDocument
{
	return [WebCoreDOMDocument class];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMDocumentFragment

@implementation DOMDocumentFragment

- (DocumentFragmentImpl *)documentFragmentImpl
{
	AbstractMethodCalled([DOMDocumentFragment class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMDocumentFragment

@implementation WebCoreDOMDocumentFragment

- (id)initWithDocumentFragmentImpl:(DocumentFragmentImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMDocumentFragment *)documentFragmentWithImpl:(DocumentFragmentImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithDocumentFragmentImpl:impl checkCache:NO] autorelease];
}

- (id)initWithDocumentFragmentImpl:(DocumentFragmentImpl *)impl
{
    return [self initWithDocumentFragmentImpl:impl checkCache:YES];
}

- (DocumentFragmentImpl *)documentFragmentImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMDocument

@implementation DOMDocument

- (DocumentImpl *)documentImpl
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMAttr
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMCDATASection
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMComment
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMDocumentFragment
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMDocumentType
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMElement
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMEntityReference
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMImplementation
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMNode
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMNodeList
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMProcessingInstruction
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (Class)classForDOMText
{
	AbstractMethodCalled([DOMDocument class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (DOMDocumentType *)doctype
{
    return [[self classForDOMDocumentType] documentTypeWithImpl:[self documentImpl]->doctype()];
}

- (DOMImplementation *)implementation
{
    return [[self classForDOMImplementation] DOMImplementationWithImpl:[self documentImpl]->implementation()];
}

- (DOMElement *)documentElement
{
    return [[self classForDOMElement] elementWithImpl:[self documentImpl]->documentElement()];
}

- (DOMElement *)createElement:(NSString *)tagName error:(NSError **)error
{
    ASSERT(tagName);

    int code;
    DOMElement *result = [[self classForDOMElement] elementWithImpl:[self documentImpl]->createElement(NSStringToDOMString(tagName), code)];
    fillInError(error, code);
    return result;
}

- (DOMDocumentFragment *)createDocumentFragment
{
    return [[self classForDOMDocumentFragment] documentFragmentWithImpl:[self documentImpl]->createDocumentFragment()];
}

- (DOMText *)createTextNode:(NSString *)data
{
    ASSERT(data);

    return [[self classForDOMText] textWithImpl:[self documentImpl]->createTextNode(NSStringToDOMString(data))];
}

- (DOMComment *)createComment:(NSString *)data
{
    ASSERT(data);

    return [[self classForDOMComment] commentWithImpl:[self documentImpl]->createComment(NSStringToDOMString(data))];
}

- (DOMCDATASection *)createCDATASection:(NSString *)data error:(NSError **)error
{
    ASSERT(data);

    // Documentation says we can raise a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report that error up to us.
    return [[self classForDOMCDATASection] CDATASectionWithImpl:[self documentImpl]->createCDATASection(NSStringToDOMString(data))];
}

- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data error:(NSError **)error
{
    ASSERT(target);
    ASSERT(data);

    // Documentation says we can raise a INVALID_CHARACTER_ERR or a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report these errors up to us.
    return [[self classForDOMProcessingInstruction] processingInstructionWithImpl:[self documentImpl]->createProcessingInstruction(NSStringToDOMString(target), NSStringToDOMString(data))];
}

- (DOMAttr *)createAttribute:(NSString *)name error:(NSError **)error
{
    ASSERT(name);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self documentImpl]));
        Attr result(doc.createAttribute(NSStringToDOMString(name)));
        AttrImpl *impl = static_cast<AttrImpl *>(result.handle());
        return [[self classForDOMAttr] attrWithImpl:impl];
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
    return [WebCoreDOMEntityReference entityReferenceWithImpl:[self documentImpl]->createEntityReference(NSStringToDOMString(name))];
}

- (DOMNodeList *)getElementsByTagName:(NSString *)tagname
{
    ASSERT(tagname);

    return [[self classForDOMNodeList] nodeListWithImpl:[self documentImpl]->getElementsByTagNameNS(0, NSStringToDOMString(tagname).implementation())];
}

- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep error:(NSError **)error
{
    int code;
    DOMNode *result = [WebCoreDOMNode nodeWithImpl:[self documentImpl]->importNode([importedNode nodeImpl], deep, code)];
    fillInError(error, code);
    return result;
}

- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code;
    DOMElement *result = [[self classForDOMElement] elementWithImpl:[self documentImpl]->createElementNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName), code)];
    fillInError(error, code);
    return result;
}

- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self documentImpl]));
        Attr result(doc.createAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName)));
        AttrImpl *impl = static_cast<AttrImpl *>(result.handle());
        return [[self classForDOMAttr] attrWithImpl:impl];
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

    return [[self classForDOMNodeList] nodeListWithImpl:[self documentImpl]->getElementsByTagNameNS(NSStringToDOMString(namespaceURI).implementation(), NSStringToDOMString(localName).implementation())];
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    ASSERT(elementId);

    return [[self classForDOMElement] elementWithImpl:[self documentImpl]->getElementById(NSStringToDOMString(elementId))];
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMDocumentFragment

@implementation WebCoreDOMDocument

- (id)initWithDocumentImpl:(DocumentImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMDocument *)documentWithImpl:(DocumentImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithDocumentImpl:impl checkCache:NO] autorelease];
}

- (id)initWithDocumentImpl:(DocumentImpl *)impl
{
    return [self initWithDocumentImpl:impl checkCache:YES];
}

- (DocumentImpl *)documentImpl
{
	return m_impl;
}

- (Class)classForDOMAttr
{
	return [WebCoreDOMAttr class];
}

- (Class)classForDOMCDATASection
{
	return [WebCoreDOMCDATASection class];
}

- (Class)classForDOMComment
{
	return [WebCoreDOMComment class];
}

- (Class)classForDOMDocumentFragment
{
	return [WebCoreDOMDocumentFragment class];
}

- (Class)classForDOMDocumentType
{
	return [WebCoreDOMDocumentType class];
}

- (Class)classForDOMElement
{
	return [WebCoreDOMElement class];
}

- (Class)classForDOMEntityReference
{
	return [WebCoreDOMEntityReference class];
}

- (Class)classForDOMImplementation
{
	return [WebCoreDOMImplementation class];
}

- (Class)classForDOMNode
{
	return [WebCoreDOMNode class];
}

- (Class)classForDOMNodeList
{
	return [WebCoreDOMNodeList class];
}

- (Class)classForDOMProcessingInstruction
{
	return [WebCoreDOMProcessingInstruction class];
}

- (Class)classForDOMText
{
	return [WebCoreDOMText class];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMCharacterData

@implementation DOMCharacterData

- (CharacterDataImpl *)characterDataImpl
{
	AbstractMethodCalled([DOMCharacterData class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
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
// WebCoreDOMCharacterData

@implementation WebCoreDOMCharacterData

- (id)initWithCharacterDataImpl:(CharacterDataImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMCharacterData *)characterDataWithImpl:(CharacterDataImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithCharacterDataImpl:impl checkCache:NO] autorelease];
}

- (id)initWithCharacterDataImpl:(CharacterDataImpl *)impl
{
    return [self initWithCharacterDataImpl:impl checkCache:YES];
}

- (CharacterDataImpl *)characterDataImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMAttr

@implementation DOMAttr

- (AttrImpl *)attrImpl
{
	AbstractMethodCalled([DOMAttr class]);
    return nil;
}

- (Class)classForDOMElement
{
	AbstractMethodCalled([DOMAttr class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

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
    return [[self classForDOMElement] elementWithImpl:[self attrImpl]->ownerElement()];
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMAttr

@implementation WebCoreDOMAttr

- (id)initWithAttrImpl:(AttrImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMAttr *)attrWithImpl:(AttrImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithAttrImpl:impl checkCache:NO] autorelease];
}

- (id)initWithAttrImpl:(AttrImpl *)impl
{
    return [self initWithAttrImpl:impl checkCache:YES];
}

- (AttrImpl *)attrImpl
{
	return m_impl;
}

- (Class)classForDOMElement
{
	return [WebCoreDOMElement class];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMElement

@implementation DOMElement

- (ElementImpl *)elementImpl
{
	AbstractMethodCalled([DOMElement class]);
    return nil;
}

- (Class)classForDOMAttr
{
	AbstractMethodCalled([DOMElement class]);
    return nil;
}

- (Class)classForDOMNodeList
{
	AbstractMethodCalled([DOMElement class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (NSString *)tagName
{
    return DOMStringToNSString([self elementImpl]->tagName());
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
    return [[self classForDOMAttr] attrWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr error:(NSError **)error
{
    ASSERT(newAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        Attr attr(AttrImpl::createInstance([newAttr attrImpl]));
        Attr result(element.setAttributeNode(attr));
        return [[self classForDOMAttr] attrWithImpl:static_cast<AttrImpl *>(result.handle())];
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
        return [[self classForDOMAttr] attrWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (DOMNodeList *)getElementsByTagName:(NSString *)name
{
    ASSERT(name);

    return [[self classForDOMNodeList] nodeListWithImpl:[self elementImpl]->getElementsByTagNameNS(0, NSStringToDOMString(name).implementation())];
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
    return [[self classForDOMAttr] attrWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr error:(NSError **)error
{
    ASSERT(newAttr);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self elementImpl]));
        Attr attr(AttrImpl::createInstance([newAttr attrImpl]));
        Attr result(element.setAttributeNodeNS(attr));
        return [[self classForDOMAttr] attrWithImpl:static_cast<AttrImpl *>(result.handle())];
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

    return [[self classForDOMNodeList] nodeListWithImpl:[self elementImpl]->getElementsByTagNameNS(NSStringToDOMString(namespaceURI).implementation(), NSStringToDOMString(localName).implementation())];
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

//------------------------------------------------------------------------------------------
// WebCoreDOMElement

@implementation WebCoreDOMElement

- (id)initWithElementImpl:(ElementImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMElement *)elementWithImpl:(ElementImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithElementImpl:impl checkCache:NO] autorelease];
}

- (id)initWithElementImpl:(ElementImpl *)impl
{
    return [self initWithElementImpl:impl checkCache:YES];
}

- (ElementImpl *)elementImpl
{
	return m_impl;
}

- (Class)classForDOMAttr
{
	return [WebCoreDOMAttr class];
}

- (Class)classForDOMNodeList
{
	return [WebCoreDOMNodeList class];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMText

@implementation DOMText

- (TextImpl *)textImpl
{
	AbstractMethodCalled([DOMText class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (DOMText *)splitText:(unsigned long)offset error:(NSError **)error
{
    int code;
    DOMText *result = [[self class] textWithImpl:[self textImpl]->splitText(offset, code)];
    fillInError(error, code);
    return result;
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMText

@implementation WebCoreDOMText

- (id)initWithTextImpl:(TextImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMText *)textWithImpl:(TextImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithTextImpl:impl checkCache:NO] autorelease];
}

- (id)initWithTextImpl:(TextImpl *)impl
{
    return [self initWithTextImpl:impl checkCache:YES];
}

- (TextImpl *)textImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMComment

@implementation DOMComment

- (CommentImpl *)commentImpl
{
	AbstractMethodCalled([DOMComment class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMComment

@implementation WebCoreDOMComment

- (id)initWithCommentImpl:(CommentImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMComment *)commentWithImpl:(CommentImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithCommentImpl:impl checkCache:NO] autorelease];
}

- (id)initWithCommentImpl:(CommentImpl *)impl
{
    return [self initWithCommentImpl:impl checkCache:YES];
}

- (CommentImpl *)commentImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMCDATASection

@implementation DOMCDATASection

- (CDATASectionImpl *)CDATASectionImpl
{
	AbstractMethodCalled([DOMCDATASection class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMCDATASection

@implementation WebCoreDOMCDATASection

- (id)initWithCDATASectionImpl:(CDATASectionImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMCDATASection *)CDATASectionWithImpl:(CDATASectionImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithCDATASectionImpl:impl checkCache:NO] autorelease];
}

- (id)initWithCDATASectionImpl:(CDATASectionImpl *)impl
{
    return [self initWithCDATASectionImpl:impl checkCache:YES];
}

- (CDATASectionImpl *)CDATASectionImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMDocumentType

@implementation DOMDocumentType

- (DocumentTypeImpl *)documentTypeImpl
{
	AbstractMethodCalled([DOMDocumentType class]);
    return nil;
}

- (Class)classForDOMNamedNodeMap
{
	AbstractMethodCalled([DOMDocumentType class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (NSString *)name
{
    return DOMStringToNSString([self documentTypeImpl]->publicId());
}

- (DOMNamedNodeMap *)entities
{
    return [[self classForDOMNamedNodeMap] namedNodeMapWithImpl:[self documentTypeImpl]->entities()];
}

- (DOMNamedNodeMap *)notations
{
    return [[self classForDOMNamedNodeMap] namedNodeMapWithImpl:[self documentTypeImpl]->notations()];
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
// WebCoreDOMDocumentType

@implementation WebCoreDOMDocumentType

- (id)initWithDocumentTypeImpl:(DocumentTypeImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMDocumentType *)documentTypeWithImpl:(DocumentTypeImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithDocumentTypeImpl:impl checkCache:NO] autorelease];
}

- (id)initWithDocumentTypeImpl:(DocumentTypeImpl *)impl
{
    return [self initWithDocumentTypeImpl:impl checkCache:YES];
}

- (DocumentTypeImpl *)documentTypeImpl
{
	return m_impl;
}

- (Class)classForDOMNamedNodeMap
{
	return [WebCoreDOMNamedNodeMap class];
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMNotation

@implementation DOMNotation

- (NotationImpl *)notationImpl
{
	AbstractMethodCalled([DOMNotation class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
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
// WebCoreDOMNotation

@implementation WebCoreDOMNotation

- (id)initWithNotationImpl:(NotationImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMNotation *)notationWithImpl:(NotationImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithNotationImpl:impl checkCache:NO] autorelease];
}

- (id)initWithNotationImpl:(NotationImpl *)impl
{
	return [self initWithNotationImpl:impl checkCache:YES];
}

- (NotationImpl *)notationImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMEntity

@implementation DOMEntity

- (EntityImpl *)entityImpl
{
	AbstractMethodCalled([DOMEntity class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
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
// WebCoreDOMEntity

@implementation WebCoreDOMEntity

- (id)initWithEntityImpl:(EntityImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMEntity *)entityWithImpl:(EntityImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithEntityImpl:impl checkCache:NO] autorelease];
}

- (id)initWithEntityImpl:(EntityImpl *)impl
{
	return [self initWithEntityImpl:impl checkCache:YES];
}

- (EntityImpl *)entityImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMEntityReference

@implementation DOMEntityReference

- (EntityReferenceImpl *)entityReferenceImpl
{
	AbstractMethodCalled([DOMEntityReference class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMEntityReference

@implementation WebCoreDOMEntityReference

- (id)initWithEntityReferenceImpl:(EntityReferenceImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMEntityReference *)entityReferenceWithImpl:(EntityReferenceImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithEntityReferenceImpl:impl checkCache:NO] autorelease];
}

- (id)initWithEntityReferenceImpl:(EntityReferenceImpl *)impl
{
    return [self initWithEntityReferenceImpl:impl checkCache:YES];
}

- (EntityReferenceImpl *)entityReferenceImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end


//------------------------------------------------------------------------------------------
// DOMProcessingInstruction

@implementation DOMProcessingInstruction

- (ProcessingInstructionImpl *)processingInstructionImpl
{
	AbstractMethodCalled([DOMProcessingInstruction class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
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
// WebCoreDOMProcessingInstruction

@implementation WebCoreDOMProcessingInstruction

- (id)initWithProcessingInstructionImpl:(ProcessingInstructionImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
    return self;
}

+ (DOMProcessingInstruction *)processingInstructionWithImpl:(ProcessingInstructionImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = wrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] initWithProcessingInstructionImpl:impl checkCache:NO] autorelease];
}

- (id)initWithProcessingInstructionImpl:(ProcessingInstructionImpl *)impl
{
	return [self initWithProcessingInstructionImpl:impl checkCache:YES];
}

- (ProcessingInstructionImpl *)processingInstructionImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
// DOMRange

@implementation DOMRange

- (RangeImpl *)rangeImpl
{
	AbstractMethodCalled([DOMRange class]);
    return nil;
}

- (Class)classForDOMDocumentFragment
{
	AbstractMethodCalled([DOMRange class]);
    return nil;
}

- (Class)classForDOMNode
{
	AbstractMethodCalled([DOMRange class]);
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (DOMNode *)startContainer:(NSError **)error
{
    int code;
    DOMNode *result = [[self classForDOMNode] nodeWithImpl:[self rangeImpl]->startContainer(code)];
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
    DOMNode *result = [[self classForDOMNode] nodeWithImpl:[self rangeImpl]->endContainer(code)];
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
    DOMNode *result = [[self classForDOMNode] nodeWithImpl:[self rangeImpl]->commonAncestorContainer(code)];
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
    DOMDocumentFragment *result = [[self classForDOMDocumentFragment] documentFragmentWithImpl:[self rangeImpl]->extractContents(code)];
    fillInError(error, code);
    return result;
}

- (DOMDocumentFragment *)cloneContents:(NSError **)error
{
    int code;
    DOMDocumentFragment *result = [[self classForDOMDocumentFragment] documentFragmentWithImpl:[self rangeImpl]->cloneContents(code)];
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
    DOMRange *result = [WebCoreDOMRange rangeWithImpl:[self rangeImpl]->cloneRange(code)];
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

//------------------------------------------------------------------------------------------
// WebCoreDOMRange

@implementation WebCoreDOMRange

- (id)initWithRangeImpl:(RangeImpl *)impl checkCache:(BOOL)checkCache
{
    if (!impl) {
        [self release];
        return nil;
    }

	if (checkCache) {
		id cachedInstance;
		cachedInstance = wrapperForImpl(impl);
		if (cachedInstance) {
			[self release];
			return [cachedInstance retain];
		}
	}

    [super init];
    m_impl = impl;
    impl->ref();
    setWrapperForImpl(self, m_impl);
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
    
    return [[[self alloc] initWithRangeImpl:impl checkCache:NO] autorelease];
}

- (id)initWithRangeImpl:(RangeImpl *)impl
{
    return [self initWithRangeImpl:impl checkCache:YES];
}

- (RangeImpl *)rangeImpl
{
	return m_impl;
}

- (void)dealloc
{
    if (m_impl) {
        removeWrapperForImpl(m_impl);
    	m_impl->deref();
    }
    [super dealloc];
}

@end

//------------------------------------------------------------------------------------------
