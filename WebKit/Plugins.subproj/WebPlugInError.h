//
//  WebPluginError.h
//  WebKit
//
//  Created by Chris Blumenberg on Fri Nov 01 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebFoundation/WebError.h>

@class WebPluginErrorPrivate;

@interface WebPluginError : WebError
{
@private
    WebPluginErrorPrivate *_private;
}

+ (WebPluginError *)pluginErrorWithCode:(int)code
                             contentURL:(NSURL *)contentURL
                          pluginPageURL:(NSURL *)URL
                             pluginName:(NSString *)pluginName
                               MIMEType:(NSString *)MIMEType;

- initWithErrorWithCode:(int)code
             contentURL:(NSURL *)contentURL
          pluginPageURL:(NSURL *)pluginPageURL
             pluginName:(NSString *)pluginName
               MIMEType:(NSString *)MIMEType;

- (NSURL *)contentURL;
- (NSURL *)pluginPageURL;
- (NSString *)pluginName;
- (NSString *)MIMEType;

@end
