/*
 *  WebPluginErrorPrivate.h
 *  WebKit
 *
 *  Created by Chris Blumenberg on Wed Feb 05 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#import <WebKit/WebPluginError.h>

@interface WebPluginError (WebPrivate)

+ (WebPluginError *)pluginErrorWithCode:(int)code
                             contentURL:(NSString *)contentURL
                          pluginPageURL:(NSString *)URL
                             pluginName:(NSString *)pluginName
                               MIMEType:(NSString *)MIMEType;

- initWithErrorWithCode:(int)code
             contentURL:(NSString *)contentURL
          pluginPageURL:(NSString *)pluginPageURL
             pluginName:(NSString *)pluginName
               MIMEType:(NSString *)MIMEType;

@end