/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#import "AXGeometryManager.h"
#import "AXIsolatedObject.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE) && PLATFORM(MAC)

#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/cocoa/AccessibilitySupportSPI.h>
#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>

namespace WebCore {

void appendBasePlatformProperties(AXPropertyVector& properties, OptionSet<AXPropertyFlag>& propertyFlags, const Ref<AccessibilityObject>& object)
{
    auto setProperty = [&] (AXProperty property, AXPropertyValueVariant&& value) {
        setPropertyIn(property, WTFMove(value), properties, propertyFlags);
    };

    // These attributes are used to serve APIs on static text, but, we cache them on the highest-level ancestor
    // to avoid caching the same value multiple times.
    auto* parent = object->parentObject();
    auto style = object->stylesForAttributedString();
    std::optional<RetainPtr<CTFontRef>> parentFont = parent ? std::optional(parent->font()) : std::nullopt;
    if (!parentFont || parentFont != style.font)
        setProperty(AXProperty::Font, style.font);
    std::optional<Color> parentTextColor = parent ? std::optional(parent->textColor()) : std::nullopt;
    if (!parentTextColor || *parentTextColor != style.textColor)
        setProperty(AXProperty::TextColor, WTFMove(style.textColor));
}

void appendPlatformProperties(AXPropertyVector& properties, OptionSet<AXPropertyFlag>& propertyFlags, const Ref<AccessibilityObject>& object)
{
    auto setProperty = [&] (AXProperty property, AXPropertyValueVariant&& value) {
        setPropertyIn(property, WTFMove(value), properties, propertyFlags);
    };

    setProperty(AXProperty::HasApplePDFAnnotationAttribute, object->hasApplePDFAnnotationAttribute());
    setProperty(AXProperty::SpeakAs, object->speakAs());
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (object->isStaticText()) {
        auto style = object->stylesForAttributedString();
        // Font and TextColor are handled in initializeBasePlatformProperties, since ignored objects could be "containers" where those styles are set.
        setProperty(AXProperty::BackgroundColor, WTFMove(style.backgroundColor));
        setProperty(AXProperty::HasLinethrough, style.hasLinethrough());
        setProperty(AXProperty::HasTextShadow, style.hasTextShadow);
        setProperty(AXProperty::IsSubscript, style.isSubscript);
        setProperty(AXProperty::IsSuperscript, style.isSuperscript);
        setProperty(AXProperty::LinethroughColor, style.linethroughColor());
        if (style.hasUnderline())
            setProperty(AXProperty::UnderlineColor, style.underlineColor());
        setProperty(AXProperty::FontOrientation, object->fontOrientation());
    }

    if (object->shouldCacheStringValue())
        setProperty(AXProperty::StringValue, object->stringValue().isolatedCopy());
#endif // ENABLE(AX_THREAD_TEXT_APIS)

#if !ENABLE(AX_THREAD_TEXT_APIS)
    // Do not cache AXProperty::AttributedText or AXProperty::TextContent when ENABLE(AX_THREAD_TEXT_APIS).
    // We should instead be synthesizing these on-the-fly using AXProperty::TextRuns.

    RetainPtr<NSAttributedString> attributedText;
    // FIXME: Don't eagerly cache textarea/contenteditable values longer than 12500 characters as rangeForCharacterRange is very expensive.
    // This should be cached once rangeForCharacterRange is more performant for large text control values.
    unsigned textControlLength = isTextControl() ? object->text().length() : 0;
    if (textControlLength < 12500) {
        if (object->hasAttributedText()) {
            std::optional<SimpleRange> range;
            if (isTextControl())
                range = rangeForCharacterRange({ 0, textControlLength });
            else
                range = object->simpleRange();
            if (range) {
                if ((attributedText = object->attributedStringForRange(*range, SpellCheck::Yes)))
                    setProperty(AXProperty::AttributedText, attributedText);
            }
        }

        // Cache the TextContent only if it is not empty and differs from the AttributedText.
        if (auto text = object->textContent()) {
            if (!attributedText || (text->length() && *text != String([attributedText string])))
                setProperty(AXProperty::TextContent, text->isolatedCopy());
        }
    }

    // Cache the StringValue only if it differs from the AttributedText.
    auto value = object->stringValue();
    if (!attributedText || value != String([attributedText string]))
        setProperty(AXProperty::StringValue, value.isolatedCopy());
#endif // !ENABLE(AX_THREAD_TEXT_APIS)

    setProperty(AXProperty::RemoteFramePlatformElement, object->remoteFramePlatformElement());

    if (object->isWebArea()) {
        setProperty(AXProperty::PreventKeyboardDOMEventDispatch, object->preventKeyboardDOMEventDispatch());
        setProperty(AXProperty::CaretBrowsingEnabled, object->caretBrowsingEnabled());
    }

    if (object->isScrollView()) {
        setProperty(AXProperty::PlatformWidget, RetainPtr(object->platformWidget()));
        setProperty(AXProperty::RemoteParent, object->remoteParent());
    }
}

AttributedStringStyle AXIsolatedObject::stylesForAttributedString() const
{
    auto underlineColor = colorAttributeValue(AXProperty::UnderlineColor);
    bool hasUnderlineColor = underlineColor != Accessibility::defaultColor();

    return {
        font(),
        textColor(),
        colorAttributeValue(AXProperty::BackgroundColor),
        boolAttributeValue(AXProperty::IsSubscript),
        boolAttributeValue(AXProperty::IsSuperscript),
        boolAttributeValue(AXProperty::HasTextShadow),
        LineDecorationStyle(
            hasUnderlineColor,
            WTFMove(underlineColor),
            boolAttributeValue(AXProperty::HasLinethrough),
            colorAttributeValue(AXProperty::LinethroughColor)
        )
    };
}

RetainPtr<RemoteAXObjectRef> AXIsolatedObject::remoteParent() const
{
    RefPtr scrollView = Accessibility::findAncestor<AXCoreObject>(*this, true, [] (const AXCoreObject& object) {
        return object.isScrollView();
    });
    RefPtr isolatedObject = dynamicDowncast<AXIsolatedObject>(scrollView);
    return isolatedObject ? isolatedObject->propertyValue<RetainPtr<id>>(AXProperty::RemoteParent) : nil;
}

FloatRect AXIsolatedObject::primaryScreenRect() const
{
    RefPtr geometryManager = tree()->geometryManager();
    return geometryManager ? geometryManager->primaryScreenRect() : FloatRect();
}

FloatRect AXIsolatedObject::convertRectToPlatformSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    if (space == AccessibilityConversionSpace::Screen)
        return convertFrameToSpace(rect, space);

