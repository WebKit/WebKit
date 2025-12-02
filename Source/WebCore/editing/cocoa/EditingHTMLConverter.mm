/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#import "EditingHTMLConverter.h"

#import "ArchiveResource.h"
#import "BoundaryPointInlines.h"
#import "CSSColorValue.h"
#import "CSSComputedStyleDeclaration.h"
#import "CSSPrimitiveValue.h"
#import "CSSSerializationContext.h"
#import "CharacterData.h"
#import "ColorCocoa.h"
#import "CommonAtomStrings.h"
#import "ComposedTreeIterator.h"
#import "ContainerNodeInlines.h"
#import "Document.h"
#import "DocumentLoader.h"
#import "Editing.h"
#import "ElementChildIteratorInlines.h"
#import "ElementInlines.h"
#import "ElementRareData.h"
#import "ElementTraversal.h"
#import "FontAttributes.h"
#import "FontCascade.h"
#import "FrameDestructionObserverInlines.h"
#import "FrameLoader.h"
#import "HTMLAttachmentElement.h"
#import "HTMLConverter.h"
#import "HTMLElement.h"
#import "HTMLImageElement.h"
#import "HTMLOListElement.h"
#import "LoaderNSURLExtras.h"
#import "LocalFrame.h"
#import "LocalizedStrings.h"
#import "NodeName.h"
#import "RenderImage.h"
#import "RenderObjectStyle.h"
#import "RenderStyleInlines.h"
#import "RenderText.h"
#import "StyleExtractor.h"
#import "StyleProperties.h"
#import "StyledElement.h"
#import "TextIterator.h"
#import "VisibleSelection.h"
#import "WebContentReader.h"
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/TZoneMallocInlines.h>

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#import "PlatformNSAdaptiveImageGlyph.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIFoundationSoftLink.h"
#import <pal/ios/UIKitSoftLink.h>
#endif

#if ENABLE(WRITING_TOOLS)
NSAttributedStringKey const WTWritingToolsPreservedAttributeName = @"WTWritingToolsPreserved";
#endif

namespace WebCore {

template<typename Data>
using ElementCache = WeakHashMap<Element, Data, WeakPtrImplWithEventTargetData>;

static String preferredFilenameForElement(const HTMLImageElement& element)
{
    if (RefPtr attachment = element.attachmentElement()) {
        if (auto title = attachment->attachmentTitle(); !title.isEmpty())
            return title;
    }

    auto altText = element.altText();

    auto urlString = element.imageSourceURL();

    auto suggestedName = [&] -> String {
        Ref document = element.document();
        RetainPtr url = document->completeURL(urlString).createNSURL();
        if (!url)
            url = [NSURL _web_URLWithString:[urlString.createNSString() stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] relativeToURL:nil];

        RefPtr frame = document->frame();
        if (frame->loader().frameHasLoaded()) {
            RefPtr dataSource = frame->loader().documentLoader();
            if (RefPtr resource = dataSource->subresource(url.get())) {
                auto& mimeType = resource->mimeType();

                if (!altText.isEmpty())
                    return suggestedFilenameWithMIMEType(url.get(), mimeType, altText);

                return suggestedFilenameWithMIMEType(url.get(), mimeType);
            }
        }

        return { };
    }();

    if (!suggestedName.isEmpty())
        return suggestedName;

    if (!altText.isEmpty())
        return altText;

    return copyImageUnknownFileLabel();
}

static RetainPtr<NSFileWrapper> fileWrapperForElement(const HTMLImageElement& element)
{
    if (CachedImage* cachedImage = element.cachedImage()) {
        if (RefPtr sharedBuffer = cachedImage->resourceBuffer()) {
            RetainPtr wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:sharedBuffer->makeContiguous()->createNSData().get()]);
            [wrapper setPreferredFilename:preferredFilenameForElement(element).createNSString().get()];
            return wrapper;
        }
    }

    auto* renderer = element.renderer();
    if (auto* renderImage = dynamicDowncast<RenderImage>(renderer)) {
        CachedResourceHandle image = renderImage->cachedImage();
        if (image && !image->errorOccurred()) {
            RetainPtr<NSFileWrapper> wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:(__bridge NSData *)image->imageForRenderer(renderer)->adapter().tiffRepresentation()]);
            [wrapper setPreferredFilename:@"image.tiff"];
            return wrapper;
        }
    }

    return nil;
}

