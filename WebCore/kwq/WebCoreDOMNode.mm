/*	
    WebCoreDOMNode.mm
    Copyright 2002, Apple, Inc. All rights reserved.
*/
#import "WebCoreDOMPrivate.h"


DOM::NamedNodeMap DOM::NamedNodeMapImpl::createInstance(NamedNodeMapImpl *impl)
{
    return DOM::NamedNodeMap(impl);
}

DOM::Attr DOM::AttrImpl::createInstance(AttrImpl *impl)
{
    return DOM::Attr(impl);
}

@implementation WebCoreDOMNode

+ (WebCoreDOMNode *)nodeWithImpl: (DOM::NodeImpl *)_impl
{
    return [[(WebCoreDOMNode *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl:(DOM::NodeImpl *)coreImpl
{
    [super init];
    impl = coreImpl;
    impl->ref();
    return self;
}

- (DOM::NodeImpl *)impl
{
    return (DOM::NodeImpl *)impl;
}

- (void)dealloc
{
    [self impl]->deref();
    [super dealloc];
}

- (NSString *)nodeName
{
    DOM::DOMString _nodeName = [self impl]->nodeName();
    return domStringToNSString(_nodeName);
}

- (NSString *)nodeValue
{
    DOM::DOMString _nodeValue = [self impl]->nodeValue();
    return domStringToNSString(_nodeValue);
}

- (void)setNodeValue: (NSString *)string
{
    DOM::Node instance([self impl]);
    DOM::DOMString _string = NSStringToDOMString(string);
    instance.setNodeValue(_string);
}

- (unsigned short)nodeType
{
    return [self impl]->nodeType();
}

- (id<WebDOMNode>)parentNode
{
    return [WebCoreDOMNode nodeWithImpl: [self impl]->parentNode()];
}

- (id<WebDOMNodeList>)childNodes;
{
    return [WebCoreDOMNodeList nodeListWithImpl: [self impl]->childNodes()];
}

- (id<WebDOMNode>)firstChild
{
    return [WebCoreDOMNode nodeWithImpl: [self impl]->firstChild()];
}

- (id<WebDOMNode>)lastChild
{
    return [WebCoreDOMNode nodeWithImpl: [self impl]->lastChild()];
}

- (id<WebDOMNode>) previousSibling
{
    return [WebCoreDOMNode nodeWithImpl: [self impl]->previousSibling()];
}

- (id<WebDOMNode>)nextSibling
{
    return [WebCoreDOMNode nodeWithImpl: [self impl]->nextSibling()];
}

- (id<WebDOMNamedNodeMap>)attributes;
{
    DOM::Node instance([self impl]);
    DOM::NamedNodeMap ret;
    
    ret = instance.attributes();
    
    return [WebCoreDOMNamedNodeMap namedNodeMapWithImpl: (DOM::NamedNodeMapImpl *)ret.handle()];
}

- (id<WebDOMDocument>)ownerDocument
{
    DOM::Node instance([self impl]);
    DOM::Document ret;
    
    ret = instance.ownerDocument();
    
    return [WebCoreDOMDocument documentWithImpl: (DOM::DocumentImpl *)ret.handle()];
}

- (id<WebDOMNode>)insert:(id<WebDOMNode>)newChild before:(id<WebDOMNode>)refChild
{
    DOM::Node instance([self impl]);
    DOM::Node _newChild([(WebCoreDOMNode *)newChild impl]);
    DOM::Node _refChild([(WebCoreDOMNode *)refChild impl]);
    DOM::Node ret;
    
    ret = instance.insertBefore (_newChild, _refChild);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)ret.handle()];
}

- (id<WebDOMNode>)replace:(id<WebDOMNode>)newChild child:(id<WebDOMNode>)oldChild
{
    DOM::Node instance([self impl]);
    DOM::Node _newChild([(WebCoreDOMNode *)newChild impl]);
    DOM::Node _oldChild([(WebCoreDOMNode *)oldChild impl]);
    DOM::Node ret;
    
    ret = instance.replaceChild (_newChild, _oldChild);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)ret.handle()];
}

- (id<WebDOMNode>)removeChild:(id<WebDOMNode>)oldChild
{
    DOM::Node instance([self impl]);
    DOM::Node _oldChild([(WebCoreDOMNode *)oldChild impl]);
    DOM::Node ret;
    
    ret = instance.removeChild (_oldChild);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)ret.handle()];
}

- (id<WebDOMNode>)appendChild:(id<WebDOMNode>)newChild;
{
    DOM::Node instance([self impl]);
    DOM::Node _newChild([(WebCoreDOMNode *)newChild impl]);
    DOM::Node ret;
    
    ret = instance.appendChild (_newChild);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)ret.handle()];
}

- (BOOL)hasChildNodes
{
    return [self impl]->hasChildNodes();
}

- (id<WebDOMNode>)cloneNode: (BOOL) deep
{
    return [WebCoreDOMNode nodeWithImpl: [self impl]->cloneNode(deep)];
}

