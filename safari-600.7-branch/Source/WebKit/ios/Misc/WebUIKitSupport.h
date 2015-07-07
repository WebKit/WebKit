/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#import <CoreGraphics/CoreGraphics.h>
#import <unicode/uchar.h>

#ifdef __cplusplus
extern "C" {
#endif

// To be able to use background tasks from within WebKit, we need to expose that UIKit functionality
// without linking to UIKit.
// We accomplish this by giving UIKit 3 methods to set up:
//   - The invalid task ID value
//   - A block for starting a background task
//   - A block for ending a background task.
typedef NSUInteger WebBackgroundTaskIdentifier;
typedef void (^VoidBlock)(void);
typedef WebBackgroundTaskIdentifier (^StartBackgroundTaskBlock)(VoidBlock);
typedef void (^EndBackgroundTaskBlock)(WebBackgroundTaskIdentifier);

void WebKitSetInvalidWebBackgroundTaskIdentifier(WebBackgroundTaskIdentifier);
void WebKitSetStartBackgroundTaskBlock(StartBackgroundTaskBlock);
void WebKitSetEndBackgroundTaskBlock(EndBackgroundTaskBlock);

// These methods are what WebKit uses to start/stop background tasks after UIKit has set things up.
WebBackgroundTaskIdentifier invalidWebBackgroundTaskIdentifier();
WebBackgroundTaskIdentifier startBackgroundTask(VoidBlock);
void endBackgroundTask(WebBackgroundTaskIdentifier);

// This method gives WebKit the notifications to listen to so it knows about app Suspend/Resume
void WebKitSetBackgroundAndForegroundNotificationNames(NSString *, NSString *);

void WebKitInitialize(void);
void WebKitSetIsClassic(BOOL);
float WebKitGetMinimumZoomFontSize(void);
    
int WebKitGetLastLineBreakInBuffer(UChar *characters, int position, int length);

const char *WebKitPlatformSystemRootDirectory(void);

#ifdef __cplusplus
}
#endif
