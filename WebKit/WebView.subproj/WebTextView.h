/*	
    WebTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import "WebSearchableTextView.h"

@class WebDataSource;
@protocol WebDocumentView;
@protocol WebDocumentDragSettings;
@protocol WebDocumentText;

@interface WebTextView : WebSearchableTextView <WebDocumentView, WebDocumentText>
{
    float _textSizeMultiplier;
}

+ (NSArray *)unsupportedTextMIMETypes;
- (void)setFixedWidthFont;

- (void)appendReceivedData:(NSData *)data fromDataSource:(WebDataSource *)dataSource;

@end
