/*	
    WebTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import "WebSearchableTextView.h"

@protocol WebDocumentView;
@protocol WebDocumentDragSettings;
@protocol WebDocumentTextEncoding;

@interface WebTextView : WebSearchableTextView <WebDocumentView, WebDocumentDragSettings, WebDocumentTextEncoding>
{
    BOOL canDragFrom;
    BOOL canDragTo;
}

- (void)setFixedWidthFont;

@end
