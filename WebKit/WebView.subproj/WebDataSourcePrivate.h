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
@class WebHistoryItem;

@protocol WebDocumentRepresentation;

@interface WebDataSourcePrivate : NSObject
{
@public
    NSData *resourceData;

    id <WebDocumentRepresentation> representation;
    
    WebController *controller;
    
    WebResourceRequest *originalRequest;
    WebResourceRequest *request;
    WebResourceResponse *response;

    // Client for main resource.
    WebMainResourceClient *mainClient;
    
    // Clients for other resources.
    NSMutableArray *subresourceClients;

    // The time when the data source was told to start loading.
    double loadingStartedTime;
    
    BOOL primaryLoadComplete;

    BOOL stopping;

    BOOL isClientRedirect;

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

    BOOL defersCallbacks;

    NSURL *iconURL;
    WebIconLoader *iconLoader;

    // item we are about to go to - used to set position in BFList on commit
    WebHistoryItem *provisionalBackForwardItem;
    // item we just left - will receive saved doc state on commit
    WebHistoryItem *previousBackForwardItem;
    // BF items that reference what we loaded - we must keep their titles up to date
    NSMutableArray *ourBackForwardItems;
}

@end

@interface WebDataSource (WebPrivate)

- (void)_setResourceData:(NSData *)data;
- (Class)_representationClass;
- (void)_setRepresentation:(id<WebDocumentRepresentation>)representation;
- (void)_setController:(WebController *)controller;
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
+ (NSMutableDictionary *)_repTypes;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
- (void)_loadIcon;
- (void)_setIconURL:(NSURL *)URL;
- (void)_setIconURL:(NSURL *)URL withType:(NSString *)iconType;
- (void)_setOverrideEncoding:(NSString *)overrideEncoding;
- (NSString *)_overrideEncoding;
- (void)_setProvisionalBackForwardItem: (WebHistoryItem *)item;
- (WebHistoryItem *)_provisionalBackForwardItem;
- (void)_setPreviousBackForwardItem: (WebHistoryItem *)item;
- (WebHistoryItem *)_previousBackForwardItem;
- (void)_addBackForwardItem:(WebHistoryItem *)item;
- (void)_addBackForwardItems:(NSArray *)items;
- (NSArray *)_backForwardItems;
- (void)_setIsClientRedirect:(BOOL)flag;
- (BOOL)_isClientRedirect;

// Convenience interface for getting here from an WebDataSource.
// This returns nil if the representation is not an WebHTMLRepresentation.
- (WebBridge *)_bridge;

- (BOOL)_isCommitted;
- (void)_commitIfReady;
- (void)_makeRepresentation;
- (void)_receivedData:(NSData *)data;
- (void)_finishedLoading;


- (void)_defersCallbacksChanged;
- (WebResourceRequest *)_originalRequest;

@end
