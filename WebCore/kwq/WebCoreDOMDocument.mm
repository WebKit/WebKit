/*	
    WebCoreDOMDocument.mm
    Copyright 2002, Apple, Inc. All rights reserved.
*/
#include "WebCoreDOMPrivate.h"

DOM::DOMImplementation DOM::DOMImplementationImpl::createInstance(DOM::DOMImplementationImpl *impl)
{
    return DOM::DOMImplementation(impl);
}

DOM::DocumentType DOM::DocumentTypeImpl::createInstance (DOM::DocumentTypeImpl *impl)
{
    return DOM::DocumentType (impl);
}

DOM::Document DOM::DocumentImpl::createInstance (DOM::DocumentImpl *impl)
{
    return DOM::Document (impl);
}

NSString *domStringToNSString(DOM::DOMString &aString)
{
    return [NSString stringWithCharacters: (unichar *)aString.unicode() length: aString.length()];
}

DOM::DOMString NSStringToDOMString(NSString *aString)
{
    QChar *chars = (QChar *)malloc([aString length] * sizeof(QChar));
    [aString getCharacters:(unichar *)chars];
    DOM::DOMString ret(chars, [aString length]);
    free (chars);
    return ret;
}


@implementation WebCoreDOMDocumentType

+ (WebCoreDOMDocumentType *)documentTypeWithImpl: (DOM::DocumentTypeImpl *)_impl
{
    return [[(WebCoreDOMDocumentType *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl: (DOM::DocumentTypeImpl *)coreImpl
{
    [super initWithImpl:coreImpl];
    return self;
}

- (DOM::DocumentTypeImpl *)impl
{
    return (DOM::DocumentTypeImpl *)impl;
}

- (NSString *)name
{
    DOM::DOMString name = [self impl]->name();
    return domStringToNSString(name);
}

- (id<WebDOMNamedNodeMap>)entities
{
    return [WebCoreDOMNamedNodeMap namedNodeMapWithImpl:[self impl]->entities()];
}

- (id<WebDOMNamedNodeMap>)notations
{
    return [WebCoreDOMNamedNodeMap namedNodeMapWithImpl:[self impl]->entities()];
}

- (NSString *)publicId
{
    DOM::DOMString publicId = [self impl]->publicId();
    return domStringToNSString(publicId);
}

- (NSString *)systemId
{
    DOM::DOMString systemId = [self impl]->systemId();
    return domStringToNSString(systemId);
}

- (NSString *)internalSubset
{
    DOM::DOMString internalSubset = [self impl]->internalSubset();
    return domStringToNSString(internalSubset);
}
@end


@implementation WebCoreDOMImplementation

+ (WebCoreDOMImplementation *)implementionatWithImpl: (DOM::DOMImplementationImpl *)_impl
{
    return [[(WebCoreDOMImplementation *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl: (DOM::DOMImplementationImpl *)coreImpl
{
    [super init];
    impl = coreImpl;
    impl->ref();
    return self;
}

- (void)dealloc
{
    impl->deref();
    [super dealloc];
}

- (DOM::DOMImplementationImpl *)impl
{
    return (DOM::DOMImplementationImpl *)impl;
}

- (BOOL)hasFeature: (NSString *)feature : (NSString *)version
{
    return [self impl]->hasFeature(NSStringToDOMString(feature),NSStringToDOMString(version));
}

- (id<WebDOMDocumentType>)createDocumentType: (NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId;
{
    DOM::DOMString _qualifiedName = NSStringToDOMString(qualifiedName);
    DOM::DOMString _publicId = NSStringToDOMString(publicId);
    DOM::DOMString _systemId = NSStringToDOMString(systemId);
    DOM::DOMImplementation instance = DOM::DOMImplementationImpl::createInstance([self impl]);
    DOM::DocumentType ret;
    
    ret = instance.createDocumentType (_qualifiedName, _publicId, _systemId);
    
    return [WebCoreDOMDocumentType documentTypeWithImpl: (DOM::DocumentTypeImpl *)ret.handle()];
}

- (id<WebDOMDocument>)createDocument: (NSString *)namespaceURI :(NSString *)qualifiedName :doctype
{
    DOM::DOMString _namespaceURI = NSStringToDOMString(namespaceURI);
    DOM::DOMString _qualifiedName = NSStringToDOMString(qualifiedName);
    DOM::DOMImplementation instance = DOM::DOMImplementationImpl::createInstance([self impl]);
    DOM::DocumentType docTypeInstance = DOM::DocumentTypeImpl::createInstance([(WebCoreDOMDocumentType *)doctype impl]);
    DOM::Document ret;
    
    ret = instance.createDocument (_namespaceURI, _qualifiedName, docTypeInstance);
    
    return [WebCoreDOMDocument documentWithImpl: (DOM::DocumentImpl *)ret.handle()];
}
@end



@implementation WebCoreDOMDocument

+ (WebCoreDOMDocument *)documentWithImpl: (DOM::DocumentImpl *)_impl
{
    return [[(WebCoreDOMDocument *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl: (DOM::DocumentImpl *)coreImpl
{
    [super initWithImpl:coreImpl];
    return self;
}

- (DOM::DocumentImpl *)impl
{
    return (DOM::DocumentImpl *)impl;
}

- (id<WebDOMDocumentType>)doctype
{
    return [WebCoreDOMDocumentType documentTypeWithImpl: [self impl]->doctype()];
}

- (id<WebDOMImplementation>)implementation
{
    return [WebCoreDOMImplementation implementationWithImpl: [self impl]->implementation()];
}

- (id<WebDOMElement>)documentElement
{
    return [WebCoreDOMElement elementWithImpl: [self impl]->documentElement()];
}

- (id<WebDOMElement>)createElement:(NSString *)tagName
{
    return [WebCoreDOMElement elementWithImpl: [self impl]->createElement(NSStringToDOMString(tagName))];
}

- (id<WebDOMElement>)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    return [WebCoreDOMElement elementWithImpl: [self impl]->createElementNS(NSStringToDOMString(namespaceURI),NSStringToDOMString(qualifiedName))];
}

- (id<WebDOMDocumentFragment>)createDocumentFragment
{
    return [WebCoreDOMDocumentFragment documentFragmentWithImpl: [self impl]->createDocumentFragment()];
}

- (id<WebDOMText>)createTextNode:(NSString *)data
{
    return [WebCoreDOMText textWithImpl: [self impl]->createTextNode(NSStringToDOMString(data))];
}

- (id<WebDOMComment>)createComment:(NSString *)data
{
    return [WebCoreDOMComment commentWithImpl: [self impl]->createComment(NSStringToDOMString(data))];
}

- (id<WebDOMCDATASection>)createCDATASection:(NSString *)data
{
    return [WebCoreDOMCDATASection CDATASectionWithImpl: [self impl]->createCDATASection(NSStringToDOMString(data))];
}

- (id<WebDOMProcessingInstruction>)createProcessingInstruction:(NSString *)target :(NSString *)data
{
    DOM::DOMString _target = NSStringToDOMString(target);
    DOM::DOMString _data = NSStringToDOMString(data);
    return [WebCoreDOMProcessingInstruction processingInstructionWithImpl: [self impl]->createProcessingInstruction(_target,_data)];
}

- (id<WebDOMAttr>)createAttribute:(NSString *)name
{
    DOM::DOMString _name = NSStringToDOMString(name);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self impl]);
    DOM::AttrImpl *attr = (DOM::AttrImpl *)instance.createAttribute(_name).handle();
    return [WebCoreDOMAttr attrWithImpl: attr];
}

- (id<WebDOMAttr>)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    DOM::DOMString _namespaceURI = NSStringToDOMString(namespaceURI);
    DOM::DOMString _qualifiedName = NSStringToDOMString(qualifiedName);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self impl]);
    DOM::Attr ret;
    
    ret= instance.createAttributeNS (_namespaceURI,_qualifiedName);
    
    return [WebCoreDOMAttr attrWithImpl:(DOM::AttrImpl *)ret.handle()];
}

- (id<WebDOMEntityReference>)createEntityReference:(NSString *)name
{
    return [WebCoreDOMEntityReference entityReferenceWithImpl: [self impl]->createEntityReference(NSStringToDOMString(name))];
}

- (id<WebDOMElement>)getElementById:(NSString *)elementId
{
    DOM::DOMString _elementId = NSStringToDOMString(elementId);
    return [WebCoreDOMElement elementWithImpl: [self impl]->getElementById(_elementId)];
}

- (id<WebDOMNodeList>)getElementsByTagName:(NSString *)tagname
{
    DOM::DOMString _tagname = NSStringToDOMString(tagname);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self impl]);
    DOM::NodeListImpl *nodeList = (DOM::NodeListImpl *)instance.getElementsByTagName(_tagname).handle();
    return [WebCoreDOMNodeList nodeListWithImpl: nodeList];
}

- (id<WebDOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    DOM::DOMString _namespaceURI = NSStringToDOMString(namespaceURI);
    DOM::DOMString _localName = NSStringToDOMString(localName);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self impl]);
    DOM::NodeList ret;
    
    ret = instance.getElementsByTagNameNS(_namespaceURI,_localName);
    
    return [WebCoreDOMNodeList nodeListWithImpl: (DOM::NodeListImpl *)ret.handle()];
}

- (id<WebDOMNode>)importNode:importedNode :(BOOL)deep
{
    DOM::Node importNode([(WebCoreDOMNode *)importedNode impl]);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self impl]);
    DOM::Node ret;
    
    ret = instance.importNode(importNode,deep);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)ret.handle()];
}

@end
