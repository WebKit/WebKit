/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "WebCoreDOM.h"

#import <Foundation/Foundation.h>

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
using DOM::DOMImplementation;
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
using DOM::NodeListImpl;
using DOM::NotationImpl;
using DOM::ProcessingInstruction;
using DOM::ProcessingInstructionImpl;
using DOM::Range;
using DOM::RangeImpl;
using DOM::TextImpl;

//------------------------------------------------------------------------------------------
// Static functions and data
#pragma mark Static functions and data 

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

static NSString *domStringToNSString(const DOMString &aString)
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
    
//------------------------------------------------------------------------------------------
// Macros

#define WEB_CORE_INTERNAL_METHODS(ObjCClass,CPlusPlusClass) \
+ (ObjCClass *)objectWithImpl:(CPlusPlusClass *)impl \
{ \
    if (!impl) \
        return nil; \
    id cachedInstance; \
    cachedInstance = wrapperForImpl(impl); \
    if (cachedInstance) \
        return [[cachedInstance retain] autorelease]; \
    ObjCClass *instance = [ObjCClass alloc]; \
    return [[instance initWith##CPlusPlusClass:impl] autorelease]; \
} \
- (id)initWith##CPlusPlusClass:(CPlusPlusClass *)impl \
{ \
    if (!impl) { \
        [self release]; \
        return nil; \
    } \
    self = [super initWithDetails:impl]; \
    if (self) \
        static_cast<CPlusPlusClass *>(details)->ref(); \
    return self; \
} \
- (CPlusPlusClass *)impl \
{ \
    ASSERT(details); \
    return static_cast<CPlusPlusClass *>(details); \
}
    
//------------------------------------------------------------------------------------------
// Factory methods

DOM::NodeList DOM::NodeListImpl::createInstance(DOM::NodeListImpl *impl)
{
    return DOM::NodeList(impl);
}

DOM::NamedNodeMap DOM::NamedNodeMapImpl::createInstance(DOM::NamedNodeMapImpl *impl)
{
    return DOM::NamedNodeMap(impl);
}

DOM::Attr DOM::AttrImpl::createInstance(DOM::AttrImpl *impl)
{
    return DOM::Attr(impl);
}

DOM::Element DOM::ElementImpl::createInstance(DOM::ElementImpl *impl)
{
    return DOM::Element(impl);
}

DOM::CharacterData DOM::CharacterDataImpl::createInstance(DOM::CharacterDataImpl *impl)
{
    return DOM::CharacterData(impl);
}

DOM::Text DOM::TextImpl::createInstance(DOM::TextImpl *impl)
{
    return DOM::Text(impl);
}

DOM::ProcessingInstruction DOM::ProcessingInstructionImpl::createInstance(ProcessingInstructionImpl *impl)
{
    return DOM::ProcessingInstruction(impl);
}

DOM::DOMImplementation DOM::DOMImplementationImpl::createInstance(DOM::DOMImplementationImpl *impl)
{
    return DOM::DOMImplementation(impl);
}

DOM::DocumentType DOM::DocumentTypeImpl::createInstance(DOM::DocumentTypeImpl *impl)
{
    return DOM::DocumentType(impl);
}

DOM::Document DOM::DocumentImpl::createInstance(DOM::DocumentImpl *impl)
{
    return DOM::Document(impl);
}

DOM::Range DOM::RangeImpl::createInstance(DOM::RangeImpl *impl)
{
    return DOM::Range(impl);
}

//------------------------------------------------------------------------------------------
// WebCoreDOMObject

@implementation WebCoreDOMObject

- (id)initWithDetails:(void *)d
{
    if (!d) {
        [self release];
        return nil;
    }

    id cachedInstance;
    cachedInstance = wrapperForImpl(d);
    if (cachedInstance) {
        [self release];
        return [cachedInstance retain];
    }

    [super init];
    details = d;
    setWrapperForImpl(self, details);
    return self;
}

- (void)dealloc
{
    if (details)
        removeWrapperForImpl(details);
    [super dealloc];
}

- (unsigned)hash
{
    return (unsigned)details;
}

- (BOOL)isEqual:(id)other
{
    if (self == other)
        return YES;
        
    if ([other isKindOfClass:[WebCoreDOMObject class]] && ((WebCoreDOMObject *)other)->details == details)
        return YES;
        
    return NO;
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMNode

@implementation WebCoreDOMNode

WEB_CORE_INTERNAL_METHODS(WebCoreDOMNode, NodeImpl)

// Note: only objects that derive directly from WebDOMObject need their own dealloc method.
// This is due to the fact that some of the details impl objects derive from
// khtml::Shared, others from khtml::TreeShared (which do not share a base type), and we 
// have to cast to the right type in order to call the deref() function.
- (void)dealloc
{
    NodeImpl *instance = static_cast<NodeImpl *>(details);
    if (instance)
        instance->deref();
    [super dealloc];
}

- (NSString *)nodeName
{
    return domStringToNSString([self impl]->nodeName());
}

- (NSString *)nodeValue
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return domStringToNSString([self impl]->nodeValue());
}

- (void)setNodeValue:(NSString *)string error:(NSError **)error
{
    ASSERT(string);
    
    int code;
    [self impl]->setNodeValue(NSStringToDOMString(string), code);
    fillInError(error, code);
}

- (unsigned short)nodeType
{
    return [self impl]->nodeType();
}

- (id <DOMNode>)parentNode
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->parentNode()];
}

