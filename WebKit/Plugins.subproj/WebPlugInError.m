//
//  WebPluginError.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Nov 01 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebKitErrors.h>
#import <WebKit/WebPluginError.h>


@interface WebPluginErrorPrivate : NSObject
{
@public
    NSURL *contentURL;
    NSURL *pluginPageURL;
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

+ (WebPluginError *)pluginErrorWithCode:(int)code
                             contentURL:(NSURL *)contentURL
                          pluginPageURL:(NSURL *)pluginPageURL
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
             contentURL:(NSURL *)contentURL
          pluginPageURL:(NSURL *)pluginPageURL
             pluginName:(NSString *)pluginName
               MIMEType:(NSString *)MIMEType;
{
    [super initWithErrorCode:code inDomain:WebErrorDomainWebKit failingURL:[contentURL absoluteString]];
    
    _private = [[WebPluginErrorPrivate alloc] init];
    _private->contentURL = [contentURL retain];
    _private->pluginPageURL = [pluginPageURL retain];
    _private->pluginName = [pluginName retain];
    _private->MIMEType = [MIMEType retain];
    
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSURL *)contentURL;
{
    return _private->contentURL;
}

- (NSURL *)pluginPageURL
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

