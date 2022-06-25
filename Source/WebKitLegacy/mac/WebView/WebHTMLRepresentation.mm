/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#import "WebHTMLRepresentation.h"

#import "DOMElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebArchive.h"
#import "WebBasePluginPackage.h"
#import "WebDataSourceInternal.h"
#import "WebDocumentPrivate.h"
#import "WebFrameInternal.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSObjectExtras.h"
#import "WebView.h"
#import <Foundation/NSURLResponse.h>
#import <JavaScriptCore/RegularExpression.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/Editor.h>
#import <WebCore/ElementInlines.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HTMLFormControlElement.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/HTMLTableCellElement.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/NodeTraversal.h>
#import <WebCore/Range.h>
#import <WebCore/RenderElement.h>
#import <WebCore/ScriptDisallowedScope.h>
#import <WebCore/TextResourceDecoder.h>
#import <WebKitLegacy/DOMHTMLInputElement.h>
#import <wtf/Assertions.h>
#import <wtf/FixedVector.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringBuilder.h>

using namespace WebCore;
using namespace HTMLNames;
using JSC::Yarr::RegularExpression;

@interface WebHTMLRepresentationPrivate : NSObject {
@public
    WebDataSource *dataSource;
    
    BOOL hasSentResponseToPlugin;
    BOOL includedInWebKitStatistics;

    id <WebPluginManualLoader> manualLoader;
    NSView *pluginView;
}
@end

@implementation WebHTMLRepresentationPrivate
@end

@implementation WebHTMLRepresentation

static RetainPtr<NSArray> createNSArray(const HashSet<String, ASCIICaseInsensitiveHash>& set)
{
    auto vector = copyToVectorOf<NSString *>(set);
    return adoptNS([[NSArray alloc] initWithObjects:vector.data() count:vector.size()]);
}

+ (NSArray *)supportedMIMETypes
{
    static NeverDestroyed<RetainPtr<NSArray>> staticSupportedMIMETypes = [[[self supportedNonImageMIMETypes] arrayByAddingObjectsFromArray:
        [self supportedImageMIMETypes]] arrayByAddingObjectsFromArray:[self supportedMediaMIMETypes]];
    return staticSupportedMIMETypes.get().get();
}

+ (NSArray *)supportedMediaMIMETypes
{
    static NeverDestroyed<RetainPtr<NSArray>> staticSupportedMediaMIMETypes = createNSArray(MIMETypeRegistry::supportedMediaMIMETypes());
    return staticSupportedMediaMIMETypes.get().get();
}

+ (NSArray *)supportedNonImageMIMETypes
{
    static NeverDestroyed<RetainPtr<NSArray>> staticSupportedNonImageMIMETypes = createNSArray(MIMETypeRegistry::supportedNonImageMIMETypes());
    return staticSupportedNonImageMIMETypes.get().get();
}

+ (NSArray *)supportedImageMIMETypes
{
    static NeverDestroyed<RetainPtr<NSArray>> staticSupportedImageMIMETypes = createNSArray(MIMETypeRegistry::supportedImageMIMETypes());
    return staticSupportedImageMIMETypes.get().get();
}

+ (NSArray *)unsupportedTextMIMETypes
{
    static NeverDestroyed<RetainPtr<NSArray>> staticUnsupportedTextMIMETypes = createNSArray(MIMETypeRegistry::unsupportedTextMIMETypes());
    return staticUnsupportedTextMIMETypes.get().get();
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    _private = [[WebHTMLRepresentationPrivate alloc] init];

    return self;
}

- (void)dealloc
{
    if (_private && _private->includedInWebKitStatistics)
        --WebHTMLRepresentationCount;

    [_private release];

    [super dealloc];
}

- (void)_redirectDataToManualLoader:(id<WebPluginManualLoader>)manualLoader forPluginView:(NSView *)pluginView
{
    _private->manualLoader = manualLoader;
    _private->pluginView = pluginView;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    _private->dataSource = dataSource;

    if (!_private->includedInWebKitStatistics && [[dataSource webFrame] _isIncludedInWebKitStatistics]) {
        _private->includedInWebKitStatistics = YES;
        ++WebHTMLRepresentationCount;
    }
}

- (BOOL)_isDisplayingWebArchive
{
    return [[_private->dataSource _responseMIMEType] _webkit_isCaseInsensitiveEqualToString:@"application/x-webarchive"];
}

