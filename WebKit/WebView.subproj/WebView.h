/*	
        IFWebController.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#import <WebKit/IFLocationChangeHandler.h>
#import <WebKit/IFWebControllerPolicyHandler.h>

/*
   ============================================================================= 

    IFWebController manages the interaction between IFWebView(s) and IFWebDataSource(s).

    The IFWebController implements required behavior.  IFWebView and IFWebDataSource
    cannot function without a controller.  
    
    It it expected that alternate implementations of IFWebController will be written for
    alternate views of the web pages described by IFWebDataSources.  For example, a web
    crawler may implement a IFWebController with no corresponding view.
    
    IFWebController may be subclassed to modify the behavior of the standard
    IFWebView and IFWebDataSource.

   ============================================================================= 
*/

@class IFDownloadHandler;
@class IFError;
@class IFLoadProgress;
@class IFURLHandle;
@class IFWebController;
@class IFWebControllerPrivate;
@class IFWebDataSource;
@class IFWebFrame;
@class IFWebView;

@protocol IFDocumentLoading;

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
- (void)receivedProgress: (IFLoadProgress *)progress forResourceHandle: (IFURLHandle *)resourceHandle fromDataSource: (IFWebDataSource *)dataSource complete: (BOOL)isComplete;

- (void)receivedError: (IFError *)error forResourceHandle: (IFURLHandle *)resourceHandle partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource;

@end


/*
   ============================================================================= 

    A class that implements IFWindowContext provides window-related methods
    that may be used by Javascript, plugins and other aspects of web pages.
    
   ============================================================================= 
*/
@protocol IFWindowContext <NSObject>

- (IFWebController *)openNewWindowWithURL:(NSURL *)url;

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


#ifdef READY_FOR_PRIME_TIME
/*
   ============================================================================= 
*/
@protocol IFContextMenuHandler
// Returns the array of menu items for this node that will be displayed in the context menu.
// Typically this would be implemented by returning the results of IFWebView defaultContextMenuItemsForNode:
// after making any desired changes or additions.
- (NSArray *)contextMenuItemsForNode: (IFDOMNode *);
@end
#endif


/*
   ============================================================================= 

    IFWebController implements all the behavior that ties together IFWebView
    and IFWebDataSource.  See each inherited protocol for a more complete
    description.
    
   ============================================================================= 
*/

@interface IFWebController : NSObject
{
@private
    IFWebControllerPrivate *_private;
}

// Calls designated initializer with nil arguments.
- init;

// Designated initializer.
- initWithView: (IFWebView *)view provisionalDataSource: (IFWebDataSource *)dataSource;

- (void)setWindowContext: (id<IFWindowContext>)context;
- (id<IFWindowContext>)windowContext;

- (void)setResourceProgressHandler: (id<IFResourceProgressHandler>)handler;
- (id<IFResourceProgressHandler>)resourceProgressHandler;

+ (IFURLPolicy)defaultURLPolicyForURL: (NSURL *)url;

- (void)setPolicyHandler: (id<IFWebControllerPolicyHandler>)handler;
- (id<IFWebControllerPolicyHandler>)policyHandler;

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag;
- (BOOL)directsAllLinksToSystemBrowser;

// FIXME:  Should this method be private?
// Called when a data source needs to create a frame.  This method encapsulates the
// specifics of creating and initializaing a view of the appropriate class.
- (IFWebFrame *)createFrameNamed: (NSString *)fname for: (IFWebDataSource *)child inParent: (IFWebDataSource *)parent allowsScrolling: (BOOL)allowsScrolling;

// Look for a frame named name, recursively.
- (IFWebFrame *)frameNamed: (NSString *)name;

// Return the top level frame.  Note that even document that are not framesets will have a
// mainFrame.
- (IFWebFrame *)mainFrame;

// Return the frame associated with the data source.  Traverses the
// frame tree to find the data source.
- (IFWebFrame *)frameForDataSource: (IFWebDataSource *)dataSource;

// Return the frame associated with the view.  Traverses the
// frame tree to find the view. 
- (IFWebFrame *)frameForView: (IFWebView *)aView;

// Typically called after requestContentPolicyForContentMIMEType: is sent to a
// locationChangeHander.  The content policy of HTML URLs should always be IFContentPolicyShow.
// Setting the policy to IFContentPolicyIgnore will cancel the load of the URL if it is still
// pending.  The path argument is only used when the policy is either IFContentPolicySave or
// IFContentPolicySaveAndOpenExternally.
- (void)haveContentPolicy: (IFContentPolicy)policy andPath: (NSString *)path  forDataSource: (IFWebDataSource *)dataSource;

// API to manage animated images.
- (void)stopAnimatedImages;
- (void)startAnimatedImages;

- (void)stopAnimatedImageLooping;
- (void)startAnimatedImageLooping;

+ (BOOL)canShowMIMEType:(NSString *)MIMEType;
+ (BOOL)canShowFile:(NSString *)path;
@end