static RetainPtr<NSFileWrapper> fileWrapperForElement(const HTMLAttachmentElement& element)
{
    auto identifier = element.uniqueIdentifier();

    RetainPtr data = [identifier.createNSString() dataUsingEncoding:NSUTF8StringEncoding];
    if (!data)
        return nil;

    // Use a filename prefixed with a sentinel value to indicate that the data is corresponding
    // to an existing HTMLAttachmentElement.

    RetainPtr wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data.get()]);
    [wrapper setPreferredFilename:makeString(WebContentReader::placeholderAttachmentFilenamePrefix, identifier).createNSString().get()];
    return wrapper;
}

static RetainPtr<NSAttributedString> attributedStringWithAttachmentForFileWrapper(NSFileWrapper *fileWrapper)
{
    if (!fileWrapper)
        return adoptNS([[NSAttributedString alloc] initWithString:@" "]).autorelease();

    RetainPtr attachment = adoptNS([[PlatformNSTextAttachment alloc] initWithFileWrapper:fileWrapper]);
    return [NSAttributedString attributedStringWithAttachment:attachment.get()];
}

static RetainPtr<NSAttributedString> attributedStringWithAttachmentForElement(const HTMLImageElement& element)
{
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    if (element.isMultiRepresentationHEIC()) {
        if (RefPtr image = element.image()) {
            if (NSAdaptiveImageGlyph *attachment = image->adapter().multiRepresentationHEIC()) {
                RetainPtr attachmentString = adoptNS([[NSString alloc] initWithFormat:@"%C", static_cast<unichar>(NSAttachmentCharacter)]);
                RetainPtr attributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:attachmentString.get()]);
                [attributedString addAttribute:NSAdaptiveImageGlyphAttributeName value:attachment range:NSMakeRange(0, 1)];
                return attributedString;
            }
        }
    }
#endif

    RetainPtr fileWrapper = fileWrapperForElement(element);
    return attributedStringWithAttachmentForFileWrapper(fileWrapper.get());
}

static RetainPtr<NSAttributedString> attributedStringWithAttachmentForElement(const HTMLAttachmentElement& element)
{
    RetainPtr fileWrapper = fileWrapperForElement(element);
    return attributedStringWithAttachmentForFileWrapper(fileWrapper.get());
}

#if ENABLE(WRITING_TOOLS)
static bool elementQualifiesForWritingToolsPreservation(Element* element)
{
    // If the element is a mail blockquote, it should be preserved after a Writing Tools composition.
    if (isMailBlockquote(*element))
        return true;

    // If the element is a tab span node, it is a tab character with `whitespace:pre`, and need not be preserved.
    if (tabSpanNode(element))
        return false;

    // If the element has no renderer, it can't have `whitespace:pre` so it need not be preserved.
    auto renderer = element->renderer();
    if (!renderer)
        return false;

    // If the element has `white-space:pre` (except for the aforementioned exceptions), it should be preserved.
    // `white-space:pre` is a shorthand for white-space-collapse:preserve && text-wrap-mode::no-wrap.
    if (renderer->style().whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve
        && renderer->style().textWrapMode() == TextWrapMode::NoWrap)
        return true;

    // Otherwise, it need not be preserved.
    return false;
}

static bool hasAncestorQualifyingForWritingToolsPreservation(Element* ancestor, ElementCache<bool>& cache)
{
    if (!ancestor)
        return false;

    auto entry = cache.find(*ancestor);
    if (entry == cache.end()) {
        auto result = elementQualifiesForWritingToolsPreservation(ancestor) || hasAncestorQualifyingForWritingToolsPreservation(ancestor->protectedParentElement().get(), cache);

        cache.set(*ancestor, result);
        return result;
    }

    return entry->value;
}
#endif // ENABLE(WRITING_TOOLS)

