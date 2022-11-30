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
#import "_WKExperimentalFeatureInternal.h"

#import <WebCore/WebCoreObjCExtras.h>

@implementation _WKExperimentalFeature

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKExperimentalFeature.class, self))
        return;

    _experimentalFeature->API::ExperimentalFeature::~ExperimentalFeature();

    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; name = %@; key = %@; defaultValue = %s >", NSStringFromClass(self.class), self, self.name, self.key, self.defaultValue ? "on" : "off"];
}

- (NSString *)name
{
    return _experimentalFeature->name();
}

- (WebFeatureStatus)status
{
    switch (_experimentalFeature->status()) {
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
    default:
        ASSERT_NOT_REACHED();
    }
}

- (NSString *)key
{
    return _experimentalFeature->key();
}

- (NSString *)details
{
    return _experimentalFeature->details();
}

- (BOOL)defaultValue
{
    return _experimentalFeature->defaultValue();
}

- (BOOL)isHidden
{
    return _experimentalFeature->isHidden();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_experimentalFeature;
}

@end
