/*	
        WKWebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/WKWebController.h>
#import <WebKit/WKWebCache.h>

/* 
   =============================================================================
   
    A WKWebDataSource represents all the state associated
    with a web page.  It is typicallly initialized with a URL, but
    may also be initialized with an NSString or NSData that hold
    HTML (?also other, i.e. image data?) content.
    
    Typical usage of a WKWebDataSource.
    
    WKWebDataSource *dataSource = [[WKWebDataSource alloc] initWithURL: url];
    id <WKWebController>myController = [[MyControllerClass alloc] init];
    [myController setDataSource: dataSource];

   Changes:
   
   2001-12-12
    
    After group discussion we decided to classify API as :
        Tier 1:  Needed by our browser (or Sherlock).
        Tier 2:  Nedded by Apple internal clients (Mail, Help, PB, other TBD).
        Tier 3:  Third party software vendors.
    
    Added finalURL and isRedirected.
   
    2001-12-13
    
        Remove setBase: and setBaseTarget:
        
        Changed return type of baseTarget to (NSString *)
        
        Added the following two methods:
            - (WKDataSource *)parent;
            - (NSArry *)children;
            - (BOOL)isMainDocument;
  
        Added the following methods:
        
            - (NSArray *)frameNames;
            - (WKWebDataSource) findDataSourceForFrameNamed: (NSString *)name;
            - (BOOL)frameExists: (NSString *)name;
            - (void)openURL: (NSURL *)url inFrameNamed: (NSString *)frameName;
            - (void)openURL: (NSURL *)url inIFrame: (id)iFrameIdentifier;

  2001-12-14

        Removed all mentions of resolved URLs, because browsers don't
        actuall treat DNS aliases specially.
        
        Moved search API to WKWebView.
        
        Moved WKPreferences to a new file, WKPreferences.h.  We are still discussing
        this item and it will not make it into the white paper.
                    
	Minor naming changes.

   ============================================================================= */
   

@interface WKWebDataSource : NSObject
{
@private
    id _dataSourcePrivate;
}

#ifdef READY_FOR_PRIMETIME


// Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
- initWithURL: (NSURL *)inputURL;

- initWithData: (NSData *)data;
- initWithString: (NSString *)string;

// Ken, need some help with one.
- initWithLoader: (WKLoader *)loader;


// Returns nil if this data source represents the main document.  Otherwise
// returns the parent data source.
- (WKDataSource *)parent;


// Returns YES if this is the main document.  The main document is the 'top'
// document, typically either a frameset or a normal HTML document.
- (BOOL)isMainDocument;

// Returns an array of WKWebDataSource.  The data sources in the array are
// the data source assoicated with a frame set or iframe.  If the main document
// is not a frameset, or has not iframes children will return nil.
- (NSArray *)children;


// Returns an array of NSStrings or nil.  The NSStrings corresponds to
// frame names.  If this data source is the main document and has no
// frames then frameNames will return nil.
- (NSArray *)frameNames;

// findDataSourceForFrameNamed: returns the child data source associated with
// the frame named 'name', or nil. 
- (WKWebDataSource) findDataSourceForFrameNamed: (NSString *)name;


- (BOOL)frameExists: (NSString *)name;


- (void)openURL: (NSURL *)url inFrameNamed: (NSString *)frameName;


- (void)openURL: (NSURL *)url inIFrame: (id)iFrameIdentifier;


// Set the controller for this data source.  NOTE:  The controller is not retained by the
// data source.  Perhaps setController: should be private?  Perhaps the back pointers
// can be managed externally, i.e. + controllerForDataSource: as a class method on 
// WKDefaultWebController?
//- (void)setController: (id <WKWebController>)controller;
- (id <WKWebController>)controller;


// May return nil if not initialized with a URL.
- (NSURL *)inputURL;


// finalURL returns the URL that was actually used.  The final URL
// may be different than the inputURL if the server redirects.
// <WKLocationChangedHandler> includes a message that is sent when
// a redirect is processed
- (NSURL *)finalURL;


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
// Returns NO if the data source is not currently loading.
- (void)stopLoading;


// Returns YES if there are any pending loads.
- (BOOL)isLoading;


// Get DOM access to the document.
- (WKDOMDocument *)document;


// Get the actual source of the docment.
- (NSString *)documentText;


// URL reference point, these should probably not be public for 1.0.
- (NSURL *)base;
- (NSString *)baseTarget;


- (NSString *)encoding;


// Style sheet
- (void)setUserStyleSheet: (NSURL *)url;
- (void)setUserStyleSheet: (NSString *)sheet;


// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that WebCore also has dependencies on the appkit.
- (NSImage *)icon;


// Is page secure, e.g. https, ftps
- (BOOL)isPageSecure;


// Returns nil or the page title.
- (NSString *)pageTitle;

#endif

@end


