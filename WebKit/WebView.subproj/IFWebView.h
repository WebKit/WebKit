/*	
        IFWebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebController.h>

/*
   ============================================================================= 

    Typical usage of a IFWebView.
    
    NSURL *url = [NSURL URLWithString: @"http://www.apple.com"];
    IFWebDataSource *dataSource = [[IFWebDataSource alloc] initWithURL: url];
    IFWebView *view = [[IFWebView alloc] initWithFrame: myFrame];
    IFDefaultWebController *controller = [[IFDefaultWebController alloc] initWithView: view dataSource: dataSource];

    [[[view controller] dataSource] startLoading];

    ...

    
    What is the behaviour of the view after it has been initialized and 
    startLoading: is called?
    
        1.  No IFLocationChangedHandler messages will be sent until 
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
            
        4.  When the controller receives a receivedDataForLocation: with a IFLoadProgress that 
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
@interface IFWebView : NSView
{
@private
    id _viewPrivate;
}

- initWithFrame: (NSRect)frame;


#ifdef TENTATIVE_API
// Set and get the delegate.
- (void)setDelegate: (id <IFWebViewDelegate>)delegate;
- (id <IFWebViewDelegate>)delegate;
#endif

 
// Set and get the controller.  Note that the controller is not retained.
// Perhaps setController: should be private?
- (id <IFWebController>)controller;


// This method is typically called by the view's controller when
// the data source is changed.
- (void)dataSourceChanged: (IFWebDataSource *)dataSource;


- (void)provisionalDataSourceChanged: (IFWebDataSource *)dataSource;


- (void)setNeedsLayout: (bool)flag;


// This method should not be public until we have a more completely
// understood way to subclass IFWebView.
- (void)layout;


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


