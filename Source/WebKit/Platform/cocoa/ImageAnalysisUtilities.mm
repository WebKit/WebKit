/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#import "ImageAnalysisUtilities.h"

#if ENABLE(IMAGE_ANALYSIS) || HAVE(VISION)

#import "CocoaImage.h"
#import "Logging.h"
#import "TransactionID.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/TextRecognitionResult.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#import <pal/cocoa/VisionSoftLink.h>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <wtf/RobinHoodHashSet.h>
#import <wtf/WorkQueue.h>

#define CRLayoutDirectionTopToBottom 3

namespace WebKit {
using namespace WebCore;

#if ENABLE(IMAGE_ANALYSIS)

RetainPtr<CocoaImageAnalyzer> createImageAnalyzer()
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    return adoptNS([PAL::allocVKCImageAnalyzerInstance() init]);
#else
    return adoptNS([PAL::allocVKImageAnalyzerInstance() init]);
#endif
}

RetainPtr<CocoaImageAnalyzerRequest> createImageAnalyzerRequest(CGImageRef image, VKAnalysisTypes types)
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    return adoptNS([(PAL::allocVKCImageAnalyzerRequestInstance()) initWithCGImage:image orientation:VKImageOrientationUp requestType:types]);
#else
    return adoptNS([PAL::allocVKImageAnalyzerRequestInstance() initWithCGImage:image orientation:VKImageOrientationUp requestType:types]);
#endif
}

static FloatQuad floatQuad(VKQuad *quad)
{
    return { quad.topLeft, quad.topRight, quad.bottomRight, quad.bottomLeft };
}

static Vector<FloatQuad> floatQuads(NSArray<VKQuad *> *vkQuads)
{
    return Vector<FloatQuad>(vkQuads.count, [vkQuads](size_t i) {
        return floatQuad(vkQuads[i]);
    });
}

