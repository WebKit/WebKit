/*	
    WebTextView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@protocol WebDocumentLoading;
@protocol WebDocumentDragSettings;
@protocol WebDocumentSearching;

@interface WebTextView : NSTextView <WebDocumentLoading, WebDocumentDragSettings, WebDocumentSearching>
{
    BOOL canDragFrom;
    BOOL canDragTo;
}
@end
