//
//  WebIconLoader.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Jul 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebKitDebug.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconDatabasePrivate.h>
#import <WebKit/WebIconLoader.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResourceLoadManager.h>

@interface WebIconLoaderPrivate : NSObject
{
@public
    WebResourceHandle *handle;
    id delegate;
    NSURL *URL;
}

@end;

@implementation WebIconLoaderPrivate

- (void)dealloc
{
    [URL release];
    [handle release];
    [super dealloc];
}

@end;

@implementation WebIconLoader

+ (void)_resizeImage:(NSImage *)image
{
    [image setScalesWhenResized:YES];
    [image setSize:NSMakeSize(IconWidth,IconHeight)];
}

+ (NSImage *)defaultIcon
{
    static NSImage *defaultIcon = nil;
    static BOOL loadedDefaultImage = NO;
    
    // Attempt to load default image only once, to avoid performance penalty of repeatedly
    // trying and failing to find it.
    if (!loadedDefaultImage) {
        NSString *pathForDefaultImage =
            [[NSBundle bundleForClass:[self class]] pathForResource:@"url_icon" ofType:@"tiff"];
        if (pathForDefaultImage != nil) {
            defaultIcon = [[NSImage alloc] initByReferencingFile: pathForDefaultImage];
            [[self class] _resizeImage:defaultIcon];
        }
        loadedDefaultImage = YES;
    }

    return defaultIcon;
}

+ (NSImage *)iconForFileAtPath:(NSString *)path
{
    static NSImage *htmlIcon = nil;
    NSImage *icon;

    if([[path pathExtension] rangeOfString:@"htm"].length != 0){
        if(!htmlIcon){
            htmlIcon = [[[NSWorkspace sharedWorkspace] iconForFileType:@"html"] retain];
            [[self class] _resizeImage:htmlIcon];
        }
        icon = htmlIcon;
    }else{
        icon = [[NSWorkspace sharedWorkspace] iconForFile:path];
        [[self class] _resizeImage:icon];
    }

    return icon;
}

+ iconLoaderWithURL:(NSURL *)URL
{
    return [[[self alloc] initWithURL:URL] autorelease];
}

- initWithURL:(NSURL *)URL
{
    [super init];
    _private = [[WebIconLoaderPrivate alloc] init];
    _private->URL = [URL retain];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSURL *)URL
{
    return _private->URL;
}

- (id)delegate
{
    return _private->delegate;
}

- (void)setDelegate:(id)delegate
{
    _private->delegate = delegate;
}

- (void)startLoading
{
    if (_private->handle != nil) {
        return;
    }

    NSImage *icon = [[WebIconDatabase sharedIconDatabase] _iconForIconURL:_private->URL];
    if (icon) {
        [_private->delegate iconLoader:self receivedPageIcon:icon];
        return;
    }
    
    _private->handle = [[WebResourceHandle alloc] initWithClient:self URL:_private->URL];
    if (_private->handle) {
        [_private->handle loadInBackground];
    }
}

- (void)stopLoading
{
    [_private->handle cancelLoadInBackground];
    [_private->handle release];
    _private->handle = nil;
}

- (NSString *)handleWillUseUserAgent:(WebResourceHandle *)handle forURL:(NSURL *)URL
{
    return nil;
}

- (void)handleDidBeginLoading:(WebResourceHandle *)sender
{
}

- (void)handleDidCancelLoading:(WebResourceHandle *)sender
{
}

- (void)handleDidFinishLoading:(WebResourceHandle *)sender data:(NSData *)data
{
    NSImage *icon = [[NSImage alloc] initWithData:data];
    if (icon) {
        [[WebIconDatabase sharedIconDatabase] _setIcon:icon forIconURL:_private->URL];
        [_private->delegate iconLoader:self receivedPageIcon:icon];
        [icon release];
    }
}

- (void)handleDidReceiveData:(WebResourceHandle *)sender data:(NSData *)data
{
}

- (void)handleDidFailLoading:(WebResourceHandle *)sender withError:(WebError *)result
{
}

- (void)handleDidRedirect:(WebResourceHandle *)sender toURL:(NSURL *)URL
{
}

@end
