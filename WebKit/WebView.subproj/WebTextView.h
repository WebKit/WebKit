/*	
    WebTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import "WebSearchableTextView.h"

@protocol WebDocumentLoading;
@protocol WebDocumentDragSettings;

@interface WebTextView : WebSearchableTextView <WebDocumentLoading, WebDocumentDragSettings>
{
    BOOL canDragFrom;
    BOOL canDragTo;
}
@end
