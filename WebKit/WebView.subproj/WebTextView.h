/*	
    WebTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import "WebSearchableTextView.h"

@protocol WebDocumentView;
@protocol WebDocumentDragSettings;

@interface WebTextView : WebSearchableTextView <WebDocumentView, WebDocumentDragSettings>
{
    BOOL canDragFrom;
    BOOL canDragTo;
}

- (void)setFixedWidthFont;

@end
