/*
     WebIconLoader.h
     Copyright 2004, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseResourceHandleDelegate.h>

@class WebIconLoaderPrivate;

/*!
    @class WebIconLoader
*/
@interface WebIconLoader : WebBaseResourceHandleDelegate
{
    WebIconLoaderPrivate *_private;
}

/*!
    @method initWithRequest:
    @param request
*/
- (id)initWithRequest:(NSURLRequest *)request;

/*!
    @method URL
*/
- (NSURL *)URL;

/*!
    @method delegate
*/
- (id)delegate;

/*!
    @method setDelegate:
    @param delegate
*/
- (void)setDelegate:(id)delegate;

/*!
    @method startLoading
*/
- (void)startLoading;

/*!
    @method stopLoading
*/
- (void)stopLoading;
@end

@interface NSObject(WebIconLoaderDelegate)
- (void)_iconLoaderReceivedPageIcon:(WebIconLoader *)iconLoader;
@end;