- (id <DOMNodeList>)childNodes
{
    return [WebCoreDOMNodeList objectWithImpl:[self impl]->childNodes()];
}

- (id <DOMNode>)firstChild
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->firstChild()];
}

- (id <DOMNode>)lastChild
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->lastChild()];
}

- (id <DOMNode>)previousSibling
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->previousSibling()];
}

- (id <DOMNode>)nextSibling
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->nextSibling()];
}

- (id <DOMNamedNodeMap>)attributes
{
    // DOM level 2 core specification says: 
    // A NamedNodeMap containing the attributes of this node (if it is an Element) or null otherwise.
    return nil;
}

- (id <DOMDocument>)ownerDocument
{
    return [WebCoreDOMDocument objectWithImpl:[self impl]->getDocument()];
}

- (id <DOMNode>)insertBefore:(id <DOMNode>)newChild :(id <DOMNode>)refChild error:(NSError **)error
{
    if (!newChild || !refChild) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    int code;
    WebCoreDOMNode *result = [WebCoreDOMNode objectWithImpl:[self impl]->insertBefore(nodeImpl(newChild), nodeImpl(refChild), code)];
    fillInError(error, code);
    return result;
}

- (id <DOMNode>)replaceChild:(id <DOMNode>)newChild :(id <DOMNode>)oldChild error:(NSError **)error
{
    if (!newChild || !oldChild) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    int code;
    WebCoreDOMNode *result = [WebCoreDOMNode objectWithImpl:[self impl]->replaceChild(nodeImpl(newChild), nodeImpl(oldChild), code)];
    fillInError(error, code);
    return result;
}

- (id <DOMNode>)removeChild:(id <DOMNode>)oldChild error:(NSError **)error
{
    if (!oldChild) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    int code;
    WebCoreDOMNode *result = [WebCoreDOMNode objectWithImpl:[self impl]->removeChild(nodeImpl(oldChild), code)];
    fillInError(error, code);
    return result;
}

- (id <DOMNode>)appendChild:(id <DOMNode>)newChild error:(NSError **)error
{
    if (!newChild) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    int code;
    WebCoreDOMNode *result = [WebCoreDOMNode objectWithImpl:[self impl]->appendChild(nodeImpl(newChild), code)];
    fillInError(error, code);
    return result;
}

- (BOOL)hasChildNodes
{
    return [self impl]->hasChildNodes();
}

- (id <DOMNode>)cloneNode:(BOOL)deep
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->cloneNode(deep)];
}

- (void)normalize
{
    [self impl]->normalize();
}

- (BOOL)isSupported:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    // Method not reflected in DOM::NodeImpl interface
    Node node([self impl]);
    return node.isSupported(NSStringToDOMString(feature), NSStringToDOMString(version));
}

- (NSString *)namespaceURI
{
    // Method not reflected in DOM::NodeImpl interface
    Node node([self impl]);
    return domStringToNSString(node.namespaceURI());
}

- (NSString *)prefix
{
    return domStringToNSString([self impl]->prefix());
}

- (void)setPrefix:(NSString *)prefix error:(NSError **)error
{
    ASSERT(prefix);

    int code;
    [self impl]->setPrefix(NSStringToDOMString(prefix), code);
    fillInError(error, code);
}

- (NSString *)localName
{
    return domStringToNSString([self impl]->localName());
}

- (BOOL)hasAttributes
{
    // Method not reflected in DOM::NodeImpl interface
    Node node([self impl]);
    return node.hasAttributes();
}

- (NSString *)HTMLString
{
    return [self impl]->recursive_toHTML(true).getNSString();
}

//
// begin deprecated methods
//
- (void)setNodeValue:(NSString *)string
{
    [self setNodeValue:string error:nil];
}

- (id<DOMNode>)insert:(id<DOMNode>)newChild before:(id<DOMNode>)refChild
{
    return [self insertBefore:newChild :refChild error:nil];
}

- (id<DOMNode>)replace:(id<DOMNode>)newChild child:(id<DOMNode>)oldChild
{
    return [self replaceChild:newChild :oldChild error:nil];
}

- (id<DOMNode>)removeChild:(id<DOMNode>)oldChild
{
    return [self removeChild:oldChild error:nil];
}

- (id<DOMNode>)appendChild:(id<DOMNode>)newChild
{
    return [self appendChild:newChild error:nil];
}

