/*	
    WebTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import "WebSearchableTextView.h"

@protocol WebDocumentView;
@protocol WebDocumentDragSettings;
@protocol WebDocumentText;

@interface WebTextView : WebSearchableTextView <WebDocumentView, WebDocumentText>
{
}

+ (NSArray *)unsupportedTextMIMETypes;
- (void)setFixedWidthFont;

@end