    return Accessibility::retrieveValueFromMainThread<FloatRect>([&rect, &space, this] () -> FloatRect {
        if (RefPtr axObject = associatedAXObject())
            return axObject->convertRectToPlatformSpace(rect, space);
        return { };
    });
}

bool AXIsolatedObject::isDetached() const
{
    return !wrapper() || [wrapper() axBackingObject] != this;
}

void AXIsolatedObject::attachPlatformWrapper(AccessibilityObjectWrapper* wrapper)
{
#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (role() == AccessibilityRole::LocalFrame) {
        AXIsolatedObject* crossFrameChild = crossFrameChildObject();
        if (crossFrameChild) {
            [wrapper attachIsolatedObject:*crossFrameChild];
            crossFrameChild->setWrapper(wrapper);
            return;
        }
    }
#endif // ENABLE_ACCESSIBILITY_LOCAL_FRAME
    [wrapper attachIsolatedObject:*this];
    setWrapper(wrapper);
}

void AXIsolatedObject::detachPlatformWrapper(AccessibilityDetachmentType detachmentType)
{
    [wrapper() detachIsolatedObject:detachmentType];
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::allSortedLiveRegions() const
{
    RefPtr tree = this->tree();
    if (!tree)
        return { };
    return tree->sortedLiveRegions();
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::allSortedNonRootWebAreas() const
{
    RefPtr tree = this->tree();
    if (!tree)
        return { };
    return tree->sortedNonRootWebAreas();
}

std::optional<NSRange> AXIsolatedObject::visibleCharacterRange() const
{
    ASSERT(!isMainThread());

    RefPtr tree = this->tree();
    if (!tree)
        return { };

    RefPtr root = tree->rootNode();
    if (!root || root->relativeFrame().isEmpty()) {
        // The viewport is an empty rect, so nothing is visible.
        return { };
    }

    const auto& mostRecentlyPaintedText = tree->mostRecentlyPaintedText();
    if (mostRecentlyPaintedText.isEmpty()) {
        // If nothing has been painted, but the viewport is not empty (evidenced by us not early-returning above),
        // assume a paint just hasn't happened yet and consider everything to be visible.
        auto markerRange = textMarkerRange();
        if (!markerRange)
            return { };
        return NSMakeRange(0, markerRange.toString().length());
    }

    RefPtr current = const_cast<AXIsolatedObject*>(this);
    const auto* currentRuns = textRuns();
    std::optional stopAtID = idOfNextSiblingIncludingIgnoredOrParent();
    auto advanceCurrent = [&] () {
        current = findObjectWithRuns(*current, AXDirection::Next, stopAtID);
        currentRuns = current ? current->textRuns() : nullptr;
    };

    if (!currentRuns)
        advanceCurrent();

    // The high-level algorithm here is:
    //   1. Find first self-or-descendant with text that is visible, and specifically the offset where it is
    //      visible based on the cached LineRange data we have from |mostRecentlyPaintedText|.
    //   2. Turn that into a text marker, then the |location| of the range is the length of the string between
    //      the first marker of |this| to the first visible descendant (|markerPriorToPaintedText|).
    //   3. Continue iterating descendants until we find the last visible one, and specifically the offset where
    //      it is last visible.
    //   4. The |length| of the range is then the string length between the first visible descendant and offset
    //      (from step 2) and the last visible descendant and offset (|lastVisibleMarker|).
    std::optional<AXTextMarker> markerPriorToPaintedText;
    std::optional<AXTextMarker> lastVisibleMarker;

    AXTextMarker thisFirstMarker = { *this, 0 };
    NSRange finalRange = NSMakeRange(0, 0);

    while (currentRuns) {
        auto iterator = mostRecentlyPaintedText.find(current->objectID());
        if (iterator != mostRecentlyPaintedText.end()) {
            const LineRange& range = iterator->value;

            if (!markerPriorToPaintedText) {
                // This specifically only counts rendered characters (and collapsed whitespace), excluding "emitted"
                // characters like those from TextEmissionBehavior. The visible character range API (at least based on
                // the main-thread implementation) expects these emitted characters to be counted too, so we can lean
                // on AXTextMarkerRange::toString(), which already knows how and when to emit un-rendered characters.
                unsigned renderedCharactersPriorToStartLine = range.startLineIndex ? currentRuns->runLengthSumTo(range.startLineIndex - 1) : 0;

                // Points to the last text position of the text belonging to this object that *was not* painted.
                markerPriorToPaintedText = AXTextMarker { *current, renderedCharactersPriorToStartLine };
                finalRange.location = AXTextMarkerRange { WTFMove(thisFirstMarker), *markerPriorToPaintedText }.toString().length();
            }
            unsigned visibleCharactersUpToEndLine = currentRuns->runLengthSumTo(range.endLineIndex);
            lastVisibleMarker = AXTextMarker { *current, visibleCharactersUpToEndLine };
        }

        advanceCurrent();
    }

    if (!markerPriorToPaintedText || !lastVisibleMarker) {
        // We weren't able to form a range of text, so return an empty visible range.
        return NSMakeRange(0, 0);
    }

    AXTextMarkerRange visibleTextRange = AXTextMarkerRange { WTFMove(*markerPriorToPaintedText), WTFMove(*lastVisibleMarker) };
    finalRange.length = visibleTextRange.toString().length();
    return finalRange;
}

std::optional<String> AXIsolatedObject::textContent() const
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return textMarkerRange().toString();
#else
    if (std::optional textContent = optionalAttributeValue<String>(AXProperty::TextContent))
        return *textContent;
    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXProperty::AttributedText))
        return String { [attributedText string] };
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    return { };
}