TextRecognitionResult makeTextRecognitionResult(CocoaImageAnalysis *analysis)
{
    NSArray<VKWKLineInfo *> *allLines = analysis.allLines;
    TextRecognitionResult result;
    result.lines.reserveInitialCapacity(allLines.count);

    bool isFirstLine = true;
    size_t nextLineIndex = 1;
    for (VKWKLineInfo *line in allLines) {
        Vector<TextRecognitionWordData> children;
        NSArray<VKWKTextInfo *> *vkChildren = line.children;
        children.reserveInitialCapacity(vkChildren.count);

        String lineText = line.string;
        unsigned searchLocation = 0;
        for (VKWKTextInfo *child in vkChildren) {
            if (searchLocation >= lineText.length()) {
                ASSERT_NOT_REACHED();
                continue;
            }

            String childText = child.string;
            auto matchLocation = lineText.find(childText, searchLocation);
            if (matchLocation == notFound) {
                ASSERT_NOT_REACHED();
                continue;
            }

            bool hasLeadingWhitespace = ([&] {
                if (matchLocation == searchLocation)
                    return !isFirstLine && !searchLocation;

                auto textBeforeMatch = StringView(lineText).substring(searchLocation, matchLocation - searchLocation);
                return !textBeforeMatch.isEmpty() && deprecatedIsSpaceOrNewline(textBeforeMatch[0]);
            })();

            searchLocation = matchLocation + childText.length();
            children.append({ WTFMove(childText), floatQuad(child.quad), hasLeadingWhitespace });
        }
        VKWKLineInfo *nextLine = nextLineIndex < allLines.count ? allLines[nextLineIndex] : nil;
        // The `shouldWrap` property indicates whether or not a line should wrap, relative to the previous line.
        bool hasTrailingNewline = nextLine && (![nextLine respondsToSelector:@selector(shouldWrap)] || ![nextLine shouldWrap]);
        bool isVertical = [line respondsToSelector:@selector(layoutDirection)] && [line layoutDirection] == CRLayoutDirectionTopToBottom;
        result.lines.append({ floatQuad(line.quad), WTFMove(children), hasTrailingNewline, isVertical });
        isFirstLine = false;
        nextLineIndex++;
    }

#if ENABLE(DATA_DETECTION)
    if ([analysis respondsToSelector:@selector(textDataDetectors)]) {
        auto dataDetectors = RetainPtr { analysis.textDataDetectors };
        result.dataDetectors.reserveInitialCapacity([dataDetectors count]);
        for (VKWKDataDetectorInfo *info in dataDetectors.get())
            result.dataDetectors.append({ info.result, floatQuads(info.boundingQuads) });
    }
#endif // ENABLE(DATA_DETECTION)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    if ([analysis isKindOfClass:PAL::getVKCImageAnalysisClass()])
        result.imageAnalysisData = TextRecognitionResult::encodeVKCImageAnalysis(analysis);
#endif

    return result;
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

static String languageCodeForLocale(NSString *localeIdentifier)
{
    return [NSLocale localeWithLocaleIdentifier:localeIdentifier].languageCode;
}

#endif

bool languageIdentifierSupportsLiveText(NSString *languageIdentifier)
{
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    auto languageCode = languageCodeForLocale(languageIdentifier);
    if (languageCode.isEmpty())
        return true;

    static NeverDestroyed<MemoryCompactRobinHoodHashSet<String>> supportedLanguages = [] {
        MemoryCompactRobinHoodHashSet<String> set;
        for (NSString *identifier in [PAL::getVKCImageAnalyzerClass() supportedRecognitionLanguages]) {
            if (auto code = languageCodeForLocale(identifier); !code.isEmpty())
                set.add(WTFMove(code));
        }
        return set;
    }();
    return supportedLanguages->contains(languageCode);
#else
    UNUSED_PARAM(languageIdentifier);
    return true;
#endif
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

static TextRecognitionResult makeTextRecognitionResult(VKCImageAnalysisTranslation *translation, TransactionID transactionID)
{
    TextRecognitionResult result;

    NSArray<VKCTranslatedParagraph *> *paragraphs = translation.paragraphs;
    result.blocks.reserveInitialCapacity(paragraphs.count);

    for (VKCTranslatedParagraph *paragraph in paragraphs) {
        if (!paragraph.text.length) {
            RELEASE_LOG(Translation, "[#%{public}s] Skipping empty translation paragraph", transactionID.loggingString().utf8().data());
            continue;
        }

        if (paragraph.isPassthrough)
            continue;

        auto quad = floatQuad(paragraph.quad);
        if (quad.p2().x() > quad.p4().x() && quad.p2().y() > quad.p4().y()
            && quad.p1().x() < quad.p3().x() && quad.p1().y() > quad.p3().y()) {
            // The entire quad is flipped about the x-axis. Maintain valid positions for each of the quad points
            // by flipping each quad point, such that the top left and bottom right positions are positioned correctly.
            quad.setP1({ quad.p1().x(), 1 - quad.p1().y() });
            quad.setP2({ quad.p2().x(), 1 - quad.p2().y() });
            quad.setP3({ quad.p3().x(), 1 - quad.p3().y() });
            quad.setP4({ quad.p4().x(), 1 - quad.p4().y() });
        }
        result.blocks.append({ paragraph.text, WTFMove(quad) });
    }

    return result;
}

static bool shouldLogFullImageTranslationResults()
{
    static std::once_flag onceFlag;
    static bool shouldLog = false;
    std::call_once(onceFlag, [&] {
        shouldLog = [NSUserDefaults.standardUserDefaults boolForKey:@"WebKitLogFullImageTranslationResults"];
    });
    return shouldLog;
}

void requestVisualTranslation(CocoaImageAnalyzer *analyzer, NSURL *imageURL, const String& sourceLocale, const String& targetLocale, CGImageRef image, CompletionHandler<void(TextRecognitionResult&&)>&& completion)
{
    auto startTime = MonotonicTime::now();
    static auto imageAnalysisRequestID = TransactionID::generateMonotonic();
    auto currentRequestID = imageAnalysisRequestID.increment();
    if (shouldLogFullImageTranslationResults())
        RELEASE_LOG(Translation, "[#%{public}s] Image translation started for %{private}@", currentRequestID.loggingString().utf8().data(), imageURL);
    else
        RELEASE_LOG(Translation, "[#%{public}s] Image translation started", currentRequestID.loggingString().utf8().data());
    auto request = createImageAnalyzerRequest(image, VKAnalysisTypeText);
    [analyzer processRequest:request.get() progressHandler:nil completionHandler:makeBlockPtr([completion = WTFMove(completion), sourceLocale, targetLocale, currentRequestID, startTime] (CocoaImageAnalysis *analysis, NSError *analysisError) mutable {
        callOnMainRunLoop([completion = WTFMove(completion), analysis = RetainPtr { analysis }, analysisError = RetainPtr { analysisError }, sourceLocale, targetLocale, currentRequestID, startTime] () mutable {
            auto imageAnalysisDelay = MonotonicTime::now() - startTime;
            if (!analysis) {
                RELEASE_LOG(Translation, "[#%{public}s] Image translation failed in %.3f sec. (error: %{public}@)", currentRequestID.loggingString().utf8().data(), imageAnalysisDelay.seconds(), analysisError.get());
                return completion({ });
            }

            if (![analysis hasResultsForAnalysisTypes:VKAnalysisTypeText]) {
                RELEASE_LOG(Translation, "[#%{public}s] Image translation completed in %.3f sec. (no text)", currentRequestID.loggingString().utf8().data(), imageAnalysisDelay.seconds());
                return completion({ });
            }

            auto allLines = [analysis allLines];
            if (shouldLogFullImageTranslationResults()) {
                StringBuilder stringToLog;
                bool firstLine = true;
                for (VKWKLineInfo *info in allLines) {
                    if (!firstLine)
                        stringToLog.append("\\n"_s);
                    stringToLog.append(String { info.string });
                    firstLine = false;
                }
                RELEASE_LOG(Translation, "[#%{public}s] Image translation recognized text in %.3f sec. (line count: %zu): \"%{private}s\"", currentRequestID.loggingString().utf8().data(), imageAnalysisDelay.seconds(), allLines.count, stringToLog.toString().utf8().data());
            } else
                RELEASE_LOG(Translation, "[#%{public}s] Image translation recognized text in %.3f sec. (line count: %zu)", currentRequestID.loggingString().utf8().data(), imageAnalysisDelay.seconds(), allLines.count);

            auto translationStartTime = MonotonicTime::now();
            auto completionBlock = makeBlockPtr([completion = WTFMove(completion), currentRequestID, translationStartTime](VKCImageAnalysisTranslation *translation, NSError *error) mutable {
                auto translationDelay = MonotonicTime::now() - translationStartTime;
                if (error) {
                    RELEASE_LOG(Translation, "[#%{public}s] Image translation failed in %.3f sec. (error: %{public}@)", currentRequestID.loggingString().utf8().data(), translationDelay.seconds(), error);
                    return completion({ });
                }

                if (shouldLogFullImageTranslationResults()) {
                    StringBuilder stringToLog;
                    bool firstLine = true;
                    for (VKCTranslatedParagraph *paragraph in translation.paragraphs) {
                        if (!firstLine)
                            stringToLog.append("\\n"_s);
                        stringToLog.append(String { paragraph.text });
                        firstLine = false;
                    }
                    RELEASE_LOG(Translation, "[#%{public}s] Image translation completed in %.3f sec. (paragraph count: %zu): \"%{private}s\"", currentRequestID.loggingString().utf8().data(), translationDelay.seconds(), translation.paragraphs.count, stringToLog.toString().utf8().data());
                } else
                    RELEASE_LOG(Translation, "[#%{public}s] Image translation completed in %.3f sec. (paragraph count: %zu)", currentRequestID.loggingString().utf8().data(), translationDelay.seconds(), translation.paragraphs.count);

                completion(makeTextRecognitionResult(translation, currentRequestID));
            });

            if ([analysis respondsToSelector:@selector(translateFrom:to:withCompletion:)])
                [analysis translateFrom:sourceLocale.createNSString().get() to:targetLocale.createNSString().get() withCompletion:completionBlock.get()];
            else
                [analysis translateTo:targetLocale.createNSString().get() withCompletion:completionBlock.get()];
        });
    }).get()];
}

void requestBackgroundRemoval(CGImageRef image, CompletionHandler<void(CGImageRef)>&& completion)
{
    if (!PAL::canLoad_VisionKitCore_vk_cgImageRemoveBackground()) {
        completion(nullptr);
        return;
    }

    // FIXME (rdar://88834023): We should find a way to avoid this extra transcoding.
    auto tiffData = transcode(image, (__bridge CFStringRef)UTTypeTIFF.identifier);
    if (![tiffData length]) {
        completion(nullptr);
        return;
    }

    auto transcodedImageSource = adoptCF(CGImageSourceCreateWithData((__bridge CFDataRef)tiffData.get(), nullptr));
    auto transcodedImage = adoptCF(CGImageSourceCreateImageAtIndex(transcodedImageSource.get(), 0, nullptr));
    if (!transcodedImage) {
        completion(nullptr);
        return;
    }

    auto sourceImageSize = CGSizeMake(CGImageGetWidth(transcodedImage.get()), CGImageGetHeight(transcodedImage.get()));
    if (!sourceImageSize.width || !sourceImageSize.height) {
        completion(nullptr);
        return;
    }

    auto startTime = MonotonicTime::now();
    auto completionBlock = makeBlockPtr([completion = WTFMove(completion), startTime](CGImageRef result, NSError *error) mutable {
        if (error)
            RELEASE_LOG(ImageAnalysis, "Remove background failed with error: %@", error);

        callOnMainRunLoop([protectedResult = RetainPtr { result }, completion = WTFMove(completion), startTime]() mutable {
            RELEASE_LOG(ImageAnalysis, "Remove background finished in %.0f ms (found subject? %d)", (MonotonicTime::now() - startTime).milliseconds(), !!protectedResult);
            completion(protectedResult.get());
        });
    });

    if (PAL::canLoad_VisionKitCore_vk_cgImageRemoveBackgroundWithDownsizing()) {
        PAL::softLinkVisionKitCorevk_cgImageRemoveBackgroundWithDownsizing(transcodedImage.get(), YES, YES, completionBlock.get());
        return;
    }

    PAL::softLinkVisionKitCorevk_cgImageRemoveBackground(transcodedImage.get(), YES, [completionBlock = WTFMove(completionBlock)](CGImageRef image, CGRect, NSError *error) mutable {
        completionBlock(image, error);
    });
}

void prepareImageAnalysisForOverlayView(PlatformImageAnalysisObject *interactionOrView)
{
    interactionOrView.activeInteractionTypes = VKImageAnalysisInteractionTypeTextSelection | VKImageAnalysisInteractionTypeDataDetectors;
    interactionOrView.wantsAutomaticContentsRectCalculation = NO;
    interactionOrView.actionInfoLiveTextButtonDisabled = NO;
    interactionOrView.actionInfoQuickActionsDisabled = NO;
    [interactionOrView setActionInfoViewHidden:NO animated:YES];
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

bool isLiveTextAvailableAndEnabled()
{
    return PAL::isVisionKitCoreFrameworkAvailable();
}

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

std::pair<RetainPtr<NSData>, RetainPtr<CFStringRef>> imageDataForRemoveBackground(CGImageRef image, const String& sourceMIMEType)
{
    static NeverDestroyed allowedMIMETypesForTranscoding = [] {
        static constexpr std::array types { "image/png"_s, "image/tiff"_s, "image/gif"_s, "image/bmp"_s };
        MemoryCompactLookupOnlyRobinHoodHashSet<String> set;
        set.reserveInitialCapacity(std::size(types));
        for (auto type : types)
            set.add(type);
        return set;
    }();

    if (!sourceMIMEType.isEmpty() && allowedMIMETypesForTranscoding->contains(sourceMIMEType))
        return transcodeWithPreferredMIMEType(image, sourceMIMEType.createCFString().get());

    return transcodeWithPreferredMIMEType(image, CFSTR("image/png"));
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#endif // ENABLE(IMAGE_ANALYSIS)

#if HAVE(VISION)

static RetainPtr<CGImageRef> imageFilledWithWhiteBackground(CGImageRef image)
{
    CGRect imageRect = CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image));

    RetainPtr colorSpace = CGImageGetColorSpace(image);
    if (!CGColorSpaceSupportsOutput(colorSpace.get()))
        colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));

    auto context = adoptCF(CGBitmapContextCreate(nil, CGRectGetWidth(imageRect), CGRectGetHeight(imageRect), 8, 4 * CGRectGetWidth(imageRect), colorSpace.get(), kCGImageAlphaPremultipliedLast));
    CGContextSetFillColorWithColor(context.get(), cachedCGColor(WebCore::Color::white).get());
    CGContextFillRect(context.get(), imageRect);
    CGContextDrawImage(context.get(), imageRect, image);

    return adoptCF(CGBitmapContextCreateImage(context.get()));
}

