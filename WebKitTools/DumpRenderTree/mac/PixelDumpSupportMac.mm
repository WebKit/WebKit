/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DumpRenderTree.h" 
#include "PixelDumpSupport.h"
#include "PixelDumpSupportCG.h"

#include "LayoutTestController.h"
#include <CoreGraphics/CGBitmapContext.h>
#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>

#import <WebKit/WebDocumentPrivate.h>
#import <WebKit/WebKit.h>

static unsigned char* screenCaptureBuffer;

static CMProfileRef currentColorProfile = 0;
static CGColorSpaceRef sharedColorSpace;

void restoreColorSpace(int ignored)
{
    // This is used as a signal handler, and thus the calls into ColorSync are unsafe
    // But we might as well try to restore the user's color profile, we're going down anyway...
    if (currentColorProfile) {
        // This call is deprecated in Leopard, but there appears to be no replacement.
        int error = CMSetDefaultProfileByUse(cmDisplayUse, currentColorProfile);
        if (error)
            fprintf(stderr, "Failed to retore previous color profile!  You may need to open System Preferences : Displays : Color and manually restore your color settings.  (Error: %i)", error);
        currentColorProfile = 0;
    }
}

static void setDefaultColorProfileToRGB()
{
    CMProfileRef genericProfile = (CMProfileRef)[[NSColorSpace genericRGBColorSpace] colorSyncProfile];
    CMProfileRef previousProfile;
    int error = CMGetDefaultProfileByUse(cmDisplayUse, &previousProfile);
    if (error) {
        fprintf(stderr, "Failed to get current color profile.  I will not be able to restore your current profile, thus I'm not changing it.  Many pixel tests may fail as a result.  (Error: %i)\n", error);
        return;
    }
    if (previousProfile == genericProfile)
        return;
    CFStringRef previousProfileName;
    CFStringRef genericProfileName;
    char previousProfileNameString[1024];
    char genericProfileNameString[1024];
    CMCopyProfileDescriptionString(previousProfile, &previousProfileName);
    CMCopyProfileDescriptionString(genericProfile, &genericProfileName);
    CFStringGetCString(previousProfileName, previousProfileNameString, sizeof(previousProfileNameString), kCFStringEncodingUTF8);
    CFStringGetCString(genericProfileName, genericProfileNameString, sizeof(previousProfileNameString), kCFStringEncodingUTF8);
    CFRelease(previousProfileName);
    CFRelease(genericProfileName);

    fprintf(stderr, "\n\nWARNING: Temporarily changing your system color profile from \"%s\" to \"%s\".\n", previousProfileNameString, genericProfileNameString);
    fprintf(stderr, "This allows the WebKit pixel-based regression tests to have consistent color values across all machines.\n");
    fprintf(stderr, "The colors on your screen will change for the duration of the testing.\n\n");
    
    if ((error = CMSetDefaultProfileByUse(cmDisplayUse, genericProfile))) {
        fprintf(stderr, "Failed to set color profile to \"%s\"! Many pixel tests will fail as a result.  (Error: %i)",
            genericProfileNameString, error);
    } else {
        currentColorProfile = previousProfile;
        signal(SIGINT, restoreColorSpace);
        signal(SIGHUP, restoreColorSpace);
        signal(SIGTERM, restoreColorSpace);
    }
}

void initializeColorSpaceAndScreeBufferForPixelTests()
{
    setDefaultColorProfileToRGB();
    screenCaptureBuffer = (unsigned char *)malloc(maxViewHeight * maxViewWidth * 4);
    sharedColorSpace = CGColorSpaceCreateDeviceRGB();
}

// Declared in PixelDumpSupportCG.h

RetainPtr<CGContextRef> getBitmapContextFromWebView()
{
    NSSize webViewSize = [[mainFrame webView] frame].size;
    return RetainPtr<CGContextRef>(AdoptCF, CGBitmapContextCreate(screenCaptureBuffer, static_cast<size_t>(webViewSize.width), static_cast<size_t>(webViewSize.height), 8, static_cast<size_t>(webViewSize.width) * 4, sharedColorSpace, kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedLast));
}

void paintWebView(CGContextRef context)
{
    RetainPtr<NSGraphicsContext> savedContext = [NSGraphicsContext currentContext];

    NSGraphicsContext* nsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
    [NSGraphicsContext setCurrentContext:nsContext];

    WebView* view = [mainFrame webView];
    [view displayIfNeeded];
    [view lockFocus];
    NSBitmapImageRep *imageRep = [[NSBitmapImageRep alloc] initWithFocusedViewRect:[view frame]];
    [view unlockFocus];
    [imageRep draw];
    [imageRep release];

    [NSGraphicsContext setCurrentContext:savedContext.get()];
}

void repaintWebView(CGContextRef context, bool horizontal)
{
    RetainPtr<NSGraphicsContext> savedContext = [NSGraphicsContext currentContext];

    NSGraphicsContext* nsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO];
    [NSGraphicsContext setCurrentContext:nsContext];

    WebView *view = [mainFrame webView];
    NSSize webViewSize = [view frame].size;

    if (horizontal) {
        for (NSRect column = NSMakeRect(0, 0, 1, webViewSize.height); column.origin.x < webViewSize.width; column.origin.x++)
            [view displayRectIgnoringOpacity:column inContext:nsContext];
    } else {
        for (NSRect line = NSMakeRect(0, 0, webViewSize.width, 1); line.origin.y < webViewSize.height; line.origin.y++)
            [view displayRectIgnoringOpacity:line inContext:nsContext];
    }

    [NSGraphicsContext setCurrentContext:savedContext.get()];
}

CGRect getSelectionRect()
{
    NSView *documentView = [[mainFrame frameView] documentView];
    if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)]) {
        NSRect rect = [documentView convertRect:[(id <WebDocumentSelection>)documentView selectionRect] fromView:nil];
        return CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    }

    ASSERT_NOT_REACHED();
    return CGRectZero;
}
