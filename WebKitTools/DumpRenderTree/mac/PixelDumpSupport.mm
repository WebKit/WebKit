/*
 * Copyright (C) 2005, 2006, 2007 Apple, Inc.  All rights reserved.
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
 
#import "PixelDumpSupport.h"

#import "DumpRenderTree.h"
#import "LayoutTestController.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h> // for CMSetDefaultProfileBySpace
#import <WebKit/WebKit.h>
#import <WebKit/WebDocumentPrivate.h>

#define COMMON_DIGEST_FOR_OPENSSL
#import <CommonCrypto/CommonDigest.h>               // for MD5 functions

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
    CFRelease(genericProfileName);
    CFRelease(previousProfileName);
    
    fprintf(stderr, "\n\nWARNING: Temporarily changing your system color profile from \"%s\" to \"%s\".\n", previousProfileNameString, genericProfileNameString);
    fprintf(stderr, "This allows the WebKit pixel-based regression tests to have consistent color values across all machines.\n");
    fprintf(stderr, "The colors on your screen will change for the duration of the testing.\n\n");
    
    if ((error = CMSetDefaultProfileByUse(cmDisplayUse, genericProfile)))
        fprintf(stderr, "Failed to set color profile to \"%s\"! Many pixel tests will fail as a result.  (Error: %i)",
            genericProfileNameString, error);
    else {
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

/* Hashes a bitmap and returns a text string for comparison and saving to a file */
static NSString *md5HashStringForBitmap(CGImageRef bitmap)
{
    MD5_CTX md5Context;
    unsigned char hash[16];
    
    unsigned bitsPerPixel = CGImageGetBitsPerPixel(bitmap);
    assert(bitsPerPixel == 32); // ImageDiff assumes 32 bit RGBA, we must as well.
    unsigned bytesPerPixel = bitsPerPixel / 8;
    unsigned pixelsHigh = CGImageGetHeight(bitmap);
    unsigned pixelsWide = CGImageGetWidth(bitmap);
    unsigned bytesPerRow = CGImageGetBytesPerRow(bitmap);
    assert(bytesPerRow >= (pixelsWide * bytesPerPixel));
    
    MD5_Init(&md5Context);
    unsigned char *bitmapData = screenCaptureBuffer;
    for (unsigned row = 0; row < pixelsHigh; row++) {
        MD5_Update(&md5Context, bitmapData, pixelsWide * bytesPerPixel);
        bitmapData += bytesPerRow;
    }
    MD5_Final(hash, &md5Context);
    
    char hex[33] = "";
    for (int i = 0; i < 16; i++) {
        snprintf(hex, 33, "%s%02x", hex, hash[i]);
    }
    
    return [NSString stringWithUTF8String:hex];
}

void dumpWebViewAsPixelsAndCompareWithExpected(NSString* currentTest, bool forceAllTestsToDumpPixels)
{
    if (!layoutTestController->dumpAsText() && !layoutTestController->dumpDOMAsWebArchive() && !layoutTestController->dumpSourceAsWebArchive()) {
        // grab a bitmap from the view
        WebView* view = [mainFrame webView];
        NSSize webViewSize = [view frame].size;
        
        CGContextRef cgContext = CGBitmapContextCreate(screenCaptureBuffer, static_cast<size_t>(webViewSize.width), static_cast<size_t>(webViewSize.height), 8, static_cast<size_t>(webViewSize.width) * 4, sharedColorSpace, kCGBitmapByteOrder32Host | kCGImageAlphaPremultipliedLast);
        
        NSGraphicsContext* savedContext = [[[NSGraphicsContext currentContext] retain] autorelease];
        NSGraphicsContext* nsContext = [NSGraphicsContext graphicsContextWithGraphicsPort:cgContext flipped:NO];
        [NSGraphicsContext setCurrentContext:nsContext];
        
        if (!layoutTestController->testRepaint()) {
            NSBitmapImageRep *imageRep;
            [view displayIfNeeded];
            [view lockFocus];
            imageRep = [[NSBitmapImageRep alloc] initWithFocusedViewRect:[view frame]];
            [view unlockFocus];
            [imageRep draw];
            [imageRep release];
        } else if (!layoutTestController->testRepaintSweepHorizontally()) {
            NSRect line = NSMakeRect(0, 0, webViewSize.width, 1);
            while (line.origin.y < webViewSize.height) {
                [view displayRectIgnoringOpacity:line inContext:nsContext];
                line.origin.y++;
            }
        } else {
            NSRect column = NSMakeRect(0, 0, 1, webViewSize.height);
            while (column.origin.x < webViewSize.width) {
                [view displayRectIgnoringOpacity:column inContext:nsContext];
                column.origin.x++;
            }
        }
        if (layoutTestController->dumpSelectionRect()) {
            NSView *documentView = [[mainFrame frameView] documentView];
            if ([documentView conformsToProtocol:@protocol(WebDocumentSelection)]) {
                [[NSColor redColor] set];
                [NSBezierPath strokeRect:[documentView convertRect:[(id <WebDocumentSelection>)documentView selectionRect] fromView:nil]];
            }
        }
        
        [NSGraphicsContext setCurrentContext:savedContext];
        
        CGImageRef bitmapImage = CGBitmapContextCreateImage(cgContext);
        CGContextRelease(cgContext);
        
        // compute the actual hash to compare to the expected image's hash
        NSString *actualHash = md5HashStringForBitmap(bitmapImage);
        printf("\nActualHash: %s\n", [actualHash UTF8String]);
        
        BOOL dumpImage;
        if (forceAllTestsToDumpPixels)
            dumpImage = YES;
        else {
            // FIXME: It's unfortunate that we hardcode the file naming scheme here.
            // At one time, the perl script had all the knowledge about file layout.
            // Some day we should restore that setup by passing in more parameters to this tool
            // or returning more information from the tool to the perl script
            NSString *baseTestPath = [currentTest stringByDeletingPathExtension];
            NSString *baselineHashPath = [baseTestPath stringByAppendingString:@"-expected.checksum"];
            NSString *baselineHash = [NSString stringWithContentsOfFile:baselineHashPath encoding:NSUTF8StringEncoding error:nil];
            NSString *baselineImagePath = [baseTestPath stringByAppendingString:@"-expected.png"];
            
            printf("BaselineHash: %s\n", [baselineHash UTF8String]);
            
            /// send the image to stdout if the hash mismatches or if there's no file in the file system
            dumpImage = ![baselineHash isEqualToString:actualHash] || access([baselineImagePath fileSystemRepresentation], F_OK) != 0;
        }
        
        if (dumpImage) {
            CFMutableDataRef imageData = CFDataCreateMutable(0, 0);
            CGImageDestinationRef imageDest = CGImageDestinationCreateWithData(imageData, CFSTR("public.png"), 1, 0);
            CGImageDestinationAddImage(imageDest, bitmapImage, 0);
            CGImageDestinationFinalize(imageDest);
            CFRelease(imageDest);
            printf("Content-length: %lu\n", CFDataGetLength(imageData));
            fwrite(CFDataGetBytePtr(imageData), 1, CFDataGetLength(imageData), stdout);
            CFRelease(imageData);
        }
        
        CGImageRelease(bitmapImage);
    }
    
    printf("#EOF\n");
}
