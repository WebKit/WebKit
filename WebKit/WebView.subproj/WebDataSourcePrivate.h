/*	WebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in
        NSWebPageDataSource.
*/

#import <WebKit/WebDataSource.h>
#import <WebKit/WebLocationChangeHandler.h>
#import <WebKit/WebBridge.h>

@class WebIconLoader;
@class WebResourceHandle;
@class WebMainResourceClient;
@protocol WebLocationChangeHandler;
@protocol WebDocumentRepresentation;

@interface WebDataSourcePrivate : NSObject
{
@public
    NSData *resourceData;

    id <WebDocumentRepresentation> representation;
    
    WebDataSource *parent;
    
    WebController *controller;
    
    // The original URL as requested during initialization.
    NSURL *inputURL;
    
    // The original URL we may have been redirected to.
    NSURL *finalURL;
    
    // Child frames of this frame.
    NSMutableDictionary *frames;

    // The main handle.
    WebResourceHandle *mainHandle;

    // The handle client for the main document associated with the
    // datasource.
    WebMainResourceClient *mainResourceHandleClient;
    
    // Active WebResourceHandles for resources associated with the
    // datasource.
    NSMutableArray *urlHandles;

    // The time when the data source was told to start loading.
    double loadingStartedTime;
    
    BOOL primaryLoadComplete;
    
    BOOL stopping;

    NSString *pageTitle;
    
    NSString *encoding;

    NSString *contentType;

    // Errors associated with resources.
    NSMutableDictionary *errors;

    // Error associated with main document.
    WebError *mainDocumentError;

    WebContentPolicy *contentPolicy;

    BOOL loading; // self and controller are retained while loading

    BOOL gotFirstByte; // got first byte
    BOOL committed; // This data source has been committed
    
    NSURL *iconURL;
    WebIconLoader *iconLoader;
    
    
    // Horrible hack to workaround another horrible hack.
    // A 'fake' data source is created for every frame to guarantee
    // that it has a part.  This flag is set if the data source
    // in a 'fake' data source.
    BOOL _isDummy;
}

@end

@interface WebDataSource (WebPrivate)
- (void)_setResourceData:(NSData *)data;
- (Class)_representationClass;
- (void)_setRepresentation: (id<WebDocumentRepresentation>)representation;
- (void)_setController: (WebController *)controller;
- (void)_setParent: (WebDataSource *)p;
- (void)_startLoading: (BOOL)forceRefresh;
- (void)_stopLoading;
- (BOOL)_isStopping;
- (void)_recursiveStopLoading;
- (void)_addResourceHandle: (WebResourceHandle *)handle;
- (void)_removeResourceHandle: (WebResourceHandle *)handle;
- (void)_setPrimaryLoadComplete: (BOOL)flag;
- (double)_loadingStartedTime;
- (void)_setTitle: (NSString *)title;
- (void)_setFinalURL: (NSURL *)url;
- (id <WebLocationChangeHandler>)_locationChangeHandler;
- (void)_setContentPolicy:(WebContentPolicy *)policy;
- (void)_setContentType:(NSString *)type;
- (void)_setEncoding:(NSString *)encoding;
- (void)_layoutChildren;
- (void)_clearErrors;
- (void)_setMainDocumentError: (WebError *)error;
- (void)_addError: (WebError *)error forResource: (NSString *)resourceDescription;
+ (NSMutableDictionary *)_repTypes;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
- (void)_removeFromFrame;
- (void)_loadIcon;
- (void)_setIconURL:(NSURL *)url;
- (void)_setIconURL:(NSURL *)url withType:(NSString *)iconType;
- (BOOL)_isDummy;
- (void)_setIsDummy: (BOOL)f;
- (WebResourceHandle*)_mainHandle;

// Convenience interface for getting here from an WebDataSource.
// This returns nil if the representation is not an WebHTMLRepresentation.
- (WebBridge *)_bridge;

- (BOOL)_isCommitted;
- (void)_commitIfReady;
- (void)_makeRepresentation;
- (void)_receivedData:(NSData *)data;

@end
