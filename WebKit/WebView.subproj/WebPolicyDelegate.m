/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebPolicyDelegatePrivate.h>

NSString *WebActionNavigationTypeKey = @"WebActionNavigationTypeKey";
NSString *WebActionElementKey = @"WebActionElementKey";
NSString *WebActionButtonKey = @"WebActionButtonKey"; 
NSString *WebActionModifierFlagsKey = @"WebActionModifierFlagsKey";
NSString *WebActionOriginalURLKey = @"WebActionOriginalURLKey";


@interface WebPolicyDecisionListenerPrivate : NSObject
{
@public
    id target;
    SEL action;
}

-(id)initWithTarget:(id)target action:(SEL)action;

@end

@implementation WebPolicyDecisionListenerPrivate

-(id)initWithTarget:(id)t action:(SEL)a
{
    self = [super init];
    if (self != nil) {
	target = [t retain];
	action = a;
    }
    return self;
}

-(void)dealloc
{
    [target release];
    [super dealloc];
}

@end

@implementation WebPolicyDecisionListener

-(id)_initWithTarget:(id)target action:(SEL)action
{
    self = [super init];
    if (self != nil) {
	_private = [[WebPolicyDecisionListenerPrivate alloc] initWithTarget:target action:action];
    }
    return self;
}

-(void)dealloc
{
    [_private release];
    [super dealloc];
}


-(void)_usePolicy:(WebPolicyAction)policy
{
    if (_private->target != nil) {
	[_private->target performSelector:_private->action withObject:(id)policy];
    }
}

-(void)_invalidate
{
    [self retain];
    [_private->target release];
    _private->target = nil;
    [self release];
}

// WebPolicyDecisionListener implementation

-(void)use
{
    [self _usePolicy:WebPolicyUse];
}

-(void)ignore
{
    [self _usePolicy:WebPolicyIgnore];
}

-(void)download
{
    [self _usePolicy:WebPolicyDownload];
}

// WebFormSubmissionListener implementation

-(void)continue
{
    [self _usePolicy:WebPolicyUse];
}

@end