AXTextMarkerRange AXIsolatedObject::textMarkerRange() const
{
    if (isSecureField()) {
        // FIXME: return a null range to match non ITM behavior, but this should be revisited since we should return ranges for secure fields.
        return { };
    }

#if !ENABLE(AX_THREAD_TEXT_APIS)
    if (auto text = textContent())
        return { tree()->treeID(), objectID(), 0, text->length() };
#endif // !ENABLE(AX_THREAD_TEXT_APIS)

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        // This object doesn't have text content of its own. Create a range pointing to the first and last
        // text positions of our descendants. We can do this by stopping text marker traversal when we try
        // to move to our sibling. For example, getting textMarkerRange() for {ID 1, Role Group}:
        //
        // {ID 1, Role Group}
        //   {ID 2, Role StaticText, "foo"}
        //   {ID 3, Role Group}
        //     {ID 4, Role StaticText, "bar"}
        // {ID 5, Role Group}
        //
        // We would expect the returned range to be: {ID 2, offset 0} to {ID 4, offset 3}
        std::optional stopAtID = idOfNextSiblingIncludingIgnoredOrParent();

        auto thisMarker = AXTextMarker { *this, 0 };
        AXTextMarkerRange range { thisMarker, thisMarker };
        auto startMarker = thisMarker.toTextRunMarker(stopAtID);
        auto endMarker = startMarker.findLastBefore(stopAtID);
        if (endMarker.isValid() && endMarker.isInTextRun()) {
            // One or more of our descendants have text, so let's form a range from the first and last text positions.
            range = { WTFMove(startMarker), WTFMove(endMarker) };
        }
        return range;
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<AXTextMarkerRange>([this] () {
        RefPtr axObject = associatedAXObject();
        return axObject ? axObject->textMarkerRange() : AXTextMarkerRange();
    });
}