- (void)receivedData:(NSData *)data withDataSource:(WebDataSource *)dataSource
{
    auto protectedSelf = retainPtr(self);
    WebFrame *webFrame = [dataSource webFrame];
    if (!webFrame)
        return;

    if (!_private->pluginView)
        [webFrame _commitData:data];

    // If the document is a stand-alone media document, now is the right time to cancel the WebKit load
    Frame* coreFrame = core(webFrame);
    if (coreFrame->document()->isMediaDocument() && coreFrame->loader().documentLoader())
        coreFrame->loader().documentLoader()->cancelMainResourceLoad(coreFrame->loader().client().pluginWillHandleLoadError(coreFrame->loader().documentLoader()->response()));

    if (_private->pluginView) {
        if (!_private->hasSentResponseToPlugin) {
            [_private->manualLoader pluginView:_private->pluginView receivedResponse:[dataSource response]];
            _private->hasSentResponseToPlugin = YES;
        }
        
        [_private->manualLoader pluginView:_private->pluginView receivedData:data];
    }
}

- (void)receivedError:(NSError *)error withDataSource:(WebDataSource *)dataSource
{
    if (_private->pluginView) {
        [_private->manualLoader pluginView:_private->pluginView receivedError:error];
    }
}

- (void)finishedLoadingWithDataSource:(WebDataSource *)dataSource
{
    WebFrame* webFrame = [dataSource webFrame];

    if (_private->pluginView) {
        [_private->manualLoader pluginViewFinishedLoading:_private->pluginView];
        return;
    }

    if (!webFrame)
        return;
    WebView *webView = [webFrame webView];
    if ([webView mainFrame] == webFrame && [webView isEditable])
        core(webFrame)->editor().applyEditingStyleToBodyElement();
}

- (BOOL)canProvideDocumentSource
{
    return [[_private->dataSource webFrame] _canProvideDocumentSource];
}

- (BOOL)canSaveAsWebArchive
{
    return [[_private->dataSource webFrame] _canSaveAsWebArchive];
}

- (NSString *)documentSource
{
    if ([self _isDisplayingWebArchive]) {            
        auto *parsedArchiveData = [_private->dataSource _documentLoader]->parsedArchiveData();
        return adoptNS([[NSString alloc] initWithData:parsedArchiveData ? parsedArchiveData->createNSData().get() : nil encoding:NSUTF8StringEncoding]).autorelease();
    }

    Frame* coreFrame = core([_private->dataSource webFrame]);
    if (!coreFrame)
        return nil;
    Document* document = coreFrame->document();
    if (!document)
        return nil;
    TextResourceDecoder* decoder = document->decoder();
    if (!decoder)
        return nil;
    NSData *data = [_private->dataSource data];
    if (!data)
        return nil;
    return decoder->encoding().decode(reinterpret_cast<const char*>([data bytes]), [data length]);
}

- (NSString *)title
{
    return nsStringNilIfEmpty([_private->dataSource _documentLoader]->title().string);
}

- (DOMDocument *)DOMDocument
{
    return [[_private->dataSource webFrame] DOMDocument];
}

#if PLATFORM(MAC)

- (NSAttributedString *)attributedText
{
    return nil;
}

- (NSAttributedString *)attributedStringFrom:(DOMNode *)startNode startOffset:(int)startOffset to:(DOMNode *)endNode endOffset:(int)endOffset
{
    if (!startNode || !endNode)
        return adoptNS([[NSAttributedString alloc] init]).autorelease();
    auto range = SimpleRange { { *core(startNode), static_cast<unsigned>(startOffset) }, { *core(endNode), static_cast<unsigned>(endOffset) } };
    return editingAttributedString(range).string.autorelease();
}

#endif

static HTMLFormElement* formElementFromDOMElement(DOMElement *element)
{
    Element* node = core(element);
    return node && node->hasTagName(formTag) ? static_cast<HTMLFormElement*>(node) : nullptr;
}

- (DOMElement *)elementWithName:(NSString *)name inForm:(DOMElement *)form
{
    HTMLFormElement* formElement = formElementFromDOMElement(form);
    if (!formElement)
        return nil;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    AtomString targetName = name;
    for (auto& weakElement : formElement->unsafeAssociatedElements()) {
        RefPtr element { weakElement.get() };
        if (element && element->asFormAssociatedElement()->name() == targetName)
            return kit(element.get());
    }
    return nil;
}

static HTMLInputElement* inputElementFromDOMElement(DOMElement* element)
{
    Element* node = core(element);
    return dynamicDowncast<HTMLInputElement>(node);
}

- (BOOL)elementDoesAutoComplete:(DOMElement *)element
{
    HTMLInputElement* inputElement = inputElementFromDOMElement(element);
    return inputElement
        && inputElement->isTextField()
        && !inputElement->isPasswordField()
        && inputElement->shouldAutocomplete();
}

