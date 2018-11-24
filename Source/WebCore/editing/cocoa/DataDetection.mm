/*
 * Copyright (C) 2014-2017 Apple, Inc.  All rights reserved.
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

#if ENABLE(DATA_DETECTION)

#import "Attr.h"
#import "CSSStyleDeclaration.h"
#import "Editing.h"
#import "ElementAncestorIterator.h"
#import "ElementTraversal.h"
#import "FrameView.h"
#import "HTMLAnchorElement.h"
#import "HTMLNames.h"
#import "HTMLTextFormControlElement.h"
#import "HitTestResult.h"
#import "Node.h"
#import "NodeList.h"
#import "NodeTraversal.h"
#import "Range.h"
#import "RenderObject.h"
#import "StyleProperties.h"
#import "Text.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleUnits.h"
#import <pal/spi/ios/DataDetectorsUISPI.h>
#import <pal/spi/mac/DataDetectorsSPI.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/text/StringBuilder.h>

#import "DataDetectorsCoreSoftLink.h"

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
template <>
struct WTF::CFTypeTrait<DDResultRef> {
    static inline CFTypeID typeID(void) { return DDResultGetCFTypeID(); }
};
#endif

namespace WebCore {

using namespace HTMLNames;

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
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
        DDResultRef result = checked_cf_cast<DDResultRef>(CFArrayGetValueAtIndex(results.get(), i));
#else
        DDResultRef result = static_cast<DDResultRef>(const_cast<CF_BRIDGED_TYPE(id) void*>(CFArrayGetValueAtIndex(results.get(), i)));
#endif
        CFRange resultRangeInContext = DDResultGetRange(result);
        if (hitLocation >= resultRangeInContext.location && (hitLocation - resultRangeInContext.location) < resultRangeInContext.length) {
            mainResult = result;
            mainResultRange = TextIterator::subrange(*contextRange, resultRangeInContext.location, resultRangeInContext.length);
            break;
        }
    }

    if (!mainResult)
        return nullptr;

    RetainPtr<DDActionContext> actionContext = adoptNS([allocDDActionContextInstance() init]);
    [actionContext setAllResults:@[ (__bridge id)mainResult ]];
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

#if PLATFORM(IOS_FAMILY)

bool DataDetection::canBePresentedByDataDetectors(const URL& url)
{
    return [softLink_DataDetectorsCore_DDURLTapAndHoldSchemes() containsObject:(NSString *)url.protocol().toStringWithoutCopying().convertToASCIILowercase()];
}

bool DataDetection::isDataDetectorLink(Element& element)
{
    if (!is<HTMLAnchorElement>(element))
        return false;

    return canBePresentedByDataDetectors(downcast<HTMLAnchorElement>(element).href());
}

bool DataDetection::requiresExtendedContext(Element& element)
{
    return equalIgnoringASCIICase(element.attributeWithoutSynchronization(x_apple_data_detectors_typeAttr), "calendar-event");
}

String DataDetection::dataDetectorIdentifier(Element& element)
{
    return element.attributeWithoutSynchronization(x_apple_data_detectors_resultAttr);
}

bool DataDetection::shouldCancelDefaultAction(Element& element)
{
    if (!isDataDetectorLink(element))
        return false;
    
    if (softLink_DataDetectorsCore_DDShouldImmediatelyShowActionSheetForURL(downcast<HTMLAnchorElement>(element).href()))
        return true;
    
    const AtomicString& resultAttribute = element.attributeWithoutSynchronization(x_apple_data_detectors_resultAttr);
    if (resultAttribute.isEmpty())
        return false;
    NSArray *results = element.document().frame()->dataDetectionResults();
    if (!results)
        return false;
    Vector<String> resultIndices = resultAttribute.string().split('/');
    DDResultRef result = [[results objectAtIndex:resultIndices[0].toInt()] coreResult];
    // Handle the case of a signature block, where we need to check the correct subresult.
    for (size_t i = 1; i < resultIndices.size(); i++) {
        results = (NSArray *)softLink_DataDetectorsCore_DDResultGetSubResults(result);
        result = (DDResultRef)[results objectAtIndex:resultIndices[i].toInt()];
    }
    return softLink_DataDetectorsCore_DDShouldImmediatelyShowActionSheetForResult(result);
}

static BOOL resultIsURL(DDResultRef result)
{
    if (!result)
        return NO;
    
    static NSSet *urlTypes = [[NSSet setWithObjects: (NSString *)get_DataDetectorsCore_DDBinderHttpURLKey(), (NSString *)get_DataDetectorsCore_DDBinderWebURLKey(), (NSString *)get_DataDetectorsCore_DDBinderMailURLKey(), (NSString *)get_DataDetectorsCore_DDBinderGenericURLKey(), (NSString *)get_DataDetectorsCore_DDBinderEmailKey(), nil] retain];
    return [urlTypes containsObject:(NSString *)softLink_DataDetectorsCore_DDResultGetType(result)];
}

static NSString *constructURLStringForResult(DDResultRef currentResult, NSString *resultIdentifier, NSDate *referenceDate, NSTimeZone *referenceTimeZone, DataDetectorTypes detectionTypes)
{
    if (!softLink_DataDetectorsCore_DDResultHasProperties(currentResult, DDResultPropertyPassiveDisplay))
        return nil;
    
    DDURLifierPhoneNumberDetectionTypes phoneTypes = (detectionTypes & DataDetectorTypePhoneNumber) ? DDURLifierPhoneNumberDetectionRegular : DDURLifierPhoneNumberDetectionNone;
    DDResultCategory category = softLink_DataDetectorsCore_DDResultGetCategory(currentResult);
    CFStringRef type = softLink_DataDetectorsCore_DDResultGetType(currentResult);
    
    if (((detectionTypes & DataDetectorTypeAddress) && (DDResultCategoryAddress == category))
        || ((detectionTypes & DataDetectorTypeTrackingNumber) && (CFStringCompare(get_DataDetectorsCore_DDBinderTrackingNumberKey(), type, 0) == kCFCompareEqualTo))
        || ((detectionTypes & DataDetectorTypeFlightNumber) && (CFStringCompare(get_DataDetectorsCore_DDBinderFlightInformationKey(), type, 0) == kCFCompareEqualTo))
        || ((detectionTypes & DataDetectorTypeLookupSuggestion) && (CFStringCompare(get_DataDetectorsCore_DDBinderParsecSourceKey(), type, 0) == kCFCompareEqualTo))
        || ((detectionTypes & DataDetectorTypePhoneNumber) && (DDResultCategoryPhoneNumber == category))
        || ((detectionTypes & DataDetectorTypeLink) && resultIsURL(currentResult))) {
        
        return softLink_DataDetectorsCore_DDURLStringForResult(currentResult, resultIdentifier, phoneTypes, referenceDate, referenceTimeZone);
    }
    if ((detectionTypes & DataDetectorTypeCalendarEvent) && (DDResultCategoryCalendarEvent == category)) {
        if (!softLink_DataDetectorsCore_DDResultIsPastDate(currentResult, (CFDateRef)referenceDate, (CFTimeZoneRef)referenceTimeZone))
            return softLink_DataDetectorsCore_DDURLStringForResult(currentResult, resultIdentifier, phoneTypes, referenceDate, referenceTimeZone);
    }
    return nil;
}

static void removeResultLinksFromAnchor(Element& element)
{
    // Perform a depth-first search for anchor nodes, which have the data detectors attribute set to true,
    // take their children and insert them before the anchor, and then remove the anchor.

    // Note that this is not using ElementChildIterator because we potentially prepend children as we iterate over them.
    for (auto* child = ElementTraversal::firstChild(element); child; child = ElementTraversal::nextSibling(*child))
        removeResultLinksFromAnchor(*child);

    auto* elementParent = element.parentElement();
    if (!elementParent)
        return;
    
    bool elementIsDDAnchor = is<HTMLAnchorElement>(element) && equalIgnoringASCIICase(element.attributeWithoutSynchronization(x_apple_data_detectorsAttr), "true");
    if (!elementIsDDAnchor)
        return;

    // Iterate over the children and move them all onto the same level as this anchor. Remove the anchor afterwards.
    while (auto* child = element.firstChild())
        elementParent->insertBefore(*child, &element);

    elementParent->removeChild(element);
}

static bool searchForLinkRemovingExistingDDLinks(Node& startNode, Node& endNode, bool& didModifyDOM)
{
    didModifyDOM = false;
    for (Node* node = &startNode; node; node = NodeTraversal::next(*node)) {
        if (is<HTMLAnchorElement>(*node)) {
            auto& anchor = downcast<HTMLAnchorElement>(*node);
            if (!equalIgnoringASCIICase(anchor.attributeWithoutSynchronization(x_apple_data_detectorsAttr), "true"))
                return true;
            removeResultLinksFromAnchor(anchor);
            didModifyDOM = true;
        }
        
        if (node == &endNode) {
            // If we found the end node and no link, return false unless an ancestor node is a link.
            // The only ancestors not tested at this point are in the direct line from self's parent to the top.
            for (auto& anchor : ancestorsOfType<HTMLAnchorElement>(startNode)) {
                if (!equalIgnoringASCIICase(anchor.attributeWithoutSynchronization(x_apple_data_detectorsAttr), "true"))
                    return true;
                removeResultLinksFromAnchor(anchor);
                didModifyDOM = true;
            }
            return false;
        }
    }
    return false;
}

static NSString *dataDetectorTypeForCategory(DDResultCategory category)
{
    switch (category) {
    case DDResultCategoryPhoneNumber:
        return @"telephone";
    case DDResultCategoryLink:
        return @"link";
    case DDResultCategoryAddress:
        return @"address";
    case DDResultCategoryCalendarEvent:
        return @"calendar-event";
    case DDResultCategoryMisc:
        return @"misc";
    default:
        return @"";
    }
}

static String dataDetectorStringForPath(NSIndexPath *path)
{
    NSUInteger length = path.length;
    
    switch (length) {
    case 0:
        return { };
    case 1:
        return String::number((unsigned long)[path indexAtPosition:0]);
    case 2: {
        StringBuilder stringBuilder;
        stringBuilder.appendNumber((unsigned long)[path indexAtPosition:0]);
        stringBuilder.append('/');
        stringBuilder.appendNumber((unsigned long)[path indexAtPosition:1]);
        return stringBuilder.toString();
    }
    default: {
        StringBuilder stringBuilder;
        stringBuilder.appendNumber((unsigned long)[path indexAtPosition:0]);
        for (NSUInteger i = 1 ; i < length ; i++) {
            stringBuilder.append('/');
            stringBuilder.appendNumber((unsigned long)[path indexAtPosition:i]);
        }

        return stringBuilder.toString();
    }
    }
}

static void buildQuery(DDScanQueryRef scanQuery, Range* contextRange)
{
    // Once we're over this number of fragments, stop at the first hard break.
    const CFIndex maxFragmentWithHardBreak = 1000;
    // Once we're over this number of fragments, we stop at the line.
    const CFIndex maxFragmentWithLinebreak = 5000;
    // Once we're over this number of fragments, we stop at the space.
    const CFIndex maxFragmentSpace = 10000;

    CFCharacterSetRef whiteSpacesSet = CFCharacterSetGetPredefined(kCFCharacterSetWhitespaceAndNewline);
    CFCharacterSetRef newLinesSet = CFCharacterSetGetPredefined(kCFCharacterSetNewline);
    
    RefPtr<Range> endRange;
    CFIndex iteratorCount = 0;
    CFIndex fragmentCount = 0;
    
    // Build the scan query adding separators.
    // For each fragment the iterator increment is stored as metadata.
    for (TextIterator iterator(contextRange); !iterator.atEnd(); iterator.advance(), iteratorCount++) {
        size_t currentTextLength = iterator.text().length();
        if (!currentTextLength) {
            softLink_DataDetectorsCore_DDScanQueryAddSeparator(scanQuery, DDTextCoalescingTypeHardBreak);
            if (iteratorCount > maxFragmentWithHardBreak)
                break;
            continue;
        }
        // Test for white space nodes, we're coalescing them.
        const UniChar* currentCharPtr = iterator.text().upconvertedCharacters();
        
        bool containsOnlyWhiteSpace = true;
        bool hasTab = false;
        bool hasNewline = false;
        int nbspCount = 0;
        for (NSUInteger i = 0; i < currentTextLength; i++) {
            if (!CFCharacterSetIsCharacterMember(whiteSpacesSet, *currentCharPtr)) {
                containsOnlyWhiteSpace = false;
                break;
            }
            
            if (CFCharacterSetIsCharacterMember(newLinesSet, *currentCharPtr))
                hasNewline = true;
            else if (*currentCharPtr == '\t')
                hasTab = true;
            
            // Multiple consecutive non breakable spaces are most likely simulated tabs.
            if (*currentCharPtr == 0xa0) {
                if (++nbspCount > 2)
                    hasTab = true;
            } else
                nbspCount = 0;

            currentCharPtr++;
        }
        if (containsOnlyWhiteSpace) {
            if (hasNewline) {
                softLink_DataDetectorsCore_DDScanQueryAddLineBreak(scanQuery);
                if (iteratorCount > maxFragmentWithLinebreak)
                    break;
            } else {
                softLink_DataDetectorsCore_DDScanQueryAddSeparator(scanQuery, hasTab ? DDTextCoalescingTypeTab : DDTextCoalescingTypeSpace);
                if (iteratorCount > maxFragmentSpace)
                    break;
            }
            continue;
        }
        
        RetainPtr<CFStringRef> currentText = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, iterator.text().upconvertedCharacters(), iterator.text().length()));
        softLink_DataDetectorsCore_DDScanQueryAddTextFragment(scanQuery, currentText.get(), CFRangeMake(0, currentTextLength), (void *)iteratorCount, (DDTextFragmentMode)0, DDTextCoalescingTypeNone);
        fragmentCount++;
    }
}

static inline CFComparisonResult queryOffsetCompare(DDQueryOffset o1, DDQueryOffset o2)
{
    if (o1.queryIndex < o2.queryIndex)
        return kCFCompareLessThan;
    if (o1.queryIndex > o2.queryIndex)
        return kCFCompareGreaterThan;
    if (o1.offset < o2.offset)
        return kCFCompareLessThan;
    if (o1.offset > o2.offset)
        return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

void DataDetection::removeDataDetectedLinksInDocument(Document& document)
{
    Vector<Ref<HTMLAnchorElement>> allAnchorElements;
    for (auto& anchor : descendantsOfType<HTMLAnchorElement>(document))
        allAnchorElements.append(anchor);

    for (auto& anchor : allAnchorElements)
        removeResultLinksFromAnchor(anchor.get());
}

NSArray *DataDetection::detectContentInRange(RefPtr<Range>& contextRange, DataDetectorTypes types, NSDictionary *context)
{
    RetainPtr<DDScannerRef> scanner = adoptCF(softLink_DataDetectorsCore_DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    RetainPtr<DDScanQueryRef> scanQuery = adoptCF(softLink_DataDetectorsCore_DDScanQueryCreate(NULL));
    buildQuery(scanQuery.get(), contextRange.get());
    
    if (types & DataDetectorTypeLookupSuggestion)
        softLink_DataDetectorsCore_DDScannerEnableOptionalSource(scanner.get(), DDScannerSourceSpotlight, true);

    // FIXME: we should add a timeout to this call to make sure it doesn't take too much time.
    if (!softLink_DataDetectorsCore_DDScannerScanQuery(scanner.get(), scanQuery.get()))
        return nil;

    RetainPtr<CFArrayRef> scannerResults = adoptCF(softLink_DataDetectorsCore_DDScannerCopyResultsWithOptions(scanner.get(), get_DataDetectorsCore_DDScannerCopyResultsOptionsForPassiveUse() | DDScannerCopyResultsOptionsCoalesceSignatures));
    if (!scannerResults)
        return nil;

    CFIndex resultCount = CFArrayGetCount(scannerResults.get());
    if (!resultCount)
        return nil;

    Vector<RetainPtr<DDResultRef>> allResults;
    Vector<RetainPtr<NSIndexPath>> indexPaths;
    NSInteger currentTopLevelIndex = 0;

    // Iterate through the scanner results to find signatures and extract all the subresults while
    // populating the array of index paths to use in the href of the anchors being created.
    for (id resultObject in (NSArray *)scannerResults.get()) {
        DDResultRef result = (DDResultRef)resultObject;
        NSIndexPath *indexPath = [NSIndexPath indexPathWithIndex:currentTopLevelIndex];
        if (CFStringCompare(softLink_DataDetectorsCore_DDResultGetType(result), get_DataDetectorsCore_DDBinderSignatureBlockKey(), 0) == kCFCompareEqualTo) {
            NSArray *subresults = (NSArray *)softLink_DataDetectorsCore_DDResultGetSubResults(result);
            
            for (NSUInteger subResultIndex = 0 ; subResultIndex < [subresults count] ; subResultIndex++) {
                indexPaths.append([indexPath indexPathByAddingIndex:subResultIndex]);
                allResults.append((DDResultRef)[subresults objectAtIndex:subResultIndex]);
            }
        } else {
            allResults.append(result);
            indexPaths.append(indexPath);
        }
        currentTopLevelIndex++;
    }

    Vector<Vector<RefPtr<Range>>> allResultRanges;
    TextIterator iterator(contextRange.get());
    CFIndex iteratorCount = 0;

    // Iterate through the array of the expanded results to create a vector of Range objects that indicate
    // where the DOM needs to be modified.
    // Each result can be contained all in one text node or can span multiple text nodes.
    for (auto& result : allResults) {
        DDQueryRange queryRange = softLink_DataDetectorsCore_DDResultGetQueryRangeForURLification(result.get());
        CFIndex iteratorTargetAdvanceCount = (CFIndex)softLink_DataDetectorsCore_DDScanQueryGetFragmentMetaData(scanQuery.get(), queryRange.start.queryIndex);
        for (; iteratorCount < iteratorTargetAdvanceCount; ++iteratorCount)
            iterator.advance();

        Vector<RefPtr<Range>> fragmentRanges;
        RefPtr<Range> currentRange = iterator.range();
        CFIndex fragmentIndex = queryRange.start.queryIndex;
        if (fragmentIndex == queryRange.end.queryIndex)
            fragmentRanges.append(TextIterator::subrange(*currentRange, queryRange.start.offset, queryRange.end.offset - queryRange.start.offset));
        else {
            if (!queryRange.start.offset)
                fragmentRanges.append(currentRange);
            else
                fragmentRanges.append(Range::create(currentRange->ownerDocument(), &currentRange->startContainer(), currentRange->startOffset() + queryRange.start.offset, &currentRange->endContainer(), currentRange->endOffset()));
        }
        
        while (fragmentIndex < queryRange.end.queryIndex) {
            ++fragmentIndex;
            iteratorTargetAdvanceCount = (CFIndex)softLink_DataDetectorsCore_DDScanQueryGetFragmentMetaData(scanQuery.get(), fragmentIndex);
            for (; iteratorCount < iteratorTargetAdvanceCount; ++iteratorCount)
                iterator.advance();

            currentRange = iterator.range();
            RefPtr<Range> fragmentRange = (fragmentIndex == queryRange.end.queryIndex) ?  Range::create(currentRange->ownerDocument(), &currentRange->startContainer(), currentRange->startOffset(), &currentRange->endContainer(), currentRange->startOffset() + queryRange.end.offset) : currentRange;
            RefPtr<Range> previousRange = fragmentRanges.last();
            if (&previousRange->startContainer() == &fragmentRange->startContainer()) {
                fragmentRange = Range::create(currentRange->ownerDocument(), &previousRange->startContainer(), previousRange->startOffset(), &fragmentRange->endContainer(), fragmentRange->endOffset());
                fragmentRanges.last() = fragmentRange;
            } else
                fragmentRanges.append(fragmentRange);
        }
        allResultRanges.append(WTFMove(fragmentRanges));
    }
    
    auto tz = adoptCF(CFTimeZoneCopyDefault());
    NSDate *referenceDate = [context objectForKey:getkDataDetectorsReferenceDateKey()] ?: [NSDate date];
    Text* lastTextNodeToUpdate = nullptr;
    String lastNodeContent;
    size_t contentOffset = 0;
    DDQueryOffset lastModifiedQueryOffset = { -1, 0 };
    
    // For each result add the link.
    // Since there could be multiple results in the same text node, the node is only modified when
    // we are about to process a different text node.
    resultCount = allResults.size();
    
    for (CFIndex resultIndex = 0; resultIndex < resultCount; ++resultIndex) {
        DDResultRef coreResult = allResults[resultIndex].get();
        DDQueryRange queryRange = softLink_DataDetectorsCore_DDResultGetQueryRangeForURLification(coreResult);
        auto& resultRanges = allResultRanges[resultIndex];

        // Compare the query offsets to make sure we don't go backwards
        if (queryOffsetCompare(lastModifiedQueryOffset, queryRange.start) >= 0)
            continue;

        if (resultRanges.isEmpty())
            continue;
        
        // Store the range boundaries as Position, because the DOM could change if we find
        // old data detector link.
        Vector<std::pair<Position, Position>> rangeBoundaries;
        rangeBoundaries.reserveInitialCapacity(resultRanges.size());
        for (auto& range : resultRanges)
            rangeBoundaries.uncheckedAppend({ range->startPosition(), range->endPosition() });

        NSString *identifier = dataDetectorStringForPath(indexPaths[resultIndex].get());
        NSString *correspondingURL = constructURLStringForResult(coreResult, identifier, referenceDate, (NSTimeZone *)tz.get(), types);
        bool didModifyDOM = false;

        if (!correspondingURL || searchForLinkRemovingExistingDDLinks(resultRanges.first()->startContainer(), resultRanges.last()->endContainer(), didModifyDOM))
            continue;
        
        if (didModifyDOM) {
            // If the DOM was modified because some old links were removed,
            // we need to recreate the ranges because they could no longer be valid.
            ASSERT(resultRanges.size() == rangeBoundaries.size());
            resultRanges.shrink(0); // Keep capacity as we are going to repopulate the Vector right away with the same number of items.
            for (auto& rangeBoundary : rangeBoundaries)
                resultRanges.uncheckedAppend(Range::create(*rangeBoundary.first.document(), rangeBoundary.first, rangeBoundary.second));
        }
        
        lastModifiedQueryOffset = queryRange.end;
        BOOL shouldUseLightLinks = softLink_DataDetectorsCore_DDShouldUseLightLinksForResult(coreResult, [indexPaths[resultIndex] length] > 1);

        for (auto& range : resultRanges) {
            auto* parentNode = range->startContainer().parentNode();
            if (!parentNode)
                continue;
            if (!is<Text>(range->startContainer()))
                continue;
            auto& currentTextNode = downcast<Text>(range->startContainer());
            Document& document = currentTextNode.document();
            String textNodeData;
            
            if (lastTextNodeToUpdate != &currentTextNode) {
                if (lastTextNodeToUpdate)
                    lastTextNodeToUpdate->setData(lastNodeContent);
                contentOffset = 0;
                if (range->startOffset() > 0)
                    textNodeData = currentTextNode.data().substring(0, range->startOffset());
            } else
                textNodeData = currentTextNode.data().substring(contentOffset, range->startOffset() - contentOffset);
            
            if (!textNodeData.isEmpty()) {
                parentNode->insertBefore(Text::create(document, textNodeData), &currentTextNode);
                contentOffset = range->startOffset();
            }
            
            // Create the actual anchor node and insert it before the current node.
            textNodeData = currentTextNode.data().substring(range->startOffset(), range->endOffset() - range->startOffset());
            Ref<Text> newTextNode = Text::create(document, textNodeData);
            parentNode->insertBefore(newTextNode.copyRef(), &currentTextNode);
            
            Ref<HTMLAnchorElement> anchorElement = HTMLAnchorElement::create(document);
            anchorElement->setHref(correspondingURL);
            anchorElement->setDir("ltr");
            if (shouldUseLightLinks) {
                document.updateStyleIfNeeded();
                auto* renderStyle = parentNode->computedStyle();
                if (renderStyle) {
                    auto textColor = renderStyle->visitedDependentColor(CSSPropertyColor);
                    if (textColor.isValid()) {
                        double h = 0;
                        double s = 0;
                        double v = 0;
                        textColor.getHSV(h, s, v);

                        // Set the alpha of the underline to 46% if the text color is white-ish (defined
                        // as having a saturation of less than 2% and a value/brightness or greater than
                        // 98%). Otherwise, set the alpha of the underline to 26%.
                        double overrideAlpha = (s < 0.02 && v > 0.98) ? 0.46 : 0.26;
                        auto underlineColor = Color(colorWithOverrideAlpha(textColor.rgb(), overrideAlpha));

                        anchorElement->setInlineStyleProperty(CSSPropertyColor, textColor.cssText());
                        anchorElement->setInlineStyleProperty(CSSPropertyTextDecorationColor, underlineColor.cssText());
                    }
                }
            } else if (is<StyledElement>(*parentNode)) {
                if (auto* style = downcast<StyledElement>(*parentNode).presentationAttributeStyle()) {
                    String color = style->getPropertyValue(CSSPropertyColor);
                    if (!color.isEmpty())
                        anchorElement->setInlineStyleProperty(CSSPropertyColor, color);
                }
            }
            anchorElement->appendChild(WTFMove(newTextNode));
            // Add a special attribute to mark this URLification as the result of data detectors.
            anchorElement->setAttributeWithoutSynchronization(x_apple_data_detectorsAttr, AtomicString("true", AtomicString::ConstructFromLiteral));
            anchorElement->setAttributeWithoutSynchronization(x_apple_data_detectors_typeAttr, dataDetectorTypeForCategory(softLink_DataDetectorsCore_DDResultGetCategory(coreResult)));
            anchorElement->setAttributeWithoutSynchronization(x_apple_data_detectors_resultAttr, identifier);

            parentNode->insertBefore(WTFMove(anchorElement), &currentTextNode);

            contentOffset = range->endOffset();
            
            lastNodeContent = currentTextNode.data().substring(range->endOffset(), currentTextNode.length() - range->endOffset());
            lastTextNodeToUpdate = &currentTextNode;
        }        
    }
    if (lastTextNodeToUpdate)
        lastTextNodeToUpdate->setData(lastNodeContent);
    
    return [getDDScannerResultClass() resultsFromCoreResults:scannerResults.get()];
}

#else

NSArray *DataDetection::detectContentInRange(RefPtr<Range>&, DataDetectorTypes, NSDictionary *)
{
    return nil;
}

void DataDetection::removeDataDetectedLinksInDocument(Document&)
{
}

#endif

const String& DataDetection::dataDetectorURLProtocol()
{
    static NeverDestroyed<String> protocol(MAKE_STATIC_STRING_IMPL("x-apple-data-detectors"));
    return protocol;
}

bool DataDetection::isDataDetectorURL(const URL& url)
{
    return url.protocolIs(dataDetectorURLProtocol());
}

} // namespace WebCore

#endif

