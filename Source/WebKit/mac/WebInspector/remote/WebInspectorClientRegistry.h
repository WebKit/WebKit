/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <wtf/HashMap.h>

class WebInspectorClient;

@protocol WebInspectorClientRegistryDelegate
- (void)didRegisterClient:(WebInspectorClient*)client;
- (void)didUnregisterClient:(WebInspectorClient*)client;
@end

@interface WebInspectorClientRegistry : NSObject {
@private
    unsigned _nextAvailablePageId;
    HashMap<unsigned, WebInspectorClient*> _pageClientMap;
    id <WebInspectorClientRegistryDelegate> _delegate;
}

@property (nonatomic, assign) id <WebInspectorClientRegistryDelegate> delegate;

+ (WebInspectorClientRegistry *)sharedRegistry;

- (void)registerClient:(WebInspectorClient*)client;
- (void)unregisterClient:(WebInspectorClient*)client;
- (WebInspectorClient*)clientForPageId:(unsigned)pageId;
- (NSDictionary *)inspectableWebViews;

@end
