/*	
   	WebHTMLRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebHTMLRepresentationPrivate;
@class NSView;

@protocol WebDocumentRepresentation;
@protocol WebDocumentSourceRepresentation;
@protocol WebDOMDocument;
@protocol WebDOMNode;
@protocol WebDOMElement;

/*!
    @class WebHTMLRepresentation
*/
@interface WebHTMLRepresentation : NSObject <WebDocumentRepresentation>
{
    WebHTMLRepresentationPrivate *_private;
}

/*!
    @method DOMDocument
    @abstract return the DOM document for this data source.
*/
- (id<WebDOMDocument>)DOMDocument;

/*!
    @method setSelectionFrom:startOffset:to:endOffset
    @abstract Set the text selection in the document.
    @param start The node that include the starting selection point.
    @param startOffset The character offset into the text of the starting node.
    @param end The node that includes the ending selection point.
    @param endOffset The character offset into the text of the ending node.
*/
- (void)setSelectionFrom:(id<WebDOMNode>)start startOffset:(int)startOffset to:(id<WebDOMNode>)end endOffset:(int)endOffset;

//- (NSAttributedString *)selectedAttributedString;

/*!
    @method documentSource
    @abstract Get the current HTML reconstructed from the current state of the DOM.
*/
- (NSString *)reconstructedDocumentSource;


- (NSAttributedString *)attributedStringFrom:(id<WebDOMNode>)startNode startOffset:(int)startOffset to:(id<WebDOMNode>)endNode endOffset:(int)endOffset;

- (id <WebDOMElement>)elementWithName:(NSString *)name inForm:(id <WebDOMElement>)form;
- (id <WebDOMElement>)elementForView:(NSView *)view;
- (BOOL)elementDoesAutoComplete:(id <WebDOMElement>)element;
- (BOOL)elementIsPassword:(id <WebDOMElement>)element;
- (id <WebDOMElement>)formForElement:(id <WebDOMElement>)element;
- (id <WebDOMElement>)currentForm;
- (NSArray *)controlsInForm:(id <WebDOMElement>)form;
- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(id <WebDOMElement>)element;
- (NSString *)matchLabels:(NSArray *)labels againstElement:(id <WebDOMElement>)element;
@end
