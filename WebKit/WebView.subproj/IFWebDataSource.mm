/*	
        IFWebDataSource.mm
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFException.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebFrame.h>
#import <WebFoundation/WebFoundation.h>

#import <xml/dom_docimpl.h>
#import <khtml_part.h>

#import <WCWebDataSource.h>

@implementation IFWebDataSource

static id IFWebDataSourceMake(void *handle) 
{
    return [[[IFWebDataSource alloc] initWithHandle: (IFURLHandle *)handle] autorelease];
}

+ (void)load
{
    WCSetIFWebDataSourceMakeFunc(IFWebDataSourceMake);
}

- (void)_commonInitialization
{
    _private = [[IFWebDataSourcePrivate alloc] init];
}

// Returns nil if object cannot be initialized due to a malformed URL (RFC 1808).
- initWithURL: (NSURL *)inputURL 
{
    IFURLHandle *handle;
    
    handle = [[[IFURLHandle alloc] initWithURL: inputURL attributes: nil flags: 0] autorelease];
    return [self initWithHandle: handle];
}

- initWithHandle: (IFURLHandle *)handle
{
    [super init];
    [self _commonInitialization];
    _private->mainHandle = [handle retain];
    _private->inputURL = [[handle url] retain];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

#ifdef TENTATIVE_API
- initWithData: (NSData *)data 
- initWithString: (NSString *)string;
- initWithLoader: (IFLoader *)loader;
#endif

- (IFWebFrame *)frame
{
    return [_private->controller frameForDataSource: self];
}

// Returns the name of the frame containing this data source, or nil
// if the data source is not in a frame set.
- (NSString *)frameName 
{
    return [[self frame] name];    
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
- (IFWebDataSource *)parent 
{
    return _private->parent;
}


// Returns an array of IFWebFrame.  The frames in the array are
// associated with a frame set or iframe.
- (NSArray *)children
{
    return [_private->frames allValues];
}

- (void)addFrame: (IFWebFrame *)frame
{
    if (_private->frames == nil)
        _private->frames = [[NSMutableDictionary alloc] init];
    [[frame dataSource] _setParent: self];   
    [_private->frames setObject: frame forKey: [frame name]];    
}

 
- (IFWebFrame *)frameNamed: (NSString *)frameName
{
    return (IFWebFrame *)[_private->frames objectForKey: frameName];
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
- (IFWebDataSource *) findDataSourceForFrameNamed: (NSString *)name
{
    return [[self frameNamed: name] dataSource];
}


- (BOOL)frameExists: (NSString *)name
{
    return [self frameNamed: name] == 0 ? NO : YES;
}


- (void)openURL: (NSURL *)url inFrameNamed: (NSString *)frameName
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::openURL:inFrameNamed: is not implemented"];
}


- (id <IFWebController>)controller
{
    // All data sources used in a document share the same
    // controller.  A single document may have many datasource corresponding to
    // frame or iframes.
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
// <IFLocationChangedHandler> includes a message that is sent when
// a redirect is processed
- (NSURL *)redirectedURL
{
    return _private->finalURL;
}


// Returns true if the inputURL has been redirected by the server,
// i.e. inputURL != redirectedURL.
- (BOOL)wasRedirected
{
    return [_private->inputURL isEqual: [self redirectedURL]];
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
    [self _recursiveStopLoading];
}


// Returns YES if there are any pending loads.
- (BOOL)isLoading
{
    int i, count;
    
    WEBKITDEBUGLEVEL (WEBKIT_LOG_LOADING, "frame %s: primaryLoadComplete %d, [data->urlHandles count] = %d, URL = %s\n", [[[self frame] name] cString], (int)_private->primaryLoadComplete, [_private->urlHandles count], [[[self inputURL] absoluteString] cString]);
    if (_private->primaryLoadComplete == NO)
        return YES;
        
    if ([_private->urlHandles count])
        return YES;
    
    count = [[self children] count];
    for (i = 0; i < count; i++){
        IFWebFrame *childFrame;
        
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
- (IFDOMDocument *)document;
#endif

// Get the actual source of the docment.
- (NSString *)documentText
{
    KHTMLPart *part = [self _part];
    
    return QSTRING_TO_NSSTRING(part->documentSource());
}


- (NSString *)documentTextFromDOM
{
    DOM::DocumentImpl *doc;
    NSString *string = nil;
    KHTMLPart *part = [self _part];
    
    if (part != 0){
        doc = [self _part]->xmlDocImpl();
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
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::base is not implemented"];
    return nil;
}


- (NSString *)baseTarget
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::baseTarget is not implemented"];
    return nil;
}


- (NSString *)encoding
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::encoding is not implemented"];
    return nil;
}

// Style sheet
- (void)setUserStyleSheetFromURL: (NSURL *)url
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::setUserStyleSheetFromURL: is not implemented"];
}

- (void)setUserStyleSheetFromString: (NSString *)sheet
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::setUserStyleSheetFromString: is not implemented"];
}

// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that WebCore also has dependencies on the appkit.
- (NSImage *)icon
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::setUserStyleSheetFromString: is not implemented"];
    return nil;
}

// Is page secure, e.g. https, ftps
- (BOOL)isPageSecure
{
    [NSException raise:IFMethodNotYetImplemented format:@"IFWebDataSource::isPageSecure is not implemented"];
    return NO;
}

// Returns nil or the page title.
- (NSString *)pageTitle
{
    return _private->pageTitle;
}

@end
