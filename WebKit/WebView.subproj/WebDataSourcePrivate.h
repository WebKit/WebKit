/*	WebDataSourcePrivate.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes
        in WebCore.  Instances of this class are referenced by _private in
        WebDataSource.
*/

#import <WebKit/WebDataSource.h>

@class WebBridge;
@class WebHistoryItem;
@class WebIconLoader;
@class WebMainResourceClient;
@class WebRequest;
@class WebResponse;
@class WebSubresourceClient;

@protocol WebDocumentRepresentation;

@interface WebDataSourcePrivate : NSObject
{
@public
    NSData *resourceData;

    id <WebDocumentRepresentation> representation;
    
    WebController *controller;
    
    // A reference to actual request used to create the data source.
    // This should only be used by the resourceLoadDelegate's
    // identifierForInitialRequest:fromDatasource: method.  It is
    // not guaranteed to remain unchanged, as requests are mutable.
    WebRequest *originalRequest;
    
    // A copy of the original request used to create the data source.
    // We have to copy the request because requests are mutable.
    WebRequest *originalRequestCopy;
    
    // The 'working' request for this datasource.  It may be mutated
    // several times from the original request to include additional
    // headers, cookie information, canonicalization and redirects.
    WebRequest *request;
    
    WebResponse *response;

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

    BOOL loading; // self and controller are retained while loading

    BOOL gotFirstByte; // got first byte
    BOOL committed; // This data source has been committed

    BOOL defersCallbacks;

    NSURL *iconURL;
    WebIconLoader *iconLoader;

    // BF items that reference what we loaded - we must keep their titles up to date
    NSMutableArray *ourBackForwardItems;

    // The action that triggered loading of this data source -
    // we keep this around for the benefit of the various policy
    // handlers.
    NSDictionary *triggeringAction;

    // The last request that we checked click policy for - kept around
    // so we can avoid asking again needlessly.
    WebRequest *lastCheckedRequest;

    // We retain all the received responses so we can play back the
    // WebResourceLoadDelegate messages if the item is loaded from the
    // page cache.
    NSMutableArray *responses;
    
    BOOL isDownloading;
    NSString *downloadPath;
    NSString *downloadDirectory;

    BOOL justOpenedForTargetedLink;

    BOOL storedInPageCache;
    BOOL loadingFromPageCache;

    WebFrame *webFrame;
}

@end

@interface WebDataSource (WebPrivate)

- (NSString *)_stringWithData:(NSData *)data;
- (void)_startLoading;
- (void)_stopLoading;
- (NSURL *)_URL;
- (WebController *)_controller;
- (void)_setResourceData:(NSData *)data;
- (Class)_representationClass;
- (void)_setRepresentation:(id<WebDocumentRepresentation>)representation;
- (void)_setController:(WebController *)controller;
- (void)_startLoading: (NSDictionary *)pageCache;
- (void)_stopLoadingInternal;
- (BOOL)_isStopping;
- (void)_recursiveStopLoading;
- (void)_addSubresourceClient:(WebSubresourceClient *)client;
- (void)_removeSubresourceClient:(WebSubresourceClient *)client;
- (void)_setPrimaryLoadComplete:(BOOL)flag;
- (double)_loadingStartedTime;
- (void)_setTitle:(NSString *)title;
- (void)_setURL:(NSURL *)URL;
- (void)_setRequest:(WebRequest *)request;
- (void)_setResponse:(WebResponse *)response;
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
- (void)_addBackForwardItem:(WebHistoryItem *)item;
- (void)_addBackForwardItems:(NSArray *)items;
- (NSArray *)_backForwardItems;
- (void)_setIsClientRedirect:(BOOL)flag;
- (BOOL)_isClientRedirect;

// Convenience interface for getting here from an WebDataSource.
// This returns nil if the representation is not an WebHTMLRepresentation.
- (WebBridge *)_bridge;

- (BOOL)_isCommitted;
- (void)_commitIfReady: (NSDictionary *)pageCache;
- (void)_makeRepresentation;
- (void)_receivedData:(NSData *)data;
- (void)_finishedLoading;
- (void)_receivedError:(WebError *)error complete:(BOOL)isComplete;
- (void)_defersCallbacksChanged;
- (WebRequest *)_originalRequest;
- (NSDictionary *)_triggeringAction;
- (void)_setTriggeringAction:(NSDictionary *)action;
- (WebRequest *)_lastCheckedRequest;
- (void)_setLastCheckedRequest:(WebRequest *)request;
- (void)_setIsDownloading:(BOOL)isDownloading;
- (void)_setDownloadPath:(NSString *)downloadPath;
- (void)_setDownloadDirectory:(NSString *)downloadDirectory;
- (NSString *)_downloadDirectory;
- (void)_setJustOpenedForTargetedLink:(BOOL)justOpened;
- (BOOL)_justOpenedForTargetedLink;
- (void)_setStoredInPageCache:(BOOL)f;
- (BOOL)_storedInPageCache;
- (BOOL)_loadingFromPageCache;

- (void)_addResponse: (WebResponse *)r;
- (NSArray *)_responses;

- (void)_stopLoadingWithError:(WebError *)error;

- (void)_setWebFrame:(WebFrame *)frame;

@end
