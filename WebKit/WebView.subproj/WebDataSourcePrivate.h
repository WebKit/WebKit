/*	WebDataSourcePrivate.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes
        in WebCore.  Instances of this class are referenced by _private in
        WebDataSource.
*/

#import <WebKit/WebDataSource.h>
#import <WebKit/WebBridge.h>

@class WebContentPolicy;
@class WebIconLoader;
@class WebMainResourceClient;
@class WebResourceHandle;
@class WebResourceRequest;
@class WebResourceResponse;
@class WebSubresourceClient;

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
    WebResourceRequest *originalRequest;
    WebResourceRequest *request;
    WebResourceResponse *response;

    // The original URL we may have been redirected to.
    NSURL *finalURL;
    
    // Child frames of this frame.
    NSMutableDictionary *frames;

    // Client for main resource, and corresponding handle.
    WebMainResourceClient *mainClient;
    WebResourceHandle *mainHandle;
    
    // Clients for other resources.
    NSMutableArray *subresourceClients;

    // The time when the data source was told to start loading.
    double loadingStartedTime;
    
    BOOL primaryLoadComplete;
    
    BOOL stopping;

    NSString *pageTitle;
    
    NSString *encoding;
    NSString *overrideEncoding;

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
    
    BOOL defersCallbacks;
}

@end

@interface WebDataSource (WebPrivate)

- (void)_setResourceData:(NSData *)data;
- (Class)_representationClass;
- (void)_setRepresentation:(id<WebDocumentRepresentation>)representation;
- (void)_setController:(WebController *)controller;
- (void)_setParent:(WebDataSource *)p;
- (void)_startLoading;
- (void)_stopLoading;
- (BOOL)_isStopping;
- (void)_recursiveStopLoading;
- (void)_addSubresourceClient:(WebSubresourceClient *)client;
- (void)_removeSubresourceClient:(WebSubresourceClient *)client;
- (void)_setPrimaryLoadComplete:(BOOL)flag;
- (double)_loadingStartedTime;
- (void)_setTitle:(NSString *)title;
- (void)_setURL:(NSURL *)URL;
- (void)_setRequest:(WebResourceRequest *)request;
- (void)_setResponse:(WebResourceResponse *)response;
- (void)_setContentPolicy:(WebContentPolicy *)policy;
- (void)_layoutChildren;
- (void)_clearErrors;
- (void)_setMainDocumentError:(WebError *)error;
- (void)_addError:(WebError *)error forResource:(NSString *)resourceDescription;
+ (NSMutableDictionary *)_repTypes;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
- (void)_loadIcon;
- (void)_setIconURL:(NSURL *)URL;
- (void)_setIconURL:(NSURL *)URL withType:(NSString *)iconType;
- (WebResourceHandle *)_mainHandle;
- (void)_setOverrideEncoding:(NSString *)overrideEncoding;
- (NSString *)_overrideEncoding;

// Convenience interface for getting here from an WebDataSource.
// This returns nil if the representation is not an WebHTMLRepresentation.
- (WebBridge *)_bridge;

- (BOOL)_isCommitted;
- (void)_commitIfReady;
- (void)_makeRepresentation;
- (void)_receivedData:(NSData *)data;

- (void)_defersCallbacksChanged;

/*!
    @method addFrame:
    @discussion Add a child frame.  This should only be called by the data source's controller
    as a result of a createFrame:inParent:.
    // [Should this be private?]
*/
- (void)addFrame: (WebFrame *)frame;


@end
