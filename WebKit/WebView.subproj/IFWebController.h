/*	
        WKWebController.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#ifdef READY_FOR_PRIMETIME

/*
   ============================================================================= 

    WKWebController manages the interaction between WKWebView and WKWebDataSource.
    Intances of WKWebController retain their view and data source.
    
    [As in the appkit classes NSWindow and NSWindowController, it will be necessary 
     to have non-retained back pointers from WKWebView and WKWebDataSource to their
     controller.  Not retaining the controller will prevent cycles.  Of course, not
     retaining is bad, as it can lead to dangling references.  We could invert the reference
     graph and add ...inView: and ...inDataSource: to the API, but that's ugly.  If it's
     good enough for the appkit, it's good enough for us.  Ugh.]
     
                                 .--(p)WKWebController --.
                                 |      .    .           |
                                 |     .      .          |
                                \|/   .        .        \|/
    (p)WKWebViewDelegate <-- (c)WKWebView     (c)WKWebDataSource --> (p)WKWebDataSourceDelegate
    
    (c) indicates a class, (p) indicates a protocol.  The solid lines indicate an 
    retained reference.  The dotted lines indicate a non-retained reference.
    
    The WKWebController implements required behavior.  WKWebView and WKWebDataSource
    cannot function without a controller.  
    
    Delegates implement optional behavior.  WKWebView doesn't require a WKWebViewDelegate,
    nor does WKWebDataSource require a WKWebDataSourceDelegate.
    
    It it expected that alternate implementations of WKWebController will be written for
    alternate views of the web page described by WKWebDataSource.  For example, a 
    view that draws the DOM tree would require both a subclass of NSView and an
    implementation of WKWebController.  It is also possible that a web crawler
    may implement a WKWebController with no corresponding view.
    
    WKConcreteWebController may be subclassed to modify the behavior of the standard
    WKWebView and WKWebDataSource.

   ============================================================================= 
    
    Changes: 
    
    2001-12-12
        Changed WKConcreteWebController to WKDefaultWebController.
        
        Changed WKLocationChangedHandler naming, replace "loadingXXX" with
        "locationChangeXXX".

        Changed loadingStopped in WKLocationChangedHandler to locationChangeStopped:(WKError *).

        Changed loadingCancelled in WKLocationChangedHandler to locationChangeCancelled:(WKError *).
        
        Changed loadedPageTitle in WKLocationChangedHandler to receivedPageTitle:.

        Added inputURL:(NSURL *) resolvedTo: (NSURL *) to WKLocationChangedHandler.
        
        Added the following two methods to WKLocationChangedHandler:
        
            - (void)inputURL: (NSURL *)inputURL resolvedTo: (NSURL *)resolvedURL;
            - (void)serverRedirectTo: (NSURL *)url;
       
        Put locationWillChangeTo: back on WKLocationChangedHandler.
        
        Changed XXXforLocation in WKLoadHandler to XXXforResource.
        
        Changed timeoutForLocation: in WKLoadHandler to receivedError:forResource:partialProgress:
        
        Added the following two methods to WKDefaultWebController:
        
            - setDirectsAllLinksToSystemBrowser: (BOOL)flag
            - (BOOL)directsAllLinksToSystemBrowser;
            
        Removed WKError.  This will be described in WKError.h.
        
*/


/*
    WKWebController implements all the behavior that ties together WKWebView
    and WKWebDataSource.  See each inherited protocol for a more complete
    description.
    
    [Don and I both agree that all these little protocols are useful to cleanly
     describe snippets of behavior, but do we explicity reference them anywhere,
     or do we just use the umbrella protocol?]
*/
@protocol WKWebController <WKLoadHandler, WKScriptContextHandler, WKFrameSetHandler, WKCredentialsHandler, WKLocationChangeHandler>

- (void)setDataSource: (WKWebDataSource *)dataSource;
- (WKWebDataSource *)dataSource

- (void)setView: (WKWebView *)view;
- (WKWebView *)view;

@end


/*
   ============================================================================= 

    WKWebViewDelegates implement protocols that modify the behavior of
    WKWebViews.  A WKWebView does not require a delegate.
*/
@protocol WKWebViewDelegate <WKContextMenuHandler>
@end


