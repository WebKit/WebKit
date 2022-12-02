/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "WKSnapshotConfigurationPrivate.h"

@implementation WKSnapshotConfiguration {
#if PLATFORM(MAC)
    BOOL _includesSelectionHighlighting;
#endif
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    self.rect = CGRectNull;
    self.afterScreenUpdates = YES;

#if PLATFORM(MAC)
    self._includesSelectionHighlighting = YES;
#endif

    return self;
}

- (void)dealloc
{
    [_snapshotWidth release];

    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    WKSnapshotConfiguration *snapshotConfiguration = [(WKSnapshotConfiguration *)[[self class] allocWithZone:zone] init];

    snapshotConfiguration.rect = self.rect;
    snapshotConfiguration.snapshotWidth = self.snapshotWidth;
    snapshotConfiguration.afterScreenUpdates = self.afterScreenUpdates;

#if PLATFORM(MAC)
    snapshotConfiguration._includesSelectionHighlighting = self._includesSelectionHighlighting;
#endif

    return snapshotConfiguration;
}

#if PLATFORM(MAC)

- (BOOL)_includesSelectionHighlighting
{
    return _includesSelectionHighlighting;
}

- (void)_setIncludesSelectionHighlighting:(BOOL)includesSelectionHighlighting
{
    _includesSelectionHighlighting = includesSelectionHighlighting;
}

#endif

@end