- (BOOL)elementIsPassword:(DOMElement *)element
{
    HTMLInputElement* inputElement = inputElementFromDOMElement(element);
    return inputElement && inputElement->isPasswordField();
}

- (DOMElement *)formForElement:(DOMElement *)element
{
    auto inputElement = inputElementFromDOMElement(element);
    return inputElement ? kit(inputElement->form()) : nil;
}

- (DOMElement *)currentForm
{
    return kit(core([_private->dataSource webFrame])->selection().currentForm());
}

- (NSArray *)controlsInForm:(DOMElement *)form
{
    auto formElement = formElementFromDOMElement(form);
    if (!formElement)
        return nil;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    auto result = createNSArray(formElement->unsafeAssociatedElements(), [] (auto& weakElement) -> DOMElement * {
        RefPtr coreElement { weakElement.get() };
        if (!coreElement || !coreElement->asFormAssociatedElement()->isEnumeratable()) // Skip option elements, other duds
            return nil;
        return kit(coreElement.get());
    });
    return [result count] ? result.autorelease() : nil;
}

// Either get cached regexp or build one that matches any of the labels.
// The regexp we build is of the form:  (STR1|STR2|STRN)
static RegularExpression* regExpForLabels(NSArray *labels)
{
    // All the ObjC calls in this method are simple array and string
    // calls which we can assume do not raise exceptions

    // Parallel arrays that we use to cache regExps.  In practice the number of expressions
    // that the app will use is equal to the number of locales is used in searching.
    static const unsigned int regExpCacheSize = 4;
    static NeverDestroyed<RetainPtr<NSMutableArray>> regExpLabels = adoptNS([[NSMutableArray alloc] initWithCapacity:regExpCacheSize]);
    static NeverDestroyed<Vector<RegularExpression*>> regExps;
    static NeverDestroyed<RegularExpression> wordRegExp("\\w"_s);

    RegularExpression* result;
    CFIndex cacheHit = [regExpLabels.get() indexOfObject:labels];
    if (cacheHit != NSNotFound)
        result = regExps.get().at(cacheHit);
    else {
        StringBuilder pattern;
        pattern.append('(');
        unsigned numLabels = [labels count];
        unsigned i;
        for (i = 0; i < numLabels; i++) {
            String label = [labels objectAtIndex:i];

            bool startsWithWordCharacter = false;
            bool endsWithWordCharacter = false;
            if (label.length()) {
                StringView labelView { label };
                startsWithWordCharacter = wordRegExp.get().match(labelView.left(1)) >= 0;
                endsWithWordCharacter = wordRegExp.get().match(labelView.right(1)) >= 0;
            }
            
            // Search for word boundaries only if label starts/ends with "word characters".
            // If we always searched for word boundaries, this wouldn't work for languages such as Japanese.
            pattern.append(i ? "|" : "", startsWithWordCharacter ? "\\b" : "", label, endsWithWordCharacter ? "\\b" : "");
        }
        pattern.append(')');
        result = new RegularExpression(pattern.toString(), JSC::Yarr::TextCaseInsensitive);
    }

    // add regexp to the cache, making sure it is at the front for LRU ordering
    if (cacheHit != 0) {
        if (cacheHit != NSNotFound) {
            // remove from old spot
            [regExpLabels.get() removeObjectAtIndex:cacheHit];
            regExps.get().remove(cacheHit);
        }
        // add to start
        [regExpLabels.get() insertObject:labels atIndex:0];
        regExps.get().insert(0, result);
        // trim if too big
        if ([regExpLabels.get() count] > regExpCacheSize) {
            [regExpLabels.get() removeObjectAtIndex:regExpCacheSize];
            RegularExpression* last = regExps.get().last();
            regExps.get().removeLast();
            delete last;
        }
    }
    return result;
}

