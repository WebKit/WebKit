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

#import "WebCoreDOMPrivate.h"

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

- (DOM::DocumentTypeImpl *)documentTypeImpl
{
    return static_cast<DOM::DocumentTypeImpl *>(impl);
}

- (NSString *)name
{
    DOM::DOMString name = [self documentTypeImpl]->name();
    return domStringToNSString(name);
}

- (id<WebDOMNamedNodeMap>)entities
{
    return [WebCoreDOMNamedNodeMap namedNodeMapWithImpl:[self documentTypeImpl]->entities()];
}

- (id<WebDOMNamedNodeMap>)notations
{
    return [WebCoreDOMNamedNodeMap namedNodeMapWithImpl:[self documentTypeImpl]->entities()];
}

- (NSString *)publicId
{
    DOM::DOMString publicId = [self documentTypeImpl]->publicId();
    return domStringToNSString(publicId);
}

- (NSString *)systemId
{
    DOM::DOMString systemId = [self documentTypeImpl]->systemId();
    return domStringToNSString(systemId);
}

- (NSString *)internalSubset
{
    DOM::DOMString internalSubset = [self documentTypeImpl]->internalSubset();
    return domStringToNSString(internalSubset);
}
@end


@implementation WebCoreDOMImplementation

+ (WebCoreDOMImplementation *)implementionatWithImpl: (DOM::DOMImplementationImpl *)_impl
{
    return [[(WebCoreDOMImplementation *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- (void)dealloc
{
    impl->deref();
    [super dealloc];
}

- (DOM::DOMImplementationImpl *)DOMImplementationImpl
{
    return static_cast<DOM::DOMImplementationImpl *>(impl);
}

- (BOOL)hasFeature: (NSString *)feature : (NSString *)version
{
    return [self DOMImplementationImpl]->hasFeature(NSStringToDOMString(feature),NSStringToDOMString(version));
}

- (id<WebDOMDocumentType>)createDocumentType: (NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId;
{
    DOM::DOMString _qualifiedName = NSStringToDOMString(qualifiedName);
    DOM::DOMString _publicId = NSStringToDOMString(publicId);
    DOM::DOMString _systemId = NSStringToDOMString(systemId);
    DOM::DOMImplementation instance = DOM::DOMImplementationImpl::createInstance([self DOMImplementationImpl]);
    DOM::DocumentType ret;
    
    ret = instance.createDocumentType (_qualifiedName, _publicId, _systemId);
    
    return [WebCoreDOMDocumentType documentTypeWithImpl: (DOM::DocumentTypeImpl *)ret.handle()];
}

- (id<WebDOMDocument>)createDocument: (NSString *)namespaceURI :(NSString *)qualifiedName :doctype
{
    DOM::DOMString _namespaceURI = NSStringToDOMString(namespaceURI);
    DOM::DOMString _qualifiedName = NSStringToDOMString(qualifiedName);
    DOM::DOMImplementation instance = DOM::DOMImplementationImpl::createInstance([self DOMImplementationImpl]);
    DOM::DocumentType docTypeInstance = DOM::DocumentTypeImpl::createInstance([(WebCoreDOMDocumentType *)doctype documentTypeImpl]);
    DOM::Document ret;
    
    ret = instance.createDocument (_namespaceURI, _qualifiedName, docTypeInstance);
    
    return [WebCoreDOMDocument documentWithImpl: (DOM::DocumentImpl *)ret.handle()];
}
@end



@implementation WebCoreDOMDocument

+ (WebCoreDOMDocument *)documentWithImpl: (DOM::DocumentImpl *)_impl
{
    return [[(WebCoreDOMDocument *)[WebCoreDOMDocument alloc] initWithImpl: _impl] autorelease];
}

- (DOM::DocumentImpl *)documentImpl
{
    return static_cast<DOM::DocumentImpl *>(impl);
}


- (id<WebDOMDocumentType>)doctype
{
    return [WebCoreDOMDocumentType documentTypeWithImpl: [self documentImpl]->doctype()];
}

- (id<WebDOMImplementation>)implementation
{
    return [WebCoreDOMImplementation implementationWithImpl: [self documentImpl]->implementation()];
}

- (id<WebDOMElement>)documentElement
{
    return [WebCoreDOMElement elementWithImpl: [self documentImpl]->documentElement()];
}

- (id<WebDOMElement>)createElement:(NSString *)tagName
{
    return [WebCoreDOMElement elementWithImpl: [self documentImpl]->createElement(NSStringToDOMString(tagName))];
}

- (id<WebDOMElement>)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    return [WebCoreDOMElement elementWithImpl: [self documentImpl]->createElementNS(NSStringToDOMString(namespaceURI),NSStringToDOMString(qualifiedName))];
}

- (id<WebDOMDocumentFragment>)createDocumentFragment
{
    return [WebCoreDOMDocumentFragment documentFragmentWithImpl: [self documentImpl]->createDocumentFragment()];
}

- (id<WebDOMText>)createTextNode:(NSString *)data
{
    return [WebCoreDOMText textWithImpl: [self documentImpl]->createTextNode(NSStringToDOMString(data))];
}

- (id<WebDOMComment>)createComment:(NSString *)data
{
    return [WebCoreDOMComment commentWithImpl: [self documentImpl]->createComment(NSStringToDOMString(data))];
}

- (id<WebDOMCDATASection>)createCDATASection:(NSString *)data
{
    return [WebCoreDOMCDATASection CDATASectionWithImpl: [self documentImpl]->createCDATASection(NSStringToDOMString(data))];
}

- (id<WebDOMProcessingInstruction>)createProcessingInstruction:(NSString *)target :(NSString *)data
{
    DOM::DOMString _target = NSStringToDOMString(target);
    DOM::DOMString _data = NSStringToDOMString(data);
    return [WebCoreDOMProcessingInstruction processingInstructionWithImpl: [self documentImpl]->createProcessingInstruction(_target,_data)];
}

- (id<WebDOMAttr>)createAttribute:(NSString *)name
{
    DOM::DOMString _name = NSStringToDOMString(name);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self documentImpl]);
    DOM::AttrImpl *attr = (DOM::AttrImpl *)instance.createAttribute(_name).handle();
    return [WebCoreDOMAttr attrWithImpl: attr];
}

- (id<WebDOMAttr>)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName
{
    DOM::DOMString _namespaceURI = NSStringToDOMString(namespaceURI);
    DOM::DOMString _qualifiedName = NSStringToDOMString(qualifiedName);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self documentImpl]);
    DOM::Attr ret;
    
    ret= instance.createAttributeNS (_namespaceURI,_qualifiedName);
    
    return [WebCoreDOMAttr attrWithImpl:(DOM::AttrImpl *)ret.handle()];
}

