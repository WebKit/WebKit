/*	IFWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in
        NSWebPageDataSource.
*/

#import <WebKit/IFWebDataSource.h>

class KHTMLPart;

@class IFURLHandle;
@class IFMainURLHandleClient;
@protocol IFLocationChangeHandler;

@interface IFWebDataSourcePrivate : NSObject
{
    IFWebDataSource *parent;
    NSMutableArray *children;
    
    id <IFWebController>controller;
    
    // The original URL as requested during initialization.
    NSURL *inputURL;
    
    // The original URL we may have been redirected to.
    NSURL *finalURL;
    
    KHTMLPart *part;
    
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
    
    NSString *pageTitle;
    
    // The location change handler for this data source.
    id <IFLocationChangeHandler>locationChangeHandler;

    BOOL loading; // self and controller are retained while loading
}

- init;
- (void)dealloc;

@end

@interface IFWebDataSource (IFPrivate)
- (void)_setController: (id <IFWebController>)controller;
- (KHTMLPart *)_part;
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
@end
