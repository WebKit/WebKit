//
//  WebNetscapePluginEmbeddedView.h
//  WebKit
//
//  Created by Administrator on Mon Sep 30 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>

#import <WebKit/WebBaseNetscapePluginView.h>

@interface WebNetscapePluginEmbeddedView : WebBaseNetscapePluginView
{
    NSURL *URL;
}

- (id)initWithFrame:(NSRect)r
             plugin:(WebNetscapePlugin *)plugin
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
               mime:(NSString *)mimeType
          arguments:(NSDictionary *)arguments;

@end
