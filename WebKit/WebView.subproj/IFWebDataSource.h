/*	
        WKWebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/WKWebController.h>

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
   
        
   ============================================================================= 
*/
   
#ifdef READY_FOR_PRIMETIME

@interface WKWebDataSource : NSObject
{
@private
    id _dataSourcePrivate;
}


// Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
- initWithURL: (NSURL *)inputUrl;

- initWithData: (NSData *)data;
- initWithString: (NSString *)string;

// Ken, need some help with one.
- initWithLoader: (WKURILoader *)loader;


// Set the controller for this data source.  NOTE:  The controller is not retained by the
// data source.  Perhaps setController: should be private?
- (void)setController: (id <WKWebController>)controller;
- (id <WKWebController>)controller;


// May return nil if not initialized with a URL.
- (NSURL *)inputURL;


// The inputURL may resolve to a different URL as a result of
// a DNS redirect.  May return nil if the URL has not yet been
// resolved.  <WKLocationChangedHandler> includes a message
// that is sent after the URL has been resolved.
- (NSURL *)resolvedURL;


// finalURL returns the URL that was actually used.  The final URL
// may be different than the resolvedURL if the server redirects.
// <WKLocationChangedHandler> includes a message that is sent after
// the URL has been resolved.
- (NSURL *)finalURL;


// Returns true if the resolvedURL has been redirected by the server,
// i.e. resolvedURL != finalURL.
- (BOOL)isRedirected;

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
- setBase: (NSURL *)url;
- (NSURL *)base;
- setBaseTarget: (NSURL *)url;
- (NSURL *)baseTarget;


- (NSString *)encoding;


// Style sheet
- (void)setUserStyleSheet: (NSURL *)url;
- (void)setUserStyleSheet: (NSString *)sheet;


// Searching, to support find in clients.  regular expressions?
- (WKSearchState *)beginSearch;
- (NSString *)searchFor: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)case state: (WKSearchState *)state;


// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that WebCore also has dependencies on the appkit.
- (NSImage *)icon;


// Is page secure, e.g. https, ftps
- (BOOL)isPageSecure;


// Returns nil or the page title.
- (NSString *)pageTitle;

@end



/*
   ============================================================================= 

    This class provides a cover for URL-based preference items. 
*/
@interface WKPreferences
+ getPreferencesForURL: (NSURL *)url;

// Encoding that will be used in none specified on page? or in header?
+ setEncoding: (NSString *)encoding;
+ (NSString *)encoding;

// Javascript preferences
- (void)setJScriptEnabled: (BOOL)flag;
- (BOOL)jScriptEnabled;

// Java preferences
- (void)setJavaEnabled: (BOOL)flag
- (BOOL)javaEnabled;

// Document refreshes allowed
- setRefreshEnabled: (BOOL)flag;
- (BOOL)refreshEnabled;

// Plugins
- (void)setPluginsEnabled: (BOOL)flag;
- (BOOL)pluginEnabled;

// Should images be loaded.
- (void)setAutoloadImages: (BOOL)flag;
- (BOOL)autoloadImages;

/*
    Specify whether only local references ( stylesheets, images, scripts, subdocuments )
    should be loaded. ( default false - everything is loaded, if the more specific
    options allow )
    This is carried over from KDE.
*/
- (void)setOnlyLocalReferences: (BOOL)flag;
- (BOOL)onlyLocalReferences;

@end

#endif