- (void)normalize
{
    [self impl]->normalize();
}

- (BOOL)isSupported:(NSString *)feature : (NSString *)version
{
    DOM::Node instance([self impl]);
    
    return instance.isSupported (NSStringToDOMString(feature), NSStringToDOMString(version));
}

- (NSString *)namespaceURI
{
    DOM::Node instance([self impl]);
    DOM::DOMString _namespaceURI = instance.namespaceURI();
    
    return domStringToNSString (_namespaceURI);
}

- (NSString *)prefix
{
    DOM::DOMString _prefix = [self impl]->prefix();
    
    return domStringToNSString(_prefix);
}

- (void)setPrefix: (NSString *)prefix
{
    DOM::Node instance([self impl]);
    instance.setPrefix(NSStringToDOMString(prefix));
}

- (NSString *)localName
{
    DOM::DOMString _localName = [self impl]->localName();
    
    return domStringToNSString(_localName);
}

- (BOOL)hasAttributes;
{
    DOM::Node instance([self impl]);
    return instance.hasAttributes();
}

@end


@implementation WebCoreDOMNodeList

+ (WebCoreDOMNodeList *)nodeListWithImpl: (DOM::NodeListImpl *)_impl
{
    return [[(WebCoreDOMNodeList *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl:(DOM::NodeListImpl *)coreImpl
{
    [super init];
    impl = coreImpl;
    impl->ref();
    return self;
}

- (DOM::NodeListImpl *)impl
{
    return (DOM::NodeListImpl *)impl;
}

- (void)dealloc
{
    [self impl]->deref();
    [super dealloc];
}

@end


@implementation WebCoreDOMNamedNodeMap

+ (WebCoreDOMNamedNodeMap *)namedNodeMapWithImpl: (DOM::NamedNodeMapImpl *)_impl
{
    return [[(WebCoreDOMNamedNodeMap *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl:(DOM::NamedNodeMapImpl *)coreImpl
{
    [super init];
    impl = coreImpl;
    impl->ref();
    return self;
}

- (DOM::NamedNodeMapImpl *)impl
{
    return (DOM::NamedNodeMapImpl *)impl;
}

- (void)dealloc
{
    [self impl]->deref();
    [super dealloc];
}

- (unsigned long) length
{
    return [self impl]->length();
}

- (id<WebDOMNode>)getNamedItem:(NSString *)name
{
    DOM::NamedNodeMap instance = DOM::NamedNodeMapImpl::createInstance([self impl]);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)instance.getNamedItem(NSStringToDOMString(name)).handle()];
}

- (id<WebDOMNode>)setNamedItem:(id<WebDOMNode>)arg
{
    DOM::NamedNodeMap instance = DOM::NamedNodeMapImpl::createInstance([self impl]);
    DOM::Node _arg ([(WebCoreDOMNode *)arg impl]);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)instance.setNamedItem(_arg).handle()];
}


- (id<WebDOMNode>)removeNamedItem:(NSString *)name;
{
    DOM::NamedNodeMap instance = DOM::NamedNodeMapImpl::createInstance([self impl]);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)instance.removeNamedItem(NSStringToDOMString(name)).handle()];
}

- (id<WebDOMNode>)item:(unsigned long) index;
{
    return [WebCoreDOMNode nodeWithImpl: [self impl]->item(index)];
}

- (id<WebDOMNode>)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
{
    DOM::NamedNodeMap instance = DOM::NamedNodeMapImpl::createInstance([self impl]);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)instance.getNamedItemNS(NSStringToDOMString(namespaceURI),NSStringToDOMString(localName)).handle()];
}

- (id<WebDOMNode>)setNamedItemNS:(id<WebDOMNode>)arg;
{
    DOM::NamedNodeMap instance = DOM::NamedNodeMapImpl::createInstance([self impl]);
    DOM::Node _arg ([(WebCoreDOMNode *)arg impl]);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)instance.setNamedItemNS(_arg).handle()];
}

- (id<WebDOMNode>)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
{
    DOM::NamedNodeMap instance = DOM::NamedNodeMapImpl::createInstance([self impl]);
    
    return [WebCoreDOMNode nodeWithImpl: (DOM::NodeImpl *)instance.removeNamedItemNS(NSStringToDOMString(namespaceURI),NSStringToDOMString(localName)).handle()];
}

@end

@implementation WebCoreDOMAttr

