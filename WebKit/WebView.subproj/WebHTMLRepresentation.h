/*	
   	WebHTMLRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebHTMLRepresentationPrivate;

@protocol WebDocumentRepresentation;
@protocol WebDOMDocument;
@protocol WebDOMNode;

/*!
    @class WebHTMLRepresentation
*/
@interface WebHTMLRepresentation : NSObject <WebDocumentRepresentation>
{
    WebHTMLRepresentationPrivate *_private;
}

/*!
    @method documentSource
    @abstract Get the actual source of the document.
*/
- (NSString *)documentSource;

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
- (void)setSelectionFrom:(id<WebDOMNode>)start startOffset:(int)startOffset to:(id<WebDOMNode>)end endOffset:(int) endOffset;

/*!
    @method documentSource
    @abstract Get the current HTML reconstructed from the current state of the DOM.
*/
- (NSString *)reconstructedDocumentSource;


/*!
    @method attributedText
    @discussion Return the document source as an attributed string.
*/
- (NSAttributedString *)attributedText;

@end
