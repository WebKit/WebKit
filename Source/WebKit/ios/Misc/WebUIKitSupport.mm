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

#if PLATFORM(IOS)

#import "WebUIKitSupport.h"

#import "WebDatabaseManagerInternal.h"
#import "WebKitSystemInterface.h"
#import "WebLocalizableStrings.h"
#import "WebPlatformStrategies.h"
#import "WebSystemInterface.h"
#import "WebViewPrivate.h"
#import <WebCore/DynamicLinkerSPI.h>
#import <WebCore/PathUtilities.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/Settings.h>
#import <WebCore/TextBreakIterator.h>
#import <WebCore/WebCoreSystemInterface.h>
#import <WebCore/WebCoreThreadSystemInterface.h>
#import <WebCore/break_lines.h>

#import <runtime/InitializeThreading.h>

using namespace WebCore;

static inline bool linkedOnOrAfterIOS5()
{
    static bool s_linkedOnOrAfterIOS5 = dyld_get_program_sdk_version() >= DYLD_IOS_VERSION_5_0;
    return s_linkedOnOrAfterIOS5;
}

void WebKitInitialize(void)
{
    static bool webkitInitialized;
    if (webkitInitialized)
        return;

    ASSERT(pthread_main_np());
    webkitInitialized = true;
    InitWebCoreThreadSystemInterface();
    [WebView enableWebThread];
    InitWebCoreSystemInterface();

    // Initialize our platform strategies.
    WebPlatformStrategies::initializeIfNecessary();

    // We'd rather eat this cost at startup than slow down situations that need to be responsive.
    // See <rdar://problem/6776301>.
    LoadWebLocalizedStrings();
    [WebView registerForMemoryNotifications];
    
    // This needs to be called before any requests are made in the process, <rdar://problem/9691871>
    WebCore::initializeHTTPConnectionSettingsOnStartup();
    WebCore::enableURLSchemeCanonicalization(linkedOnOrAfterIOS5());
}

void WebKitSetIsClassic(BOOL flag)
{
    // FIXME: Remove this once it stops being called.
}

float WebKitGetMinimumZoomFontSize(void)
{
    return Settings::defaultMinimumZoomFontSize();
}

int WebKitGetLastLineBreakInBuffer(UChar *characters, int position, int length)
{
    int lastBreakPos = position;
    int breakPos = 0;
    LazyLineBreakIterator breakIterator(String(characters, length));
    while ((breakPos = nextBreakablePosition(breakIterator, breakPos)) < position)
        lastBreakPos = breakPos++;
    return lastBreakPos < position ? (NSUInteger)lastBreakPos : INT_MAX;
}

const char *WebKitPlatformSystemRootDirectory(void)
{
#if PLATFORM(IOS_SIMULATOR)
    static const char *platformSystemRootDirectory = nil;
    if (!platformSystemRootDirectory) {
        char *simulatorRoot = getenv("IPHONE_SIMULATOR_ROOT");
        platformSystemRootDirectory = simulatorRoot ? simulatorRoot : "/";
    }
    return platformSystemRootDirectory;
#else
    return "/";
#endif
}

void WebKitSetBackgroundAndForegroundNotificationNames(NSString *didEnterBackgroundName, NSString *willEnterForegroundName)
{
    // FIXME: Remove this function.
}

static WebBackgroundTaskIdentifier invalidTaskIdentifier = 0;
static StartBackgroundTaskBlock startBackgroundTaskBlock = 0;
static EndBackgroundTaskBlock endBackgroundTaskBlock = 0;

void WebKitSetInvalidWebBackgroundTaskIdentifier(WebBackgroundTaskIdentifier taskIdentifier)
{
    invalidTaskIdentifier = taskIdentifier;
}

void WebKitSetStartBackgroundTaskBlock(StartBackgroundTaskBlock startBlock)
{
    Block_release(startBackgroundTaskBlock);
    startBackgroundTaskBlock = Block_copy(startBlock);    
}

void WebKitSetEndBackgroundTaskBlock(EndBackgroundTaskBlock endBlock)
{
    Block_release(endBackgroundTaskBlock);
    endBackgroundTaskBlock = Block_copy(endBlock);    
}

WebBackgroundTaskIdentifier invalidWebBackgroundTaskIdentifier()
{
    return invalidTaskIdentifier;
}

WebBackgroundTaskIdentifier startBackgroundTask(VoidBlock expirationHandler)
{
    if (!startBackgroundTaskBlock)
        return invalidTaskIdentifier;
    return startBackgroundTaskBlock(expirationHandler);
}

void endBackgroundTask(WebBackgroundTaskIdentifier taskIdentifier)
{
    if (!endBackgroundTaskBlock)
        return;
    endBackgroundTaskBlock(taskIdentifier);
}

CGPathRef WebKitCreatePathWithShrinkWrappedRects(NSArray* cgRects, CGFloat radius)
{
    Vector<FloatRect> rects;
    rects.reserveInitialCapacity([cgRects count]);

    const char* cgRectEncodedString = @encode(CGRect);

    for (NSValue *rectValue in cgRects) {
        CGRect cgRect;
        [rectValue getValue:&cgRect];

        if (strcmp(cgRectEncodedString, rectValue.objCType))
            return nullptr;
        rects.append(cgRect);
    }

    return CGPathRetain(PathUtilities::pathWithShrinkWrappedRects(rects, radius).platformPath());
}

#endif // PLATFORM(IOS)
