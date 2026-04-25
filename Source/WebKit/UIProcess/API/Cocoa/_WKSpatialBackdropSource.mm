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
#import "_WKSpatialBackdropSourceInternal.h"

#import <WebCore/SpatialBackdropSource.h>

@implementation _WKSpatialBackdropSource {
    RetainPtr<NSURL> m_sourceURL;
    RetainPtr<NSURL> m_modelURL;
    RetainPtr<NSURL> m_environmentMapURL;
}

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
- (instancetype)initWithSpatialBackdropSource:(const WebCore::SpatialBackdropSource&) spatialBackdropSource
{
    if (!(self = [super init]))
        return nil;

    m_sourceURL = spatialBackdropSource.m_sourceURL.createNSURL();
    m_modelURL = spatialBackdropSource.m_modelURL.createNSURL();
    if (spatialBackdropSource.m_environmentMapURL)
        m_environmentMapURL = spatialBackdropSource.m_environmentMapURL.value().createNSURL();

    return self;
}

- (void)dealloc
{
    [super dealloc];
}
#endif

- (NSURL *)sourceURL
{
    return m_sourceURL.get();
}

- (NSURL *)modelURL
{
    return m_modelURL.get();
}

- (NSURL *)environmentMapURL
{
    return m_environmentMapURL.get();
}

@end