static RefPtr<Element> enclosingElement(const Node& node, ElementCache<RefPtr<Element>>& cache, Function<bool(Element *)>&& predicate)
{
    Vector<Ref<Element>> ancestors;
    RefPtr<Element> result;
    for (RefPtr ancestor = node.parentElementInComposedTree(); ancestor; ancestor = ancestor->parentElementInComposedTree()) {
        if (predicate(ancestor.get())) {
            result = ancestor.get();
            break;
        }

        auto entry = cache.find(*ancestor);
        if (entry != cache.end()) {
            result = entry->value;
            break;
        }

        ancestors.append(*ancestor);
    }

    for (auto& ancestor : ancestors)
        cache.add(ancestor.get(), result);

    return result;
}

static RefPtr<Element> enclosingLinkElement(const Node& node, ElementCache<RefPtr<Element>>& cache)
{
    return enclosingElement(node, cache, [](auto* element) {
        return element->isLink();
    });
}

static RefPtr<Element> enclosingListElement(const Node& node, ElementCache<RefPtr<Element>>& cache)
{
    return enclosingElement(node, cache, [](auto* element) {
        return element->hasTagName(HTMLNames::olTag) || element->hasTagName(HTMLNames::ulTag);
    });
}

// The `enclosingListCache` associates an element with its nearest enclosing list ancestor that (ol or ul)
// and the `textListsForListElements` associates an enclosing list element with its array of text lists.
static void associateElementWithTextLists(Element* element, ElementCache<RefPtr<Element>>& enclosingListCache, ElementCache<RetainPtr<NSArray<NSTextList *>>>& textListsForListElements)
{
    // Base case #1:
    // Eventually, an element will be null since there will be no more list elements in the ancestor hierarchy to find.
    // Therefore, associate the element with an empty array of text lists.
    if (!element)
        return;

    // Base case #2:
    // If the element is already associated with an array of text lists, no work needs to be done.
    if (textListsForListElements.contains(*element))
        return;

    // Recursive case:
    // The NSAttributedString API contract requires that the array of NSTextList's start at the outermost list
    // and end at the innermost list. In the TextIterator traversal, for a given element, it's "enclosing list element"
    // will be the innermost list, by definition.
    //
    // For nested lists, things get a bit complicated. The antecedents of the inner most text list correspond to the
    // associated text lists for each enclosing list element of the one before, recursively. Consequently, memoization
    // is used to acquire all antecedents of the element's text list, and then the final array of text lists for the
    // element is the array of the text list antecedents followed by the element's own text list itself.

    RetainPtr ancestors = adoptNS([[NSArray alloc] init]);

    if (RefPtr parent = enclosingListElement(*element, enclosingListCache)) {
        associateElementWithTextLists(parent.get(), enclosingListCache, textListsForListElements);
        ancestors = textListsForListElements.get(*parent);
    }

    TextList list;
    list.ordered = element->hasTagName(HTMLNames::olTag);

    if (CheckedPtr renderer = element->renderer())
        list.styleType = renderer->style().listStyleType();

    if (RefPtr olElement = dynamicDowncast<HTMLOListElement>(element))
        list.startingItemNumber = olElement->start();

    RetainPtr nsTextList = list.createTextList();
    RetainPtr allLists = [ancestors arrayByAddingObject:nsTextList.get()];
    textListsForListElements.set(*element, allLists);
}

