/*	
        WKWebController.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

@interface WKWebController : NSObject <WKLoadHandler, WKScriptContextHandler, WKFrameSetHandler, WKCredentialsHandler, WKLocationChangeHandler, WKContextMenuHandler>
@end

// See the comments in WKWebPageView above for more description about this protocol.
@protocol WKLocationChangeHandler
- (BOOL)locationWillChange;
- (void)loadingCancelled;
- (void)loadingStopped;
- (void)loadingFinished;
- (void)loadingStarted;
@end



@protocol WKContextMenuHandler
/*
    We decided to implement this in terms of a fixed set of types rather
    than per node.
    
    What items should be supported in the default context menus?
*/
@end

/*
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
    Implementors of this protocol will receive messages indicating
    data as it arrives.
    
    This method will be called even if the data source
    is initialized with something other than a URL.
*/
@protocol  WKLoadHandler

/*
    A new chunk of data has been received.  This could be a partial load
    of a url.  It may be useful to do incremental layout, although
    typically for non-base URLs this should be done after a URL (i.e. image)
    has been completely downloaded.
*/
- (void)receivedDataForLocation: (WKLoadProgress *)progress;

@end



/*
    A WKLoadProgress capture the state associated with a load progress
    indication.  Should we use a struct?
*/
@interface WKLoadProgress 
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalLoaded;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
    NSString *location; // Needs to be a string, not URL.
    LOAD_TYPES type;	// load types, either image, css, jscript, or html
}
@end



/*
    A class that implements WKScriptContextHandler provides all the state information
    that may be used by Javascript (AppleScript?).
    
*/
@protocol WKScriptContextHandler

- (void)setStatusText: (NSString *)text;
- (NSString *)statusText;

// Need API for things like window size and position, window ids,
// screen goemetry.  Essentially all the 'view' items that are
// accessible from Javascript.

@end


/*
*/
@protocol WKFrameSetHandler
- (NSArray *)frameNames;
- (WKFrame *)findFrameNamed: (NSString *)name;
- (BOOL)frameExists: (NSString *)name;
- (BOOL)openURLInFrame: (WKFrame *)aFrame url: (NSURL *)url;
@end

@protocol WKFrame
//
@end
