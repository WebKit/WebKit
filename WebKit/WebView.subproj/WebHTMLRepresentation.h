/*	
   	WebHTMLRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebHTMLRepresentationPrivate;
@class NSView;

@protocol WebDocumentRepresentation;
@protocol WebDocumentSourceRepresentation;
@protocol DOMDocument;
@protocol DOMNode;
@protocol DOMElement;

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
- (id<DOMDocument>)DOMDocument;

/*!
    @method setSelectionFrom:startOffset:to:endOffset
    @abstract Set the text selection in the document.
    @param start The node that include the starting selection point.
    @param startOffset The character offset into the text of the starting node.
    @param end The node that includes the ending selection point.
    @param endOffset The character offset into the text of the ending node.
*/
- (void)setSelectionFrom:(id<DOMNode>)start startOffset:(int)startOffset to:(id<DOMNode>)end endOffset:(int)endOffset;

//- (NSAttributedString *)selectedAttributedString;

/*!
    @method documentSource
    @abstract Get the current HTML reconstructed from the current state of the DOM.
*/
- (NSString *)reconstructedDocumentSource;


- (NSAttributedString *)attributedStringFrom:(id<DOMNode>)startNode startOffset:(int)startOffset to:(id<DOMNode>)endNode endOffset:(int)endOffset;

- (id <DOMElement>)elementWithName:(NSString *)name inForm:(id <DOMElement>)form;
- (id <DOMElement>)elementForView:(NSView *)view;
- (BOOL)elementDoesAutoComplete:(id <DOMElement>)element;
- (BOOL)elementIsPassword:(id <DOMElement>)element;
- (id <DOMElement>)formForElement:(id <DOMElement>)element;
- (id <DOMElement>)currentForm;
- (NSArray *)controlsInForm:(id <DOMElement>)form;
- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(id <DOMElement>)element;
- (NSString *)matchLabels:(NSArray *)labels againstElement:(id <DOMElement>)element;

- (NSString *)HTMLString;

@end