- (void)setPrefix:(NSString *)prefix
{
    [self setPrefix:prefix error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMNamedNodeMap

@implementation WebCoreDOMNamedNodeMap

WEB_CORE_INTERNAL_METHODS(WebCoreDOMNamedNodeMap, NamedNodeMapImpl)

// Note: only objects that derive directly from WebDOMObject need their own dealloc method.
// This is due to the fact that some of the details impl objects derive from
// khtml::Shared, others from khtml::TreeShared (which do not share a base type), and we 
// have to cast to the right type in order to call the deref() function.
- (void)dealloc
{
    NamedNodeMapImpl *instance = static_cast<NamedNodeMapImpl *>(details);
    if (instance)
        instance->deref();
    [super dealloc];
}

- (id <DOMNode>)getNamedItem:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::NamedNodeMapImpl interface
    NamedNodeMap map = NamedNodeMapImpl::createInstance([self impl]);
    Node result(map.getNamedItem(NSStringToDOMString(name)));
    return [WebCoreDOMNode objectWithImpl:result.handle()];
}

- (id <DOMNode>)setNamedItem:(id <DOMNode>)arg error:(NSError **)error
{
    if (!arg) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self impl]);
        Node result(map.setNamedItem(nodeImpl(arg)));
        return [WebCoreDOMNode objectWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMNode>)removeNamedItem:(NSString *)name error:(NSError **)error
{
    if (!name) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self impl]);
        Node result(map.removeNamedItem(NSStringToDOMString(name)));
        return [WebCoreDOMNode objectWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMNode>)item:(unsigned long)index
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->item(index)];
}

- (unsigned long)length
{
    return [self impl]->length();
}

- (id <DOMNode>)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    if (!namespaceURI || !localName) {
        return nil;
    }

    // Method not reflected in DOM::NamedNodeMapImpl interface
    NamedNodeMap map = NamedNodeMapImpl::createInstance([self impl]);
    Node result(map.getNamedItemNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
    return [WebCoreDOMNode objectWithImpl:result.handle()];
}