- (id<WebDOMEntityReference>)createEntityReference:(NSString *)name
{
    return [WebCoreDOMEntityReference entityReferenceWithImpl: [self documentImpl]->createEntityReference(NSStringToDOMString(name))];
}

- (id<WebDOMElement>)getElementById:(NSString *)elementId
{
    DOM::DOMString _elementId = NSStringToDOMString(elementId);
    return [WebCoreDOMElement elementWithImpl: [self documentImpl]->getElementById(_elementId)];
}

- (id<WebDOMNodeList>)getElementsByTagName:(NSString *)tagname
{
    DOM::DOMString _tagname = NSStringToDOMString(tagname);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self documentImpl]);
    DOM::NodeListImpl *nodeList = (DOM::NodeListImpl *)instance.getElementsByTagName(_tagname).handle();
    return [WebCoreDOMNodeList nodeListWithImpl: nodeList];
}

- (id<WebDOMNodeList>)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName
{
    DOM::DOMString _namespaceURI = NSStringToDOMString(namespaceURI);
    DOM::DOMString _localName = NSStringToDOMString(localName);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self documentImpl]);
    DOM::NodeList ret;
    
    ret = instance.getElementsByTagNameNS(_namespaceURI,_localName);
    
    return [WebCoreDOMNodeList nodeListWithImpl: (DOM::NodeListImpl *)ret.handle()];
}

- (id<WebDOMNode>)importNode:importedNode :(BOOL)deep
{
    DOM::Node importNode([(WebCoreDOMNode *)importedNode impl]);
    DOM::Document instance = DOM::DocumentImpl::createInstance([self documentImpl]);
    DOM::Node ret;
    
    ret = instance.importNode(importNode,deep);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)ret.handle()];
}

@end
