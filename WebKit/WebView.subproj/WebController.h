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
    of a URL.  It may be useful to do incremental layout, although
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

@end


/*
   ============================================================================= 
   WebContextMenuHandler determine what context menu items are visible over
   a clicked element.
   ============================================================================= 
*/

// These strings are keys into the element dictionary provided in 
// contextMenuItemsForElement.
extern NSString *WebContextMenuElementLinkURLKey;
extern NSString *WebContextMenuElementLinkLabelKey;
extern NSString *WebContextMenuElementImageURLKey;
extern NSString *WebContextMenuElementStringKey;
extern NSString *WebContextMenuElementImageKey;
extern NSString *WebContextMenuElementFrameKey;

@protocol WebContextMenuHandler <NSObject>
// Returns the array of NSMenuItems that will be displayed in the context menu 
// for the dictionary representation of the clicked element.
- (NSArray *)contextMenuItemsForElement: (NSDictionary *)element  defaultMenuItems: (NSArray *)menuItems;
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
- initWithView: (WebView *)view provisionalDataSource: (WebDataSource *)dataSource controllerSetName: (NSString *)name;

- (void)setWindowContext: (id<WebWindowContext>)context;
- (id<WebWindowContext>)windowContext;

- (void)setResourceProgressHandler: (id<WebResourceProgressHandler>)handler;
- (id<WebResourceProgressHandler>)resourceProgressHandler;

- (void)setDownloadProgressHandler: (id<WebResourceProgressHandler>)handler;
- (id<WebResourceProgressHandler>)downloadProgressHandler;

- (void)setContextMenuHandler: (id<WebContextMenuHandler>)handler;
- (id<WebContextMenuHandler>)contextMenuHandler;

- (void)setLocationChangeHandler:(id <WebLocationChangeHandler>)handler;
- (id <WebLocationChangeHandler>)locationChangeHandler;

- (void)setPolicyHandler: (id<WebControllerPolicyHandler>)handler;
- (id<WebControllerPolicyHandler>)policyHandler;

+ (WebURLPolicy *)defaultURLPolicyForURL: (NSURL *)URL;

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

// API to manage animated images.
- (void)stopAnimatedImages;
- (void)startAnimatedImages;

- (void)stopAnimatedImageLooping;
- (void)startAnimatedImageLooping;

+ (BOOL)canShowMIMEType:(NSString *)MIMEType;
+ (BOOL)canShowFile:(NSString *)path;

- (WebBackForwardList *)backForwardList;

- (void)setUseBackForwardList: (BOOL)flag;
- (BOOL)useBackForwardList;
- (BOOL)goBack;
- (BOOL)goForward;

- (void)setTextSizeMultiplier:(float)multiplier; // 1.0 is normal size
- (float)textSizeMultiplier;

// Set the application name. This name will be used in user-agent strings
// that are chosen for best results in rendering web pages.
- (void)setApplicationNameForUserAgent:(NSString *)applicationName;

// Set the user agent explicitly. Setting the user-agent string to nil means
// that WebKit should construct the best possible user-agent string for each URL
// for best results rendering web pages. Setting it to any string means
// that WebKit should use that user-agent string for all purposes until it is set
// back to nil.
- (void)setUserAgent:(NSString *)userAgentString;

// Get the appropriate user-agent string for a particular URL.
- (NSString *)userAgentForURL:(NSURL *)URL;

@end
