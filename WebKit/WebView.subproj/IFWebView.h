/*	
        IFWebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

/*
    ============================================================================= 
*/

@class IFWebDataSource;
@protocol IFWebController;

@class IFWebViewPrivate;

@interface IFWebView : NSView
{
@private
    IFWebViewPrivate *_private;
}

- initWithFrame: (NSRect)frame;

// Note that the controller is not retained.
- (id <IFWebController>)controller;

// These methods is typically called by the view's controller when
// the data source is changed.
- (void)dataSourceChanged: (IFWebDataSource *)dataSource;
- (void)provisionalDataSourceChanged: (IFWebDataSource *)dataSource;

- (void)setNeedsLayout: (bool)flag;

// This method should not be public until we have a more completely
// understood way to subclass IFWebView.
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
// Generally called by IFContextMenuHandlers from contextMenuItemsForNode:
#ifdef TENTATIVE_API
- (NSArray *)defaultContextMenuItemsForNode: (IFDOMNode *);
#endif
- (void)setContextMenusEnabled: (BOOL)flag;
- (BOOL)contextMenusEnabled;

// Remove the selection.
- (void)deselectText;

// Search from the end of the currently selected location, or from the beginning of the document if nothing
// is selected.
- (void)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)caseFlag;

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
            
        subclassing of IFWebView
*/
