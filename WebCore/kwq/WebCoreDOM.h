/*	
    WebCoreDOM.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/
#import <Foundation/Foundation.h>

#ifdef __cplusplus

namespace DOM {
    class DOMImplementationImpl;
    class NodeImpl;
    class NodeListImpl;
    class NamedNodeMapImpl;
}

typedef DOM::DOMImplementationImpl DOMImplementationImpl;
typedef DOM::NodeImpl NodeImpl;
typedef DOM::NodeListImpl NodeListImpl;
typedef DOM::NamedNodeMapImpl NamedNodeMapImpl;

#else

@class DOMImplementationImpl;
@class NodeImpl;
@class NodeListImpl;
@class NamedNodeMapImpl;

#endif

@protocol WebDOMAttr;
@protocol WebDOMCharacterData;
@protocol WebDOMComment;
@protocol WebDOMCDATASection;
@protocol WebDOMDocument;
@protocol WebDOMDocumentFragment;
@protocol WebDOMDocumentType;
@protocol WebDOMElement;
@protocol WebDOMEntityReference;
@protocol WebDOMImplementation;
@protocol WebDOMNamedNodeMap;
@protocol WebDOMNode;
@protocol WebDOMNodeList;
@protocol WebDOMProcessingInstruction;
@protocol WebDOMText;

@interface WebCoreDOMNode : NSObject <WebDOMNode>
{
    NodeImpl *impl;
}
@end

@interface WebCoreDOMNamedNodeMap : NSObject <WebDOMNamedNodeMap>
{
    NamedNodeMapImpl *impl;
}
@end

@interface WebCoreDOMNodeList : NSObject <WebDOMNodeList>
{
    NodeListImpl *impl;
}
@end

@interface WebCoreDOMImplementation : NSObject <WebDOMImplementation>
{
    DOMImplementationImpl *impl;
}
@end

@interface WebCoreDOMDocumentType : WebCoreDOMNode <WebDOMDocumentType>
@end

@interface WebCoreDOMDocument : WebCoreDOMNode <WebDOMDocument>
@end

@interface WebCoreDOMEntityReference : WebCoreDOMNode <WebDOMEntityReference>
@end

@interface WebCoreDOMElement : WebCoreDOMNode <WebDOMElement>
@end

@interface WebCoreDOMAttr : WebCoreDOMNode <WebDOMAttr> 
@end

@interface WebCoreDOMDocumentFragment : WebCoreDOMNode <WebDOMDocumentFragment>
@end

@interface WebCoreDOMCharacterData : WebCoreDOMNode <WebDOMCharacterData>
@end

@interface WebCoreDOMText : WebCoreDOMCharacterData <WebDOMText>
@end

@interface WebCoreDOMComment : WebCoreDOMCharacterData <WebDOMComment>
@end

@interface WebCoreDOMCDATASection : WebCoreDOMText <WebDOMCDATASection>
@end

@interface WebCoreDOMProcessingInstruction : WebCoreDOMNode <WebDOMProcessingInstruction>
@end


