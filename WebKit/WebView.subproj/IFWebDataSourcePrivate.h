/*	IFWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in
        NSWebPageDataSource.
*/

#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFLocationChangeHandler.h>


@class IFURLHandle;
@class IFMainURLHandleClient;
@protocol IFLocationChangeHandler;
@protocol IFDocumentRepresentation;

@interface IFWebDataSourcePrivate : NSObject
{
    NSData *resourceData;

    id representation;
    
    IFWebDataSource *parent;
    NSMutableArray *children;
    
    IFWebController *controller;
    
    // The original URL as requested during initialization.
    NSURL *inputURL;
    
    // The original URL we may have been redirected to.
    NSURL *finalURL;
    
    // Child frames of this frame.
    NSMutableDictionary *frames;

    // The main handle.
    IFURLHandle *mainHandle;

    // The handle client for the main document associated with the
    // datasource.
    IFMainURLHandleClient *mainURLHandleClient;
    
    // Active IFURLHandles for resources associated with the
    // datasource.
    NSMutableArray *urlHandles;

    // The time when the data source was told to start loading.
    double loadingStartedTime;
    
    BOOL primaryLoadComplete;
    
    BOOL stopping;
    
    NSString *pageTitle, *downloadPath, *encoding, *contentType;

    // Errors associated with resources.
    NSMutableDictionary *errors;

    // Error associated with main document.
    IFError *mainDocumentError;

    // The location change handler for this data source.
    id <IFLocationChangeHandler>locationChangeHandler;

    IFContentPolicy contentPolicy;

    BOOL loading; // self and controller are retained while loading
}

@end

@interface IFWebDataSource (IFPrivate)
- (void)_setResourceData:(NSData *)data;
- (void)_setRepresentation:(id <IFDocumentRepresentation>)representation;
- (void)_setController: (IFWebController *)controller;
- (void)_setParent: (IFWebDataSource *)p;
- (void)_startLoading: (BOOL)forceRefresh;

- (void)_stopLoading;
- (BOOL)_isStopping;
- (void)_recursiveStopLoading;
- (void)_addURLHandle: (IFURLHandle *)handle;
- (void)_removeURLHandle: (IFURLHandle *)handle;
- (void)_setPrimaryLoadComplete: (BOOL)flag;
- (double)_loadingStartedTime;
- (void)_setTitle: (NSString *)title;
- (void)_setFinalURL: (NSURL *)url;
- (id <IFLocationChangeHandler>)_locationChangeHandler;
- (void)_setLocationChangeHandler: (id <IFLocationChangeHandler>)l;
- (void)_setDownloadPath:(NSString *)path;
- (void)_setContentPolicy:(IFContentPolicy)policy;
- (void)_setContentType:(NSString *)type;
- (void)_setEncoding:(NSString *)encoding;
- (IFWebDataSource *) _recursiveDataSourceForLocationChangeHandler:(id <IFLocationChangeHandler>)handler;

- (void)_clearErrors;
- (void)_setMainDocumentError: (IFError *)error;
- (void)_addError: (IFError *)error forResource: (NSString *)resourceDescription;
- (BOOL)_isDocumentHTML;
+ (NSMutableDictionary *)_repTypes;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
@end
