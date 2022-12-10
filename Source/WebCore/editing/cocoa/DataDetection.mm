/*
 * Copyright (C) 2014-2020 Apple, Inc. All rights reserved.
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
#import "ColorConversion.h"
#import "ColorSerialization.h"
#import "CommonAtomStrings.h"
#import "DataDetectionResultsStorage.h"
#import "Editing.h"
#import "ElementAncestorIterator.h"
#import "ElementRareData.h"
#import "ElementTraversal.h"
#import "FrameView.h"
#import "HTMLAnchorElement.h"
#import "HTMLDivElement.h"
#import "HTMLNames.h"
#import "HTMLTextFormControlElement.h"
#import "HitTestResult.h"
#import "ImageOverlay.h"
#import "NodeList.h"
#import "NodeTraversal.h"
#import "QualifiedName.h"
#import "Range.h"
#import "RenderObject.h"
#import "StyleProperties.h"
#import "Text.h"
#import "TextIterator.h"
#import "TextRecognitionResult.h"
#import "VisiblePosition.h"
#import "VisibleUnits.h"
#import <pal/spi/ios/DataDetectorsUISPI.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>
#import <pal/mac/DataDetectorsSoftLink.h>

#if PLATFORM(MAC)
template<> struct WTF::CFTypeTrait<DDResultRef> {
    static inline CFTypeID typeID(void) { return DDResultGetCFTypeID(); }
};
#endif

namespace WebCore {

using namespace HTMLNames;

#if PLATFORM(MAC)

static std::optional<DetectedItem> detectItem(const VisiblePosition& position, const SimpleRange& contextRange)
{
    if (position.isNull())
        return { };
    String fullPlainTextString = plainText(contextRange);
    CFIndex hitLocation = characterCount(*makeSimpleRange(contextRange.start, position));

    auto scanner = adoptCF(DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    auto scanQuery = adoptCF(DDScanQueryCreateFromString(kCFAllocatorDefault, fullPlainTextString.createCFString().get(), CFRangeMake(0, fullPlainTextString.length())));

    if (!DDScannerScanQuery(scanner.get(), scanQuery.get()))
        return { };

    auto results = adoptCF(DDScannerCopyResultsWithOptions(scanner.get(), DDScannerCopyResultsOptionsNoOverlap));

    // Find the DDResultRef that intersects the hitTestResult's VisiblePosition.
    DDResultRef mainResult = nullptr;
    std::optional<SimpleRange> mainResultRange;
    CFIndex resultCount = CFArrayGetCount(results.get());
    for (CFIndex i = 0; i < resultCount; i++) {
        auto result = checked_cf_cast<DDResultRef>(CFArrayGetValueAtIndex(results.get(), i));
        CFRange resultRangeInContext = DDResultGetRange(result);
        if (hitLocation >= resultRangeInContext.location && (hitLocation - resultRangeInContext.location) < resultRangeInContext.length) {
            mainResult = result;
            mainResultRange = resolveCharacterRange(contextRange, resultRangeInContext);
            break;
        }
    }

    if (!mainResult)
        return { };

    auto view = mainResultRange->start.document().view();
    if (!view)
        return { };

    auto actionContext = adoptNS([PAL::allocDDActionContextInstance() init]);
    [actionContext setAllResults:@[ (__bridge id)mainResult ]];
    [actionContext setMainResult:mainResult];

    return { {
        WTFMove(actionContext),
        view->contentsToWindow(enclosingIntRect(unitedBoundingBoxes(RenderObject::absoluteTextQuads(*mainResultRange)))),
        *mainResultRange,
    } };
}

std::optional<DetectedItem> DataDetection::detectItemAroundHitTestResult(const HitTestResult& hitTestResult)
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return { };

    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return { };
    auto renderer = node->renderer();
    if (!renderer)
        return { };

    VisiblePosition position;
    std::optional<SimpleRange> contextRange;

    if (!is<HTMLTextFormControlElement>(*node)) {
        position = renderer->positionForPoint(hitTestResult.localPoint(), nullptr);
        if (position.isNull())
            position = firstPositionInOrBeforeNode(node);

        contextRange = rangeExpandedAroundPositionByCharacters(position, 250);
    } else {
        Frame* frame = node->document().frame();
        if (!frame)
            return { };

        IntPoint framePoint = hitTestResult.roundedPointInInnerNodeFrame();
        if (!frame->rangeForPoint(framePoint))
            return { };

        position = frame->visiblePositionForPoint(framePoint);
        if (position.isNull())
            return { };

        contextRange = enclosingTextUnitOfGranularity(position, TextGranularity::LineGranularity, SelectionDirection::Forward);
    }

    if (!contextRange)
        return { };

    return detectItem(position, *contextRange);
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

bool DataDetection::canBePresentedByDataDetectors(const URL& url)
{
    return [PAL::softLink_DataDetectorsCore_DDURLTapAndHoldSchemes() containsObject:(NSString *)url.protocol().convertToASCIILowercase()];
}

bool DataDetection::isDataDetectorLink(Element& element)
{
    if (!is<HTMLAnchorElement>(element))
        return false;

    return canBePresentedByDataDetectors(downcast<HTMLAnchorElement>(element).href());
}

bool DataDetection::requiresExtendedContext(Element& element)
{
    return equalLettersIgnoringASCIICase(element.attributeWithoutSynchronization(x_apple_data_detectors_typeAttr), "calendar-event"_s);
}

String DataDetection::dataDetectorIdentifier(Element& element)
{
    return element.attributeWithoutSynchronization(x_apple_data_detectors_resultAttr);
}

bool DataDetection::canPresentDataDetectorsUIForElement(Element& element)
{
    if (!isDataDetectorLink(element))
        return false;
    
    if (PAL::softLink_DataDetectorsCore_DDShouldImmediatelyShowActionSheetForURL(downcast<HTMLAnchorElement>(element).href()))
        return true;
    
    auto& resultAttribute = element.attributeWithoutSynchronization(x_apple_data_detectors_resultAttr);
    if (resultAttribute.isEmpty())
        return false;

    auto* dataDetectionResults = element.document().frame()->dataDetectionResultsIfExists();
    if (!dataDetectionResults)
        return false;

    NSArray *results = dataDetectionResults->documentLevelResults();
    if (!results)
        return false;

    auto resultIndices = StringView { resultAttribute }.split('/');
    auto indexIterator = resultIndices.begin();
    auto result = [results[parseIntegerAllowingTrailingJunk<int>(*indexIterator).value_or(0)] coreResult];

    // Handle the case of a signature block, where we need to follow the path down one or more subresult levels.
    while (++indexIterator != resultIndices.end()) {
        results = (__bridge NSArray *)PAL::softLink_DataDetectorsCore_DDResultGetSubResults(result);
        result = (__bridge DDResultRef)results[parseIntegerAllowingTrailingJunk<int>(*indexIterator).value_or(0)];
    }

    return PAL::softLink_DataDetectorsCore_DDShouldImmediatelyShowActionSheetForResult(result);
}

static BOOL resultIsURL(DDResultRef result)
{
    if (!result)
        return NO;
    
    static NeverDestroyed<RetainPtr<NSSet>> urlTypes = [NSSet setWithObjects:
        (NSString *)PAL::get_DataDetectorsCore_DDBinderHttpURLKey(),
        (NSString *)PAL::get_DataDetectorsCore_DDBinderWebURLKey(),
        (NSString *)PAL::get_DataDetectorsCore_DDBinderMailURLKey(),
        (NSString *)PAL::get_DataDetectorsCore_DDBinderGenericURLKey(),
        (NSString *)PAL::get_DataDetectorsCore_DDBinderEmailKey(), nil];
    return [urlTypes.get() containsObject:(NSString *)PAL::softLink_DataDetectorsCore_DDResultGetType(result)];
}

static NSString *constructURLStringForResult(DDResultRef currentResult, NSString *resultIdentifier, NSDate *referenceDate, NSTimeZone *referenceTimeZone, OptionSet<DataDetectorType> detectionTypes)
{
    if (!PAL::softLink_DataDetectorsCore_DDResultHasProperties(currentResult, DDResultPropertyPassiveDisplay))
        return nil;

    auto phoneTypes = detectionTypes.contains(DataDetectorType::PhoneNumber) ? DDURLifierPhoneNumberDetectionRegular : DDURLifierPhoneNumberDetectionNone;
    auto category = PAL::softLink_DataDetectorsCore_DDResultGetCategory(currentResult);
    auto type = PAL::softLink_DataDetectorsCore_DDResultGetType(currentResult);

    if ((detectionTypes.contains(DataDetectorType::Address) && DDResultCategoryAddress == category)
        || (detectionTypes.contains(DataDetectorType::TrackingNumber) && CFEqual(PAL::get_DataDetectorsCore_DDBinderTrackingNumberKey(), type))
        || (detectionTypes.contains(DataDetectorType::FlightNumber) && CFEqual(PAL::get_DataDetectorsCore_DDBinderFlightInformationKey(), type))
        || (detectionTypes.contains(DataDetectorType::LookupSuggestion) && CFEqual(PAL::get_DataDetectorsCore_DDBinderParsecSourceKey(), type))
        || (detectionTypes.contains(DataDetectorType::PhoneNumber) && DDResultCategoryPhoneNumber == category)
        || (detectionTypes.contains(DataDetectorType::Link) && resultIsURL(currentResult))) {
        return PAL::softLink_DataDetectorsCore_DDURLStringForResult(currentResult, resultIdentifier, phoneTypes, referenceDate, referenceTimeZone);
    }
    if (detectionTypes.contains(DataDetectorType::CalendarEvent) && DDResultCategoryCalendarEvent == category) {
        if (!PAL::softLink_DataDetectorsCore_DDResultIsPastDate(currentResult, (CFDateRef)referenceDate, (CFTimeZoneRef)referenceTimeZone))
            return PAL::softLink_DataDetectorsCore_DDURLStringForResult(currentResult, resultIdentifier, phoneTypes, referenceDate, referenceTimeZone);
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
    
    bool elementIsDDAnchor = is<HTMLAnchorElement>(element) && equalLettersIgnoringASCIICase(element.attributeWithoutSynchronization(x_apple_data_detectorsAttr), "true"_s);
    if (!elementIsDDAnchor)
        return;

    // Iterate over the children and move them all onto the same level as this anchor. Remove the anchor afterwards.
    while (auto* child = element.firstChild())
        elementParent->insertBefore(*child, &element);

    elementParent->removeChild(element);
}

static bool searchForLinkRemovingExistingDDLinks(Node& startNode, Node& endNode)
{
    for (Node* node = &startNode; node; node = NodeTraversal::next(*node)) {
        if (is<HTMLAnchorElement>(*node)) {
            auto& anchor = downcast<HTMLAnchorElement>(*node);
            if (!equalLettersIgnoringASCIICase(anchor.attributeWithoutSynchronization(x_apple_data_detectorsAttr), "true"_s))
                return true;
            removeResultLinksFromAnchor(anchor);
        }
        
        if (node == &endNode) {
            // If we found the end node and no link, return false unless an ancestor node is a link.
            // The only ancestors not tested at this point are in the direct line from self's parent to the top.
            for (auto& anchor : ancestorsOfType<HTMLAnchorElement>(startNode)) {
                if (!equalLettersIgnoringASCIICase(anchor.attributeWithoutSynchronization(x_apple_data_detectorsAttr), "true"_s))
                    return true;
                removeResultLinksFromAnchor(anchor);
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
    auto length = path.length;
    switch (length) {
    case 0:
        return { };
    case 1:
        return makeString([path indexAtPosition:0]);
    case 2:
        return makeString([path indexAtPosition:0], '/', [path indexAtPosition:1]);
    default:
        StringBuilder builder;
        builder.append([path indexAtPosition:0]);
        for (NSUInteger i = 1; i < length; i++)
            builder.append('/', [path indexAtPosition:i]);
        return builder.toString();
    }
}

static void buildQuery(DDScanQueryRef scanQuery, const SimpleRange& contextRange)
{
    // Once we're over this number of fragments, stop at the first hard break.
    const CFIndex maxFragmentWithHardBreak = 1000;
    // Once we're over this number of fragments, we stop at the line.
    const CFIndex maxFragmentWithLinebreak = 5000;
    // Once we're over this number of fragments, we stop at the space.
    const CFIndex maxFragmentSpace = 10000;

    CFCharacterSetRef whiteSpacesSet = CFCharacterSetGetPredefined(kCFCharacterSetWhitespaceAndNewline);
    CFCharacterSetRef newLinesSet = CFCharacterSetGetPredefined(kCFCharacterSetNewline);
    
    CFIndex iteratorCount = 0;
    CFIndex fragmentCount = 0;
    
    // Build the scan query adding separators.
    // For each fragment the iterator increment is stored as metadata.
    for (TextIterator iterator(contextRange); !iterator.atEnd(); iterator.advance(), iteratorCount++) {
        StringView currentText = iterator.text();
        size_t currentTextLength = currentText.length();
        if (!currentTextLength) {
            PAL::softLink_DataDetectorsCore_DDScanQueryAddSeparator(scanQuery, DDTextCoalescingTypeHardBreak);
            if (iteratorCount > maxFragmentWithHardBreak)
                break;
            continue;
        }
        // Test for white space nodes, we're coalescing them.
        auto currentTextUpconvertedCharacters = currentText.upconvertedCharacters();
        auto currentCharPtr = currentTextUpconvertedCharacters.get();
        
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
                PAL::softLink_DataDetectorsCore_DDScanQueryAddLineBreak(scanQuery);
                if (iteratorCount > maxFragmentWithLinebreak)
                    break;
            } else {
                PAL::softLink_DataDetectorsCore_DDScanQueryAddSeparator(scanQuery, hasTab ? DDTextCoalescingTypeTab : DDTextCoalescingTypeSpace);
                if (iteratorCount > maxFragmentSpace)
                    break;
            }
            continue;
        }
        
        auto currentTextCFString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(currentTextUpconvertedCharacters.get()), currentTextLength));
        PAL::softLink_DataDetectorsCore_DDScanQueryAddTextFragment(scanQuery, currentTextCFString.get(), CFRangeMake(0, currentTextLength), (void *)iteratorCount, (DDTextFragmentMode)0, DDTextCoalescingTypeNone);
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

NSArray *DataDetection::detectContentInRange(const SimpleRange& contextRange, OptionSet<DataDetectorType> types, NSDictionary *context)
{
    auto scanner = adoptCF(PAL::softLink_DataDetectorsCore_DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    auto scanQuery = adoptCF(PAL::softLink_DataDetectorsCore_DDScanQueryCreate(NULL));
    buildQuery(scanQuery.get(), contextRange);
    
    if (types.contains(DataDetectorType::LookupSuggestion))
        PAL::softLink_DataDetectorsCore_DDScannerEnableOptionalSource(scanner.get(), DDScannerSourceSpotlight, true);

    // FIXME: we should add a timeout to this call to make sure it doesn't take too much time.
    if (!PAL::softLink_DataDetectorsCore_DDScannerScanQuery(scanner.get(), scanQuery.get()))
        return nil;

    auto scannerResults = adoptCF(PAL::softLink_DataDetectorsCore_DDScannerCopyResultsWithOptions(scanner.get(), PAL::get_DataDetectorsCore_DDScannerCopyResultsOptionsForPassiveUse() | DDScannerCopyResultsOptionsCoalesceSignatures));
    if (!scannerResults)
        return nil;

    if (!CFArrayGetCount(scannerResults.get()))
        return nil;

    Vector<RetainPtr<DDResultRef>> allResults;
    Vector<RetainPtr<NSIndexPath>> indexPaths;
    NSInteger currentTopLevelIndex = 0;

    // Iterate through the scanner results to find signatures and extract all the subresults while
    // populating the array of index paths to use in the href of the anchors being created.
    for (id resultObject in (NSArray *)scannerResults.get()) {
        DDResultRef result = (DDResultRef)resultObject;
        NSIndexPath *indexPath = [NSIndexPath indexPathWithIndex:currentTopLevelIndex];
        if (CFEqual(PAL::softLink_DataDetectorsCore_DDResultGetType(result), PAL::get_DataDetectorsCore_DDBinderSignatureBlockKey())) {
            NSArray *subresults = (NSArray *)PAL::softLink_DataDetectorsCore_DDResultGetSubResults(result);
            
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

    Vector<Vector<SimpleRange>> allResultRanges;
    {
        TextIterator iterator(contextRange);
        CFIndex iteratorCount = 0;

        // Iterate through the array of the expanded results to create a vector of Range objects that indicate
        // where the DOM needs to be modified.
        // Each result can be contained all in one text node or can span multiple text nodes.
        for (auto& result : allResults) {
            DDQueryRange queryRange = PAL::softLink_DataDetectorsCore_DDResultGetQueryRangeForURLification(result.get());
            CFIndex iteratorTargetAdvanceCount = (CFIndex)PAL::softLink_DataDetectorsCore_DDScanQueryGetFragmentMetaData(scanQuery.get(), queryRange.start.queryIndex);
            for (; iteratorCount < iteratorTargetAdvanceCount && !iterator.atEnd(); ++iteratorCount)
                iterator.advance();
            if (iterator.atEnd()) {
                ASSERT_NOT_REACHED();
                return nil;
            }

            Vector<SimpleRange> fragmentRanges;
            CFIndex fragmentIndex = queryRange.start.queryIndex;
            if (fragmentIndex == queryRange.end.queryIndex) {
                CharacterRange fragmentRange;
                fragmentRange.location = queryRange.start.offset;
                fragmentRange.length = queryRange.end.offset - queryRange.start.offset;
                fragmentRanges.append(resolveCharacterRange(iterator.range(), fragmentRange));
            } else {
                auto range = iterator.range();
                range.start.offset += queryRange.start.offset;
                fragmentRanges.append(range);
            }

            while (fragmentIndex < queryRange.end.queryIndex) {
                ++fragmentIndex;
                iteratorTargetAdvanceCount = (CFIndex)PAL::softLink_DataDetectorsCore_DDScanQueryGetFragmentMetaData(scanQuery.get(), fragmentIndex);
                for (; iteratorCount < iteratorTargetAdvanceCount && !iterator.atEnd(); ++iteratorCount)
                    iterator.advance();
                if (iterator.atEnd()) {
                    ASSERT_NOT_REACHED();
                    return nil;
                }

                auto fragmentRange = iterator.range();
                if (fragmentIndex == queryRange.end.queryIndex)
                    fragmentRange.end.offset = fragmentRange.start.offset + queryRange.end.offset;
                auto& previousRange = fragmentRanges.last();
                if (previousRange.start.container.ptr() == fragmentRange.start.container.ptr())
                    previousRange.end = fragmentRange.end;
                else
                    fragmentRanges.append(fragmentRange);
            }
            allResultRanges.append(WTFMove(fragmentRanges));
        }
    }

    auto tz = adoptCF(CFTimeZoneCopyDefault());
    NSDate *referenceDate = [context objectForKey:getkDataDetectorsReferenceDateKey()] ?: [NSDate date];
    RefPtr<Text> lastTextNodeToUpdate;
    String lastNodeContent;
    unsigned contentOffset = 0;
    DDQueryOffset lastModifiedQueryOffset = { .queryIndex = -1, .offset = 0 };

    // For each result add the link.
    // Since there could be multiple results in the same text node, the node is only modified when
    // we are about to process a different text node.
    CFIndex resultCount = allResults.size();
    for (CFIndex resultIndex = 0; resultIndex < resultCount; ++resultIndex) {
        DDResultRef coreResult = allResults[resultIndex].get();
        DDQueryRange queryRange = PAL::softLink_DataDetectorsCore_DDResultGetQueryRangeForURLification(coreResult);
        auto& resultRanges = allResultRanges[resultIndex];

        // Compare the query offsets to make sure we don't go backwards
        if (queryOffsetCompare(lastModifiedQueryOffset, queryRange.start) >= 0)
            continue;

        if (resultRanges.isEmpty())
            continue;
        
        NSString *identifier = dataDetectorStringForPath(indexPaths[resultIndex].get());
        NSString *correspondingURL = constructURLStringForResult(coreResult, identifier, referenceDate, (NSTimeZone *)tz.get(), types);

        if (!correspondingURL || searchForLinkRemovingExistingDDLinks(resultRanges.first().start.container, resultRanges.last().end.container))
            continue;

        lastModifiedQueryOffset = queryRange.end;
        BOOL shouldUseLightLinks = PAL::softLink_DataDetectorsCore_DDShouldUseLightLinksForResult(coreResult, [indexPaths[resultIndex] length] > 1);

        for (auto& range : resultRanges) {
            auto* parentNode = range.start.container->parentNode();
            if (!parentNode)
                continue;

            if (!is<Text>(range.start.container))
                continue;

            auto& currentTextNode = downcast<Text>(range.start.container.get());
            Document& document = currentTextNode.document();
            String textNodeData;

            if (lastTextNodeToUpdate != &currentTextNode) {
                if (lastTextNodeToUpdate)
                    lastTextNodeToUpdate->setData(lastNodeContent);
                contentOffset = 0;
                if (range.start.offset > 0)
                    textNodeData = currentTextNode.data().left(range.start.offset);
            } else
                textNodeData = currentTextNode.data().substring(contentOffset, range.start.offset - contentOffset);

            if (!textNodeData.isEmpty()) {
                parentNode->insertBefore(Text::create(document, WTFMove(textNodeData)), &currentTextNode);
                contentOffset = range.start.offset;
            }

            // Create the actual anchor node and insert it before the current node.
            textNodeData = currentTextNode.data().substring(range.start.offset, range.end.offset - range.start.offset);
            auto newTextNode = Text::create(document, WTFMove(textNodeData));
            parentNode->insertBefore(newTextNode.copyRef(), &currentTextNode);
            
            Ref<HTMLAnchorElement> anchorElement = HTMLAnchorElement::create(document);
            anchorElement->setHref(correspondingURL);
            anchorElement->setDir("ltr"_s);

            if (shouldUseLightLinks) {
                document.updateStyleIfNeeded();

                auto* renderStyle = parentNode->computedStyle();
                if (renderStyle) {
                    auto textColor = renderStyle->visitedDependentColor(CSSPropertyColor);
                    if (textColor.isValid()) {
                        // FIXME: Consider using LCHA<float> rather than HSLA<float> for better perceptual results and to avoid clamping to sRGB gamut, which is what HSLA does.
                        auto hsla = textColor.toColorTypeLossy<HSLA<float>>().resolved();

                        // Force the lightness of the underline color to the middle, and multiply the alpha by 38%,
                        // so the color will appear on light and dark backgrounds, since only one color can be specified.
                        hsla.lightness = 50.0f;
                        hsla.alpha *= 0.38f;
                        
                        // FIXME: Consider keeping color in LCHA (if that change is made) or converting back to the initial underlying color type to avoid unnecessarily clamping colors outside of sRGB.
                        auto underlineColor = convertColor<SRGBA<uint8_t>>(hsla);

                        anchorElement->setInlineStyleProperty(CSSPropertyColor, CSSValueCurrentcolor);
                        anchorElement->setInlineStyleProperty(CSSPropertyTextDecorationColor, serializationForCSS(static_cast<Color>(underlineColor)));
                    }
                }
            }

            anchorElement->appendChild(WTFMove(newTextNode));

            // Add a special attribute to mark this URLification as the result of data detectors.
            anchorElement->setAttributeWithoutSynchronization(x_apple_data_detectorsAttr, trueAtom());
            anchorElement->setAttributeWithoutSynchronization(x_apple_data_detectors_typeAttr, dataDetectorTypeForCategory(PAL::softLink_DataDetectorsCore_DDResultGetCategory(coreResult)));
            anchorElement->setAttributeWithoutSynchronization(x_apple_data_detectors_resultAttr, identifier);

            parentNode->insertBefore(WTFMove(anchorElement), &currentTextNode);

            contentOffset = range.end.offset;

            lastNodeContent = currentTextNode.data().substring(range.end.offset, currentTextNode.length() - range.end.offset);
            lastTextNodeToUpdate = &currentTextNode;
        }        
    }

    if (lastTextNodeToUpdate)
        lastTextNodeToUpdate->setData(lastNodeContent);

    return [PAL::getDDScannerResultClass() resultsFromCoreResults:scannerResults.get()];
}

#else

NSArray *DataDetection::detectContentInRange(const SimpleRange&, OptionSet<DataDetectorType>, NSDictionary *)
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

bool DataDetection::isDataDetectorAttribute(const QualifiedName& name)
{
    if (name == x_apple_data_detectorsAttr)
        return true;

    if (name == x_apple_data_detectors_resultAttr)
        return true;

    if (name == x_apple_data_detectors_typeAttr)
        return true;

    if (name == hrefAttr)
        return true;

    return false;
}

bool DataDetection::isDataDetectorElement(const Element& element)
{
    return is<HTMLAnchorElement>(element) && equalLettersIgnoringASCIICase(element.attributeWithoutSynchronization(x_apple_data_detectorsAttr), "true"_s);
}

std::optional<std::pair<Ref<HTMLElement>, IntRect>> DataDetection::findDataDetectionResultElementInImageOverlay(const FloatPoint& location, const HTMLElement& imageOverlayHost)
{
    Vector<Ref<HTMLElement>> dataDetectorElements;
    for (auto& child : descendantsOfType<HTMLElement>(*imageOverlayHost.shadowRoot())) {
        if (ImageOverlay::isDataDetectorResult(child))
            dataDetectorElements.append(child);
    }

    for (auto& element : dataDetectorElements) {
        auto elementBounds = element->boundsInRootViewSpace();
        if (elementBounds.contains(roundedIntPoint(location)))
            return { { WTFMove(element), elementBounds } };
    }

    return std::nullopt;
}

#if ENABLE(IMAGE_ANALYSIS)

Ref<HTMLDivElement> DataDetection::createElementForImageOverlay(Document& document, const TextRecognitionDataDetector& info)
{
    auto container = HTMLDivElement::create(document);
    if (RefPtr frame = document.frame()) {
        auto resultIdentifier = frame->dataDetectionResults().addImageOverlayDataDetectionResult(info.result.get());
        container->setAttributeWithoutSynchronization(x_apple_data_detectors_resultAttr, AtomString::number(resultIdentifier.toUInt64()));
    }
    return container;
}

#endif // ENABLE(IMAGE_ANALYSIS)

} // namespace WebCore

#endif
