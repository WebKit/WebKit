/*	
        WKWebController.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>


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
    
    WKDefaultWebController may be subclassed to modify the behavior of the standard
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
  
  2001-12-13
  
        Removed WKFrameSetHandler, placed that functionality on WKWebDataSource.
        
        Changed WKLocationChangeHandler to add a parameter specifying the data source
        that sent the message.

  2001-12-14

        Removed inputURL:resolvedTo: methods, per discussion with Don.

        Remove WKContextMenuHandler for want of a better way to describe the
        not-yet-existing WKDOMNode.  We can't think of any initial clients that want
        to override the default behavior anyway.  Put it in WKGrabBag.h for now.

        Simplified WKLocationChangeHandler and updated
	WKAuthenticationHandler to use interfaces instead of structs..
*/


@class WKWebDataSource;
@class WKError;
@class WKWebView;
@class WKWebFrame;

#ifdef TENTATIVE_API
/*
   ============================================================================= 

    WKWebViewDelegates implement protocols that modify the behavior of
    WKWebViews.  A WKWebView does not require a delegate.
*/
@protocol WKWebViewDelegate <?>
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

    See the comments in WKWebPageView above for more description about this protocol.
*/
@protocol WKLocationChangeHandler

- (BOOL)locationWillChangeTo: (NSURL *)url;

- (void)locationChangeStarted;
- (void)locationChangeInProgress;
- (void)locationChangeDone: (WKError *)error;

- (void)receivedPageTitle: (NSString *)title;

- (void)serverRedirectTo: (NSURL *)url;

@end
#endif


/*
   ============================================================================= 

    A frame-aware version of the the WKLocationChangeHandler

    See the comments in WKWebPageView above for more description about this protocol.
*/
@protocol WKLocationChangeHandler

// locationWillChangeTo: is required, but will it be sent by the dataSource?  More
// likely the controller will receive a change request from the view.  That argues for
// placing locationWillChangeTo: in a different protocol, and making it more or a complete
// handshake.
- (BOOL)locationWillChangeTo: (NSURL *)url;

- (void)locationChangeStartedForDataSource: (WKWebDataSource *)dataSource;
- (void)locationChangeInProgressForDataSource: (WKWebDataSource *)dataSource;
- (void)locationChangeDone: (WKError *)error forDataSource: (WKWebDataSource *)dataSource;

- (void)receivedPageTitle: (NSString *)title forDataSource: (WKWebDataSource *)dataSource;

- (void)serverRedirectTo: (NSURL *)url forDataSource: (WKWebDataSource *)dataSource;

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
#ifdef TENTATIVE_API
    WK_LOAD_TYPES type;	// load types, either image, css, jscript, or html
#endif
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
- (void)receivedProgress: (WKLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (WKWebDataSource *)dataSource;

- (void)receivedError: (WKError *)error forResource: (NSString *)resourceDescription partialProgress: (WKLoadProgress *)progress fromDataSource: (WKWebDataSource *)dataSource;

@end



/*
   ============================================================================= 

*/

@interface WKSimpleAuthenticationResult 
{
    NSString *username;
    NSString *password;
    // May need an extra rememberThisPassword flag if the loader mechanism is
    // going to provide a persistent credentials cache (for starters we can have
    // just a session cache)
}
@end

@interface WKSimpleAuthenticationRequest 
{
    NSURL *url;         // nil if for something non-URI based
    NSString *domain;   // http authentication domain or some representation of 
                        // auth domain for non-URI-based locations; otherwise nil.
    NSString *username; // username, if already provided, otherwise nil
    BOOL plaintextPassword; // password will be sent in the clear
                            // TRUE for http basic auth or ftp
                            // FALSE for http digest auth
    unsigned previousFailures; // number of times in a row authenticating to this 
                               // location has failed; useful to be able to show a 
                               // different dialog based on count
}
@end


@protocol WKAuthenticationHandler
// Can we make this work without blocking the UI, or do we need to make it explicitly async
// somehow?
- (WKSimpleAuthenticationResult *) authenticate: (WKSimpleAuthenticationRequest *)request;

// do we need anything for fancier authentication schemes like kerberos or GSSAPI?

// Do we provide a default dialog?
@end


/*
   ============================================================================= 

*/
@protocol WKWebDataSourceErrorHandler
- receivedError: (WKError *)error forDataSource: (WKWebDataSource *)dataSource;
@end


/*
   ============================================================================= 

    A class that implements WKScriptContextHandler provides all the state information
    that may be used by Javascript (AppleScript?).
    
*/
@protocol WKScriptContextHandler

// setStatusText and statusText are used by Javascript's status bar text methods.
- (void)setStatusText: (NSString *)text forDataSource: (WKWebDataSource *)dataSource;
- (NSString *)statusTextForDataSource: (WKWebDataSource *)dataSource;

// Need API for things like window size and position, window ids,
// screen goemetry.  Essentially all the 'view' items that are
// accessible from Javascript.

@end





/*
   ============================================================================= 

    WKWebController implements all the behavior that ties together WKWebView
    and WKWebDataSource.  See each inherited protocol for a more complete
    description.
    
    [Don and I both agree that all these little protocols are useful to cleanly
     describe snippets of behavior, but do we explicity reference them anywhere,
     or do we just use the umbrella protocol?]
*/
@protocol WKWebController <WKLoadHandler, WKScriptContextHandler, WKAuthenticationHandler, WKLocationChangeHandler>


// Called when a data source needs to create a frame.  This method encapsulates the
// specifics of creating and initializaing a view of the appropriate class.
- (WKWebFrame *)createFrameNamed: (NSString *)fname for: (WKWebDataSource *)child inParent: (WKWebDataSource *)parent;

@end



