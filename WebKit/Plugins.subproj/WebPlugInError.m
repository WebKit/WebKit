//
//  WebPluginError.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Nov 01 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/WebKitErrors.h>
#import <WebKit/WebPluginError.h>


@interface WebPluginErrorPrivate : NSObject
{
@public
    NSString *MIMEType;
    NSURL *pluginPageURL;
    NSString *pluginName;
}

@end

@implementation WebPluginErrorPrivate

- (void)dealloc
{
    [MIMEType release];
    [pluginPageURL release];
    [pluginName release];
    [super dealloc];
}

@end

@implementation WebPluginError

+ (WebPluginError *)pluginErrorWithCode:(int)code
                                    URL:(NSURL *)URL
                          pluginPageURL:(NSURL *)pluginPageURL
                             pluginName:(NSString *)pluginName
                               MIMEType:(NSString *)MIMEType;
{
    WebPluginError *error = [[WebPluginError alloc] initWithErrorWithCode:code
                                                                      URL:URL
                                                            pluginPageURL:pluginPageURL
                                                               pluginName:pluginName
                                                                 MIMEType:MIMEType];
    return [error autorelease];
}

- initWithErrorWithCode:(int)code
                    URL:(NSURL *)URL
          pluginPageURL:(NSURL *)pluginPageURL
             pluginName:(NSString *)pluginName
               MIMEType:(NSString *)MIMEType;
{
    [super initWithErrorCode:code inDomain:WebErrorDomainWebKit failingURL:[URL absoluteString]];
    
    _private = [[WebPluginErrorPrivate alloc] init];
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