// FIXME: This should take an Element&.
static NSString* searchForLabelsBeforeElement(Frame* frame, NSArray* labels, Element* element, size_t* resultDistance, bool* resultIsInCellAbove)
{
    ASSERT(element);
    RegularExpression* regExp = regExpForLabels(labels);
    // We stop searching after we've seen this many chars
    constexpr unsigned charsSearchedThreshold = 500;
    // This is the absolute max we search.  We allow a little more slop than
    // charsSearchedThreshold, to make it more likely that we'll search whole nodes.
    constexpr unsigned maxCharsSearched = 600;
    // If the starting element is within a table, the cell that contains it
    HTMLTableCellElement* startingTableCell = nullptr;
    bool searchedCellAbove = false;
    
    if (resultDistance)
        *resultDistance = notFound;
    if (resultIsInCellAbove)
        *resultIsInCellAbove = false;

    // walk backwards in the node tree, until another element, or form, or end of tree
    unsigned lengthSearched = 0;
    Node* n;
    for (n = NodeTraversal::previous(*element); n && lengthSearched < charsSearchedThreshold; n = NodeTraversal::previous(*n)) {
        if (is<HTMLFormElement>(*n) || is<HTMLFormControlElement>(*n)) {
            // We hit another form element or the start of the form - bail out
            break;
        }
        if (n->hasTagName(tdTag) && !startingTableCell) {
            startingTableCell = static_cast<HTMLTableCellElement*>(n);
        } else if (n->hasTagName(trTag) && startingTableCell) {
            NSString *result = frame->searchForLabelsAboveCell(*regExp, startingTableCell, resultDistance);
            if ([result length]) {
                if (resultIsInCellAbove)
                    *resultIsInCellAbove = true;
                return result;
            }
            searchedCellAbove = true;
        } else if (n->isTextNode() && n->renderer() && n->renderer()->style().visibility() == Visibility::Visible) {
            // For each text chunk, run the regexp
            String nodeString = n->nodeValue();
            // add 100 for slop, to make it more likely that we'll search whole nodes
            if (lengthSearched + nodeString.length() > maxCharsSearched)
                nodeString = nodeString.right(charsSearchedThreshold - lengthSearched);
            int pos = regExp->searchRev(nodeString);
            if (pos >= 0) {
                if (resultDistance)
                    *resultDistance = lengthSearched;
                return nodeString.substring(pos, regExp->matchedLength());
            }
            lengthSearched += nodeString.length();
        }
    }

    // If we started in a cell, but bailed because we found the start of the form or the
    // previous element, we still might need to search the row above us for a label.
    if (startingTableCell && !searchedCellAbove) {
        NSString *result = frame->searchForLabelsAboveCell(*regExp, startingTableCell, resultDistance);
        if ([result length]) {
            if (resultIsInCellAbove)
                *resultIsInCellAbove = true;
            return result;
        }
    }
    
    return nil;
}

static NSString *matchLabelsAgainstString(NSArray *labels, const String& stringToMatch)
{
    if (stringToMatch.isEmpty())
        return nil;
    
    String mutableStringToMatch = stringToMatch;
    
    // Make numbers and _'s in field names behave like word boundaries, e.g., "address2"
    replace(mutableStringToMatch, RegularExpression("\\d"_s), " "_s);
    mutableStringToMatch = makeStringByReplacingAll(mutableStringToMatch, '_', ' ');
    
    RegularExpression* regExp = regExpForLabels(labels);
    // Use the largest match we can find in the whole string
    int pos;
    int length;
    int bestPos = -1;
    int bestLength = -1;
    int start = 0;
    do {
        pos = regExp->match(mutableStringToMatch, start);
        if (pos != -1) {
            length = regExp->matchedLength();
            if (length >= bestLength) {
                bestPos = pos;
                bestLength = length;
            }
            start = pos + 1;
        }
    } while (pos != -1);
    
    if (bestPos != -1)
        return mutableStringToMatch.substring(bestPos, bestLength);
    return nil;
}

static NSString *matchLabelsAgainstElement(NSArray *labels, Element* element)
{
    if (!element)
        return nil;

    // Match against the name element, then against the id element if no match is found for the name element.
    // See 7538330 for one popular site that benefits from the id element check.
    auto resultFromNameAttribute = matchLabelsAgainstString(labels, element->attributeWithoutSynchronization(nameAttr));
    if (resultFromNameAttribute.length)
        return resultFromNameAttribute;

    return matchLabelsAgainstString(labels, element->attributeWithoutSynchronization(idAttr));
}


- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element
{
    return [self searchForLabels:labels beforeElement:element resultDistance:nullptr resultIsInCellAbove:nullptr];
}

- (NSString *)searchForLabels:(NSArray *)labels beforeElement:(DOMElement *)element resultDistance:(NSUInteger*)outDistance resultIsInCellAbove:(BOOL*)outIsInCellAbove
{
    size_t distance;
    bool isInCellAbove;
    
    NSString *result = searchForLabelsBeforeElement(core([_private->dataSource webFrame]), labels, core(element), &distance, &isInCellAbove);
    
    if (outDistance) {
        if (distance == notFound)
            *outDistance = NSNotFound;
        else
            *outDistance = distance;
    }

    if (outIsInCellAbove)
        *outIsInCellAbove = isInCellAbove;
    
    return result;
}

- (NSString *)matchLabels:(NSArray *)labels againstElement:(DOMElement *)element
{
    return matchLabelsAgainstElement(labels, core(element));
}

@end
