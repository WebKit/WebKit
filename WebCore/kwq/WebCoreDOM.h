/*	
    WebCoreDOM.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/
#import <Foundation/Foundation.h>

namespace DOM {
    class DOMImplementationImpl;
    class NodeImpl;
    class NodeListImpl;
    class NamedNodeMapImpl;
}

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
    DOM::NodeImpl *impl;
}
@end

@interface WebCoreDOMNamedNodeMap : NSObject <WebDOMNamedNodeMap>
{
    DOM::NamedNodeMapImpl *impl;
}
@end

@interface WebCoreDOMNodeList : NSObject <WebDOMNodeList>
{
    DOM::NodeListImpl *impl;
}
@end

@interface WebCoreDOMImplementation : NSObject <WebDOMImplementation>
{
    DOM::DOMImplementationImpl *impl;
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