void requestPayloadForQRCode(CGImageRef image, CompletionHandler<void(NSString *)>&& completion)
{
    ASSERT(RunLoop::isMain());

    if (!image || !PAL::isVisionFrameworkAvailable())
        return completion(nil);

    auto queue = WorkQueue::create("com.apple.WebKit.ImageAnalysisUtilities.QRCodePayloadRequest"_s);
    queue->dispatch([image = retainPtr(image), completion = WTFMove(completion)]() mutable {
        auto adjustedImage = imageFilledWithWhiteBackground(image.get());

        auto callCompletionOnMainRunLoopWithResult = [completion = WTFMove(completion)](NSString *result) mutable {
            callOnMainRunLoop([completion = WTFMove(completion), result = retainPtr(result)]() mutable {
                if (!completion)
                    return;
                completion(result.get());
            });
        };

        auto completionHandler = makeBlockPtr([callCompletionOnMainRunLoopWithResult = WTFMove(callCompletionOnMainRunLoopWithResult)](VNRequest *request, NSError *error) mutable {
            if (error) {
                callCompletionOnMainRunLoopWithResult(nil);
                return;
            }

            for (VNBarcodeObservation *result in request.results) {
                if (![result.symbology isEqualToString:PAL::get_Vision_VNBarcodeSymbologyQR()])
                    continue;

                callCompletionOnMainRunLoopWithResult(result.payloadStringValue);
                return;
            }

            callCompletionOnMainRunLoopWithResult(nil);
        });

        auto request = adoptNS([PAL::allocVNDetectBarcodesRequestInstance() initWithCompletionHandler:completionHandler.get()]);
        [request setSymbologies:@[ PAL::get_Vision_VNBarcodeSymbologyQR() ]];

        NSError *error = nil;
        auto handler = adoptNS([PAL::allocVNImageRequestHandlerInstance() initWithCGImage:adjustedImage.get() options:@{ }]);
        [handler performRequests:@[ request.get() ] error:&error];

        if (error)
            completionHandler(nil, error);
    });
}

#endif // HAVE(VISION)

} // namespace WebKit

#endif // ENABLE(IMAGE_ANALYSIS) || HAVE(VISION)
