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
#import "WebLocalizableStringsInternal.h"
#import "WebPlatformStrategies.h"
#import "WebViewPrivate.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/BreakLines.h>
#import <WebCore/PathUtilities.h>
#import <WebCore/ResourceRequest.h>
#import <WebCore/Settings.h>
#import <WebCore/WebBackgroundTaskController.h>
#import <WebCore/WebCoreThreadSystemInterface.h>
#import <wtf/spi/darwin/dyldSPI.h>

using namespace WebCore;

// See <rdar://problem/7902473> Optimize WebLocalizedString for why we do this on a background thread on a timer callback
static void LoadWebLocalizedStringsTimerCallback(CFRunLoopTimerRef timer, void *info)
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^ {
        // We don't care if we find this string, but searching for it will load the plist and save the results.
        // FIXME: It would be nicer to do this in a more direct way.
        UI_STRING_KEY_INTERNAL("Typing", "Typing (Undo action name)", "Undo action name");
    });
}

static void LoadWebLocalizedStrings()
{
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(), 0, 0, 0, &LoadWebLocalizedStringsTimerCallback, NULL);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);
    CFRelease(timer);
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

    // Initialize our platform strategies.
    WebPlatformStrategies::initializeIfNecessary();

    // We'd rather eat this cost at startup than slow down situations that need to be responsive.
    // See <rdar://problem/6776301>.
    LoadWebLocalizedStrings();
    
    // This needs to be called before any requests are made in the process, <rdar://problem/9691871>
    WebCore::initializeHTTPConnectionSettingsOnStartup();
}

void WebKitSetIsClassic(BOOL flag)
{
    // FIXME: Remove this once it stops being called.
}

float WebKitGetMinimumZoomFontSize(void)
{
    return WebCore::Settings::defaultMinimumZoomFontSize();
}

int WebKitGetLastLineBreakInBuffer(UChar *characters, int position, int length)
{
    unsigned lastBreakPos = position;
    unsigned breakPos = 0;
    LazyLineBreakIterator breakIterator(StringView(characters, length));
    while (static_cast<int>(breakPos = nextBreakablePosition(breakIterator, breakPos)) < position)
        lastBreakPos = breakPos++;
    return static_cast<int>(lastBreakPos) < position ? lastBreakPos : INT_MAX;
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

void WebKitSetInvalidWebBackgroundTaskIdentifier(WebBackgroundTaskIdentifier taskIdentifier)
{
    [[WebBackgroundTaskController sharedController] setInvalidBackgroundTaskIdentifier:taskIdentifier];
}

void WebKitSetStartBackgroundTaskBlock(StartBackgroundTaskBlock startBlock)
{
    [[WebBackgroundTaskController sharedController] setBackgroundTaskStartBlock:startBlock];
}

void WebKitSetEndBackgroundTaskBlock(EndBackgroundTaskBlock endBlock)
{
    [[WebBackgroundTaskController sharedController] setBackgroundTaskEndBlock:endBlock];
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
