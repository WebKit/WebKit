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

- (NSString *)contentURL;
- (NSString *)pluginPageURL;
- (NSString *)pluginName;
- (NSString *)MIMEType;

@end