AXTextMarkerRange AXIsolatedObject::textMarkerRangeForNSRange(const NSRange& range) const
{
    if (range.location == NSNotFound)
        return { };

    if (auto text = textContent()) {
        unsigned start = range.location;
        unsigned end = range.location + range.length;
        if (start < text->length() && end <= text->length())
            return { tree()->treeID(), objectID(), start, end };
    }

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        if (std::optional markerRange = markerRangeFrom(range, *this)) {
            if (range.length > markerRange->toString().length())
                return { };
            return WTFMove(*markerRange);
        }
        return { };
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<AXTextMarkerRange>([&range, this] () -> AXTextMarkerRange {
        RefPtr axObject = associatedAXObject();
        return axObject ? axObject->textMarkerRangeForNSRange(range) : AXTextMarkerRange();
    });
}

std::optional<String> AXIsolatedObject::platformStringValue() const
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return textMarkerRange().toString();
#else
    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXProperty::AttributedText))
        return [attributedText string];
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    return { };
}

unsigned AXIsolatedObject::textLength() const
{
    ASSERT(isTextControl());

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return textMarkerRange().toString().length();
#else
    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXProperty::AttributedText))
        return [attributedText length];
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    return 0;
}

RetainPtr<id> AXIsolatedObject::remoteFramePlatformElement() const
{
    return propertyValue<RetainPtr<id>>(AXProperty::RemoteFramePlatformElement);
}

RetainPtr<NSAttributedString> AXIsolatedObject::attributedStringForTextMarkerRange(AXTextMarkerRange&& markerRange, SpellCheck spellCheck) const
{
    if (!markerRange)
        return nil;

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return markerRange.toAttributedString(spellCheck).autorelease();
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    // At the moment we are only handling ranges that are confined to a single object, and for which we cached the AttributeString.
    // FIXME: Extend to cases where the range expands multiple objects.

    bool isConfined = markerRange.isConfinedTo(markerRange.start().objectID());
    if (isConfined && markerRange.start().objectID() != objectID()) {
        // markerRange is confined to an object different from this. That is the case when clients use the webarea to request AttributedStrings for a range obtained from an inner descendant.
        // Delegate to the inner object in this case.
        if (RefPtr object = markerRange.start().object())
            return object->attributedStringForTextMarkerRange(WTFMove(markerRange), spellCheck);
    }

    RetainPtr<NSAttributedString> attributedText = nil;
#if !ENABLE(AX_THREAD_TEXT_APIS)
    attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXProperty::AttributedText);
