/*	IFWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _dataSourcePrivate in
        NSWebPageDataSource.
*/
#import <Cocoa/Cocoa.h>

#import <WebFoundation/WebFoundation.h>

#import <WebKit/IFWebDataSource.h>

// includes from kde
#include <khtmlview.h>

@class IFMainURLHandleClient;

@interface IFWebDataSourcePrivate : NSObject
{
    IFWebDataSource *parent;
    NSMutableArray *children;
    
    id <IFWebController>controller;
    
    // The original URL as requested during initialization.
    NSURL *inputURL;
    
    KHTMLPart *part;
    
    // Child frames of this frame.
    NSMutableDictionary *frames;
    
    // The handle client for the main document associated with the
    // datasource.
    IFMainURLHandleClient *mainURLHandleClient;
    
    // Active IFURLHandles for resources associated with the
    // datasource.
    NSMutableArray *urlHandles;

    // The time when the data source was told to start loading.
    double loadingStartedTime;
    
    bool primaryLoadComplete;
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
- (void)_recursiveStopLoading;
- (void)_addURLHandle: (IFURLHandle *)handle;
- (void)_removeURLHandle: (IFURLHandle *)handle;
- (void)_setPrimaryLoadComplete: (BOOL)flag;
- (double)_loadingStartedTime;
@end
