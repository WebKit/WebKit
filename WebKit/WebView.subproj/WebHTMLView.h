/*	
        WebHTMLView.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.
*/

#import <Cocoa/Cocoa.h>
#import <WebKit/WebDocument.h>

/*
    ============================================================================= 
*/

@class WebDataSource;
@class WebController;
@class WebHTMLViewPrivate;

@interface WebHTMLView : NSView <WebDocumentView, WebDocumentDragSettings, WebDocumentSearching, WebDocumentTextEncoding>
{
@private
    WebHTMLViewPrivate *_private;
}

- initWithFrame: (NSRect)frame;

- (void)setNeedsLayout: (BOOL)flag;

// Set needsToApplyStyles if you change anything that might impact styles, like
// font preferences.
- (void)setNeedsToApplyStyles: (BOOL)flag;

// Reapplies style information to the document.  This should not be called directly,
// instead call setNeedsToApplyStyles:.
- (void)reapplyStyles;

- (void)setContextMenusEnabled: (BOOL)flag;
- (BOOL)contextMenusEnabled;

// Remove the selection.
- (void)deselectText;

// Get an attributed string that represents the current selection.
- (NSAttributedString *)selectedText;

@end

