//
//  WebIconLoader.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Jul 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class WebDataSource;
@class WebResourceHandle;
@class WebIconLoaderPrivate;
@protocol WebResourceClient;

@interface WebIconLoader : NSObject <WebResourceClient>
{
    WebIconLoaderPrivate *_private;
}

+ (NSImage *)defaultIcon;

- initWithURL:(NSURL *)iconURL;
- (void)setDelegate:(id)delegate;
- (void)startLoading;
- (void)startLoadingOnlyFromCache;
- (void)stopLoading;
@end

@interface NSObject(WebIconLoaderDelegate)
- (void)iconLoader:(WebIconLoader *)iconLoader receivedPageIcon:(NSImage *)image;
@end;