- (id <DOMNode>)setNamedItemNS:(id <DOMNode>)arg error:(NSError **)error
{
    if (!arg) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self impl]);
        Node result(map.setNamedItemNS(nodeImpl(arg)));
        return [WebCoreDOMNode objectWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMNode>)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName error:(NSError **)error
{
    if (!namespaceURI || !localName) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    // Method not reflected in DOM::NamedNodeMapImpl interface
    try {
        NamedNodeMap map = NamedNodeMapImpl::createInstance([self impl]);
        Node result(map.removeNamedItemNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
        return [WebCoreDOMNode objectWithImpl:result.handle()];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

//
// begin deprecated methods
//
- (id<DOMNode>)setNamedItem:(id<DOMNode>)arg
{
    return [self setNamedItem:arg error:nil];
}

- (id<DOMNode>)removeNamedItem:(NSString *)name
{
    return [self removeNamedItem:name error:nil];
}

- (id<DOMNode>)setNamedItemNS:(id<DOMNode>)arg
{
    return [self setNamedItemNS:arg error:nil];
}

- (id<DOMNode>)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName
{
    return [self removeNamedItemNS:namespaceURI :localName error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMNodeList

@implementation WebCoreDOMNodeList

WEB_CORE_INTERNAL_METHODS(WebCoreDOMNodeList, NodeListImpl)

// Note: only objects that derive directly from WebDOMObject need their own dealloc method.
// This is due to the fact that some of the details impl objects derive from
// khtml::Shared, others from khtml::TreeShared (which do not share a base type), and we 
// have to cast to the right type in order to call the deref() function.
- (void)dealloc
{
    NodeListImpl *instance = static_cast<NodeListImpl *>(details);
    if (instance)
        instance->deref();
    [super dealloc];
}

- (id <DOMNode>)item:(unsigned long)index
{
    return [WebCoreDOMNode objectWithImpl:[self impl]->item(index)];
}

- (unsigned long)length
{
    return [self impl]->length();
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMImplementation

@implementation WebCoreDOMImplementation

WEB_CORE_INTERNAL_METHODS(WebCoreDOMImplementation, DOMImplementationImpl)

// Note: only objects that derive directly from WebDOMObject need their own dealloc method.
// This is due to the fact that some of the details impl objects derive from
// khtml::Shared, others from khtml::TreeShared (which do not share a base type), and we 
// have to cast to the right type in order to call the deref() function.
- (void)dealloc
{
    DOMImplementationImpl *instance = static_cast<DOMImplementationImpl *>(details);
    if (instance)
        instance->deref();
    [super dealloc];
}

- (BOOL)hasFeature:(NSString *)feature :(NSString *)version
{
    ASSERT(feature);
    ASSERT(version);

    return [self impl]->hasFeature(NSStringToDOMString(feature), NSStringToDOMString(version));
}

- (id <DOMDocumentType>)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId error:(NSError **)error
{
    ASSERT(qualifiedName);
    ASSERT(publicId);
    ASSERT(systemId);

    int code;
    DocumentTypeImpl *impl = [self impl]->createDocumentType(NSStringToDOMString(qualifiedName), NSStringToDOMString(publicId), NSStringToDOMString(systemId), code);
    id <DOMDocumentType> result = [WebCoreDOMDocumentType objectWithImpl:impl];
    fillInError(error, code);
    return result;
}

- (id <DOMDocument>)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(id <DOMDocumentType>)doctype error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code;
    DocumentType dt = DocumentTypeImpl::createInstance(documentTypeImpl(doctype));
    DocumentImpl *impl = [self impl]->createDocument(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName), dt, code);
    id <DOMDocument> result = [WebCoreDOMDocument objectWithImpl:impl];
    fillInError(error, code);
    return result;
}

//
// begin deprecated methods
//
- (id<DOMDocumentType>)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId :(int *)exceptionCode
{
    ASSERT(qualifiedName);
    ASSERT(publicId);
    ASSERT(systemId);

    DocumentTypeImpl *impl = [self impl]->createDocumentType(NSStringToDOMString(qualifiedName), NSStringToDOMString(publicId), NSStringToDOMString(systemId), *exceptionCode);
    return [WebCoreDOMDocumentType objectWithImpl:impl];
}

- (id<DOMDocument>)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(id<DOMDocumentType>)doctype
{
    return [self createDocument:namespaceURI :qualifiedName :doctype error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMDocumentFragment

@implementation WebCoreDOMDocumentFragment

WEB_CORE_INTERNAL_METHODS(WebCoreDOMDocumentFragment, DocumentFragmentImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMDocument

@implementation WebCoreDOMDocument

WEB_CORE_INTERNAL_METHODS(WebCoreDOMDocument, DocumentImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (id <DOMDocumentType>)doctype
{
    return [WebCoreDOMDocumentType objectWithImpl:[self impl]->doctype()];
}

- (id <DOMImplementation>)implementation
{
    return [WebCoreDOMImplementation objectWithImpl:[self impl]->implementation()];
}

- (id <DOMElement>)documentElement
{
    return [WebCoreDOMElement objectWithImpl:[self impl]->documentElement()];
}

- (id <DOMElement>)createElement:(NSString *)tagName error:(NSError **)error
{
    ASSERT(tagName);

    int code;
    id <DOMElement> result = [WebCoreDOMElement objectWithImpl:[self impl]->createElement(NSStringToDOMString(tagName), code)];
    fillInError(error, code);
    return result;
}

- (id <DOMDocumentFragment>)createDocumentFragment
{
    return [WebCoreDOMDocumentFragment objectWithImpl:[self impl]->createDocumentFragment()];
}

- (id <DOMText>)createTextNode:(NSString *)data
{
    ASSERT(data);

    return [WebCoreDOMText objectWithImpl:[self impl]->createTextNode(NSStringToDOMString(data))];
}

- (id <DOMComment>)createComment:(NSString *)data
{
    ASSERT(data);

    return [WebCoreDOMComment objectWithImpl:[self impl]->createComment(NSStringToDOMString(data))];
}

- (id <DOMCDATASection>)createCDATASection:(NSString *)data error:(NSError **)error
{
    ASSERT(data);

    // Documentation says we can raise a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report that error up to us.
    return [WebCoreDOMCDATASection objectWithImpl:[self impl]->createCDATASection(NSStringToDOMString(data))];
}

- (id <DOMProcessingInstruction>)createProcessingInstruction:(NSString *)target :(NSString *)data error:(NSError **)error
{
    ASSERT(target);
    ASSERT(data);

    // Documentation says we can raise a INVALID_CHARACTER_ERR or a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report these errors up to us.
    return [WebCoreDOMProcessingInstruction objectWithImpl:[self impl]->createProcessingInstruction(NSStringToDOMString(target), NSStringToDOMString(data))];
}

- (id <DOMAttr>)createAttribute:(NSString *)name error:(NSError **)error
{
    ASSERT(name);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self impl]));
        Attr result(doc.createAttribute(NSStringToDOMString(name)));
        AttrImpl *impl = static_cast<AttrImpl *>(result.handle());
        return [WebCoreDOMAttr objectWithImpl:impl];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMEntityReference>)createEntityReference:(NSString *)name error:(NSError **)error
{
    ASSERT(name);

    // Documentation says we can raise a INVALID_CHARACTER_ERR or a NOT_SUPPORTED_ERR.
    // However, the lower layer does not report these errors up to us.
    return [WebCoreDOMEntityReference objectWithImpl:[self impl]->createEntityReference(NSStringToDOMString(name))];
}

- (id <DOMNodeList>)getElementsByTagName:(NSString *)tagname
{
    ASSERT(tagname);

    return [WebCoreDOMNodeList objectWithImpl:[self impl]->getElementsByTagNameNS(0, NSStringToDOMString(tagname).implementation())];
}

- (id <DOMNode>)importNode:(id <DOMNode>)importedNode :(BOOL)deep error:(NSError **)error
{
    int code;
    WebCoreDOMNode *result = [WebCoreDOMNode objectWithImpl:[self impl]->importNode(nodeImpl(importedNode), deep, code)];
    fillInError(error, code);
    return result;
}

- (id <DOMElement>)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    int code;
    id <DOMElement> result = [WebCoreDOMElement objectWithImpl:[self impl]->createElementNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName), code)];
    fillInError(error, code);
    return result;
}

- (id <DOMAttr>)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);

    // Method not reflected in DOM::DocumentImpl interface
    try {
        Document doc(DocumentImpl::createInstance([self impl]));
        Attr result(doc.createAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(qualifiedName)));
        AttrImpl *impl = static_cast<AttrImpl *>(result.handle());
        return [WebCoreDOMAttr objectWithImpl:impl];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [WebCoreDOMNodeList objectWithImpl:[self impl]->getElementsByTagNameNS(NSStringToDOMString(namespaceURI).implementation(), NSStringToDOMString(localName).implementation())];
}

