//
//  WebPluginError.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Nov 01 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebKitErrors.h>
#import <WebKit/WebPluginErrorPrivate.h>


@interface WebPluginErrorPrivate : NSObject
{
@public
    NSString *contentURL;
    NSString *pluginPageURL;
    NSString *MIMEType;
    NSString *pluginName;
}

@end

@implementation WebPluginErrorPrivate

- (void)dealloc
{
    [contentURL release];
    [pluginPageURL release];
    [pluginName release];
    [MIMEType release];
    [super dealloc];
}

@end

@implementation WebPluginError

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSString *)contentURL;
{
    return _private->contentURL;
}

- (NSString *)pluginPageURL
{
    return _private->pluginPageURL;
}

- (NSString *)pluginName
{
    return _private->pluginName;
}

- (NSString *)MIMEType
{
    return _private->MIMEType;
}

@end

@implementation WebPluginError (WebPrivate)

+ (WebPluginError *)pluginErrorWithCode:(int)code
                             contentURL:(NSString *)contentURL
                          pluginPageURL:(NSString *)pluginPageURL
                             pluginName:(NSString *)pluginName
                               MIMEType:(NSString *)MIMEType;
{
    WebPluginError *error = [[WebPluginError alloc] initWithErrorWithCode:code
                                                               contentURL:contentURL
                                                            pluginPageURL:pluginPageURL
                                                               pluginName:pluginName
                                                                 MIMEType:MIMEType];
    return [error autorelease];
}

- initWithErrorWithCode:(int)code
             contentURL:(NSString *)contentURL
          pluginPageURL:(NSString *)pluginPageURL
             pluginName:(NSString *)pluginName
               MIMEType:(NSString *)MIMEType;
{
    [super initWithErrorCode:code inDomain:WebErrorDomainWebKit failingURL:contentURL];

    _private = [[WebPluginErrorPrivate alloc] init];
    _private->contentURL = [contentURL retain];
    _private->pluginPageURL = [pluginPageURL retain];
    _private->pluginName = [pluginName retain];
    _private->MIMEType = [MIMEType retain];

    return self;
}

@end


