/*	
        WKWebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/WKWebController.h>

/*
    A WKWebDataSource represents all the state associated
    with a web page.  It is typicallly initialized with a URL, but
    may also be initialized with an NSString or NSData that hold
    HTML (?also other, i.e. image data?) content.
    
    Typical usage of a WKWebDataSource.
    
    WKWebDataSource *dataSource = [[WKWebDataSource alloc] initWithURL: url];
    
    [dataSource setFrameSetHandler: (WKFrameSetHandler *)myManager];
    [dataSource setScriptContextHandler: (WKScriptContextHandler *)myContext];
    [dataSource setLoadHandler: (WKLoadHandler *)loadHandler];
    [dataSource setCredentialsHandler: (WKCredentialsHandler *)credentialsHandler];
    ...
    or
    ...
    [dataSource setController: (WKWebController *)myController];
        
    Questions:  
        Multiple protocols or controller?
        Should we use CF XML data types, or our own?
*/

#ifdef READY_FOR_PRIMETIME

@interface WKWebDataSource : NSObject
{
@private
    id _dataSourcePrivate;
}


// Can these init methods return nil? i.e. if URL is invalid, or should they
// throw expections?
- initWithURL: (NSURL *)url;
- initWithData: (NSData *)data;
- initWithString: (NSString *)string;

// ?? How do you create subclasses of WKURILoader, how is this
// expected to be used?
- initWithLoader: (WKURILoader *)loader;

// ?? We don't have a NS streams API!!
- initWithStream: (NSStream *)stream

// May return nil if not initialized with a URL.
- (NSURL *)url;

// Start actually getting (if initialized with a URL) and parsing data.
- (void)startLoading;

// Cancel and pending loads.
- (BOOL)stopLoading;

// Get DOM access to the document.
- (WKDOMDocument *)document;

// Get the actual source of the docment.
- (NSString *)documentText;

// Get a 'pretty' version of the document, created by traversal of the DOM.
- (NSString *)formattedDocumentText;

// Get the currently focused node.  Is this appropriate for the model, or
// should this be handled by the view?
- (WKDOMNode *)activeNode;

// URL reference point, these should problably not be public for 1.0.
- setBase: (NSURL *)url;
- (NSURL *)base;
- setBaseTarget: (NSURL *)url;
- (NSURL *)baseTarget;

- (NSString *)encoding;

// Style sheet
- (void)setUserStyleSheet: (NSURL *)url;
- (void)setUserStyleSheet: (NSString *)sheet;

// Searching, to support find in clients.  regular expressions?
- (WKSearchState *)findTextBegin;
- (NSString *)findTextNext: (WKRegularExpression *)exp direction: (BOOL)forward state: (WKSearchState *)state;
- (NSString *)findTextNext: (NSString *)string direction: (BOOL)forward caseSensitive: (BOOL)case state: (WKSearchState *)state;

// Selection
- (NSString *)selectedText;
- (WKDOMRange *)selectedRange;
- (void)setSelection: (WKDOMRange *range);
- (BOOL)hasSelection;
- (void)selectAll;

#ifdef HAVE_WKCONTROLLER
- (void)setController: (WKWebController *)controller;
#else
- (void)setLoadHandler: (WKLoadHandler *)fmanager;
- (void)setFrameSetHandler: (WKFrameSetHandler *)fmanager;
- (void)setScriptContextHandler: (WKScriptContextHandler *)context;
#endif

- executeScript: (NSString *)string;
// Same as above expect uses the node as 'this' value
- executeScript: (NSString *)string withNode: (WKDOMNode *)node;

// This API reflects the KDE API, but is it sufficient?
- (BOOL)scheduleScript: (NSString *)string withNode: (WKDOMNode *)node 
- executeScheduledScript;


// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This may be removed to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that this means WebCore, specifically KWQ, also doesn't have dependencies on the AppKit.
- (NSImage *)icon;

// Is page secure, i.e. https, ftps
// Should this perhap be on the URL?
// This would the be implemented like this
// return [[self url] isSecure];
- (BOOL)isPageSecure;

// ---------------------- Convience methods ----------------------
- (NSString *)pageTitle;
// ---------------------------------------------------------------


/*
    Notifications?
        In general, how often should we notify?  We should use notifications
        if we anticipate multiple clients, no return types, and generally
        asynchronous or indirect behaviour.
        
        notifications:
            WKSelectionChangedNotification
            WKNodeActivatedNotification
            WKDocumentChangedNotification
    
    Error handling:
        exceptions?
        nil returns?
        errors by notification?
        
        error conditions:
            timeout
            unrecognized/handled mime-type
            javascript errors
            invalid url
            parsing errors
            
*/
@end



/*
    The class provide a cover for URL based preference items. 
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

