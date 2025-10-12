/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "_WKTextExtractionInternal.h"

#import "WKWebViewInternal.h"
#import <WebKit/WKError.h>
#import <wtf/RetainPtr.h>

@implementation _WKTextExtractionConfiguration

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _canIncludeIdentifiers = YES;
    _shouldFilterText = YES;
    _targetRect = CGRectNull;
    return self;
}

@end

@implementation _WKTextExtractionInteraction {
    RetainPtr<NSString> _nodeIdentifier;
    RetainPtr<NSString> _text;
}

@synthesize action = _action;
@synthesize replaceAll = _replaceAll;
@synthesize location = _location;
@synthesize hasSetLocation = _hasSetLocation;

- (instancetype)initWithAction:(_WKTextExtractionAction)action
{
    if (!(self = [super init]))
        return nil;

    _location = CGPointZero;
    _action = action;
    return self;
}

- (NSString *)nodeIdentifier
{
    return _nodeIdentifier.get();
}

- (void)setNodeIdentifier:(NSString *)nodeIdentifier
{
    _nodeIdentifier = adoptNS(nodeIdentifier.copy);
}

- (NSString *)text
{
    return _text.get();
}

- (void)setText:(NSString *)text
{
    _text = adoptNS(text.copy);
}

- (void)setLocation:(CGPoint)location
{
    _hasSetLocation = YES;
    _location = location;
}

- (void)debugDescriptionInWebView:(WKWebView *)webView completionHandler:(void (^)(NSString *, NSError *))completionHandler
{
    [webView _describeInteraction:self completionHandler:completionHandler];
}

@end

@implementation _WKTextExtractionInteractionResult {
    RetainPtr<NSError> _error;
}

- (instancetype)initWithErrorDescription:(NSString *)errorDescription
{
    if (!(self = [super init]))
        return nil;

    if (errorDescription)
        _error = [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSDebugDescriptionErrorKey: errorDescription }];

    return self;
}

- (NSError *)error
{
    return _error.get();
}

@end
