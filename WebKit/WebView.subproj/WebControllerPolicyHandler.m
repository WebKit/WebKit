//
//  WebControllerPolicyHandler.m
//  WebKit
//
//  Created by Christopher Blumenberg on Thu Jul 25 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/WebControllerPolicyHandler.h>


@implementation WebPolicy : NSObject
- initWithPolicyAction: (WebPolicyAction)action
{
    [super init];
    policyAction = action;
    return self;
}

- (WebPolicyAction)policyAction
{
    return policyAction;
}

@end

@implementation WebURLPolicy : WebPolicy

+ webPolicyWithURLAction: (WebURLAction)action
{
    return [[[WebURLPolicy alloc] initWithPolicyAction:action] autorelease];
}

@end

@implementation WebFileURLPolicy : WebPolicy

+ webPolicyWithFileAction: (WebFileAction)action
{
    return [[[WebURLPolicy alloc] initWithPolicyAction:action] autorelease];
}

@end

@implementation WebContentPolicy : WebPolicy

+ webPolicyWithContentAction: (WebContentAction)action andPath: (NSString *)thePath
{
    return [[[WebContentPolicy alloc] initWithContentPolicyAction:action andPath:thePath] autorelease];
}

- initWithContentPolicyAction: (WebContentAction)action andPath: (NSString *)thePath
{
    [super initWithPolicyAction:action];
    path = [thePath retain];
    return self;
}

- (void)dealloc
{
    [path release];
    [super dealloc];
}

- (NSString *)path
{
    return path;
}

@end

@implementation WebClickPolicy : WebPolicy

- initWithClickPolicyAction: (WebClickAction)action andPath: (NSString *)thePath
{
    [super initWithPolicyAction:action];
    path = [thePath retain];
    return self;
}

+ webPolicyWithClickAction: (WebClickAction)action andPath: (NSString *)thePath
{
    return [[[WebClickPolicy alloc] initWithClickPolicyAction:action andPath:thePath] autorelease];
}

- (void)dealloc
{
    [path release];
    [super dealloc];
}

- (NSString *)path
{
    return path;
}

@end
