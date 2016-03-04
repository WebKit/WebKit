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

#ifndef HTMLImageElement_h
#define HTMLImageElement_h

#include "FormNamedItem.h"
#include "GraphicsTypes.h"
#include "HTMLElement.h"
#include "HTMLImageLoader.h"

namespace WebCore {

class HTMLFormElement;
struct ImageCandidate;

class HTMLImageElement : public HTMLElement, public FormNamedItem {
    friend class HTMLFormElement;
public:
    static Ref<HTMLImageElement> create(Document&);
    static Ref<HTMLImageElement> create(const QualifiedName&, Document&, HTMLFormElement*);
    static Ref<HTMLImageElement> createForJSConstructor(Document&, const int* optionalWidth, const int* optionalHeight);

    virtual ~HTMLImageElement();

    int width(bool ignorePendingStylesheets = false);
    int height(bool ignorePendingStylesheets = false);

    int naturalWidth() const;
    int naturalHeight() const;
    const AtomicString& currentSrc() const { return m_currentSrc; }

    bool isServerMap() const;

    const AtomicString& altText() const;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }

    CachedImage* cachedImage() const { return m_imageLoader.image(); }

    void setLoadManually(bool loadManually) { m_imageLoader.setLoadManually(loadManually); }

    bool matchesCaseFoldedUsemap(const AtomicStringImpl&) const;

    const AtomicString& alt() const;

    void setHeight(int);

    URL src() const;
    void setSrc(const String&);

    void setCrossOrigin(const AtomicString&);
    String crossOrigin() const;

    void setWidth(int);

    int x() const;
    int y() const;

    bool complete() const;

#if PLATFORM(IOS)
    bool willRespondToMouseClickEvents() override;
#endif

    bool hasPendingActivity() const { return m_imageLoader.hasPendingActivity(); }

    bool canContainRangeEndPoint() const override { return false; }

    const AtomicString& imageSourceURL() const override;

    bool hasShadowControls() const { return m_experimentalImageMenuEnabled; }
    
    HTMLPictureElement* pictureElement() const;
    void setPictureElement(HTMLPictureElement*);

protected:
    HTMLImageElement(const QualifiedName&, Document&, HTMLFormElement* = 0);

    void didMoveToNewDocument(Document* oldDocument) override;

private:
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;

    void didAttachRenderers() override;
    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;
    void setBestFitURLAndDPRFromImageCandidate(const ImageCandidate&);

    bool canStartSelection() const override;

    bool isURLAttribute(const Attribute&) const override;
    bool attributeContainsURL(const Attribute&) const override;
    String completeURLsInAttributeValue(const URL& base, const Attribute&) const override;

    bool draggable() const override;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void removedFrom(ContainerNode&) override;

    bool isFormAssociatedElement() const final { return false; }
    FormNamedItem* asFormNamedItem() final { return this; }
    HTMLImageElement& asHTMLElement() final { return *this; }
    const HTMLImageElement& asHTMLElement() const final { return *this; }

    void selectImageSource();

    ImageCandidate bestFitSourceFromPictureElement();

    HTMLImageLoader m_imageLoader;
    HTMLFormElement* m_form;
    HTMLFormElement* m_formSetByParser;

    CompositeOperator m_compositeOperator;
    AtomicString m_bestFitImageURL;
    AtomicString m_currentSrc;
    AtomicString m_caseFoldedUsemap;
    float m_imageDevicePixelRatio;
    bool m_experimentalImageMenuEnabled;
    bool m_hadNameBeforeAttributeChanged { false }; // FIXME: We only need this because parseAttribute() can't see the old value.

#if ENABLE(SERVICE_CONTROLS)
    void updateImageControls();
    void createImageControls();
    void destroyImageControls();
    bool hasImageControls() const;
    bool childShouldCreateRenderer(const Node&) const override;
#endif

    friend class HTMLPictureElement;
};

} //namespace

#endif
