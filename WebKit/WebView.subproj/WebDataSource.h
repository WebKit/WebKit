/*	
        WebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

#import <WebKit/WebController.h>

/* 
    =============================================================================
   
    A WebDataSource represents the data associated with a web page.

    ============================================================================= */

@class WebFrame;
@class WebResourceHandle;
@class WebController;

#ifdef TENTATIVE_API
@class WebLoader;
#endif

@class WebDataSourcePrivate;
@protocol WebDocumentRepresentation;

@interface WebDataSource : NSObject
{
@private
    WebDataSourcePrivate *_private;
}

// Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
-(id)initWithURL:(NSURL *)inputURL;
-(id)initWithURL:(NSURL *)theURL attributes:(NSDictionary *)theAttributes;
-(id)initWithURL:(NSURL *)theURL attributes:(NSDictionary *)theAttributes flags:(unsigned)theFlags;

#ifdef TENTATIVE_API
- initWithData: (NSData *)data;
- initNSURL _web_URLWithString: (NSString *)string;
- initWithLoader: (WebLoader *)loader;
#endif

- (NSData *)data;

- (id <WebDocumentRepresentation>)representation;

// Returns YES if this is the main document.  The main document is the 'top'
// document, typically either a frameset or a normal HTML document.
- (BOOL)isMainDocument;

// Returns nil if this data source represents the main document.  Otherwise
// returns the parent data source.
- (WebDataSource *)parent;

// Return the frame that represents this data source. Same as above.
- (WebFrame *)webFrame;

// Add a child frame.  This should only be called by the data source's controller
// as a result of a createFrame:inParent:.
// [Should this be private?]
- (void)addFrame: (WebFrame *)frame;

// Returns an array of WebFrame.  The frames in the array are
// associated with a frame set or iframe.
- (NSArray *)children;

- (WebFrame *)frameNamed: (NSString *)frameName;

// Returns an array of NSStrings or nil.  The NSStrings corresponds to
// frame names.  If this data source is the main document and has no
// frames then frameNames will return nil.
- (NSArray *)frameNames;

// findDataSourceForFrameNamed: returns the child data source associated with
// the frame named 'name', or nil. 
- (WebDataSource *) findDataSourceForFrameNamed: (NSString *)name;

- (BOOL)frameExists: (NSString *)name;

- (void)openURL: (NSURL *)url inFrameNamed: (NSString *)frameName;

// Set the controller for this data source.  NOTE:  The controller is not retained by the
// data source.  Perhaps setController: should be private?  Perhaps the back pointers
// can be managed externally, i.e. + controllerForDataSource: as a class method on 
// WebController?
//- (void)setController: (WebController *)controller;
- (WebController *)controller;

// May return nil if not initialized with a URL.
- (NSURL *)inputURL;

// redirectedURL returns the URL that was actually used if there was a redirect.
// The value of redirectedURL will change if more than one redirect occurs.  If no
// redirect occurs the value of redirectedURL will be nil.  To monitor change in
// the value of the redirected URL override the <WebLocationChangeHandler> 
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
- (WebDOMDocument *)document;
#endif

- (BOOL)isDocumentHTML;

// Get the source of the document by reconstructing it from the DOM.
- (NSString *)documentTextFromDOM;


// Get the actual source of the document.
- (NSString *)documentSource;


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

- (WebContentPolicy)contentPolicy;

// MIME type
- (NSString *)contentType;

// extension based on the MIME type 
- (NSString *)fileType;

- (NSString *)downloadPath;

- (NSDictionary *)errors;

- (WebError *)mainDocumentError;

+ (void) registerRepresentationClass:(Class)repClass forMIMEType:(NSString *)MIMEType;

- (void) makeRepresentation;

@end
