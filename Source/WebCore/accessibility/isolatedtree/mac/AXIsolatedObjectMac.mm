/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "AXIsolatedObject.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE) && PLATFORM(MAC)

#import "WebAccessibilityObjectWrapperMac.h"
#import <pal/spi/cocoa/AccessibilitySupportSPI.h>
#import <pal/spi/cocoa/AccessibilitySupportSoftLink.h>

namespace WebCore {

void AXIsolatedObject::initializePlatformProperties(const Ref<const AccessibilityObject>& object)
{
    setProperty(AXProperty::HasApplePDFAnnotationAttribute, object->hasApplePDFAnnotationAttribute());
    setProperty(AXProperty::SpeechHint, object->speechHintAttributeValue().isolatedCopy());
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (object->isStaticText()) {
        auto style = object->stylesForAttributedString();
        setProperty(AXProperty::BackgroundColor, WTFMove(style.backgroundColor));
        setProperty(AXProperty::Font, style.font);
        setProperty(AXProperty::HasLinethrough, style.hasLinethrough());
        setProperty(AXProperty::HasTextShadow, style.hasTextShadow);
        setProperty(AXProperty::HasUnderline, style.hasUnderline());
        setProperty(AXProperty::IsSubscript, style.isSubscript);
        setProperty(AXProperty::IsSuperscript, style.isSuperscript);
        setProperty(AXProperty::LinethroughColor, style.linethroughColor());
        // FIXME: We're going to end up caching a lot of the same TextColor properties over and over.
        //  1. Should we implement some kind of Color interning?
        //  2. Or maybe have a "main" text color mechanism, where when we create the isolated tree, we try to compute the
        //     "main" text color by walking n-number of RenderTexts. Then AXIsolatedObject::setProperty, if the Color is
        //     equivalent to the "main" one, we consider the color to be "isDefaultValue", thus not caching it.
        //       - Probably want to do this for both TextColor and BackgroundColor.
        //       - Maybe set this as an OpportunisticTask to re-compute the main colors, and if it has changed, walk all
        //         objects and uncache properties for those with the new "main" color? Not sure if this is worth it...
        //       - Only trigger the walk if font-color changes?
        // Resolve this FIXME before shipping AX_THREAD_TEXT_APIS.
        setProperty(AXProperty::TextColor, WTFMove(style.textColor));
        setProperty(AXProperty::UnderlineColor, style.underlineColor());
    }
    // FIXME: Can we compute this off the main-thread with our cached text runs?
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
        m_platformWidget = object->platformWidget();
        m_remoteParent = object->remoteParentObject();
    }
}

AttributedStringStyle AXIsolatedObject::stylesForAttributedString() const
{
    return {
        font(),
        colorAttributeValue(AXProperty::TextColor),
        colorAttributeValue(AXProperty::BackgroundColor),
        boolAttributeValue(AXProperty::IsSubscript),
        boolAttributeValue(AXProperty::IsSuperscript),
        boolAttributeValue(AXProperty::HasTextShadow),
        LineDecorationStyle(
            boolAttributeValue(AXProperty::HasUnderline),
            colorAttributeValue(AXProperty::UnderlineColor),
            boolAttributeValue(AXProperty::HasLinethrough),
            colorAttributeValue(AXProperty::LinethroughColor)
        )
    };
}

RemoteAXObjectRef AXIsolatedObject::remoteParentObject() const
{
    auto* scrollView = Accessibility::findAncestor<AXCoreObject>(*this, true, [] (const AXCoreObject& object) {
        return object.isScrollView();
    });
    auto* isolatedObject = dynamicDowncast<AXIsolatedObject>(scrollView);
    return isolatedObject ? isolatedObject->m_remoteParent.get() : nil;
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
        if (auto* axObject = associatedAXObject())
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
    [wrapper attachIsolatedObject:this];
    setWrapper(wrapper);
}

void AXIsolatedObject::detachPlatformWrapper(AccessibilityDetachmentType detachmentType)
{
    [wrapper() detachIsolatedObject:detachmentType];
}

std::optional<String> AXIsolatedObject::textContent() const
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return textMarkerRange().toString();
#else
    if (m_propertyMap.contains(AXProperty::TextContent))
        return stringAttributeValue(AXProperty::TextContent);
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
        auto* stopObject = nextSiblingIncludingIgnoredOrParent();

        auto thisMarker = AXTextMarker { *this, 0 };
        AXTextMarkerRange range { thisMarker, thisMarker };
        auto endMarker = thisMarker.findLastBefore(stopObject ? std::optional { stopObject->objectID() } : std::nullopt);
        if (endMarker.isValid() && endMarker.isInTextRun()) {
            // One or more of our descendants have text, so let's form a range from the first and last text positions.
            range = { thisMarker.toTextRunMarker(), WTFMove(endMarker) };
        }
        return range;
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<AXTextMarkerRange>([this] () {
        auto* axObject = associatedAXObject();
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

    return Accessibility::retrieveValueFromMainThread<AXTextMarkerRange>([&range, this] () -> AXTextMarkerRange {
        auto* axObject = associatedAXObject();
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

    // If the range spans the beginning of the node, account for the length of the text marker prefix, if any.
    if (!nsRange->location)
        nsRange->length += textContentPrefixFromListMarker().length();

    if (!attributedStringContainsRange(attributedText.get(), *nsRange))
        return nil;

    NSMutableAttributedString *result = [[NSMutableAttributedString alloc] initWithAttributedString:[attributedText attributedSubstringFromRange:*nsRange]];
    if (!result.length)
        return result;

    auto resultRange = NSMakeRange(0, result.length);
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
        performFunctionOnMainThreadAndWait([result = retainPtr(result), &resultRange] (AccessibilityObject* axObject) {
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
        if (auto* object = associatedAXObject())
            return object->clickPoint();
        return { };
    });
}

bool AXIsolatedObject::pressedIsPresent() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<bool>([this] () -> bool {
        if (auto* object = associatedAXObject())
            return object->pressedIsPresent();
        return false;
    });
}

Vector<String> AXIsolatedObject::determineDropEffects() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<Vector<String>>([this] () -> Vector<String> {
        if (auto* object = associatedAXObject())
            return object->determineDropEffects();
        return { };
    });
}

int AXIsolatedObject::layoutCount() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<int>([this] () -> int {
        if (auto* object = associatedAXObject())
            return object->layoutCount();
        return { };
    });
}

Vector<String> AXIsolatedObject::classList() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<Vector<String>>([this] () -> Vector<String> {
        if (auto* object = associatedAXObject())
            return object->classList();
        return { };
    });
}

String AXIsolatedObject::computedRoleString() const
{
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);

    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (auto* object = associatedAXObject())
            return object->computedRoleString().isolatedCopy();
        return { };
    });
}
// End purposely un-cached properties block.

} // WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE) && PLATFORM(MAC)
