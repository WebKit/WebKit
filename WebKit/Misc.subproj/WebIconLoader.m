//
//  WebIconLoader.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Jul 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebIconLoader.h>

#import <WebFoundation/WebFoundation.h>
#import <WebFoundation/WebNSURLExtras.h>

@interface WebIconLoaderPrivate : NSObject
{
@public
    WebResourceHandle *resourceHandle;
    id delegate;
    NSURL *URL;
}

@end;

@implementation WebIconLoaderPrivate

- (void)dealloc
{
    [URL release];
    [resourceHandle release];
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
            htmlIcon = [[[NSWorkspace sharedWorkspace] iconForFile:path] retain];
            [[self class] _resizeImage:htmlIcon];
        }
        icon = htmlIcon;
    }else{
        icon = [[NSWorkspace sharedWorkspace] iconForFile:path];
        [[self class] _resizeImage:icon];
    }

    return icon;
}


- initWithURL:(NSURL *)iconURL
{
    [super init];
    _private = [[WebIconLoaderPrivate alloc] init];
    _private->URL = [iconURL retain];
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (void)setDelegate:(id)delegate
{
    _private->delegate = delegate;
}

- (void)startLoading
{
    _private->resourceHandle = [[WebResourceHandle alloc] initWithURL:_private->URL];
    [_private->resourceHandle addClient:self];
    [_private->resourceHandle loadInBackground];
}

- (void)startLoadingOnlyFromCache
{
    [self startLoading];
}

- (void)stopLoading
{
    [_private->resourceHandle cancelLoadInBackground];
}

- (void)WebResourceHandleDidBeginLoading:(WebResourceHandle *)sender
{

}

- (void)WebResourceHandleDidCancelLoading:(WebResourceHandle *)sender
{

}

- (void)WebResourceHandleDidFinishLoading:(WebResourceHandle *)sender data:(NSData *)data
{
    NSImage *image = [[NSImage alloc] initWithData:data];
    if (image) {
        [[self class] _resizeImage:image];
        [_private->delegate iconLoader:self receivedPageIcon:image];
        [image release];
    }
}

- (void)WebResourceHandle:(WebResourceHandle *)sender dataDidBecomeAvailable:(NSData *)data
{
}

- (void)WebResourceHandle:(WebResourceHandle *)sender didFailLoadingWithResult:(WebError *)result
{
}

- (void)WebResourceHandle:(WebResourceHandle *)sender didRedirectToURL:(NSURL *)URL
{
}

@end
