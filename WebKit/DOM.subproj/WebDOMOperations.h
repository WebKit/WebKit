/*	
    WebDOMOperations.h
    Copyright (C) 2004 Apple, Inc. All rights reserved.    
 
    Public header file.
*/

#import <WebKit/DOMCore.h>
#import <WebKit/DOMHTML.h>
#import <WebKit/DOMRange.h>

@class WebArchive;
@class WebFrame;

@interface DOMNode (WebDOMNodeOperations)

/*!
    @method webArchive
    @result A WebArchive representing the node and the children of the node.
*/
- (WebArchive *)webArchive;

@end

@interface DOMDocument (WebDOMDocumentOperations)

/*!
    @method webFrame
    @abstract Returns the frame of the DOM document.
*/
- (WebFrame *)webFrame;

@end

@interface DOMRange (WebDOMRangeOperations)

/*!
    @method webArchive
    @result A WebArchive representing the range.
*/
- (WebArchive *)webArchive;

@end

@interface DOMHTMLFrameElement (WebDOMHTMLFrameElementOperations)

/*!
    @method contentFrame
    @abstract Returns the content frame of the element.
*/
- (WebFrame *)contentFrame;

@end

@interface DOMHTMLIFrameElement (WebDOMHTMLIFrameElementOperations)

/*!
    @method contentFrame
    @abstract Returns the content frame of the element.
*/
- (WebFrame *)contentFrame;

@end

@interface DOMHTMLObjectElement (WebDOMHTMLObjectElementOperations)

/*!
    @method contentFrame
    @abstract Returns the content frame of the element.
    @discussion Returns non-nil only if the object represents a child frame
    such as if the data of the object is HTML content.
*/
- (WebFrame *)contentFrame;

@end

