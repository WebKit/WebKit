/*	
   	WebHTMLRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import <WebKit/WebDocumentPrivate.h>

@class WebHTMLRepresentationPrivate;
@class NSView;

@class DOMNode;
@class DOMElement;

@protocol WebDocumentMarkup;
@protocol WebDocumentRepresentation;
@protocol WebDocumentSourceRepresentation;

/*!
    @class WebHTMLRepresentation
*/
@interface WebHTMLRepresentation : NSObject <WebDocumentRepresentation, WebDocumentDOM>
{
    WebHTMLRepresentationPrivate *_private;
}

- (NSAttributedString *)attributedStringFrom:(DOMNode *)startNode startOffset:(int)startOffset to:(DOMNode *)endNode endOffset:(int)endOffset;

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form;
- (DOMElement *)elementForView:(NSView *)view;
- (BOOL)elementDoesAutoComplete:(DOMElement *)element;
- (BOOL)elementIsPassword:(DOMElement *)element;
- (DOMElement *)formForElement:(DOMElement *)element;
- (DOMElement *)currentForm;
- (NSArray *)controlsInForm:(DOMElement *)form;
- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element;
- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element;

@end
