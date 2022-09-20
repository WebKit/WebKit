/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "ActiveDOMObject.h"
#include "DecodingOptions.h"
#include "FormNamedItem.h"
#include "GraphicsLayer.h"
#include "GraphicsTypes.h"
#include "HTMLElement.h"
#include "MediaQueryEvaluator.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class CachedImage;
class DeferredPromise;
class HTMLAttachmentElement;
class HTMLFormElement;
class HTMLImageLoader;
class HTMLMapElement;

struct ImageCandidate;

enum class ReferrerPolicy : uint8_t;
enum class RelevantMutation : bool;

class HTMLImageElement : public HTMLElement, public FormNamedItem, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(HTMLImageElement);
    friend class HTMLFormElement;
public:
    static Ref<HTMLImageElement> create(Document&);
    static Ref<HTMLImageElement> create(const QualifiedName&, Document&, HTMLFormElement* = nullptr);
    static Ref<HTMLImageElement> createForLegacyFactoryFunction(Document&, std::optional<unsigned> width, std::optional<unsigned> height);

    virtual ~HTMLImageElement();

    WEBCORE_EXPORT unsigned width(bool ignorePendingStylesheets = false);
    WEBCORE_EXPORT unsigned height(bool ignorePendingStylesheets = false);

    WEBCORE_EXPORT int naturalWidth() const;
    WEBCORE_EXPORT int naturalHeight() const;
    const AtomString& currentSrc() const { return m_currentSrc; }

    bool isServerMap() const;

    const AtomString& altText() const;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }

    WEBCORE_EXPORT CachedImage* cachedImage() const;

    void setLoadManually(bool);

    bool matchesUsemap(const AtomStringImpl&) const;
    HTMLMapElement* associatedMapElement() const;

    WEBCORE_EXPORT const AtomString& alt() const;

    WEBCORE_EXPORT void setHeight(unsigned);

    URL src() const;
    void setSrc(const AtomString&);

    WEBCORE_EXPORT void setCrossOrigin(const AtomString&);
    WEBCORE_EXPORT String crossOrigin() const;

    WEBCORE_EXPORT void setWidth(unsigned);

    WEBCORE_EXPORT int x() const;
    WEBCORE_EXPORT int y() const;

    WEBCORE_EXPORT bool complete() const;

    void setDecoding(AtomString&&);
    String decoding() const;

    DecodingMode decodingMode() const;
    
    WEBCORE_EXPORT void decode(Ref<DeferredPromise>&&);

#if PLATFORM(IOS_FAMILY)
    bool willRespondToMouseClickEventsWithEditability(Editability) const override;

    enum class IgnoreTouchCallout { No, Yes };
    bool willRespondToMouseClickEventsWithEditability(Editability, IgnoreTouchCallout) const;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    void setAttachmentElement(Ref<HTMLAttachmentElement>&&);
    RefPtr<HTMLAttachmentElement> attachmentElement() const;
    const String& attachmentIdentifier() const;
    void didUpdateAttachmentIdentifier();
#endif

    WEBCORE_EXPORT size_t pendingDecodePromisesCountForTesting() const;

    bool canContainRangeEndPoint() const override { return false; }

    const AtomString& imageSourceURL() const override;
    
#if ENABLE(SERVICE_CONTROLS)
    bool isImageMenuEnabled() const { return m_isImageMenuEnabled; }
    void setImageMenuEnabled(bool value) { m_isImageMenuEnabled = value; }
#endif

    HTMLPictureElement* pictureElement() const;
    void setPictureElement(HTMLPictureElement*);

#if USE(SYSTEM_PREVIEW)
    WEBCORE_EXPORT bool isSystemPreviewImage() const;
#endif

    void loadDeferredImage();

    AtomString srcsetForBindings() const;
    void setSrcsetForBindings(const AtomString&);

    const AtomString& loadingForBindings() const;
    void setLoadingForBindings(const AtomString&);

    bool isLazyLoadable() const;
    static bool hasLazyLoadableAttributeValue(const AtomString&);

    bool isDeferred() const;

    bool isDroppedImagePlaceholder() const { return m_isDroppedImagePlaceholder; }
    void setIsDroppedImagePlaceholder() { m_isDroppedImagePlaceholder = true; }

    void evaluateDynamicMediaQueryDependencies();

    void setReferrerPolicyForBindings(const AtomString&);
    String referrerPolicyForBindings() const;
    ReferrerPolicy referrerPolicy() const;

    bool allowsOrientationOverride() const;

    bool allowsAnimation() const;
    WEBCORE_EXPORT void setAllowsAnimation(bool);
    WEBCORE_EXPORT void resetAllowsAnimation();

protected:
    HTMLImageElement(const QualifiedName&, Document&, HTMLFormElement* = nullptr);

    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;

private:
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;
    void parseAttribute(const QualifiedName&, const AtomString&) override;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const override;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;
    void collectExtraStyleForPresentationalHints(MutableStyleProperties&) override;
    void invalidateAttributeMapping();

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document& targetDocument) final;

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    void didAttachRenderers() override;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    void setBestFitURLAndDPRFromImageCandidate(const ImageCandidate&);

    bool canStartSelection() const override;

    bool isURLAttribute(const Attribute&) const override;
    bool attributeContainsURL(const Attribute&) const override;
    String completeURLsInAttributeValue(const URL& base, const Attribute&, ResolveURLs = ResolveURLs::Yes) const override;

    bool isDraggableIgnoringAttributes() const final { return true; }

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;

    bool isFormAssociatedElement() const final { return false; }
    FormNamedItem* asFormNamedItem() final { return this; }
    HTMLImageElement& asHTMLElement() final { return *this; }
    const HTMLImageElement& asHTMLElement() const final { return *this; }

    bool isInteractiveContent() const final;

    void selectImageSource(RelevantMutation);

    ImageCandidate bestFitSourceFromPictureElement();

    void copyNonAttributePropertiesFromElement(const Element&) final;

    float effectiveImageDevicePixelRatio() const;
    
#if ENABLE(SERVICE_CONTROLS)
    bool childShouldCreateRenderer(const Node&) const override;
#endif

    HTMLSourceElement* sourceElement() const;
    void setSourceElement(HTMLSourceElement*);

    std::unique_ptr<HTMLImageLoader> m_imageLoader;
    WeakPtr<HTMLFormElement, WeakPtrImplWithEventTargetData> m_form;
    WeakPtr<HTMLFormElement, WeakPtrImplWithEventTargetData> m_formSetByParser;

    CompositeOperator m_compositeOperator;
    AtomString m_bestFitImageURL;
    AtomString m_currentSrc;
    AtomString m_parsedUsemap;
    float m_imageDevicePixelRatio;
#if ENABLE(SERVICE_CONTROLS)
    bool m_isImageMenuEnabled { false };
#endif
    bool m_hadNameBeforeAttributeChanged { false }; // FIXME: We only need this because parseAttribute() can't see the old value.
    bool m_isDroppedImagePlaceholder { false };

    WeakPtr<HTMLPictureElement, WeakPtrImplWithEventTargetData> m_pictureElement;
    // The source element that was selected to provide the source URL.
    WeakPtr<HTMLSourceElement, WeakPtrImplWithEventTargetData> m_sourceElement;
    MediaQueryDynamicResults m_mediaQueryDynamicResults;

#if ENABLE(ATTACHMENT_ELEMENT)
    String m_pendingClonedAttachmentID;
#endif

    Image* image() const;

    friend class HTMLPictureElement;
};

} //namespace
