/*	
        IFWebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebController.h>
#import <WebKit/IFWebFrame.h>

/* 
    =============================================================================
   
    A IFWebDataSource represents the data associated with a web page.

    ============================================================================= */

#ifdef TENTATIVE_API
@class IFLoader;
#endif



@interface IFWebDataSource : NSObject
{
@private
    id _dataSourcePrivate;
}


// Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
- initWithURL: (NSURL *)inputURL;

#ifdef TENTATIVE_API
- initWithData: (NSData *)data;
- initWithString: (NSString *)string;
- initWithLoader: (IFLoader *)loader;
#endif

// Returns YES if this is the main document.  The main document is the 'top'
// document, typically either a frameset or a normal HTML document.
- (BOOL)isMainDocument;


// Returns nil if this data source represents the main document.  Otherwise
// returns the parent data source.
- (IFWebDataSource *)parent;


// Set the frame that represents this data source.
//- (void)setFrame: (IFWebFrame *)f;


// Return the frame that represents this data source.
- (IFWebFrame *)frame;


// Add a child frame.  This should only be called by the data source's controller
// as a result of a createFrame:inParent:.
// [Should this be private?]
- (void)addFrame: (IFWebFrame *)frame;


// Returns an array of IFWebFrame.  The frames in the array are
// associated with a frame set or iframe.
- (NSArray *)children;


- (IFWebFrame *)frameNamed: (NSString *)frameName;

// Returns an array of NSStrings or nil.  The NSStrings corresponds to
// frame names.  If this data source is the main document and has no
// frames then frameNames will return nil.
- (NSArray *)frameNames;

// findDataSourceForFrameNamed: returns the child data source associated with
// the frame named 'name', or nil. 
- (IFWebDataSource *) findDataSourceForFrameNamed: (NSString *)name;


- (BOOL)frameExists: (NSString *)name;


- (void)openURL: (NSURL *)url inFrameNamed: (NSString *)frameName;



// Set the controller for this data source.  NOTE:  The controller is not retained by the
// data source.  Perhaps setController: should be private?  Perhaps the back pointers
// can be managed externally, i.e. + controllerForDataSource: as a class method on 
// IFDefaultWebController?
//- (void)setController: (id <IFWebController>)controller;
- (id <IFWebController>)controller;


// May return nil if not initialized with a URL.
- (NSURL *)inputURL;


// redirectedURL returns the URL that was actually used if there was a redirect.
// The value of redirectedURL will change if more than one redirect occurs.  If no
// redirect occurs the value of redirectedURL will be nil.  To monitor change in
// the value of the redirected URL override the <IFLocationChangeHandler> 
// serverRedirectTo:forDataSource: method.
- (NSURL *)redirectedURL;


// Returns true if the inputURL has been redirected by the server,
// i.e. inputURL != finalURL.
- (BOOL)wasRedirected;

// Start actually getting (if initialized with a URL) and parsing data. If the data source
// is still performing a previous load it will be stopped.
// If forceRefresh is YES the document will load from the net, not the cache.
- (void)startLoading: (BOOL)forceRefresh;


// Cancels any pending loads.  A data source is conceptually only ever loading
// one document at a time, although one document may have many related
// resources.  stopLoading will stop all loads related to the data source.
- (void)stopLoading;


// Returns YES if there are any pending loads.
- (BOOL)isLoading;


#ifdef TENTATIVE_API
// Get DOM access to the document.
- (IFDOMDocument *)document;
#endif

// Get the source of the document by reconstructing it from the DOM.
- (NSString *)documentTextFromDOM;


// Get the actual source of the document.
- (NSString *)documentText;


// URL reference point, these should probably not be public for 1.0.
- (NSURL *)base;
- (NSString *)baseTarget;


- (NSString *)encoding;


// Style sheet
- (void)setUserStyleSheetFromURL: (NSURL *)url;
- (void)setUserStyleSheetFromString: (NSString *)sheet;


// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that WebCore also has dependencies on the appkit.
- (NSImage *)icon;


// Is page secure, e.g. https, ftps
- (BOOL)isPageSecure;


// Returns nil or the page title.
- (NSString *)pageTitle;

- (NSString *)frameName;

@end


