/*	
        WebDataSource.mm
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDocument.h>
#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebException.h>
#import <WebKit/WebHTMLRepresentation.h>
#import <WebKit/WebMainResourceClient.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/WebKitStatisticsPrivate.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebNSDictionaryExtras.h>

@implementation WebDataSource

-(id)initWithURL:(NSURL *)theURL
{
    return [self initWithURL:theURL attributes:nil flags:0];
}

-(id)initWithURL:(NSURL *)theURL attributes:(NSDictionary *)theAttributes
{
    return [self initWithURL:theURL attributes:theAttributes flags:0];
}

-(id)initWithURL:(NSURL *)theURL attributes:(NSDictionary *)theAttributes flags:(unsigned)theFlags;
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _private = [[WebDataSourcePrivate alloc] init];
    _private->inputURL = [theURL retain];
    _private->mainHandle = [[WebResourceHandle alloc] initWithURL: _private->inputURL attributes:theAttributes flags:theFlags];
    
    ++WebDataSourceCount;
    
    return self;
}

- (void)dealloc
{
    --WebDataSourceCount;
    
    [_private release];
    
    [super dealloc];
}

- (NSData *)data
{
    if(!_private->resourceData){
        return [_private->mainHandle resourceData];
    }else{
        return _private->resourceData;
    }
}

- (id <WebDocumentRepresentation>) representation
{
    return _private->representation;
}

- (WebFrame *)webFrame
{
    return [_private->controller frameForDataSource: self];
}

// Returns the name of the frame containing this data source, or nil
// if the data source is not in a frame set.
- (NSString *)frameName 
{
    return [[self webFrame] name];    
}

// Returns YES if this is the main document.  The main document is the 'top'
// document, typically either a frameset or a normal HTML document.
- (BOOL)isMainDocument
{
    if (_private->parent == nil)
        return YES;
    return NO;
}

// Returns nil if this data source represents the main document.  Otherwise
// returns the parent data source.
- (WebDataSource *)parent 
{
    return _private->parent;
}


// Returns an array of WebFrame.  The frames in the array are
// associated with a frame set or iframe.
- (NSArray *)children
{
    return [_private->frames allValues];
}

- (void)addFrame: (WebFrame *)frame
{
    if (_private->frames == nil)
        _private->frames = [[NSMutableDictionary alloc] init];
    [[frame dataSource] _setParent: self];   
    [_private->frames setObject: frame forKey: [frame name]];    
}

 
- (WebFrame *)frameNamed: (NSString *)frameName
{
    return (WebFrame *)[_private->frames objectForKey: frameName];
}



// Returns an array of NSStrings or nil.  The NSStrings corresponds to
// frame names.  If this data source is the main document and has no
// frames then frameNames will return nil.
- (NSArray *)frameNames
{
    return [_private->frames allKeys];
}


// findDataSourceForFrameNamed: returns the child data source associated with
// the frame named 'name', or nil. 
- (WebDataSource *) findDataSourceForFrameNamed: (NSString *)name
{
    return [[self frameNamed: name] dataSource];
}


- (BOOL)frameExists: (NSString *)name
{
    return [self frameNamed: name] == 0 ? NO : YES;
}


- (void)openURL: (NSURL *)url inFrameNamed: (NSString *)frameName
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::openURL:inFrameNamed: is not implemented"];
}


- (WebController *)controller
{
    // All data sources used in a document share the same controller.
    // A single document may have many data sources corresponding to
    // frames or iframes.
    if (_private->parent != nil)
        return [_private->parent controller];
    return _private->controller;
}


// May return nil if not initialized with a URL.
- (NSURL *)inputURL
{
    return _private->inputURL;
}


// finalURL returns the URL that was actually used.  The final URL
// may be different than the inputURL if the server redirects.
// <WebLocationChangedHandler> includes a message that is sent when
// a redirect is processed
- (NSURL *)redirectedURL
{
    return _private->finalURL;
}


// Returns true if the inputURL has been redirected by the server,
// i.e. inputURL != redirectedURL.
- (BOOL)wasRedirected
{
    return [self redirectedURL] != nil && ![_private->inputURL isEqual: [self redirectedURL]];
}


// Start actually getting (if initialized with a URL) and parsing data. If the data source
// is still performing a previous load it will be stopped.
// If forceRefresh is YES the document will load from the net, not the cache.
- (void)startLoading: (BOOL)forceRefresh
{
    [self _startLoading: forceRefresh];
}


// Cancels any pending loads.  A data source is conceptually only ever loading
// one document at a time, although one document may have many related
// resources.  stopLoading will stop all loads related to the data source.  This
// method will also stop loads that may be loading in child frames.
- (void)stopLoading
{
    // stop download here because we can't rely on WebResourceHandleDidCancelLoading
    // as it isn't sent when the app quits
    [[_private->mainResourceHandleClient downloadHandler] cancel];
    [self _recursiveStopLoading];
}


// Returns YES if there are any pending loads.
- (BOOL)isLoading
{
    int i, count;
    
    // First check to see if the datasource's frame is in the complete state
    if ([[self webFrame] _state] == WebFrameStateComplete)
        return NO;
        
    //WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "frame %s: primaryLoadComplete %d, [data->urlHandles count] = %d, URL = %s\n", [[[self webFrame] name] cString], (int)_private->primaryLoadComplete, [_private->urlHandles count], [[[self inputURL] absoluteString] cString]);
    if (_private->primaryLoadComplete == NO && _private->loading)
        return YES;
        
    if ([_private->urlHandles count])
        return YES;
    
    count = [[self children] count];
    for (i = 0; i < count; i++){
        WebFrame *childFrame;
        
        childFrame = [[self children] objectAtIndex: i];
        if ([[childFrame dataSource] isLoading])
            return YES;
        if ([[childFrame provisionalDataSource] isLoading])
            return YES;
    }
    return NO;
}


#ifdef TENTATIVE_API
// Get DOM access to the document.
- (WebDOMDocument *)document;
#endif

- (BOOL)isDocumentHTML
{
    return [[self representation] isKindOfClass: [WebHTMLRepresentation class]];
}

// Get the actual source of the docment.
// FIXME: Move to WebHTMLRepresentation
- (NSString *)documentSource
{
    // FIMXE: other encodings
    if([self isDocumentHTML]){
        return [[[NSString alloc] initWithData:[self data] encoding:NSASCIIStringEncoding] autorelease];
    }
    return nil;
}

// URL reference point, these should probably not be public for 1.0.
- (NSURL *)base
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::base is not implemented"];
    return nil;
}


- (NSString *)baseTarget
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::baseTarget is not implemented"];
    return nil;
}


- (NSString *)encoding
{
    return _private->encoding;
}

// Style sheet
- (void)setUserStyleSheetFromURL: (NSURL *)url
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::setUserStyleSheetFromURL: is not implemented"];
}

- (void)setUserStyleSheetFromString: (NSString *)sheet
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::setUserStyleSheetFromString: is not implemented"];
}

// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that WebCore also has dependencies on the appkit.
- (NSImage *)icon
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::setUserStyleSheetFromString: is not implemented"];
    return nil;
}

// Is page secure, e.g. https, ftps
- (BOOL)isPageSecure
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::isPageSecure is not implemented"];
    return NO;
}

// Returns nil or the page title.
- (NSString *)pageTitle
{
    return _private->pageTitle;
}

- (WebContentPolicy *) contentPolicy
{
    return _private->contentPolicy;
}

- (NSString *)contentType
{
    return _private->contentType;
}

- (NSString *)fileType
{
    return [[WebFileTypeMappings sharedMappings] preferredExtensionForMIMEType:[self contentType]];
}

- (NSDictionary *)errors
{
    return _private->errors;
}

- (WebError *)mainDocumentError
{
    return _private->mainDocumentError;
}

+ (void)registerRepresentationClass:(Class)repClass forMIMEType:(NSString *)MIMEType
{
    // FIXME: OK to allow developers to override built-in reps?
    [[self _repTypes] setObject:repClass forKey:MIMEType];
}

@end
