/*
 * Copyright (C) 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "config.h"
#import "PixelDumpSupport.h"

#import "DumpRenderTree.h"
#import "PixelDumpSupportCG.h"

#define COMMON_DIGEST_FOR_OPENSSL
#import <CommonCrypto/CommonDigest.h>
#import <MobileCoreServices/UTCoreTypes.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <UIKit/UIView_Private.h>
#import <UIKit/UIWebBrowserView.h>
#import <WebKit/WebCoreThread.h>

#import <wtf/RefPtr.h>

extern UIWebBrowserView *gWebBrowserView;

PassRefPtr<BitmapContext> createBitmapContextFromWebView(bool onscreen, bool incrementalRepaint, bool sweepHorizontally,
 bool drawSelectionRect)
{
    // FIXME: Implement; see PixelDumpSupportMac.mm.
    return 0;
}

void dumpWebViewAsPixelsAndCompareWithExpected(const std::string& expectedHash)
{
    // TODO: <rdar://problem/6558366> DumpRenderTree: Investigate testRepaintSweepHorizontally and dumpSelectionRect
    
    // Take snapshot
    WebThreadLock();
    [gWebBrowserView setNeedsDisplay];
    [gWebBrowserView layoutTilesNow];
    CGImageRef snapshot = [gWebBrowserView createSnapshotWithRect:[[mainFrame webView] frame]];
    NSData *pngData = UIImagePNGRepresentation([UIImage imageWithCGImage:snapshot]);
    
    // Hash the PNG data
    char actualHash[33];
    unsigned char result[CC_MD5_DIGEST_LENGTH];
    CC_MD5([pngData bytes], [pngData length], result);
    actualHash[0] = '\0';
    for (int i = 0; i < 16; i++)
        snprintf(actualHash, 33, "%s%02x", actualHash, result[i]);
    printf("\nActualHash: %s\n", actualHash);
    
    // Print the image
    printf("Content-Type: image/png\n");
    printf("Content-Length: %lu\n", (unsigned long)[pngData length]);
    fwrite([pngData bytes], 1, [pngData length], stdout);
    CGImageRelease(snapshot);
}
