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
#import <WebFoundation/WebAssertions.h>
#import <WebKit/WebKitStatisticsPrivate.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebResourceRequest.h>
#import <WebFoundation/WebNSDictionaryExtras.h>

@implementation WebDataSource

-(id)initWithURL:(NSURL *)URL
{
    id result = nil;

    WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
    if (request) {
        result = [self initWithRequest:request];
        [request release];
    }
    else {
        [self release];
    }
    
    return result;
}

-(id)initWithRequest:(WebResourceRequest *)request
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _private = [[WebDataSourcePrivate alloc] init];
    _private->request = [request retain];
    _private->inputURL = [[request canonicalURL] retain];

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
        return [_private->mainClient resourceData];
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

    // Check to make sure a duplicate frame name didn't creep in.
    ASSERT([_private->frames objectForKey:[frame name]] == nil);

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


- (void)openURL: (NSURL *)URL inFrameNamed: (NSString *)frameName
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::openURL:inFrameNamed: is not implemented"];
}


- (WebController *)controller
{
    // All data sources used in a document share the same controller.
    // A single document may have many data sources corresponding to
    // frames or iframes.
    ASSERT(_private->parent == nil || [_private->parent controller] == _private->controller);
    return _private->controller;
}

-(WebResourceRequest *)request
{
    return _private->request;
}

// May return nil if not initialized with a URL.
- (NSURL *)URL
{
    return _private->finalURL ? _private->finalURL : _private->inputURL;
}


// May return nil if not initialized with a URL.
- (NSURL *)originalURL
{
    return _private->inputURL;
}


- (void)startLoading
{
    [self _startLoading];
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
    // FIXME: This comment says that the state check is just an optimization, but that's
    // not true. There's a window where the state is complete, but primaryLoadComplete
    // is still NO and loading is still YES, because _setPrimaryLoadComplete has not yet
    // been called. We should fix that and simplify this code here.
    
    // As an optimization, check to see if the frame is in the complete state.
    // If it is, we aren't loading, so we don't have to check all the child frames.    
    if ([[self webFrame] _state] == WebFrameStateComplete) {
        return NO;
    }
    
    if (!_private->primaryLoadComplete && _private->loading) {
        return YES;
    }
    if ([_private->subresourceClients count]) {
	return YES;
    }
    
    // Put in the auto-release pool because it's common to call this from a run loop source,
    // and then the entire list of frames lasts until the next autorelease.
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    NSEnumerator *e = [[self children] objectEnumerator];
    WebFrame *childFrame;
    while ((childFrame = [e nextObject])) {
        if ([[childFrame dataSource] isLoading] || [[childFrame provisionalDataSource] isLoading]) {
            break;
        }
    }
    [pool release];
    
    return childFrame != nil;
}


- (BOOL)isDocumentHTML
{
    return [[self representation] isKindOfClass: [WebHTMLRepresentation class]];
}

// Get the actual source of the docment.
// FIXME: Move to WebHTMLRepresentation
- (NSString *)documentSource
{
    // FIMXE: Converting to string with ASCII encoding is not appropriate, although it works for some pages.
    if ([self isDocumentHTML]) {
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
- (void)setUserStyleSheetFromURL:(NSURL *)URL
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::setUserStyleSheetFromURL: is not implemented"];
}

- (void)setUserStyleSheetFromString:(NSString *)sheet
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::setUserStyleSheetFromString: is not implemented"];
}

// a.k.a shortcut icons, http://msdn.microsoft.com/workshop/Author/dhtml/howto/ShortcutIcon.asp.
// This method may be moved to a category to prevent unnecessary linkage to the AppKit.  Note, however
// that WebCore also has dependencies on the appkit.
- (NSImage *)icon
{
    [NSException raise:WebMethodNotYetImplemented format:@"WebDataSource::icon is not implemented"];
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