// FIXME: Encapsulate all these parameters into a type for readability and maintainability.
static void updateAttributes(const Node* node, const RenderStyle& style, OptionSet<IncludedElement> includedElements, ElementCache<bool>& elementQualifiesForWritingToolsPreservationCache, ElementCache<RefPtr<Element>>& enclosingLinkCache, ElementCache<RefPtr<Element>>& enclosingListCache, NSMutableDictionary<NSAttributedStringKey, id> *attributes, ElementCache<RetainPtr<NSArray<NSTextList *>>>& textListsForListElements)
{
#if ENABLE(WRITING_TOOLS)
    if (includedElements.contains(IncludedElement::PreservedContent)) {
        if (hasAncestorQualifyingForWritingToolsPreservation(node->protectedParentElement().get(), elementQualifiesForWritingToolsPreservationCache))
            [attributes setObject:@(1) forKey:WTWritingToolsPreservedAttributeName];
        else
            [attributes removeObjectForKey:WTWritingToolsPreservedAttributeName];
    }
#else
    UNUSED_PARAM(node);
    UNUSED_PARAM(includedElements);
    UNUSED_PARAM(elementQualifiesForWritingToolsPreservationCache);
#endif

    if (style.textDecorationLineInEffect().hasUnderline())
        [attributes setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];
    else
        [attributes removeObjectForKey:NSUnderlineStyleAttributeName];

    if (style.textDecorationLineInEffect().hasLineThrough())
        [attributes setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSStrikethroughStyleAttributeName];
    else
        [attributes removeObjectForKey:NSStrikethroughStyleAttributeName];

    CheckedRef fontCascade = style.fontCascade();
    if (auto ctFont = fontCascade->primaryFont()->ctFont())
        [attributes setObject:(__bridge PlatformFont *)ctFont forKey:NSFontAttributeName];
    else {
        auto size = fontCascade->primaryFont()->platformData().size();
#if PLATFORM(IOS_FAMILY)
        PlatformFont *platformFont = [PlatformFontClass systemFontOfSize:size];
#else
        PlatformFont *platformFont = [[NSFontManager sharedFontManager] convertFont:WebDefaultFont() toSize:size];
#endif
        [attributes setObject:platformFont forKey:NSFontAttributeName];
    }

    auto textAlignment = NSTextAlignmentNatural;
    switch (style.textAlign()) {
    case Style::TextAlign::Right:
    case Style::TextAlign::WebKitRight:
        textAlignment = NSTextAlignmentRight;
        break;
    case Style::TextAlign::Left:
    case Style::TextAlign::WebKitLeft:
        textAlignment = NSTextAlignmentLeft;
        break;
    case Style::TextAlign::Center:
    case Style::TextAlign::WebKitCenter:
        textAlignment = NSTextAlignmentCenter;
        break;
    case Style::TextAlign::Justify:
        textAlignment = NSTextAlignmentJustified;
        break;
    case Style::TextAlign::Start:
        if (style.hasExplicitlySetDirection())
            textAlignment = style.isLeftToRightDirection() ? NSTextAlignmentLeft : NSTextAlignmentRight;
        break;
    case Style::TextAlign::End:
        textAlignment = style.isLeftToRightDirection() ? NSTextAlignmentRight : NSTextAlignmentLeft;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (textAlignment != NSTextAlignmentNatural) {
        RetainPtr<NSMutableParagraphStyle> paragraphStyle = adoptNS([(attributes[NSParagraphStyleAttributeName] ?: [PlatformNSParagraphStyle defaultParagraphStyle]) mutableCopy]);
        [paragraphStyle setAlignment:textAlignment];
        [attributes setObject:paragraphStyle.get() forKey:NSParagraphStyleAttributeName];
    }

    Color foregroundColor = style.visitedDependentColorWithColorFilter(CSSPropertyColor);
    if (foregroundColor.isVisible())
        [attributes setObject:cocoaColor(foregroundColor).get() forKey:NSForegroundColorAttributeName];
    else
        [attributes removeObjectForKey:NSForegroundColorAttributeName];

    Color backgroundColor = style.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (backgroundColor.isVisible())
        [attributes setObject:cocoaColor(backgroundColor).get() forKey:NSBackgroundColorAttributeName];
    else
        [attributes removeObjectForKey:NSBackgroundColorAttributeName];

    auto linkURL = [&] -> URL {
        RefPtr enclosingLink = enclosingLinkElement(*node, enclosingLinkCache);
        if (!enclosingLink)
            return { };

        return enclosingLink->absoluteLinkURL();
    }();

    if (RetainPtr linkNSURL = linkURL.createNSURL(); linkURL.isEmpty() || !linkNSURL)
        [attributes removeObjectForKey:NSLinkAttributeName];
    else
        [attributes setObject:linkNSURL.get() forKey:NSLinkAttributeName];

    if (includedElements.contains(IncludedElement::TextLists)) {
        if (RefPtr enclosingList = enclosingListElement(*node, enclosingListCache)) {
            RetainPtr<NSMutableParagraphStyle> paragraphStyle = adoptNS([(attributes[NSParagraphStyleAttributeName] ?: [PlatformNSParagraphStyle defaultParagraphStyle]) mutableCopy]);

            associateElementWithTextLists(enclosingList.get(), enclosingListCache, textListsForListElements);
            if (RetainPtr lists = textListsForListElements.get(*enclosingList))
                [paragraphStyle setTextLists:lists.get()];

            attributes[NSParagraphStyleAttributeName] = paragraphStyle.get();
        } else if (RetainPtr<NSMutableParagraphStyle> paragraphStyle = adoptNS([attributes[NSParagraphStyleAttributeName] mutableCopy])) {
            [paragraphStyle setTextLists:@[ ]];
            attributes[NSParagraphStyleAttributeName] = paragraphStyle.get();
        }
    }
}

// This function uses TextIterator, which makes offsets in its result compatible with HTML editing.
enum class ReplaceAllNoBreakSpaces : bool { No, Yes };
static AttributedString editingAttributedStringInternal(const SimpleRange& range, TextIteratorBehaviors behaviors, OptionSet<IncludedElement> includedElements, ReplaceAllNoBreakSpaces replaceAllNoBreakSpaces)
{
    ElementCache<RefPtr<Element>> enclosingLinkCache;
    ElementCache<RefPtr<Element>> enclosingListCache;
    ElementCache<RetainPtr<NSArray<NSTextList *>>> textListsForListElements;
    ElementCache<bool> elementQualifiesForWritingToolsPreservationCache;

    RetainPtr string = adoptNS([[NSMutableAttributedString alloc] init]);
    RetainPtr attributes = adoptNS([[NSMutableDictionary alloc] init]);
    NSUInteger stringLength = 0;
    for (TextIterator it { range, behaviors }; !it.atEnd(); it.advance()) {
        RefPtr node = it.node();

        if (RefPtr imageElement = dynamicDowncast<HTMLImageElement>(node.get()); imageElement && includedElements.contains(IncludedElement::Images)) {
            RetainPtr attachmentAttributedString = attributedStringWithAttachmentForElement(*imageElement);
            [string appendAttributedString:attachmentAttributedString.get()];
            stringLength += [attachmentAttributedString length];
        }

        if (RefPtr attachmentElement = dynamicDowncast<HTMLAttachmentElement>(node.get()); attachmentElement && includedElements.contains(IncludedElement::Attachments)) {
            RetainPtr attachmentAttributedString = attributedStringWithAttachmentForElement(*attachmentElement);
            [string appendAttributedString:attachmentAttributedString.get()];
            stringLength += [attachmentAttributedString length];
        }

        auto currentTextLength = it.text().length();
        if (!currentTextLength)
            continue;

        // In some cases the text iterator emits text that is not associated with a node.
        // In those cases, base the style on the container.
        if (!node)
            node = it.range().start.container.ptr();
        auto renderer = node->renderer();

        if (renderer)
            updateAttributes(node.get(), renderer->checkedStyle(), includedElements, elementQualifiesForWritingToolsPreservationCache, enclosingLinkCache, enclosingListCache, attributes.get(), textListsForListElements);
        else if (!includedElements.contains(IncludedElement::NonRenderedContent))
            continue;

        bool replaceNoBreakSpaces = [&] {
            if (replaceAllNoBreakSpaces == ReplaceAllNoBreakSpaces::Yes)
                return true;

            return renderer && renderer->style().nbspMode() == NBSPMode::Space;
        }();

        RetainPtr<NSString> text;
        if (!replaceNoBreakSpaces)
            text = it.text().createNSStringWithoutCopying();
        else
            text = makeStringByReplacingAll(it.text(), noBreakSpace, ' ').createNSString();

        [string replaceCharactersInRange:NSMakeRange(stringLength, 0) withString:text.get()];
        [string setAttributes:attributes.get() range:NSMakeRange(stringLength, currentTextLength)];
        stringLength += currentTextLength;
    }

    return AttributedString::fromNSAttributedString(WTFMove(string));
}

AttributedString editingAttributedString(const SimpleRange& range, OptionSet<IncludedElement> includedElements)
{
    return editingAttributedStringInternal(range, { }, includedElements, ReplaceAllNoBreakSpaces::No);
}

AttributedString editingAttributedStringReplacingNoBreakSpace(const SimpleRange& range, TextIteratorBehaviors behaviors, OptionSet<IncludedElement> includedElements)
{
    return editingAttributedStringInternal(range, behaviors, includedElements, ReplaceAllNoBreakSpaces::Yes);
}

}
