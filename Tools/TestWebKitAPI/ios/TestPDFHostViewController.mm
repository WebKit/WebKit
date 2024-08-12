/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "TestPDFHostViewController.h"

#if HAVE(PDFKIT) && PLATFORM(IOS_FAMILY)

#import "ClassMethodSwizzler.h"
#import "PDFKitSPI.h"

@interface TestPDFHostViewController : PDFHostViewController

@end

@implementation TestPDFHostViewController

- (void)setDelegate:(id)delegate
{
}

- (void)setDocumentData:(NSData *)data withScrollView:(UIScrollView *)scrollView
{
}

- (NSInteger)currentPageIndex
{
    return 0;
}

- (NSInteger)pageCount
{
    return 1;
}

- (CGFloat)minimumZoomScale
{
    return 1;
}

- (CGFloat)maximumZoomScale
{
    return 1;
}

- (void)findString:(NSString *)string withOptions:(NSStringCompareOptions)options
{
}

- (void)cancelFindString
{
}

- (void)cancelFindStringWithHighlightsCleared:(BOOL)clearHighlights
{
}

- (void)focusOnSearchResultAtIndex:(NSUInteger)searchIndex
{
}

- (void)clearSearchHighlights
{
}

- (void)goToPageIndex:(NSInteger)pageIndex
{
}

- (void)updatePDFViewLayout
{
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    return NO;
}

- (UIView *)pageNumberIndicator
{
    return [[UIView new] autorelease];
}

- (void)snapshotViewRect:(CGRect)rect snapshotWidth:(NSNumber *)width afterScreenUpdates:(BOOL)afterScreenUpdates withResult:(void (^)(UIImage *image))completion
{
}

- (void)beginPDFViewRotation
{
}

- (void)endPDFViewRotation
{
}

@end

namespace TestWebKitAPI {

static void swizzledCreateHostViewForExtensionIdentifier(id, SEL, void (^callback)(PDFHostViewController *), NSString *)
{
    callback([[TestPDFHostViewController new] autorelease]);
}

std::unique_ptr<ClassMethodSwizzler> createPDFHostViewControllerSwizzler()
{
    return WTF::makeUnique<ClassMethodSwizzler>([PDFHostViewController class], @selector(createHostView:forExtensionIdentifier:), reinterpret_cast<IMP>(swizzledCreateHostViewForExtensionIdentifier));
}

} // namespace TestWebKitAPI

#endif // HAVE(PDFKIT) && PLATFORM(IOS_FAMILY)
