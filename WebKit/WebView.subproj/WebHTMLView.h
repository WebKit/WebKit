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

// These methods is typically called by the view's controller when
// the data source is changed.
- (void)provisionalDataSourceChanged:(WebDataSource *)dataSource;
- (void)provisionalDataSourceCommitted:(WebDataSource *)dataSource;
- (void)dataSourceUpdated:(WebDataSource *)dataSource;

- (void)setNeedsLayout: (bool)flag;

// This method should not be public until we have a more completely
// understood way to subclass WebView.
- (void)layout;

// Set needsToApplyStyles if you change anything that might impact styles, like
// font preferences.
- (void)setNeedsToApplyStyles: (bool)flag;

// Reapplies style information to the document.  This should not be called directly,
// instead call setNeedsToApplyStyles:.
- (void)reapplyStyles;

// Stop animating animated GIFs, etc.
- (void)stopAnimations;

// Drag and drop links and images.  Others?
- (void)setCanDragFrom: (BOOL)flag;
- (BOOL)canDragFrom;
- (void)setCanDragTo: (BOOL)flag;
- (BOOL)canDragTo;

// Returns an array of built-in context menu items for this node.
// Generally called by WebContextMenuHandlers from contextMenuItemsForNode:
#ifdef TENTATIVE_API
- (NSArray *)defaultContextMenuItemsForNode: (WebDOMNode *);
#endif
- (void)setContextMenusEnabled: (BOOL)flag;
- (BOOL)contextMenusEnabled;

// Remove the selection.
- (void)deselectText;

// Get an attributed string that represents the current selection.
- (NSAttributedString *)selectedText;

@end

/*
    Areas still to consider:

        ALT image text and link URLs
            Should this be built-in?  or able to be overriden?
            
        node events
		
	client access to form elements for auto-completion, passwords
        
        selection
            Selection on data source is reflected in view.
            Does the view also need a cover selection API?
            
        subclassing of WebView
*/
