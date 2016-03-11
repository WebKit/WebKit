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
#import "DataDetectorsCoreSoftLink.h"
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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/DataDetectorsAdditions.h>
#endif

const char *dataDetectorsURLScheme = "x-apple-data-detectors";
const char *dataDetectorsAttributeTypeKey = "x-apple-data-detectors-type";
const char *dataDetectorsAttributeResultKey = "x-apple-data-detectors-result";

namespace WebCore {

bool DataDetection::isDataDetectorLink(Element* element)
{
    return element->getAttribute(dataDetectorsURLScheme) == "true";
}

bool DataDetection::requiresExtendedContext(Element* element)
{
    return element->getAttribute(dataDetectorsAttributeTypeKey) == "calendar-event";
}

String DataDetection::dataDetectorIdentifier(Element* element)
{
    return element->getAttribute(dataDetectorsAttributeResultKey);
}

bool DataDetection::shouldCancelDefaultAction(Element* element)
{
#if PLATFORM(MAC)
    UNUSED_PARAM(element);
    return false;
#else
    // FIXME: We should also compute the DDResultRef and check the result category.
    if (!is<HTMLAnchorElement>(*element))
        return false;
    if (element->getAttribute(dataDetectorsURLScheme) != "true")
        return false;
    String type = element->getAttribute(dataDetectorsAttributeTypeKey);
    if (type == "misc" || type == "calendar-event" || type == "telephone")
        return true;
    return false;
#endif
}

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

#if PLATFORM(IOS)
    
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
#if USE(APPLE_INTERNAL_SDK)
        || ((detectionTypes & DataDetectorTypeSpotlightSuggestion) && (CFStringCompare(DDBinderSpotlightSourceKey, type, 0) == kCFCompareEqualTo))
#endif
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

static void removeResultLinksFromAnchor(Node* node, Node* nodeParent)
{
    // Perform a depth-first search for anchor nodes, which have the DDURLScheme attribute set to true,
    // take their children and insert them before the anchor,
    // and then remove the anchor.
    
    if (!node)
        return;
    
    BOOL nodeIsDDAnchor = is<HTMLAnchorElement>(*node) && downcast<Element>(*node).getAttribute(dataDetectorsURLScheme) == "true";
    
    RefPtr<NodeList> children = node->childNodes();
    unsigned childCount = children->length();
    for (size_t i = 0; i < childCount; i++) {
        Node *child = children->item(i);
        if (is<Element>(*child))
            removeResultLinksFromAnchor(child, node);
    }
    
    if (nodeIsDDAnchor && nodeParent) {
        children = node->childNodes();
        childCount = children->length();
        
        // Iterate over the children and move them all onto the same level as this anchor.
        // Remove the anchor afterwards.
        for (size_t i = 0; i < childCount; i++) {
            Node *child = children->item(0);
            nodeParent->insertBefore(child, node, ASSERT_NO_EXCEPTION);
        }
        nodeParent->removeChild(node, ASSERT_NO_EXCEPTION);
    }
}

static bool searchForLinkRemovingExistingDDLinks(Node* startNode, Node* endNode, bool &didModifyDOM)
{
    didModifyDOM = false;
    Node *node = startNode;
    while (node) {
        if (is<HTMLAnchorElement>(*node)) {
            if (downcast<Element>(*node).getAttribute(dataDetectorsURLScheme) != "true")
                return true;
            removeResultLinksFromAnchor(node, node->parentElement());
            didModifyDOM = true;
        }
        
        if (node == endNode) {
            // If we found the end node and no link, return false unless an ancestor node is a link.
            // The only ancestors not tested at this point are in the direct line from self's parent to the top.
            node = startNode->parentNode();
            while (node) {
                if (is<HTMLAnchorElement>(*node)) {
                    if (downcast<Element>(*node).getAttribute(dataDetectorsURLScheme) != "true")
                        return true;
                    removeResultLinksFromAnchor(node, node->parentElement());
                    didModifyDOM = true;
                }
                node = node->parentNode();
            }
            return false;
        }
        
        RefPtr<NodeList> childNodes = node->childNodes();
        if (childNodes->length())
            node = childNodes->item(0);
        else {
            Node *newNode = node->nextSibling();
            Node *parentNode = node;
            while (!newNode) {
                parentNode = parentNode->parentNode();
                if (!parentNode)
                    return false;
                newNode = parentNode->nextSibling();
            }
            node = newNode;
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

static String dataDetectorStringForPath(NSIndexPath* path)
{
    NSUInteger length = path.length;
    
    switch (length) {
    case 0:
        return String();
        
    case 1:
        return String::format("%lu", (unsigned long)[path indexAtPosition:0]);
        
    case 2:
        return String::format("%lu/%lu", (unsigned long)[path indexAtPosition:0], (unsigned long)[path indexAtPosition:1]);
        
    default:
        {
            String componentsString = String::format("%lu", (unsigned long)[path indexAtPosition:0]);
            for (NSUInteger i = 1 ; i < length ; i++) {
                componentsString.append("/");
                componentsString.append(String::format("%lu", (unsigned long)[path indexAtPosition:i]));
            }

            return componentsString;
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
        const UniChar *currentCharPtr = iterator.text().upconvertedCharacters();
        
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

NSArray *DataDetection::detectContentInRange(RefPtr<Range>& contextRange, DataDetectorTypes types)
{
    RetainPtr<DDScannerRef> scanner = adoptCF(softLink_DataDetectorsCore_DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    RetainPtr<DDScanQueryRef> scanQuery = adoptCF(softLink_DataDetectorsCore_DDScanQueryCreate(NULL));
    buildQuery(scanQuery.get(), contextRange.get());
    
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000
    if (types & DataDetectorTypeSpotlightSuggestion)
        softLink_DataDetectorsCore_DDScannerEnableOptionalSource(scanner.get(), DDScannerSourceSpotlight, true);
#endif
    
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
        while (iteratorCount < iteratorTargetAdvanceCount) {
            iterator.advance();
            iteratorCount++;
        }

        Vector<RefPtr<Range>> fragmentRanges;
        RefPtr<Range> currentRange = iterator.range();
        CFIndex fragmentIndex = queryRange.start.queryIndex;
        if (fragmentIndex == queryRange.end.queryIndex)
            fragmentRanges.append(TextIterator::subrange(currentRange.get(), queryRange.start.offset, queryRange.end.offset - queryRange.start.offset));
        else {
            if (!queryRange.start.offset)
                fragmentRanges.append(currentRange);
            else
                fragmentRanges.append(Range::create(currentRange->ownerDocument(), &currentRange->startContainer(), currentRange->startOffset() + queryRange.start.offset, &currentRange->endContainer(), currentRange->endOffset()));
        }
        
        while (fragmentIndex < queryRange.end.queryIndex) {
            fragmentIndex++;
            iteratorTargetAdvanceCount = (CFIndex)softLink_DataDetectorsCore_DDScanQueryGetFragmentMetaData(scanQuery.get(), fragmentIndex);
            while (iteratorCount < iteratorTargetAdvanceCount) {
                iterator.advance();
                iteratorCount++;
            }
            currentRange = iterator.range();
            fragmentRanges.append(currentRange);
        }
        allResultRanges.append(fragmentRanges);
    }
    
    CFTimeZoneRef tz = CFTimeZoneCopyDefault();
    NSDate *referenceDate = [NSDate date];
    Text* lastTextNodeToUpdate = nullptr;
    String lastNodeContent;
    size_t contentOffset = 0;
    DDQueryOffset lastModifiedQueryOffset = {-1, 0};
    
    // For each result add the link.
    // Since there could be multiple results in the same text node, the node is only modified when
    // we are about to process a different text node.
    resultCount = allResults.size();
    
    for (CFIndex resultIndex = 0; resultIndex < resultCount; resultIndex++) {
        DDResultRef coreResult = allResults[resultIndex].get();
        DDQueryRange queryRange = softLink_DataDetectorsCore_DDResultGetQueryRangeForURLification(coreResult);
        Vector<RefPtr<Range>> resultRanges = allResultRanges[resultIndex];

        // Compare the query offsets to make sure we don't go backwards
        if (queryOffsetCompare(lastModifiedQueryOffset, queryRange.start) >= 0)
            continue;

        if (!resultRanges.size())
            continue;

        NSString *identifier = dataDetectorStringForPath(indexPaths[resultIndex].get());
        NSString *correspondingURL = constructURLStringForResult(coreResult, identifier, referenceDate, (NSTimeZone *)tz, types);
        bool didModifyDOM = false;
        
        // Store the range boundaries as Position, because the DOM could change if we find
        // old data detector link.
        Vector<std::pair<Position, Position>> rangeBoundaries;
        for (auto& range : resultRanges)
            rangeBoundaries.append(std::make_pair(range->startPosition(), range->endPosition()));

        if (!correspondingURL || searchForLinkRemovingExistingDDLinks(&resultRanges.first()->startContainer(), &resultRanges.last()->endContainer(), didModifyDOM))
            continue;
        
        if (didModifyDOM) {
            // If the DOM was modified because some old links were removed,
            // we need to recreate the ranges because they could no longer be valid.
            resultRanges.clear();
            for (auto& rangeBoundary : rangeBoundaries)
                resultRanges.append(Range::create(*rangeBoundary.first.document(), rangeBoundary.first, rangeBoundary.second));
        }
        
        lastModifiedQueryOffset = queryRange.end;

        for (auto& range : resultRanges) {
            Node* parentNode = range->startContainer().parentNode();
            Text& currentTextNode = downcast<Text>(range->startContainer());
            Document& document = currentTextNode.document();
            String textNodeData;
            
            if (lastTextNodeToUpdate != &currentTextNode) {
                if (lastTextNodeToUpdate)
                    lastTextNodeToUpdate->setData(lastNodeContent);
                contentOffset = 0;
                if (range->startOffset() > 0)
                    textNodeData = currentTextNode.substringData(0, range->startOffset(), ASSERT_NO_EXCEPTION);
            } else
                textNodeData = currentTextNode.substringData(contentOffset, range->startOffset() - contentOffset, ASSERT_NO_EXCEPTION);
            
            if (!textNodeData.isEmpty()) {
                RefPtr<Node> newNode = Text::create(document, textNodeData);
                parentNode->insertBefore(newNode, &currentTextNode, ASSERT_NO_EXCEPTION);
                contentOffset = range->startOffset();
            }
            
            // Create the actual anchor node and insert it before the current node.
            textNodeData = currentTextNode.substringData(range->startOffset(), range->endOffset() - range->startOffset(), ASSERT_NO_EXCEPTION);
            RefPtr<Node> newNode = Text::create(document, textNodeData);
            parentNode->insertBefore(newNode, &currentTextNode, ASSERT_NO_EXCEPTION);
            
            RefPtr<HTMLAnchorElement> anchorElement = HTMLAnchorElement::create(document);
            anchorElement->setHref(correspondingURL);
            anchorElement->setDir("ltr");
            RefPtr<Attr> color = downcast<Element>(parentNode)->getAttributeNode("color");
            if (color)
                anchorElement->setAttribute(HTMLNames::styleAttr, color->style()->cssText());
            
            anchorElement->Node::appendChild(newNode, ASSERT_NO_EXCEPTION);
            parentNode->insertBefore(anchorElement, &currentTextNode, ASSERT_NO_EXCEPTION);
            // Add a special attribute to mark this URLification as the result of data detectors.
            anchorElement->setAttribute(QualifiedName(nullAtom, dataDetectorsURLScheme, nullAtom), "true");
            anchorElement->setAttribute(QualifiedName(nullAtom, dataDetectorsAttributeTypeKey, nullAtom), dataDetectorTypeForCategory(softLink_DataDetectorsCore_DDResultGetCategory(coreResult)));
            anchorElement->setAttribute(QualifiedName(nullAtom, dataDetectorsAttributeResultKey, nullAtom), identifier);
            contentOffset = range->endOffset();
            
            lastNodeContent = currentTextNode.substringData(range->endOffset(), currentTextNode.length() - range->endOffset(), ASSERT_NO_EXCEPTION);
            lastTextNodeToUpdate = &currentTextNode;
        }        
    }
    if (lastTextNodeToUpdate)
        lastTextNodeToUpdate->setData(lastNodeContent);
    
    return [get_DataDetectorsCore_DDScannerResultClass() resultsFromCoreResults:scannerResults.get()];
}

#else
NSArray *DataDetection::detectContentInRange(RefPtr<Range>&, DataDetectorTypes)
{
    return nil;
}
#endif
} // namespace WebCore