- (id <DOMElement>)getElementById:(NSString *)elementId
{
    ASSERT(elementId);

    return [WebCoreDOMElement objectWithImpl:[self impl]->getElementById(NSStringToDOMString(elementId))];
}

//
// begin deprecated methods
//
- (id<DOMElement>)createElement:(NSString *)tagName
{
    return [self createElement:tagName error:nil];
}

- (id<DOMElement>)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    return [self createElementNS:namespaceURI :qualifiedName error:nil];
}

- (id<DOMCDATASection>)createCDATASection:(NSString *)data
{
    return [self createCDATASection:data error:nil];
}

- (id<DOMProcessingInstruction>)createProcessingInstruction:(NSString *)target :(NSString *)data
{
    return [self createProcessingInstruction:target :data error:nil];
}

- (id<DOMAttr>)createAttribute:(NSString *)name;
{
    return [self createAttribute:name error:nil];
}

- (id<DOMEntityReference>)createEntityReference:(NSString *)name
{
    return [self createEntityReference:name error:nil];
}

- (id<DOMNode>)importNode:(id<DOMNode>)importedNode :(BOOL)deep
{
    return [self importNode:importedNode :deep error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMCharacterData

@implementation WebCoreDOMCharacterData

WEB_CORE_INTERNAL_METHODS(WebCoreDOMCharacterData, CharacterDataImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (NSString *)data
{
    // Documentation says we can raise a DOMSTRING_SIZE_ERR.
    // However, the lower layer does not report that error up to us.
    return domStringToNSString([self impl]->data());
}

- (void)setData:(NSString *)data error:(NSError **)error
{
    ASSERT(data);
    
    int code;
    [self impl]->setData(NSStringToDOMString(data), code);
    fillInError(error, code);
}

- (unsigned long)length
{
    return [self impl]->length();
}

- (NSString *)substringData:(unsigned long)offset :(unsigned long)count error:(NSError **)error
{
    int code;
    NSString *result = domStringToNSString([self impl]->substringData(offset, count, code));
    fillInError(error, code);
    return result;
}

- (void)appendData:(NSString *)arg error:(NSError **)error
{
    ASSERT(arg);
    
    int code;
    [self impl]->appendData(NSStringToDOMString(arg), code);
    fillInError(error, code);
}

- (void)insertData:(unsigned long)offset :(NSString *)arg error:(NSError **)error
{
    ASSERT(arg);
    
    int code;
    [self impl]->insertData(offset, NSStringToDOMString(arg), code);
    fillInError(error, code);
}

- (void)deleteData:(unsigned long)offset :(unsigned long) count error:(NSError **)error;
{
    int code;
    [self impl]->deleteData(offset, count, code);
    fillInError(error, code);
}

- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg error:(NSError **)error
{
    ASSERT(arg);

    int code;
    [self impl]->replaceData(offset, count, NSStringToDOMString(arg), code);
    fillInError(error, code);
}

//
// begin deprecated methods
//
- (void)setData: (NSString *)data
{
    [self setData:data error:nil];
}

- (NSString *)substringData: (unsigned long)offset :(unsigned long)count
{
    return [self substringData:offset :count error:nil];
}

- (void)appendData:(NSString *)arg
{
    [self appendData:arg error:nil];
}

- (void)insertData:(unsigned long)offset :(NSString *)arg
{
    [self insertData:offset :arg error:nil];
}

- (void)deleteData:(unsigned long)offset :(unsigned long)count
{
    [self deleteData:offset :count error:nil];
}

- (void)replaceData:(unsigned long)offset :(unsigned long)count :(NSString *)arg
{
    [self replaceData:offset :count :arg error:nil];
}
//
// end deprecated methods
//

@end


//------------------------------------------------------------------------------------------
// WebCoreDOMAttr

@implementation WebCoreDOMAttr

WEB_CORE_INTERNAL_METHODS(WebCoreDOMAttr, AttrImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (NSString *)name
{
    return domStringToNSString([self impl]->nodeName());
}

- (BOOL)specified
{
    return [self impl]->specified();
}

- (NSString *)value
{
    return domStringToNSString([self impl]->nodeValue());
}

- (void)setValue:(NSString *)value error:(NSError **)error
{
    ASSERT(value);

    int code;
    [self impl]->setValue(NSStringToDOMString(value), code);
    fillInError(error, code);
}

- (id <DOMElement>)ownerElement
{
    return [WebCoreDOMElement objectWithImpl:[self impl]->ownerElement()];
}

//
// begin deprecated methods
//
- (void)setValue:(NSString *)value
{
    [self setValue:value error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMElement

@implementation WebCoreDOMElement

WEB_CORE_INTERNAL_METHODS(WebCoreDOMElement, ElementImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (NSString *)tagName
{
    return domStringToNSString([self impl]->tagName());
}

- (NSString *)getAttribute:(NSString *)name
{
    ASSERT(name);

    return domStringToNSString([self impl]->getAttribute(NSStringToDOMString(name)));
}

- (void)setAttribute:(NSString *)name :(NSString *)value error:(NSError **)error
{
    ASSERT(name);
    ASSERT(value);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self impl]));
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
        Element element(ElementImpl::createInstance([self impl]));
        element.removeAttribute(NSStringToDOMString(name));
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
    }
}

- (id <DOMAttr>)getAttributeNode:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self impl]));
    Attr result(element.getAttributeNode(NSStringToDOMString(name)));
    return [WebCoreDOMAttr objectWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (id <DOMAttr>)setAttributeNode:(id <DOMAttr>)newAttr error:(NSError **)error
{
    if (!newAttr) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self impl]));
        Attr attr(AttrImpl::createInstance(attrImpl(newAttr)));
        Attr result(element.setAttributeNode(attr));
        return [WebCoreDOMAttr objectWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMAttr>)removeAttributeNode:(id <DOMAttr>)oldAttr error:(NSError **)error
{
    if (!oldAttr) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self impl]));
        Attr attr(AttrImpl::createInstance(attrImpl(oldAttr)));
        Attr result(element.removeAttributeNode(attr));
        return [WebCoreDOMAttr objectWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMNodeList>)getElementsByTagName:(NSString *)name
{
    ASSERT(name);

    return [WebCoreDOMNodeList objectWithImpl:[self impl]->getElementsByTagNameNS(0, NSStringToDOMString(name).implementation())];
}

- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    Element element(ElementImpl::createInstance([self impl]));
    return domStringToNSString(element.getAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value error:(NSError **)error
{
    ASSERT(namespaceURI);
    ASSERT(qualifiedName);
    ASSERT(value);

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self impl]));
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
        Element element(ElementImpl::createInstance([self impl]));
        element.removeAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName));
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
    }
}

- (id <DOMAttr>)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self impl]));
    Attr result(element.getAttributeNodeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName)));
    return [WebCoreDOMAttr objectWithImpl:static_cast<AttrImpl *>(result.handle())];
}

- (id <DOMAttr>)setAttributeNodeNS:(id <DOMAttr>)newAttr error:(NSError **)error
{
    if (!newAttr) {
        fillInError(error, NOT_FOUND_ERR);
        return nil;
    }

    // Method not reflected in DOM::ElementImpl interface
    try {
        Element element(ElementImpl::createInstance([self impl]));
        Attr attr(AttrImpl::createInstance(attrImpl(newAttr)));
        Attr result(element.setAttributeNodeNS(attr));
        return [WebCoreDOMAttr objectWithImpl:static_cast<AttrImpl *>(result.handle())];
    } 
    catch (const DOMException &e) {
        fillInError(error, e.code);
        return nil;
    }
}

- (id <DOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    return [WebCoreDOMNodeList objectWithImpl:[self impl]->getElementsByTagNameNS(NSStringToDOMString(namespaceURI).implementation(), NSStringToDOMString(localName).implementation())];
}

- (BOOL)hasAttribute:(NSString *)name
{
    ASSERT(name);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self impl]));
    return element.hasAttribute(NSStringToDOMString(name));
}

- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    ASSERT(namespaceURI);
    ASSERT(localName);

    // Method not reflected in DOM::ElementImpl interface
    Element element(ElementImpl::createInstance([self impl]));
    return element.hasAttributeNS(NSStringToDOMString(namespaceURI), NSStringToDOMString(localName));
}

//
// begin deprecated methods
//
- (void)setAttribute:(NSString *)name :(NSString *)value
{
    [self setAttribute:name :value error:nil];
}

- (void)removeAttribute:(NSString *)name
{
    [self removeAttribute:name error:nil];
}

