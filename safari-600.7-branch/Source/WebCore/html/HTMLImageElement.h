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
    static PassRefPtr<HTMLImageElement> create(Document&);
    static PassRefPtr<HTMLImageElement> create(const QualifiedName&, Document&, HTMLFormElement*);
    static PassRefPtr<HTMLImageElement> createForJSConstructor(Document&, const int* optionalWidth, const int* optionalHeight);

    virtual ~HTMLImageElement();

    int width(bool ignorePendingStylesheets = false);
    int height(bool ignorePendingStylesheets = false);

    int naturalWidth() const;
    int naturalHeight() const;
#if ENABLE_PICTURE_SIZES
    const AtomicString& currentSrc() const { return m_currentSrc; }
#endif

    bool isServerMap() const;

    String altText() const;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }

    CachedImage* cachedImage() const { return m_imageLoader.image(); }
    void setCachedImage(CachedImage* i) { m_imageLoader.setImage(i); };

    void setLoadManually(bool loadManually) { m_imageLoader.setLoadManually(loadManually); }

    bool matchesLowercasedUsemap(const AtomicStringImpl&) const;

    const AtomicString& alt() const;

    void setHeight(int);

    URL src() const;
    void setSrc(const String&);

    void setWidth(int);

    int x() const;
    int y() const;

    bool complete() const;

#if PLATFORM(IOS)
    virtual bool willRespondToMouseClickEvents() override;
#endif

    bool hasPendingActivity() const { return m_imageLoader.hasPendingActivity(); }

    virtual bool canContainRangeEndPoint() const override { return false; }

    virtual const AtomicString& imageSourceURL() const override;

    bool hasShadowControls() const { return m_experimentalImageMenuEnabled; }

protected:
    HTMLImageElement(const QualifiedName&, Document&, HTMLFormElement* = 0);

    virtual void didMoveToNewDocument(Document* oldDocument) override;

private:
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;

    virtual void didAttachRenderers() override;
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    void setBestFitURLAndDPRFromImageCandidate(const ImageCandidate&);

    virtual bool canStartSelection() const override;

    virtual bool isURLAttribute(const Attribute&) const override;
    virtual bool attributeContainsURL(const Attribute&) const override;
    virtual String completeURLsInAttributeValue(const URL& base, const Attribute&) const override;

    virtual bool draggable() const override;

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;

    virtual bool isFormAssociatedElement() const override final { return false; }
    virtual FormNamedItem* asFormNamedItem() override final { return this; }
    virtual HTMLImageElement& asHTMLElement() override final { return *this; }
    virtual const HTMLImageElement& asHTMLElement() const override final { return *this; }

    HTMLImageLoader m_imageLoader;
    HTMLFormElement* m_form;
    CompositeOperator m_compositeOperator;
    AtomicString m_bestFitImageURL;
#if ENABLE_PICTURE_SIZES
    AtomicString m_currentSrc;
#endif
    AtomicString m_lowercasedUsemap;
    float m_imageDevicePixelRatio;
    bool m_experimentalImageMenuEnabled;

#if ENABLE(SERVICE_CONTROLS)
    void updateImageControls();
    void createImageControls();
    void destroyImageControls();
    bool hasImageControls() const;
    virtual bool childShouldCreateRenderer(const Node&) const override;
#endif
};

NODE_TYPE_CASTS(HTMLImageElement)

} //namespace

#endif
