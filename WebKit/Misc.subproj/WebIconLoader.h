//
//  WebIconLoader.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Jul 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class WebIconLoaderPrivate;
@protocol WebResourceClient;

@interface WebIconLoader : NSObject <WebResourceClient>
{
    WebIconLoaderPrivate *_private;
}

+ iconLoaderWithURL:(NSURL *)URL;
- initWithURL:(NSURL *)URL;
- (NSURL *)URL;
- (id)delegate;
- (void)setDelegate:(id)delegate;
- (void)startLoading;
- (void)stopLoading;
@end

@interface NSObject(WebIconLoaderDelegate)
- (void)iconLoader:(WebIconLoader *)iconLoader receivedPageIcon:(NSImage *)image;
@end;