+ (WebCoreDOMAttr *)attrWithImpl: (DOM::AttrImpl *)_impl
{
    return [[(WebCoreDOMAttr *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl:(DOM::AttrImpl *)coreImpl
{
    [super init];
    impl = coreImpl;
    impl->ref();
    return self;
}

- (DOM::AttrImpl *)impl
{
    return (DOM::AttrImpl *)impl;
}

- (void)dealloc
{
    [self impl]->deref();
    [super dealloc];
}

- (NSString *)name
{
    DOM::Attr instance = DOM::AttrImpl::createInstance([self impl]);
    DOM::DOMString _name = instance.name();
    return domStringToNSString(_name);
}

- (BOOL)specified
{
    return [self impl]->specified();
}

- (NSString *)value
{
    DOM::Attr instance = DOM::AttrImpl::createInstance([self impl]);
    DOM::DOMString _value = instance.value();
    
    return domStringToNSString(_value);
}

- (void)setValue:(NSString *)value;
{
    DOM::Attr instance = DOM::AttrImpl::createInstance([self impl]);
    instance.setValue (NSStringToDOMString(value));
}

- (id<WebDOMElement>)ownerElement
{
    DOM::Attr instance = DOM::AttrImpl::createInstance([self impl]);
    return [WebCoreDOMElement elementWithImpl: (DOM::ElementImpl *)instance.ownerElement().handle()];
}

@end


@implementation WebCoreDOMCDATASection

+ (WebCoreDOMCDATASection *)CDATASectionWithImpl: (DOM::CDATASectionImpl *)_impl
{
    return [[(WebCoreDOMCDATASection *)[[self class] alloc] initWithImpl: _impl] autorelease];
}

- initWithImpl:(DOM::CDATASectionImpl *)coreImpl
{
    [super init];
    impl = coreImpl;
    impl->ref();
    return self;
}

- (DOM::CDATASectionImpl *)impl
{
    return (DOM::CDATASectionImpl *)impl;
}

- (void)dealloc
{
    [self impl]->deref();
    [super dealloc];
}

@end

@implementation WebCoreDOMDocumentFragment

+ (WebCoreDOMDocumentFragment *)documentFragmentWithImpl: (DOM::DocumentFragmentImpl *)_impl { return [[(WebCoreDOMDocumentFragment *)[[self class] alloc] initWithImpl: _impl] autorelease]; }
- initWithImpl:(DOM::DocumentFragmentImpl *)coreImpl { [super init]; impl = coreImpl; impl->ref(); return self; }
- (DOM::DocumentFragmentImpl *)impl { return (DOM::DocumentFragmentImpl *)impl; }
- (void)dealloc { [self impl]->deref();  [super dealloc]; }

@end

@implementation WebCoreDOMComment

+ (WebCoreDOMComment *)commentWithImpl: (DOM::CommentImpl *)_impl { return [[(WebCoreDOMComment *)[[self class] alloc] initWithImpl: _impl] autorelease]; }
- initWithImpl:(DOM::CommentImpl *)coreImpl { [super init]; impl = coreImpl; impl->ref(); return self; }
- (DOM::CommentImpl *)impl { return (DOM::CommentImpl *)impl; }
- (void)dealloc { [self impl]->deref();  [super dealloc]; }

@end

@implementation WebCoreDOMElement

+ (WebCoreDOMElement *)elementWithImpl: (DOM::ElementImpl *)_impl { return [[(WebCoreDOMElement *)[[self class] alloc] initWithImpl: _impl] autorelease]; }
- initWithImpl:(DOM::ElementImpl *)coreImpl { [super init]; impl = coreImpl; impl->ref(); return self; }
- (DOM::ElementImpl *)impl { return (DOM::ElementImpl *)impl; }
- (void)dealloc { [self impl]->deref();  [super dealloc]; }

@end


@implementation WebCoreDOMEntityReference

+ (WebCoreDOMEntityReference *)entityReferenceWithImpl: (DOM::EntityReferenceImpl *)_impl { return [[(WebCoreDOMEntityReference *)[[self class] alloc] initWithImpl: _impl] autorelease]; }
- initWithImpl:(DOM::EntityReferenceImpl *)coreImpl { [super init]; impl = coreImpl; impl->ref(); return self; }
- (DOM::EntityReferenceImpl *)impl { return (DOM::EntityReferenceImpl *)impl; }
- (void)dealloc { [self impl]->deref();  [super dealloc]; }

@end


@implementation WebCoreDOMText

+ (WebCoreDOMText *)textWithImpl: (DOM::TextImpl *)_impl { return [[(WebCoreDOMText *)[[self class] alloc] initWithImpl: _impl] autorelease]; }
- initWithImpl:(DOM::TextImpl *)coreImpl { [super init]; impl = coreImpl; impl->ref(); return self; }
- (DOM::TextImpl *)impl { return (DOM::TextImpl *)impl; }
- (void)dealloc { [self impl]->deref();  [super dealloc]; }

@end


@implementation WebCoreDOMProcessingInstruction

+ (WebCoreDOMProcessingInstruction *)processingInstructionWithImpl: (DOM::ProcessingInstructionImpl *)_impl
{ return [[(WebCoreDOMProcessingInstruction *)[[self class] alloc] initWithImpl: _impl] autorelease]; }
- initWithImpl:(DOM::ProcessingInstructionImpl *)coreImpl { [super init]; impl = coreImpl; impl->ref(); return self; }
- (DOM::ProcessingInstructionImpl *)impl { return (DOM::ProcessingInstructionImpl *)impl; }
- (void)dealloc { [self impl]->deref();  [super dealloc]; }

@end


