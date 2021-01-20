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

#if PLATFORM(MAC)

#import <AppKit/AppKit.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

@interface EventSerializer : NSObject

+ (NSDictionary *)dictionaryForEvent:(CGEventRef)event relativeToTime:(CGEventTimestamp)referenceTimestamp;

+ (RetainPtr<CGEventRef>)createEventForDictionary:(NSDictionary *)dict inWindow:(NSWindow *)window relativeToTime:(CGEventTimestamp)referenceTimestamp;

@end

@interface EventStreamPlayer : NSObject {
    RetainPtr<NSMutableArray> _remainingEventDictionaries;
    RetainPtr<NSWindow> _window;
    BlockPtr<void ()> _completionHandler;
    uint64_t _startTime;
}

+ (void)playStream:(NSArray<NSDictionary *> *)eventDicts window:(NSWindow *)window completionHandler:(void(^)())completionHandler;

@end

#endif
