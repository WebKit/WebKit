/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "_WKFeatureInternal.h"
#import "_WKExperimentalFeature.h"
#import "_WKInternalDebugFeature.h"

#import <WebCore/WebCoreObjCExtras.h>

@implementation _WKFeature

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKFeature.class, self))
        return;

    _wrappedFeature->API::Feature::~Feature();

    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; name = %@; key = %@; defaultValue = %s >", NSStringFromClass(self.class), self, self.name, self.key, self.defaultValue ? "on" : "off"];
}

- (NSString *)name
{
    return _wrappedFeature->name();
}

- (WebFeatureStatus)status
{
    switch (_wrappedFeature->status()) {
    case API::FeatureStatus::Embedder:
        return WebFeatureStatusEmbedder;
    case API::FeatureStatus::Unstable:
        return WebFeatureStatusUnstable;
    case API::FeatureStatus::Internal:
        return WebFeatureStatusInternal;
    case API::FeatureStatus::Developer:
        return WebFeatureStatusDeveloper;
    case API::FeatureStatus::Testable:
        return WebFeatureStatusTestable;
    case API::FeatureStatus::Preview:
        return WebFeatureStatusPreview;
    case API::FeatureStatus::Stable:
        return WebFeatureStatusStable;
    case API::FeatureStatus::Mature:
        return WebFeatureStatusMature;
    default:
        ASSERT_NOT_REACHED();
    }
}

- (NSString *)key
{
    return _wrappedFeature->key();
}

- (NSString *)details
{
    return _wrappedFeature->details();
}

- (BOOL)defaultValue
{
    return _wrappedFeature->defaultValue();
}

- (BOOL)isHidden
{
    return _wrappedFeature->isHidden();
}

// For binary compatibility, some interfaces declare that they use the old
// _WKExperimentalFeature and _WKInternalDebugFeature classes, even though all
// instantiated features are actually instances of _WKFeature. Override
// isKindOfClass to prevent clients from detecting the change in instance type.
- (BOOL)isKindOfClass:(Class)aClass
{
    return [super isKindOfClass:aClass] || [aClass isEqual:[_WKExperimentalFeature class]] || [aClass isEqual:[_WKInternalDebugFeature class]];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_wrappedFeature;
}

@end
