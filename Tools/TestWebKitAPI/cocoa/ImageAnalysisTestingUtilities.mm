/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "ImageAnalysisTestingUtilities.h"

#if HAVE(VK_IMAGE_ANALYSIS)

#import <wtf/BlockPtr.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>

@interface TestVKQuad : NSObject
- (instancetype)initWithTopLeft:(CGPoint)topLeft topRight:(CGPoint)topRight bottomLeft:(CGPoint)bottomLeft bottomRight:(CGPoint)bottomRight;
@property (nonatomic, readonly) CGPoint topLeft;
@property (nonatomic, readonly) CGPoint topRight;
@property (nonatomic, readonly) CGPoint bottomLeft;
@property (nonatomic, readonly) CGPoint bottomRight;
@end

@implementation TestVKQuad

- (instancetype)initWithTopLeft:(CGPoint)topLeft topRight:(CGPoint)topRight bottomLeft:(CGPoint)bottomLeft bottomRight:(CGPoint)bottomRight
{
    if (!(self = [super init]))
        return nil;

    _topLeft = topLeft;
    _topRight = topRight;
    _bottomLeft = bottomLeft;
    _bottomRight = bottomRight;
    return self;
}

@end

@interface TestVKWKTextInfo : NSObject
- (instancetype)initWithString:(NSString *)string quad:(VKQuad *)quad;
@end

@implementation TestVKWKTextInfo {
    RetainPtr<NSString> _string;
    RetainPtr<VKQuad> _quad;
}

- (instancetype)initWithString:(NSString *)string quad:(VKQuad *)quad
{
    if (!(self = [super init]))
        return nil;

    _string = string;
    _quad = quad;
    return self;
}

- (NSString *)string
{
    return _string.get();
}

- (VKQuad *)quad
{
    return _quad.get();
}

@end

@interface TestVKWKLineInfo : TestVKWKTextInfo
- (instancetype)initWithString:(NSString *)string quad:(VKQuad *)quad children:(NSArray<VKWKTextInfo *> *)children;
@end

@implementation TestVKWKLineInfo {
    RetainPtr<NSArray> _children;
}

- (instancetype)initWithString:(NSString *)string quad:(VKQuad *)quad children:(NSArray<VKWKTextInfo *> *)children
{
    if (!(self = [super initWithString:string quad:quad]))
        return nil;

    _children = children;
    return self;
}

- (NSArray<VKWKTextInfo *> *)children
{
    return _children.get();
}

@end

@interface TestVKImageAnalysis : NSObject
- (instancetype)initWithLines:(NSArray<VKWKLineInfo *> *)lines;
#if HAVE(VK_IMAGE_ANALYSIS_FOR_MACHINE_READABLE_CODES)
@property (nonatomic, readonly) UIMenu *mrcMenu;
@property (nonatomic, weak) UIViewController *presentingViewControllerForMrcAction;
@property (nonatomic) CGRect rectForMrcActionInPresentingViewController;
#endif
@end

@implementation TestVKImageAnalysis {
    RetainPtr<NSArray> _lines;
}

- (instancetype)initWithLines:(NSArray<VKWKLineInfo *> *)lines
{
    if (!(self = [super init]))
        return nil;

    _lines = lines;
    return self;
}

- (NSArray<VKWKLineInfo *> *)allLines
{
    return _lines.get();
}

- (BOOL)hasResultsForAnalysisTypes:(VKAnalysisTypes)analysisTypes
{
    // We only simulate text results for the time being.
    return analysisTypes == VKAnalysisTypeText && [_lines count];
}

@end

@interface TestVKImageAnalyzerRequest : NSObject
@property (nonatomic, readonly) CGImageRef image;
@property (nonatomic, readonly) VKImageOrientation orientation;
@property (nonatomic, readonly) VKAnalysisTypes requestType;
@property (nonatomic, copy) NSURL *imageURL;
@property (nonatomic, copy) NSURL *pageURL;
@end

@implementation TestVKImageAnalyzerRequest {
    RetainPtr<CGImageRef> _image;
    VKImageOrientation _orientation;
    VKAnalysisTypes _requestType;
    RetainPtr<NSURL> _imageURL;
    RetainPtr<NSURL> _pageURL;
}

- (instancetype)initWithCGImage:(CGImageRef)image orientation:(VKImageOrientation)orientation requestType:(VKAnalysisTypes)requestType
{
    if (!(self = [super init]))
        return nil;

    _image = image;
    _orientation = orientation;
    _requestType = requestType;
    return self;
}

