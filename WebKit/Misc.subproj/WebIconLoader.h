/*
     WebIconLoader.h
     Copyright 2002, Apple, Inc. All rights reserved.

     Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebIconLoaderPrivate;

/*!
    @class WebIconLoader
*/
@interface WebIconLoader : NSObject
{
    WebIconLoaderPrivate *_private;
}

/*!
    @method iconLoaderWithURL:
    @param URL
*/
+ iconLoaderWithURL:(NSURL *)URL;

/*!
    @method initWithURL:
    @param URL
*/
- initWithURL:(NSURL *)URL;

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
