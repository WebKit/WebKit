/*	
        WKWebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/WKWebDataSource.h>
#import <WebKit/WKWebController.h>

#ifdef READY_FOR_PRIMETIME
/*
   ============================================================================= 

    Typical usage of a WKWebView.
    
    NSURL *url = [NSURL URLWithString: @"http://www.apple.com"];
    WKWebDataSource *dataSource = [[WKWebDataSource alloc] initWithURL: url];
    WKWebView *view = [[WKWebView alloc] initWithFrame: myFrame];
    WKConcreteWebController *controller = [[WKConcreteWebController alloc] initWithView: view dataSource: dataSource];

    [[[view controller] dataSource] startLoading];

    ...

    or

    ...
    
    WKWebView *view = [[WKWebView alloc] initWithFrame: myFrame url: url];
    [[[view controller] dataSource] startLoading];

    
    What is the behaviour of the view after it has been initialized and 
    startLoading: is called?
    
        1.  No WKLocationChangedHandler messages will be sent until 
            startLoading: is called.  After startLoading is called a loadingStarted
            message will be sent to the controller.  The view will remain unchanged 
            until the controller receives a receivedDataForLocation: message 
            from the data source.  
                                    
            If stopLoading is called before receipt of a receivedDataForLocation:, loading will 
            abort, the view will be unchanged, and the controller will receive a loadingCancelled 
            message.
            
            Controllers should initiate progress indicators upon receipt of loadingStarted,
            and terminate when either a loadingCancelled or loadingStopped is received.
        
        2.  After the controller receives it's first receivedDataForLocation: the contents of
            the view will be cleared and layout may begin.
            
        3.  Upon subsequent receipt of receivedDataForLocation: messages the controller
            may trigger a document layout.  Note that layouts may be coalesced.  If stopLoading
            is called before the document has fully loaded, the layout will be incomplete, and a
            loadingStopped message will be sent to the controller
            
        4.  When the controller receives a receivedDataForLocation: with a WKLoadProgress that 
            contains bytesSoFar==totalToLoad the location specified is completely loaded.  Clients
            may display the location string to indicate completion of a loaded resource.
            When the controller receives a loadingFinished message the main document and all it
            resources have been loaded.  Controllers should terminate progress indicators at 
            this point.
                    
    ============================================================================= 
    
    Changes:
    
    2001-12-14
        
        Added the following methods:
            - (void)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)case
            - deselectText;
            - (NSAttributedString *)selectedText;
       
        
        Remove explicit API to get/set the selection range.  This will be postponed until we
        have a DOM API that allows us to express selection ranges correctly.  Instead we have API
        that should support searching and getting a NSAttributedString that corresponds to
        the selected text.
        
*/
@interface WKWebView : NSView
{
@private
    id _viewPrivate;
}

- initWithFrame: (NSRect)frame;

// Convenience method.  initWithFrame:url: will create a controller and data source.
- initWithFrame: (NSRect)frame url: (NSURL *)url;


// Set and get the delegate.
- (void)setDelegate: (id <WKWebViewDelegate>)delegate;
- (id <WKWebViewDelegate>)delegate;

 
// Set and get the controller.  Note that the controller is not retained.
// Perhaps setController: should be private?
//- (void)setController: (id <WKWebController>)controller;
- (id <WKWebController>)controller;


// This method should not be public until we have a more completely
// understood way to subclass WKWebView.
- (void)layout;


// Stop animating animated GIFs, etc.
- (void)stopAnimations;


// Font API
- (void)setFontSizes: (NSArray *)sizes;
- (NSArray *)fontSizes;
- (void)resetFontSizes;
- (void)setStandardFont: (NSSFont *)font;
- (NSFont *)standardFont;
- (void)setFixedFont: (NSSFont *)font;
- (NSFont *)fixedFont;


// Drag and drop links and images.  Others?
- (void)setCanDragFrom: (BOOL)flag;
- (BOOL)canDragFrom;
- (void)setCanDragTo: (BOOL)flag;
- (BOOL)canDragTo;


// Returns an array of built-in context menu items for this node.
// Generally called by WKContextMenuHandlers from contextMenuItemsForNode:
- (NSArray *)defaultContextMenuItemsForNode: (WKDOMNode *);
- (void)setContextMenusEnabled: (BOOL)flag;
- (BOOL)contextMenusEnabled;


// Remove the selection.
- deselectText;


// Search from the end of the currently selected location, or from the beginning of the document if nothing
// is selected.
- (void)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)case


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
            
        subclassing of WKWebView
*/

#endif