- (CGImageRef)image
{
    return _image.get();
}

- (NSURL *)imageURL
{
    return _imageURL.get();
}

- (void)setImageURL:(NSURL *)url
{
    _imageURL = adoptNS(url.copy);
}

- (NSURL *)pageURL
{
    return _pageURL.get();
}

- (void)setPageURL:(NSURL *)url
{
    _pageURL = adoptNS(url.copy);
}

@end

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

@interface FakeRemoveBackgroundResult : NSObject
- (instancetype)initWithImage:(CGImageRef)image cropRect:(CGRect)cropRect;
- (CGImageRef)createCGImage;
@property (nonatomic, readonly) CGRect cropRect;
@end

@implementation FakeRemoveBackgroundResult {
    RetainPtr<CGImageRef> _image;
    CGRect _cropRect;
}

- (instancetype)initWithImage:(CGImageRef)image cropRect:(CGRect)cropRect
{
    if (!(self = [super init]))
        return nil;

    _image = image;
    _cropRect = cropRect;
    return self;
}

- (CGImageRef)createCGImage
{
    // VKCRemoveBackgroundResult expects callers to release this CGImage, so we need to leak a +1 retain count.
    return RetainPtr { _image }.leakRef();
}

- (CGRect)cropRect
{
    return _cropRect;
}

@end

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

namespace TestWebKitAPI {

RetainPtr<VKQuad> createQuad(CGPoint topLeft, CGPoint topRight, CGPoint bottomLeft, CGPoint bottomRight)
{
    return adoptNS(static_cast<VKQuad *>([[TestVKQuad alloc] initWithTopLeft:topLeft topRight:topRight bottomLeft:bottomLeft bottomRight:bottomRight]));
}

RetainPtr<VKWKTextInfo> createTextInfo(NSString *string, VKQuad *quad)
{
    return adoptNS(static_cast<VKWKTextInfo *>([[TestVKWKTextInfo alloc] initWithString:string quad:quad]));
}

RetainPtr<VKWKLineInfo> createLineInfo(NSString *string, VKQuad *quad, NSArray<VKWKTextInfo *> *children)
{
    return adoptNS(static_cast<VKWKLineInfo *>([[TestVKWKLineInfo alloc] initWithString:string quad:quad children:children]));
}

RetainPtr<VKImageAnalysis> createImageAnalysis(NSArray<VKWKLineInfo *> *lines)
{
    return adoptNS(static_cast<VKImageAnalysis *>([[TestVKImageAnalysis alloc] initWithLines:lines]));
}

RetainPtr<VKImageAnalysis> createImageAnalysisWithSimpleFixedResults()
{
    auto quad = createQuad(CGPointMake(0, 0), CGPointMake(1, 0), CGPointMake(1, 0.5), CGPointMake(0, 0.5));
    auto text = createTextInfo(@"Foo bar", quad.get());
    auto line = createLineInfo(@"Foo bar", quad.get(), @[ text.get() ]);
    return createImageAnalysis(@[ line.get() ]);
}

RetainPtr<VKImageAnalyzerRequest> createRequest(CGImageRef image, VKImageOrientation orientation, VKAnalysisTypes types)
{
    return adoptNS(static_cast<VKImageAnalyzerRequest *>([[TestVKImageAnalyzerRequest alloc] initWithCGImage:image orientation:orientation requestType:types]));
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

IMP makeRequestHandler(CGImageRef image, CGRect cropRect)
{
    return imp_implementationWithBlock([image = RetainPtr { image }, cropRect](VKCRemoveBackgroundRequestHandler *, VKCRemoveBackgroundRequest *, void (^completion)(VKCRemoveBackgroundResult *, NSError *)) {
        auto result = adoptNS([[FakeRemoveBackgroundResult alloc] initWithImage:image.get() cropRect:cropRect]);
        completion(static_cast<VKCRemoveBackgroundResult *>(result.get()), nil);
    });
}

RemoveBackgroundSwizzler::RemoveBackgroundSwizzler(CGImageRef image, CGRect cropRect)
    : m_removeBackgroundRequestSwizzler { PAL::getVKCRemoveBackgroundRequestHandlerClassSingleton(), @selector(performRequest:completion:), makeRequestHandler(image, cropRect) }
{
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

} // namespace TestWebKitAPI

#endif // HAVE(VK_IMAGE_ANALYSIS)
