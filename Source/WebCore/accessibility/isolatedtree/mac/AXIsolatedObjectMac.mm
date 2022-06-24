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

namespace WebCore {

void AXIsolatedObject::initializePlatformProperties(Ref<const AXCoreObject> object, IsRoot isRoot)
{
    setProperty(AXPropertyName::HasApplePDFAnnotationAttribute, object->hasApplePDFAnnotationAttribute());
    setProperty(AXPropertyName::SpeechHint, object->speechHintAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::CaretBrowsingEnabled, object->caretBrowsingEnabled());

    if (isRoot == IsRoot::Yes)
        setProperty(AXPropertyName::PreventKeyboardDOMEventDispatch, object->preventKeyboardDOMEventDispatch());

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

FloatRect AXIsolatedObject::convertRectToPlatformSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    return Accessibility::retrieveValueFromMainThread<FloatRect>([&rect, &space, this]() -> FloatRect {
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

AXTextMarkerRangeRef AXIsolatedObject::textMarkerRangeForNSRange(const NSRange& range) const
{
    return Accessibility::retrieveAutoreleasedValueFromMainThread<AXTextMarkerRangeRef>([&range, this] () -> RetainPtr<AXTextMarkerRangeRef> {
        auto* axObject = associatedAXObject();
        return axObject ? axObject->textMarkerRangeForNSRange(range) : nullptr;
    });
}

bool AXIsolatedObject::preventKeyboardDOMEventDispatch() const
{
    if (auto root = tree()->rootNode())
        return root->boolAttributeValue(AXPropertyName::PreventKeyboardDOMEventDispatch);
    return false;
}

void AXIsolatedObject::setPreventKeyboardDOMEventDispatch(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setPreventKeyboardDOMEventDispatch(value);
    });
}

String AXIsolatedObject::descriptionAttributeValue() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::Description);
}

String AXIsolatedObject::helpTextAttributeValue() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::HelpText);
}

String AXIsolatedObject::titleAttributeValue() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::TitleAttributeValue);
}

} // WebCore

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE) && PLATFORM(MAC)
