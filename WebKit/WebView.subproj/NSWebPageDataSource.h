/*	NSWebPageDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

/*
    A NSWebPageDataSource represents all the state associated
    with a web page.  It is typicallly instantiated withe a URL, but
    may also be initialized with an NSString or NSData that hold
    HTML content.
    
    Typical usage of a WKWebDataSource.
    
    WKWebPageDataSource *dataSource = [[WKWebPageDataSource alloc] initWithURL: url];
    [dataSource setFrameSetManager: (WKFrameSetManagement *)myManager];
    [dataSource setScriptExecutionContext: (WKScriptContext *)myContext];
    
    Although, usually a WKWebView is the owner and user of data source, and it
    becomes the WKScriptContext and WKFrameSetManagement for a data source.
*/

#ifdef READY_FOR_PRIMETIME

@interface NSWebPageDataSource : NSObject
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

- (void)setFrameSetManager: (WKFrameSetManagement *)fmanager;

- (void)setScriptExecutionContext: (WKScriptContext *)context;

- executeScript: (NSString *)string;
// Same as above expect uses the node as 'this' value
- executeScript: (NSString *)string withNode: (WKDOMNode *)node;

// This API reflects the KDE API, but is it sufficient?
- (BOOL)scheduleScript: (NSString *)string withNode: (WKDOMNode *)node 
- executeScheduledScript;


// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
- (NSImage *)pageImage;


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
    Implementors of this protocol will received messages indicating
    data streaming.  These methods will be called even if the data source
    is initialized with something other than a URL.
*/
@protocol  WKWebDataSourceClient

// A new chunk of data has been received.  This could be partial load
// of a url.  It may be useful to do incremental layout, although
// typically for non-base URLs this should be done after a URL (i.e. image)
// has been completed downloaded.
// ? Should we also provide data?  I think perhaps not, as all access to the
// document should be via the DOM.
- (void)receivedDataForURL: (NSURL *)url;

// A URL has completed loading.  This could be used to perform layout.
- (void)finishedReceivingDataForURL: (NSURL *)url;

//  Called when all the resources associated with a document have been loaded.
- documentReceived;

@end


/*
    A class that implements WKScriptContext provides all the state information
    that may be used by Javascript (AppleScript?).  This includes quirky methods nad
    
*/
@protocol WKScriptContext

- (void)setStatusText: (NSString *)text;
- (NSString *)statusText;

// Need API for things like window size and position, window ids,
// screen goemetry.  Essentially all the 'view' items that are
// accessible from Javascript.

@end


/*
*/
@protocol WKFrameSetManagement
- (NSArray *)frameNames;
- (id)findFrameNamed: (NSString *)name;
- (BOOL)frameExists: (NSString *)name;
- (BOOL)openURLInFrame: (id)aFrame url: (NSURL *)url;
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

// Should only allow local references (i.e. help view?)
- (void)setOnlyLocalReferences: (BOOL)flag;
- (BOOL)onlyLocalReferences;

@end

#else
@interface NSWebPageDataSource : NSObject
{
@private
    id _dataSourcePrivate;
}

@end
#endif

