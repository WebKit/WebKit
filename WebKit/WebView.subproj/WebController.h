/*	
        WebController.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#import <WebKit/WebLocationChangeHandler.h>
#import <WebKit/WebControllerPolicyHandler.h>

/*
   ============================================================================= 

    WebController manages the interaction between WebView(s) and WebDataSource(s).

    The WebController implements required behavior.  WebView and WebDataSource
    cannot function without a controller.  
    
    It it expected that alternate implementations of WebController will be written for
    alternate views of the web pages described by WebDataSources.  For example, a web
    crawler may implement a WebController with no corresponding view.
    
    WebController may be subclassed to modify the behavior of the standard
    WebView and WebDataSource.

   ============================================================================= 
*/

@class WebBackForwardList;
@class WebController;
@class WebControllerPrivate;
@class WebDataSource;
@class WebDownloadHandler;
@class WebError;
@class WebFrame;
@class WebLoadProgress;
@class WebResourceHandle;
@class WebView;

@protocol WebDocumentLoading;

/*
   ============================================================================= 

    Implementors of this protocol will receive messages indicating
    data has been received.
    
    The methods in this protocol will be called even if the data source
    is initialized with something other than a URL.

   ============================================================================= 
*/

@protocol WebResourceProgressHandler <NSObject>

/*
    A new chunk of data has been received.  This could be a partial load
    of a url.  It may be useful to do incremental layout, although
    typically for non-base URLs this should be done after a URL (i.e. image)
    has been completely downloaded.
*/
- (void)receivedProgress: (WebLoadProgress *)progress forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource complete: (BOOL)isComplete;

- (void)receivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle partialProgress: (WebLoadProgress *)progress fromDataSource: (WebDataSource *)dataSource;

@end


/*
   ============================================================================= 

    A class that implements WebWindowContext provides window-related methods
    that may be used by Javascript, plugins and other aspects of web pages.
    
   ============================================================================= 
*/
@protocol WebWindowContext <NSObject>

- (WebController *)openNewWindowWithURL:(NSURL *)URL;

- (void)setStatusText: (NSString *)text;
- (NSString *)statusText;

- (BOOL)areToolbarsVisible;
- (void)setToolbarsVisible:(BOOL)visible;

- (BOOL)isStatusBarVisible;
- (void)setStatusBarVisible:(BOOL)visible;

// Even though a caller could set the frame directly using the NSWindow,
// this method is provided so implementors of this protocol can do special
// things on programmatic move/resize, like avoiding autosaving of the size.
- (void)setFrame:(NSRect)frame;
   
- (NSWindow *)window;

// FIXME: This is temporary. It's used to tell the client to "go back"
// when the delete key is pressed. But we are going to move back/forward
// handling into WebKit, and then this can be removed.
- (void)goBack;

@end


#define WebContextLinkURL  @"WebContextLinkURL"
#define WebContextImageURL @"WebContextImageURL"
#define WebContextString   @"WebContextString"
#define WebContextImage    @"WebContextImage"
#define WebContextFrame    @"WebContextFrame"


/*
   ============================================================================= 
*/
@protocol WebContextMenuHandler <NSObject>
// Returns the array of NSMenuItems that will be displayed in the context menu 
// for the dictionary representation of the clicked element.
- (NSArray *)contextMenuItemsForElementInfo: (NSDictionary *)elementInfo  defaultMenuItems: (NSArray *)menuItems;
@end



/*
   ============================================================================= 

    WebController implements all the behavior that ties together WebView
    and WebDataSource.  See each inherited protocol for a more complete
    description.
    
   ============================================================================= 
*/

@interface WebController : NSObject
{
@private
    WebControllerPrivate *_private;
}

// Calls designated initializer with nil arguments.
- init;

// Designated initializer.
- initWithView: (WebView *)view provisionalDataSource: (WebDataSource *)dataSource;

- (void)setWindowContext: (id<WebWindowContext>)context;
- (id<WebWindowContext>)windowContext;

- (void)setResourceProgressHandler: (id<WebResourceProgressHandler>)handler;
- (id<WebResourceProgressHandler>)resourceProgressHandler;

- (void)setDownloadProgressHandler: (id<WebResourceProgressHandler>)handler;
- (id<WebResourceProgressHandler>)downloadProgressHandler;

- (void)setContextMenuHandler: (id<WebContextMenuHandler>)handler;
- (id<WebContextMenuHandler>)contextMenuHandler;

+ (WebURLPolicy)defaultURLPolicyForURL: (NSURL *)url;

- (void)setPolicyHandler: (id<WebControllerPolicyHandler>)handler;
- (id<WebControllerPolicyHandler>)policyHandler;

- (void)setLocationChangeHandler:(id <WebLocationChangeHandler>)handler;
- (id <WebLocationChangeHandler>)locationChangeHandler;

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag;
- (BOOL)directsAllLinksToSystemBrowser;

// FIXME:  Should this method be private?
// Called when a data source needs to create a frame.  This method encapsulates the
// specifics of creating and initializing a view of the appropriate class.
- (WebFrame *)createFrameNamed: (NSString *)fname for: (WebDataSource *)child inParent: (WebDataSource *)parent allowsScrolling: (BOOL)allowsScrolling;

// Look for a frame named name, recursively.
- (WebFrame *)frameNamed: (NSString *)name;

// Return the top level frame.  Note that even document that are not framesets will have a
// mainFrame.
- (WebFrame *)mainFrame;

// Return the frame associated with the data source.  Traverses the
// frame tree to find the data source.
- (WebFrame *)frameForDataSource: (WebDataSource *)dataSource;

// Return the frame associated with the view.  Traverses the
// frame tree to find the view. 
- (WebFrame *)frameForView: (WebView *)aView;

// Typically called after requestContentPolicyForContentMIMEType: is sent to a
// locationChangeHander.  The content policy of HTML URLs should always be WebContentPolicyShow.
// Setting the policy to WebContentPolicyIgnore will cancel the load of the URL if it is still
// pending.  The path argument is only used when the policy is either WebContentPolicySave or
// WebContentPolicySaveAndOpenExternally.
- (void)haveContentPolicy: (WebContentPolicy)policy andPath: (NSString *)path  forDataSource: (WebDataSource *)dataSource;

// API to manage animated images.
- (void)stopAnimatedImages;
- (void)startAnimatedImages;

- (void)stopAnimatedImageLooping;
- (void)startAnimatedImageLooping;

+ (BOOL)canShowMIMEType:(NSString *)MIMEType;
+ (BOOL)canShowFile:(NSString *)path;

- (WebBackForwardList *)backForwardList;

@end
