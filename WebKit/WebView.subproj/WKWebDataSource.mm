/*	
        WKWebDataSource.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/WKWebDataSource.h>
#import <WebKit/WKWebDataSourcePrivate.h>
#import <WebKit/WKException.h>
#import <WebKit/WebKitDebug.h>

#include <xml/dom_docimpl.h>

@implementation WKWebDataSource

+ (void)initialize {

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *fontSizeArray = [NSArray arrayWithObjects:@"7", @"8", @"9", @"10", @"12", @"13", @"14", @"16", nil];
    NSNumber *pluginsEnabled = [NSNumber numberWithBool:TRUE];
    
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
//        @"0xffffffff", 		@"WebKitLogLevel",
        @"0x0", 		@"WebKitLogLevel",
        @"Arial", 		@"WebKitStandardFont",
        @"Courier",  		@"WebKitFixedFont",
        @"Times-Roman", 	@"WebKitSerifFont",
        @"Arial", 		@"WebKitSansSerifFont", 
        @"Times-Roman", 	@"WebKitCursiveFont",
        @"Times-Roman", 	@"WebKitFantasyFont",
        @"6", 			@"WebKitMinimumFontSize",
        fontSizeArray,		@"WebKitFontSizes",
        pluginsEnabled,		@"WebKitPluginsEnabled",
        nil];

    [defaults registerDefaults:dict];
}


- (void)_commonInitialization
{
    ((WKWebDataSourcePrivate *)_dataSourcePrivate) = [[WKWebDataSourcePrivate alloc] init];
}

// Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
- initWithURL: (NSURL *)inputURL
{
    [super init];
    [self _commonInitialization];
    ((WKWebDataSourcePrivate *)_dataSourcePrivate)->inputURL = inputURL;
    return self;
}

- (void)dealloc
{
    [_dataSourcePrivate release];
}

#ifdef TENTATIVE_API
- initWithData: (NSData *)data 
- initWithString: (NSString *)string;
- initWithLoader: (WKLoader *)loader;
#endif


// Returns the name of the frame containing this data source, or nil
// if the data source is not in a frame set.
- (NSString *)frameName 
{
    return ((WKWebDataSourcePrivate *)_dataSourcePrivate)->frameName;    
}


// Returns YES if this is the main document.  The main document is the 'top'
// document, typically either a frameset or a normal HTML document.
- (BOOL)isMainDocument
{
    if (((WKWebDataSourcePrivate *)_dataSourcePrivate)->parent == nil)
        return YES;
    return NO;
}


// Returns nil if this data source represents the main document.  Otherwise
// returns the parent data source.
- (WKWebDataSource *)parent 
{
    return ((WKWebDataSourcePrivate *)_dataSourcePrivate)->parent;
}


// Returns an array of WKWebDataSource.  The data sources in the array are
// the data source assoicated with a frame set or iframe.  If the main document
// is not a frameset, or has not iframes children will return nil.
- (NSArray *)children
{
    return [((WKWebDataSourcePrivate *)_dataSourcePrivate)->frames allValues];
}

- (void)addFrame: (WKWebFrame *)frame
{
    WKWebDataSourcePrivate *data = (WKWebDataSourcePrivate *)_dataSourcePrivate;

    if (data->frames == nil)
        data->frames = [[NSMutableDictionary alloc] init];
    [data->frames setObject: frame forKey: [frame name]];    
}

 
- (WKWebFrame *)frameNamed: (NSString *)frameName
{
    WKWebDataSourcePrivate *data = (WKWebDataSourcePrivate *)_dataSourcePrivate;

    return (WKWebFrame *)[data->frames objectForKey: frameName];
}



// Returns an array of NSStrings or nil.  The NSStrings corresponds to
// frame names.  If this data source is the main document and has no
// frames then frameNames will return nil.
- (NSArray *)frameNames
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::frameNames is not implemented"];
    return nil;
}


// findDataSourceForFrameNamed: returns the child data source associated with
// the frame named 'name', or nil. 
- (WKWebDataSource *) findDataSourceForFrameNamed: (NSString *)name
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::findDataSourceForFrameNamed: is not implemented"];
    return nil;
}


- (BOOL)frameExists: (NSString *)name
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::frameExists: is not implemented"];
    return NO;
}


- (void)openURL: (NSURL *)url inFrameNamed: (NSString *)frameName
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::openURL:inFrameNamed: is not implemented"];
}


- (void)openURL: (NSURL *)url inIFrame: (id)iFrameIdentifier
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::openURL:inIFrame: is not implemented"];
}


- (id <WKWebController>)controller
{
    // All data source from a document (frameset) share the same
    // controller.
    if (((WKWebDataSourcePrivate *)_dataSourcePrivate)->parent != nil)
        return [((WKWebDataSourcePrivate *)_dataSourcePrivate)->parent controller];
    return ((WKWebDataSourcePrivate *)_dataSourcePrivate)->controller;
}


// May return nil if not initialized with a URL.
- (NSURL *)inputURL
{
    return ((WKWebDataSourcePrivate *)_dataSourcePrivate)->inputURL;
}


// finalURL returns the URL that was actually used.  The final URL
// may be different than the inputURL if the server redirects.
// <WKLocationChangedHandler> includes a message that is sent when
// a redirect is processed
- (NSURL *)finalURL
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::finalURL is not implemented"];
    return nil;
}


// Returns true if the inputURL has been redirected by the server,
// i.e. inputURL != finalURL.
- (BOOL)wasRedirected
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::wasRedirected is not implemented"];
    return NO;
}


// Start actually getting (if initialized with a URL) and parsing data. If the data source
// is still performing a previous load it will be stopped.
// If forceRefresh is YES the document will load from the net, not the cache.
- (void)startLoading: (BOOL)forceRefresh
{
    KURL url = [[[self inputURL] absoluteString] cString];
    
    WEBKITDEBUG1 ("url = %s\n", [[[self inputURL] absoluteString] cString]);
    [self _part]->openURL (url);
}


// Cancels any pending loads.  A data source is conceptually only ever loading
// one document at a time, although one document may have many related
// resources.  stopLoading will stop all loads related to the data source.
// Returns NO if the data source is not currently loading.
- (void)stopLoading
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::stopLoading is not implemented"];
}


// Returns YES if there are any pending loads.
- (BOOL)isLoading
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::isLoading is not implemented"];
    return NO;
}


#ifdef TENTATIVE_API
// Get DOM access to the document.
- (WKDOMDocument *)document;
#endif

// Get the actual source of the docment.
- (NSString *)documentText
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::documentText is not implemented"];
    return nil;
}


- (NSString *)documentTextFromDOM
{
    DOM::DocumentImpl *doc;
    NSString *string = nil;
    KHTMLPart *part = [self _part];
    
    if (part != 0){
        doc = (DOM::DocumentImpl *)[self _part]->xmlDocImpl();
        if (doc != 0){
            QString str = doc->recursive_toHTML(1);
            string = QSTRING_TO_NSSTRING(str);
        }
    }
    if (string == nil) {
        string = @"";
    }
    return string;
}


// URL reference point, these should probably not be public for 1.0.
- (NSURL *)base
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::base is not implemented"];
    return nil;
}


- (NSString *)baseTarget
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::baseTarget is not implemented"];
    return nil;
}


- (NSString *)encoding
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::encoding is not implemented"];
    return nil;
}


// Style sheet
- (void)setUserStyleSheetFromURL: (NSURL *)url
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::setUserStyleSheetFromURL: is not implemented"];
}


- (void)setUserStyleSheetFromString: (NSString *)sheet
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::setUserStyleSheetFromString: is not implemented"];
}


// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that WebCore also has dependencies on the appkit.
- (NSImage *)icon
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::setUserStyleSheetFromString: is not implemented"];
    return nil;
}


// Is page secure, e.g. https, ftps
- (BOOL)isPageSecure
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::isPageSecure is not implemented"];
    return NO;
}


// Returns nil or the page title.
- (NSString *)pageTitle
{
    [NSException raise:WKMethodNotYetImplemented format:@"WKWebDataSource::pageTitle is not implemented"];
    return nil;
}


@end
