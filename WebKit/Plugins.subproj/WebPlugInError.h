//
//  WebPluginError.h
//  WebKit
//
//  Created by Chris Blumenberg on Fri Nov 01 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
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
                                    URL:(NSURL *)URL
                          pluginPageURL:(NSURL *)URL
                             pluginName:(NSString *)pluginName
                               MIMEType:(NSString *)MIMEType;

- initWithErrorWithCode:(int)code
                    URL:(NSURL *)URL
          pluginPageURL:(NSURL *)pluginPageURL
             pluginName:(NSString *)pluginName
               MIMEType:(NSString *)MIMEType;

- (NSURL *)pluginPageURL;
- (NSString *)pluginName;
- (NSString *)MIMEType;

@end
