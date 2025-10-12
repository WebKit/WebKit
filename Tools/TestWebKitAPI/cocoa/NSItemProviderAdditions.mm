/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "NSItemProviderAdditions.h"

#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

@implementation NSItemProvider (NSItemProviderAdditions)

- (void)registerDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier withData:(NSData *)data
{
    [self registerDataRepresentationForTypeIdentifier:typeIdentifier withData:data loadingDelay:0];
}

- (void)registerDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier withData:(NSData *)data loadingDelay:(NSTimeInterval)delay
{
    auto retainedData = retainPtr(data);
    [self registerDataRepresentationForTypeIdentifier:typeIdentifier visibility:NSItemProviderRepresentationVisibilityAll loadHandler: [retainedData, delay] (DataLoadCompletionBlock block) -> NSProgress * {
        if (delay > 0) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)), mainDispatchQueueSingleton(), [block = makeBlockPtr(block), retainedData] {
                block(retainedData.get(), nil);
            });
        } else
            block(retainedData.get(), nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
}

@end