/*
   ============================================================================= 

    WKWebDataSourceDelegate implement protocols that modify the behavior of
    WKWebDataSources.  A WKWebDataSources does not require a delegate.
*/
@protocol WKWebDataSourceDelegate <?>
@end


/*
   ============================================================================= 

    Is WKConcreteWebController the right name?  The name fits with the
    scheme used by Foundation, although Foundation's concrete classes
    typically aren't public.
*/
@interface WKDefaultWebController : NSObject <WKWebController>

- initWithView: (WKWebView *) dataSource: (WKWebDataSource *)dataSource;

- setDirectsAllLinksToSystemBrowser: (BOOL)flag
- (BOOL)directsAllLinksToSystemBrowser;

@end

/*
   ============================================================================= 

    See the comments in WKWebPageView above for more description about this protocol.
*/
@protocol WKLocationChangeHandler

- (BOOL)locationWillChangeTo: (NSURL *)url;

- (void)locationChangeStarted;
- (void)locationChangeCancelledL: (WKError *)error;
- (void)locationChangeStopped: (WKError *)error;
- (void)locationChangeFinished;

- (void)receivedPageTitle: (NSString *)title;

- (void)inputURL: (NSURL *)inputURL resolvedTo: (NSURL *)resolvedURL;
- (void)serverRedirectTo: (NSURL *)url;

@end


/*
   ============================================================================= 

    A WKLoadProgress capture the state associated with a load progress
    indication.  Should we use a struct?
*/
@interface WKLoadProgress 
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalToLoad;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
    WK_LOAD_TYPES type;	// load types, either image, css, jscript, or html
}
@end


/*
   ============================================================================= 

    Implementors of this protocol will receive messages indicating
    data has been received.
    
    The methods in this protocol will be called even if the data source
    is initialized with something other than a URL.
*/
@protocol  WKLoadHandler

/*
    A new chunk of data has been received.  This could be a partial load
    of a url.  It may be useful to do incremental layout, although
    typically for non-base URLs this should be done after a URL (i.e. image)
    has been completely downloaded.
*/
- (void)receivedProgress: (WKLoadProgress *)progress forResource: (NSString *)resourceDescription;

- (void)receivedError: (WKError *)error forResource: (NSString *)resourceDescription partialProgress: (WKLoadProgress *)progress;

@end

/*
   ============================================================================= 
*/
@protocol WKContextMenuHandler
// Returns the array of menu items for this node that will be displayed in the context menu.
// Typically this would be implemented by returning the results of WKWebView defaultContextMenuItemsForNode:
// after making any desired changes or additions.
- (NSArray *)contextMenuItemsForNode: (WKDOMNode *);
@end


/*
   ============================================================================= 

*/
@protocol WKCredentialsHandler
// Ken will come up with a proposal for this.  We decided not to have a generic API,
// rather we'll have an API that explicitly knows about the authentication
// attributes needed.
// Client should use this API to collect information necessary to authenticate,
// usually by putting up a dialog.
// Do we provide a default dialog?
@end


/*
   ============================================================================= 

*/
@protocol WKWebDataSourceErrorHandler
- error: (WKError *)error;
@end


/*
   ============================================================================= 

    A class that implements WKScriptContextHandler provides all the state information
    that may be used by Javascript (AppleScript?).
    
*/
@protocol WKScriptContextHandler

// setStatusText and statusText are used by Javascript's status bar text methods.
- (void)setStatusText: (NSString *)text;
- (NSString *)statusText;

// Need API for things like window size and position, window ids,
// screen goemetry.  Essentially all the 'view' items that are
// accessible from Javascript.

@end


/*
   ============================================================================= 

*/
@protocol WKFrameSetHandler
- (NSArray *)frameNames;
- (id <WKFrame>) findFrameNamed: (NSString *)name;
- (BOOL)frameExists: (NSString *)name;
- (void)openURL: (NSURL *)url inFrame: (id <WKFrame>) frame;
@end


/*
   ============================================================================= 

*/
@protocol WKFrame
@end

#endif