- (id<DOMAttr>)setAttributeNode:(id<DOMAttr>)newAttr
{
    return [self setAttributeNode:newAttr error:nil];
}

- (id<DOMAttr>)removeAttributeNode:(id<DOMAttr>)oldAttr
{
    return [self removeAttributeNode:oldAttr error:nil];
}

- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value
{
    [self setAttributeNS:namespaceURI :qualifiedName :value error:nil];
}

- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName
{
    [self removeAttributeNS:namespaceURI :localName error:nil];
}

- (id<DOMAttr>)setAttributeNodeNS:(id<DOMAttr>)newAttr
{
    return [self setAttributeNodeNS:newAttr error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMText

@implementation WebCoreDOMText

WEB_CORE_INTERNAL_METHODS(WebCoreDOMText, TextImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (id <DOMText>)splitText:(unsigned long)offset error:(NSError **)error
{
    int code;
    id <DOMText> result = [WebCoreDOMText objectWithImpl:[self impl]->splitText(offset, code)];
    fillInError(error, code);
    return result;
}

//
// begin deprecated methods
//
- (id<DOMText>)splitText:(unsigned long)offset
{
    return [self splitText:offset error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMComment

@implementation WebCoreDOMComment

WEB_CORE_INTERNAL_METHODS(WebCoreDOMComment, CommentImpl)

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMCDATASection

@implementation WebCoreDOMCDATASection

WEB_CORE_INTERNAL_METHODS(WebCoreDOMCDATASection, CDATASectionImpl)

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMDocumentType

@implementation WebCoreDOMDocumentType

WEB_CORE_INTERNAL_METHODS(WebCoreDOMDocumentType, DocumentTypeImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (NSString *)name
{
    return domStringToNSString([self impl]->publicId());
}

- (id <DOMNamedNodeMap>)entities
{
    return [WebCoreDOMNamedNodeMap objectWithImpl:[self impl]->entities()];
}

- (id <DOMNamedNodeMap>)notations
{
    return [WebCoreDOMNamedNodeMap objectWithImpl:[self impl]->notations()];
}

- (NSString *)publicId
{
    return domStringToNSString([self impl]->publicId());
}

- (NSString *)systemId
{
    return domStringToNSString([self impl]->systemId());
}

- (NSString *)internalSubset
{
    return domStringToNSString([self impl]->internalSubset());
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMNotation

@implementation WebCoreDOMNotation

WEB_CORE_INTERNAL_METHODS(WebCoreDOMNotation, NotationImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (NSString *)publicId
{
    return domStringToNSString([self impl]->publicId());
}

- (NSString *)systemId
{
    return domStringToNSString([self impl]->systemId());
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMEntity

@implementation WebCoreDOMEntity

WEB_CORE_INTERNAL_METHODS(WebCoreDOMEntity, EntityImpl)

- (NSString *)publicId
{
    return domStringToNSString([self impl]->publicId());
}

- (NSString *)systemId
{
    return domStringToNSString([self impl]->systemId());
}

- (NSString *)notationName
{
    return domStringToNSString([self impl]->notationName());
}

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMEntityReference

@implementation WebCoreDOMEntityReference

WEB_CORE_INTERNAL_METHODS(WebCoreDOMEntityReference, EntityReferenceImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMProcessingInstruction

@implementation WebCoreDOMProcessingInstruction

WEB_CORE_INTERNAL_METHODS(WebCoreDOMProcessingInstruction, ProcessingInstructionImpl)

// Note: This object does not need its own dealloc method, since it derives from
// WebCoreDOMNode. See the dealloc method comment on that method for more information. 

- (NSString *)target
{
    return domStringToNSString([self impl]->target());
}

- (NSString *)data
{
    return domStringToNSString([self impl]->data());
}

- (void)setData:(NSString *)data error:(NSError **)error
{
    ASSERT(data);

    int code;
    [self impl]->setData(NSStringToDOMString(data), code);
    fillInError(error, code);
}

//
// begin deprecated methods
//
- (void)setData:(NSString *)data
{
    [self setData:data error:nil];
}
//
// end deprecated methods
//

@end

//------------------------------------------------------------------------------------------
// WebCoreDOMRange

@implementation WebCoreDOMRange

WEB_CORE_INTERNAL_METHODS(WebCoreDOMRange, RangeImpl)

// Note: only objects that derive directly from WebDOMObject need their own dealloc method.
// This is due to the fact that some of the details impl objects derive from
// khtml::Shared, others from khtml::TreeShared (which do not share a base type), and we 
// have to cast to the right type in order to call the deref() function.
- (void)dealloc
{
    RangeImpl *instance = static_cast<RangeImpl *>(details);
    if (instance)
        instance->deref();
    [super dealloc];
}

- (id <DOMNode>)startContainer:(NSError **)error
{
    int code;
    id <DOMNode> result = [WebCoreDOMNode objectWithImpl:[self impl]->startContainer(code)];
    fillInError(error, code);
    return result;
}

- (long)startOffset:(NSError **)error
{
    int code;
    long result = [self impl]->startOffset(code);
    fillInError(error, code);
    return result;
}

- (id <DOMNode>)endContainer:(NSError **)error
{
    int code;
    id <DOMNode> result = [WebCoreDOMNode objectWithImpl:[self impl]->endContainer(code)];
    fillInError(error, code);
    return result;
}

- (long)endOffset:(NSError **)error
{
    int code;
    long result = [self impl]->endOffset(code);
    fillInError(error, code);
    return result;
}

- (BOOL)collapsed:(NSError **)error
{
    int code;
    BOOL result = [self impl]->collapsed(code);
    fillInError(error, code);
    return result;
}

- (id <DOMNode>)commonAncestorContainer:(NSError **)error
{
    int code;
    id <DOMNode> result = [WebCoreDOMNode objectWithImpl:[self impl]->commonAncestorContainer(code)];
    fillInError(error, code);
    return result;
}

- (void)setStart:(id <DOMNode>)refNode :(long)offset error:(NSError **)error
{
    int code;
    [self impl]->setStart(nodeImpl(refNode), offset, code);
    fillInError(error, code);
}

- (void)setEnd:(id <DOMNode>)refNode :(long)offset error:(NSError **)error
{
    int code;
    [self impl]->setEnd(nodeImpl(refNode), offset, code);
    fillInError(error, code);
}

- (void)setStartBefore:(id <DOMNode>)refNode error:(NSError **)error
{
    int code;
    [self impl]->setStartBefore(nodeImpl(refNode), code);
    fillInError(error, code);
}

- (void)setStartAfter:(id <DOMNode>)refNode error:(NSError **)error
{
    int code;
    [self impl]->setStartAfter(nodeImpl(refNode), code);
    fillInError(error, code);
}

- (void)setEndBefore:(id <DOMNode>)refNode error:(NSError **)error
{
    int code;
    [self impl]->setEndBefore(nodeImpl(refNode), code);
    fillInError(error, code);
}

- (void)setEndAfter:(id <DOMNode>)refNode error:(NSError **)error
{
    int code;
    [self impl]->setEndAfter(nodeImpl(refNode), code);
    fillInError(error, code);
}

- (void)collapse:(BOOL)toStart error:(NSError **)error
{
    int code;
    [self impl]->collapse(toStart, code);
    fillInError(error, code);
}

- (void)selectNode:(id <DOMNode>)refNode error:(NSError **)error
{
    int code;
    [self impl]->selectNode(nodeImpl(refNode), code);
    fillInError(error, code);
}

- (void)selectNodeContents:(id <DOMNode>)refNode error:(NSError **)error
{
    int code;
    [self impl]->selectNodeContents(nodeImpl(refNode), code);
    fillInError(error, code);
}

- (short)compareBoundaryPoints:(unsigned short)how :(id <DOMRange>)sourceRange error:(NSError **)error
{
    int code;
    short result = [self impl]->compareBoundaryPoints(static_cast<Range::CompareHow>(how), rangeImpl(sourceRange), code);
    fillInError(error, code);
    return result;
}

- (void)deleteContents:(NSError **)error
{
    int code;
    [self impl]->deleteContents(code);
    fillInError(error, code);
}

- (id <DOMDocumentFragment>)extractContents:(NSError **)error
{
    int code;
    id <DOMDocumentFragment> result = [WebCoreDOMDocumentFragment objectWithImpl:[self impl]->extractContents(code)];
    fillInError(error, code);
    return result;
}

- (id <DOMDocumentFragment>)cloneContents:(NSError **)error
{
    int code;
    id <DOMDocumentFragment> result = [WebCoreDOMDocumentFragment objectWithImpl:[self impl]->cloneContents(code)];
    fillInError(error, code);
    return result;
}

- (void)insertNode:(id <DOMNode>)newNode error:(NSError **)error
{
    int code;
    [self impl]->insertNode(nodeImpl(newNode), code);
    fillInError(error, code);
}

- (void)surroundContents:(id <DOMNode>)newParent error:(NSError **)error
{
    int code;
    [self impl]->surroundContents(nodeImpl(newParent), code);
    fillInError(error, code);
}

- (id <DOMRange>)cloneRange:(NSError **)error
{
    int code;
    id <DOMRange> result = [WebCoreDOMRange objectWithImpl:[self impl]->cloneRange(code)];
    fillInError(error, code);
    return result;
}

- (NSString *)toString:(NSError **)error
{
    int code;
    NSString *result = domStringToNSString([self impl]->toString(code));
    fillInError(error, code);
    return result;
}

- (void)detach:(NSError **)error
{
    int code;
    [self impl]->detach(code);
    fillInError(error, code);
}

@end

//------------------------------------------------------------------------------------------
