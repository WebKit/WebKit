/*	WebDataSourcePrivate.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes
        in WebCore.  Instances of this class are referenced by _private in
        WebDataSource.
*/

#import <WebKit/WebDataSource.h>

@class NSError;
@class NSURLRequest;
@class NSURLResponse;
@class WebArchive;
@class WebBaseResourceHandleDelegate;
@class WebBridge;
@class WebHistoryItem;
@class WebIconLoader;
@class WebMainResourceClient;
@class WebResource;
@class WebView;

@protocol WebDocumentRepresentation;

@interface WebDataSourcePrivate : NSObject
{
@public
    NSMutableData *resourceData;

    id <WebDocumentRepresentation> representation;
    
    WebView *webView;
    
    // A reference to actual request used to create the data source.
    // This should only be used by the resourceLoadDelegate's
    // identifierForInitialRequest:fromDatasource: method.  It is
    // not guaranteed to remain unchanged, as requests are mutable.
    NSURLRequest *originalRequest;
    
    // A copy of the original request used to create the data source.
    // We have to copy the request because requests are mutable.
    NSURLRequest *originalRequestCopy;
    
    // The 'working' request for this datasource.  It may be mutated
    // several times from the original request to include additional
    // headers, cookie information, canonicalization and redirects.
    NSMutableURLRequest *request;
    
    NSURLResponse *response;

    // Client for main resource.
    WebMainResourceClient *mainClient;
    
    // Clients for other resources.
    NSMutableArray *subresourceClients;
    NSMutableArray *plugInStreamClients;

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
    NSError *mainDocumentError;

    BOOL loading; // self and webView are retained while loading

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
    NSURLRequest *lastCheckedRequest;

    // We retain all the received responses so we can play back the
    // WebResourceLoadDelegate messages if the item is loaded from the
    // page cache.
    NSMutableArray *responses;

    BOOL justOpenedForTargetedLink;

    BOOL storedInPageCache;
    BOOL loadingFromPageCache;

    WebFrame *webFrame;
    
    NSMutableDictionary *subresources;
    NSMutableDictionary *pendingSubframeArchives;
}

@end

@interface WebDataSource (WebPrivate)

// API Considerations:

/*!
    @method subresources
    @abstract Returns all the subresources associated with the data source.
    @description The returned array only contains subresources that have fully downloaded.
*/
- (NSArray *)subresources;

/*!
    method subresourceForURL:
    @abstract Returns a subresource for a given URL.
    @param URL The URL of the subresource.
    @description Non-nil is returned if the data source has fully downloaded a subresource with the given URL.
*/
- (WebResource *)subresourceForURL:(NSURL *)URL;

/*!
    @method addSubresource:
    @abstract Adds a subresource to the data source.
    @param subresource The subresource to be added.
    @description addSubresource: adds a subresource to the data source's list of subresources.
    Later, if something causes the data source to load the URL of the subresource, the data source
    will load the data from the subresource instead of from the network. For example, if one wants to add
    an image that is already downloaded to a web page, addSubresource: can be called so that the data source
    uses the downloaded image rather than accessing the network. NOTE: If the data source already has a
    subresource with the same URL, addSubresource: will replace it.
*/
- (void)addSubresource:(WebResource *)subresource;


// Other private methods
- (void)addSubresources:(NSArray *)subresources;
- (NSFileWrapper *)_fileWrapperForURL:(NSURL *)URL;

- (WebArchive *)_archive;
- (WebArchive *)_archiveWithMarkupString:(NSString *)markupString nodes:(NSArray *)nodes;
- (void)_addSubframeArchives:(NSArray *)subframeArchives;
- (WebArchive *)_popSubframeArchiveWithName:(NSString *)frameName;

- (void)_replaceSelectionWithMarkupString:(NSString *)markupString baseURL:(NSURL *)baseURL;
- (BOOL)_replaceSelectionWithArchive:(WebArchive *)archive;
- (void)_replaceSelectionWithImageResource:(WebResource *)resource;

- (NSError *)_mainDocumentError;
- (NSString *)_stringWithData:(NSData *)data;
- (void)_startLoading;
- (void)_stopLoading;
- (NSURL *)_URL;
- (NSURL *)_URLForHistory;
- (WebView *)_webView;
- (void)_setRepresentation:(id<WebDocumentRepresentation>)representation;
- (void)_setWebView:(WebView *)webView;
- (void)_startLoading:(NSDictionary *)pageCache;
- (void)_stopLoadingInternal;
- (BOOL)_isStopping;
- (void)_recursiveStopLoading;
- (void)_addSubresourceClient:(WebBaseResourceHandleDelegate *)client;
- (void)_removeSubresourceClient:(WebBaseResourceHandleDelegate *)client;
- (void)_addPlugInStreamClient:(WebBaseResourceHandleDelegate *)client;
- (void)_removePlugInStreamClient:(WebBaseResourceHandleDelegate *)client;
- (void)_setPrimaryLoadComplete:(BOOL)flag;
- (double)_loadingStartedTime;
- (void)_setTitle:(NSString *)title;
- (void)_setURL:(NSURL *)URL;
- (void)__adoptRequest:(NSMutableURLRequest *)request;
- (void)_setRequest:(NSURLRequest *)request;
- (void)_setResponse:(NSURLResponse *)response;
- (void)_clearErrors;
- (void)_setMainDocumentError:(NSError *)error;
+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission;
+ (Class)_representationClassForMIMEType:(NSString *)MIMEType;
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
- (void)_commitIfReady:(NSDictionary *)pageCache;
- (void)_makeRepresentation;
- (void)_receivedData:(NSData *)data;
- (void)_setData:(NSData *)data;
- (void)_finishedLoading;
- (void)_receivedError:(NSError *)error complete:(BOOL)isComplete;
- (void)_defersCallbacksChanged;
- (NSURLRequest *)_originalRequest;
- (NSDictionary *)_triggeringAction;
- (void)_setTriggeringAction:(NSDictionary *)action;
- (NSURLRequest *)_lastCheckedRequest;
- (void)_setLastCheckedRequest:(NSURLRequest *)request;
- (void)_setJustOpenedForTargetedLink:(BOOL)justOpened;
- (BOOL)_justOpenedForTargetedLink;
- (void)_setStoredInPageCache:(BOOL)f;
- (BOOL)_storedInPageCache;
- (BOOL)_loadingFromPageCache;

- (void)_addResponse:(NSURLResponse *)r;
- (NSArray *)_responses;

- (void)_stopLoadingWithError:(NSError *)error;

- (void)_setWebFrame:(WebFrame *)frame;

- (BOOL)_isDocumentHTML;
- (NSString *)_title;

@end
