/*
        WebNetscapePluginEmbeddedView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/AppKit.h>

#import <WebKit/WebBaseNetscapePluginView.h>

@interface WebNetscapePluginEmbeddedView : WebBaseNetscapePluginView
{
    NSURL *URL;
}

- (id)initWithFrame:(NSRect)r
             plugin:(WebNetscapePluginPackage *)plugin
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
               mime:(NSString *)mimeType
         attributes:(NSDictionary *)attributes;

@end
