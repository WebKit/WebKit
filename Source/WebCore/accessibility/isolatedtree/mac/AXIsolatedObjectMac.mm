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
    setProperty(AXPropertyName::HasApplePDFAnnotationAttribute, object->hasApplePDFAnnotationAttribute());
    setProperty(AXPropertyName::SpeechHint, object->speechHintAttributeValue().isolatedCopy());

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
                    setProperty(AXPropertyName::AttributedText, attributedText);
            }
        }

        // Cache the TextContent only if it is not empty and differs from the AttributedText.
        if (auto text = object->textContent()) {
            if (!attributedText || (text->length() && *text != String([attributedText string])))
                setProperty(AXPropertyName::TextContent, text->isolatedCopy());
        }
    }

    // Cache the StringValue only if it differs from the AttributedText.
    auto value = object->stringValue();
    if (!attributedText || value != String([attributedText string]))
        setProperty(AXPropertyName::StringValue, value.isolatedCopy());

    if (object->isWebArea()) {
        setProperty(AXPropertyName::PreventKeyboardDOMEventDispatch, object->preventKeyboardDOMEventDispatch());
        setProperty(AXPropertyName::CaretBrowsingEnabled, object->caretBrowsingEnabled());
    }

    if (object->isScrollView()) {
        m_platformWidget = object->platformWidget();
        m_remoteParent = object->remoteParentObject();
    }
}

RemoteAXObjectRef AXIsolatedObject::remoteParentObject() const
{
    auto* scrollView = Accessibility::findAncestor<AXCoreObject>(*this, true, [] (const AXCoreObject& object) {
        return object.isScrollView();
    });
    return is<AXIsolatedObject>(scrollView) ? downcast<AXIsolatedObject>(scrollView)->m_remoteParent.get() : nil;
}

FloatRect AXIsolatedObject::primaryScreenRect() const
{
    RefPtr geometryManager = tree()->geometryManager();
    return geometryManager ? geometryManager->primaryScreenRect() : FloatRect();
}

FloatRect AXIsolatedObject::convertRectToPlatformSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
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
    if (m_propertyMap.contains(AXPropertyName::TextContent))
        return stringAttributeValue(AXPropertyName::TextContent);
    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText))
        return String { [attributedText string] };
    return std::nullopt;
}

AXTextMarkerRange AXIsolatedObject::textMarkerRange() const
{
    if (isSecureField()) {
        // FIXME: return a null range to match non ITM behavior, but this should be revisited since we should return ranges for secure fields.
        return { };
    }

    if (auto text = textContent())
        return { tree()->treeID(), objectID(), 0, text->length() };

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
    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText))
        return [attributedText string];
    return std::nullopt;
}

unsigned AXIsolatedObject::textLength() const
{
    ASSERT(isTextControl());

    if (auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText))
        return [attributedText length];
    return 0;
}

RetainPtr<NSAttributedString> AXIsolatedObject::attributedStringForTextMarkerRange(AXTextMarkerRange&& markerRange, SpellCheck spellCheck) const
{
    if (!markerRange)
        return nil;

    // At the moment we are only handling ranges that are confined to a single object, and for which we cached the AttributeString.
    // FIXME: Extend to cases where the range expands multiple objects.

    bool isConfined = markerRange.isConfinedTo(markerRange.start().objectID());
    if (isConfined && markerRange.start().objectID() != objectID()) {
        // markerRange is confined to an object different from this. That is the case when clients use the webarea to request AttributedStrings for a range obtained from an inner descendant.
        // Delegate to the inner object in this case.
        if (RefPtr object = markerRange.start().object())
            return object->attributedStringForTextMarkerRange(WTFMove(markerRange), spellCheck);
    }

    auto attributedText = propertyValue<RetainPtr<NSAttributedString>>(AXPropertyName::AttributedText);
    if (!isConfined || !attributedText) {
        return Accessibility::retrieveValueFromMainThread<RetainPtr<NSAttributedString>>([markerRange = WTFMove(markerRange), &spellCheck, this] () mutable -> RetainPtr<NSAttributedString> {
            if (RefPtr axObject = associatedAXObject()) {
                // Ensure that the TextMarkers have valid Node references, in case the range was created on the AX thread.
                markerRange.start().setNodeIfNeeded();
                markerRange.end().setNodeIfNeeded();
                return axObject->attributedStringForTextMarkerRange(WTFMove(markerRange), spellCheck);
            }
            return { };
        });
    }

    auto nsRange = markerRange.nsRange();
    if (!nsRange)
        return nil;

    if (!attributedStringContainsRange(attributedText.get(), *nsRange))
        return nil;

    NSMutableAttributedString *result = [[NSMutableAttributedString alloc] initWithAttributedString:[attributedText attributedSubstringFromRange:*nsRange]];
    if (!result.length)
        return result;

    auto resultRange = NSMakeRange(0, result.length);
    // The AttributedString is cached with spelling info. If the caller does not request spelling info, we have to remove it before returning.
    if (spellCheck == SpellCheck::No) {
        [result removeAttribute:AXDidSpellCheckAttribute range:resultRange];
        [result removeAttribute:NSAccessibilityMisspelledTextAttribute range:resultRange];
        [result removeAttribute:NSAccessibilityMarkedMisspelledTextAttribute range:resultRange];
    } else if (AXObjectCache::shouldSpellCheck()) {
        // For ITM, we should only ever eagerly spellcheck for testing purposes.
        ASSERT(_AXGetClientForCurrentRequestUntrusted() == kAXClientTypeWebKitTesting);
        // We're going to spellcheck, so remove AXDidSpellCheck: NO.
        [result removeAttribute:AXDidSpellCheckAttribute range:resultRange];
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

    setProperty(AXPropertyName::PreventKeyboardDOMEventDispatch, value);
    performFunctionOnMainThread([value] (auto* object) {
        object->setPreventKeyboardDOMEventDispatch(value);
    });
}

void AXIsolatedObject::setCaretBrowsingEnabled(bool value)
{
    ASSERT(!isMainThread());
    ASSERT(isWebArea());

    setProperty(AXPropertyName::CaretBrowsingEnabled, value);
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
