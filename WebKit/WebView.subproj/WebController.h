/*	
        IFWebController.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#import <WebKit/IFLocationChangeHandler.h>

/*
   ============================================================================= 

    IFWebController manages the interaction between IFWebView(s) and IFWebDataSource(s).

    The IFWebController implements required behavior.  IFWebView and IFWebDataSource
    cannot function without a controller.  
    
    It it expected that alternate implementations of IFWebController will be written for
    alternate views of the web pages described by IFWebDataSources.  For example, a web
    crawler may implement a IFWebController with no corresponding view.
    
    IFBaseWebController may be subclassed to modify the behavior of the standard
    IFWebView and IFWebDataSource.

   ============================================================================= 
*/

@class IFDownloadHandler;
@class IFError;
@class IFLoadProgress;
@class IFWebDataSource;
@class IFWebFrame;
@protocol IFWebController;


/*
   ============================================================================= 

    Implementors of this protocol will receive messages indicating
    data has been received.
    
    The methods in this protocol will be called even if the data source
    is initialized with something other than a URL.

   ============================================================================= 
*/

@protocol IFResourceProgressHandler <NSObject>

/*
    A new chunk of data has been received.  This could be a partial load
    of a url.  It may be useful to do incremental layout, although
    typically for non-base URLs this should be done after a URL (i.e. image)
    has been completely downloaded.
*/
- (void)receivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource;

- (void)receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource;

@end

/*
   ============================================================================= 

    A class that implements IFScriptContextHandler provides all the state information
    that may be used by Javascript (AppleScript?).
    
   ============================================================================= 
*/
@protocol IFScriptContextHandler <NSObject>

// setStatusText and statusText are used by Javascript's status bar text methods.
- (void)setStatusText: (NSString *)text forDataSource: (IFWebDataSource *)dataSource;
- (NSString *)statusTextForDataSource: (IFWebDataSource *)dataSource;

// Need API for things like window size and position, window ids,
// screen goemetry.  Essentially all the 'view' items that are

// FIXME: not strictly a scripting issue
- (id<IFWebController>)openNewWindowWithURL:(NSURL *)url;
@end

/*
   ============================================================================= 

    IFWebController implements all the behavior that ties together IFWebView
    and IFWebDataSource.  See each inherited protocol for a more complete
    description.
    
   ============================================================================= 
*/

typedef enum {
    IFURLPolicyUseContentPolicy,
    IFURLPolicyOpenExternally,
    IFURLPolicyIgnore
} IFURLPolicy;

@protocol IFWebController <NSObject, IFResourceProgressHandler, IFScriptContextHandler>

// Called when a data source needs to create a frame.  This method encapsulates the
// specifics of creating and initializaing a view of the appropriate class.
- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)child inParent: (IFWebDataSource *)parent inScrollView: (BOOL)inScrollView;

// Look for a frame named name, recursively.
- (IFWebFrame *)frameNamed: (NSString *)name;

// Return the top level frame.  Note that even document that are not framesets will have a
// mainFrame.
- (IFWebFrame *)mainFrame;

// Return the frame associated with the data source.  Traverses the
// frame tree to find the data source.
- (IFWebFrame *)frameForDataSource: (IFWebDataSource *)dataSource;

// Return the frame associated with the view.  Traverses the
// frame tree to find the data source.  Typically aView is
// an IFWebView.
- (IFWebFrame *)frameForView: (NSView *)aView;

// DEPRECATED
- (id <IFLocationChangeHandler>)provideLocationChangeHandlerForFrame: (IFWebFrame *)frame;

- (id <IFLocationChangeHandler>)provideLocationChangeHandlerForFrame: (IFWebFrame *)frame andURL: (NSURL *)url;

// URLPolicyForURL: is used to determine what to do BEFORE a URL is loaded, i.e.
// before it is clicked or loaded via a URL bar.  Clients can choose to handle the
// URL normally (i.e. Alexander), hand the URL off to launch services (i.e. Mail), or
// ignore the URL (i.e. Help Viewer?).  This API could potentially be used by mac manager
// to filter allowable URLs.
- (IFURLPolicy)URLPolicyForURL: (NSURL *)url;

// We may have different errors that cause the the policy to be un-implementable, i.e.
// launch services failure, etc.
- (void)unableToImplementURLPolicyForURL: (NSURL *)url error: (IFError *)error;

// FIXME:  this method should be moved to a protocol
// Called when a plug-in for a certain mime type is not installed
- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)url;

// Typically called after requestContentPolicyForContentMIMEType: is sent to a locationChangeHander.
// The content policy of HTML URLs should always be IFContentPolicyShow.  Setting the policy to 
// IFContentPolicyIgnore will cancel the load of the URL if it is still pending.  The path argument 
// is only used when the policy is either IFContentPolicySave or IFContentPolicyOpenExternally.
- (void)haveContentPolicy: (IFContentPolicy)policy andPath: (NSString *)path forLocationChangeHandler: (id <IFLocationChangeHandler>)handler;

// API to manage animated images.
- (void)stopAnimatedImages;
- (void)startAnimatedImages;

- (void)stopAnimatedImageLooping;
- (void)startAnimatedImageLooping;


@end