#endif // !ENABLE(AX_THREAD_TEXT_APIS)
    if (!isConfined || !attributedText) {
        return Accessibility::retrieveValueFromMainThread<RetainPtr<NSAttributedString>>([markerRange = WTFMove(markerRange), &spellCheck, this] () mutable -> RetainPtr<NSAttributedString> {
            if (RefPtr axObject = associatedAXObject())
                return axObject->attributedStringForTextMarkerRange(WTFMove(markerRange), spellCheck);
            return { };
        });
    }

    auto nsRange = markerRange.nsRange();
    if (!nsRange)
        return nil;

    // If the range spans the beginning of the node, account for the length of the list marker prefix, if any.
    if (!nsRange->location)
        nsRange->length += textContentPrefixFromListMarker().length();

    if (!attributedStringContainsRange(attributedText.get(), *nsRange))
        return nil;

    RetainPtr result = adoptNS([[NSMutableAttributedString alloc] initWithAttributedString:[attributedText attributedSubstringFromRange:*nsRange]]);
    if (!result.get().length)
        return result;

    auto resultRange = NSMakeRange(0, result.get().length);
    // The AttributedString is cached with spelling info. If the caller does not request spelling info, we have to remove it before returning.
    if (spellCheck == SpellCheck::No) {
        [result removeAttribute:NSAccessibilityDidSpellCheckAttribute range:resultRange];
        [result removeAttribute:NSAccessibilityMisspelledTextAttribute range:resultRange];
        [result removeAttribute:NSAccessibilityMarkedMisspelledTextAttribute range:resultRange];
    } else if (AXObjectCache::shouldSpellCheck()) {
        // For ITM, we should only ever eagerly spellcheck for testing purposes.
        ASSERT(_AXGetClientForCurrentRequestUntrusted() == kAXClientTypeWebKitTesting);
        // We're going to spellcheck, so remove AXDidSpellCheck: NO.
        [result removeAttribute:NSAccessibilityDidSpellCheckAttribute range:resultRange];
        performFunctionOnMainThreadAndWait([result = RetainPtr { result }, &resultRange] (AccessibilityObject* axObject) {
            if (auto* node = axObject->node())
                attributedStringSetSpelling(result.get(), *node, String { [result string] }, resultRange);
        });
    }
    return result;
}

void AXIsolatedObject::setPreventKeyboardDOMEventDispatch(bool value)
{
    ASSERT(!isMainThread());
    ASSERT(isWebArea());

    setProperty(AXProperty::PreventKeyboardDOMEventDispatch, value);
    performFunctionOnMainThread([value] (auto* object) {
        object->setPreventKeyboardDOMEventDispatch(value);
    });
}

void AXIsolatedObject::setCaretBrowsingEnabled(bool value)
{
    ASSERT(!isMainThread());
    ASSERT(isWebArea());

    setProperty(AXProperty::CaretBrowsingEnabled, value);
    performFunctionOnMainThread([value] (auto* object) {
        object->setCaretBrowsingEnabled(value);
    });
}

// The methods in this comment block are intentionally retrieved from the main-thread
// and not cached because we don't expect AX clients to ever request them.
IntPoint AXIsolatedObject::clickPoint()
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<IntPoint>([this] () -> IntPoint {
        if (RefPtr object = associatedAXObject())
            return object->clickPoint();
        return { };
    });
}

bool AXIsolatedObject::pressedIsPresent() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<bool>([this] () -> bool {
        if (RefPtr object = associatedAXObject())
            return object->pressedIsPresent();
        return false;
    });
}

Vector<String> AXIsolatedObject::determineDropEffects() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<Vector<String>>([this] () -> Vector<String> {
        if (RefPtr object = associatedAXObject())
            return object->determineDropEffects();
        return { };
    });
}

int AXIsolatedObject::layoutCount() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<int>([this] () -> int {
        if (RefPtr object = associatedAXObject())
            return object->layoutCount();
        return { };
    });
}

Vector<String> AXIsolatedObject::classList() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<Vector<String>>([this] () -> Vector<String> {
        if (RefPtr object = associatedAXObject())
            return object->classList();
        return { };
    });
}

String AXIsolatedObject::computedRoleString() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (RefPtr object = associatedAXObject())
            return object->computedRoleString().isolatedCopy();
        return { };
    });
}
// End purposely un-cached properties block.

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE) && PLATFORM(MAC)
