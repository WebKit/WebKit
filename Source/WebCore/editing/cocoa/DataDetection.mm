/*
 * Copyright (C) 2014 Apple, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DataDetection.h"

#import "Attr.h"
#import "CSSStyleDeclaration.h"
#import "DataDetectorsSPI.h"
#import "FrameView.h"
#import "HTMLAnchorElement.h"
#import "HTMLTextFormControlElement.h"
#import "HitTestResult.h"
#import "Node.h"
#import "NodeList.h"
#import "NodeTraversal.h"
#import "Range.h"
#import "RenderObject.h"
#import "Text.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleUnits.h"
#import "htmlediting.h"

namespace WebCore {

#if PLATFORM(MAC)

static RetainPtr<DDActionContext> detectItemAtPositionWithRange(VisiblePosition position, RefPtr<Range> contextRange, FloatRect& detectedDataBoundingBox, RefPtr<Range>& detectedDataRange)
{
    String fullPlainTextString = plainText(contextRange.get());
    int hitLocation = TextIterator::rangeLength(makeRange(contextRange->startPosition(), position).get());

    RetainPtr<DDScannerRef> scanner = adoptCF(DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    RetainPtr<DDScanQueryRef> scanQuery = adoptCF(DDScanQueryCreateFromString(kCFAllocatorDefault, fullPlainTextString.createCFString().get(), CFRangeMake(0, fullPlainTextString.length())));

    if (!DDScannerScanQuery(scanner.get(), scanQuery.get()))
        return nullptr;

    RetainPtr<CFArrayRef> results = adoptCF(DDScannerCopyResultsWithOptions(scanner.get(), DDScannerCopyResultsOptionsNoOverlap));

    // Find the DDResultRef that intersects the hitTestResult's VisiblePosition.
    DDResultRef mainResult = nullptr;
    RefPtr<Range> mainResultRange;
    CFIndex resultCount = CFArrayGetCount(results.get());
    for (CFIndex i = 0; i < resultCount; i++) {
        DDResultRef result = (DDResultRef)CFArrayGetValueAtIndex(results.get(), i);
        CFRange resultRangeInContext = DDResultGetRange(result);
        if (hitLocation >= resultRangeInContext.location && (hitLocation - resultRangeInContext.location) < resultRangeInContext.length) {
            mainResult = result;
            mainResultRange = TextIterator::subrange(contextRange.get(), resultRangeInContext.location, resultRangeInContext.length);
            break;
        }
    }

    if (!mainResult)
        return nullptr;

    RetainPtr<DDActionContext> actionContext = adoptNS([allocDDActionContextInstance() init]);
    [actionContext setAllResults:@[ (id)mainResult ]];
    [actionContext setMainResult:mainResult];

    Vector<FloatQuad> quads;
    mainResultRange->absoluteTextQuads(quads);
    detectedDataBoundingBox = FloatRect();
    FrameView* frameView = mainResultRange->ownerDocument().view();
    for (const auto& quad : quads)
        detectedDataBoundingBox.unite(frameView->contentsToWindow(quad.enclosingBoundingBox()));
    
    detectedDataRange = mainResultRange;
    
    return actionContext;
}

RetainPtr<DDActionContext> DataDetection::detectItemAroundHitTestResult(const HitTestResult& hitTestResult, FloatRect& detectedDataBoundingBox, RefPtr<Range>& detectedDataRange)
{
    if (!DataDetectorsLibrary())
        return nullptr;

    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return nullptr;
    auto renderer = node->renderer();
    if (!renderer)
        return nullptr;

    VisiblePosition position;
    RefPtr<Range> contextRange;

    if (!is<HTMLTextFormControlElement>(*node)) {
        position = renderer->positionForPoint(hitTestResult.localPoint(), nullptr);
        if (position.isNull())
            position = firstPositionInOrBeforeNode(node);

        contextRange = rangeExpandedAroundPositionByCharacters(position, 250);
        if (!contextRange)
            return nullptr;
    } else {
        Frame* frame = node->document().frame();
        if (!frame)
            return nullptr;

        IntPoint framePoint = hitTestResult.roundedPointInInnerNodeFrame();
        if (!frame->rangeForPoint(framePoint))
            return nullptr;

        VisiblePosition position = frame->visiblePositionForPoint(framePoint);
        if (position.isNull())
            return nullptr;

        contextRange = enclosingTextUnitOfGranularity(position, LineGranularity, DirectionForward);
        if (!contextRange)
            return nullptr;
    }

    return detectItemAtPositionWithRange(position, contextRange, detectedDataBoundingBox, detectedDataRange);
}
#endif // PLATFORM(MAC)

void DataDetection::detectContentInRange(RefPtr<Range>&, DataDetectorTypes)
{
}

} // namespace WebCore
