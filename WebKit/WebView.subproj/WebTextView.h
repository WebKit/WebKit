/*	
    WebTextView.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebSearchableTextView.h>

@class WebDataSource;

@interface WebTextView : WebSearchableTextView <WebDocumentView, WebDocumentText, WebDocumentSelection>
{
    float _textSizeMultiplier;
}

+ (NSArray *)unsupportedTextMIMETypes;
- (void)setFixedWidthFont;

- (void)appendReceivedData:(NSData *)data fromDataSource:(WebDataSource *)dataSource;

@end
