/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WKFoundation.h>

#import <WebKit/_WKRemoteObjectInterface.h>

@protocol BaseRemoteObjectProtocol <NSObject>

- (void)sayHello:(NSString *)hello;

@end

@protocol OtherBaseRemoteObjectProtocol <NSObject>

- (void)sayHello:(NSString *)hello completionHandler:(void (^)(NSString *))completionHandler;

@end

@protocol RemoteObjectProtocol <BaseRemoteObjectProtocol, OtherBaseRemoteObjectProtocol>

- (void)selectionAndClickInformationForClickAtPoint:(NSValue *)pointValue completionHandler:(void (^)(NSDictionary *))completionHandler;
- (void)takeRange:(NSRange)range completionHandler:(void (^)(NSUInteger location, NSUInteger length))completionHandler;
- (void)takeSize:(CGSize)size completionHandler:(void (^)(CGFloat width, CGFloat height))completionHandler;
- (void)doNotCallCompletionHandler:(void (^)())completionHandler;

@end

static inline _WKRemoteObjectInterface *remoteObjectInterface()
{
    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(RemoteObjectProtocol)];

    [interface setClasses:[NSSet setWithObjects:[NSDictionary class], [NSString class], [NSURL class], nil] forSelector:@selector(selectionAndClickInformationForClickAtPoint:completionHandler:) argumentIndex:0 ofReply:YES];

    return interface;
}
