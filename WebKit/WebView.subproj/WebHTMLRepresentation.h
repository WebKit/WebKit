/*	
   	WebHTMLRepresentation.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebHTMLRepresentationPrivate;

@protocol WebDocumentRepresentation;
@protocol WebDOMDocument;

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
