/*
    WebPluginContainer.h
    Copyright 2004, Apple, Inc. All rights reserved.
    
    Public header file.
*/

#import <Cocoa/Cocoa.h>

@interface NSObject (WebPlugInContainerPrivate)

- (id)_webPluginContainerCheckIfAllowedToLoadRequest:(NSURLRequest *)Request inFrame:(NSString *)target resultObject:(id)obj selector:(SEL)selector;

- (void)_webPluginContainerCancelCheckIfAllowedToLoadRequest:(id)checkIdentifier;

@end
