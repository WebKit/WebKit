/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 */

#pragma once

#include "SVGAnimatedPropertyImpl.h"
#include "SVGLangSpace.h"
#include "SVGLocatable.h"
#include "SVGNames.h"
#include "SVGParsingError.h"
#include "SVGPropertyOwnerRegistry.h"
#include "StyledElement.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AffineTransform;
class CSSStyleDeclaration;
class DeprecatedCSSOMValue;
class Document;
class SVGDocumentExtensions;
class SVGElementRareData;
class SVGPropertyAnimatorFactory;
class SVGSVGElement;
class SVGUseElement;

void mapAttributeToCSSProperty(HashMap<AtomStringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName);

class SVGElement : public StyledElement, public SVGLangSpace, public SVGPropertyOwner {
    WTF_MAKE_ISO_ALLOCATED(SVGElement);
public:
    bool isOutermostSVGSVGElement() const;

    SVGSVGElement* ownerSVGElement() const;
    SVGElement* viewportElement() const;

    String title() const override;
    RefPtr<DeprecatedCSSOMValue> getPresentationAttribute(const String& name);
    virtual bool supportsMarkers() const { return false; }
    bool hasRelativeLengths() const { return !m_elementsWithRelativeLengths.isEmpty(); }
    virtual bool needsPendingResourceHandling() const { return true; }
    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);
    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;

    virtual bool isSVGGraphicsElement() const { return false; }
    virtual bool isSVGGeometryElement() const { return false; }
    virtual bool isFilterEffect() const { return false; }
    virtual bool isGradientStop() const { return false; }
    virtual bool isTextContent() const { return false; }
    virtual bool isSMILElement() const { return false; }

    // For SVGTests
    virtual bool isValid() const { return true; }

    virtual void svgAttributeChanged(const QualifiedName&);

    void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
    void sendSVGLoadEventIfPossibleAsynchronously();
    void svgLoadEventTimerFired();
    virtual Timer* svgLoadEventTimer();

    virtual AffineTransform* supplementalTransform() { return nullptr; }

    void invalidateSVGAttributes() { ensureUniqueElementData().setAnimatedSVGAttributesAreDirty(true); }
    void invalidateSVGPresentationAttributeStyle()
    {
        ensureUniqueElementData().setPresentationAttributeStyleIsDirty(true);
        // Trigger style recalculation for "elements as resource" (e.g. referenced by feImage).
        invalidateStyle();
    }

    // The instances of an element are clones made in shadow trees to implement <use>.
    const HashSet<SVGElement*>& instances() const;

    bool getBoundingBox(FloatRect&, SVGLocatable::StyleUpdateStrategy = SVGLocatable::AllowStyleUpdate);

    SVGElement* correspondingElement() const;
    RefPtr<SVGUseElement> correspondingUseElement() const;

    void setCorrespondingElement(SVGElement*);

    Optional<ElementStyle> resolveCustomStyle(const RenderStyle& parentStyle, const RenderStyle* shadowHostStyle) override;

    static QualifiedName animatableAttributeForName(const AtomString&);
#ifndef NDEBUG
    bool isAnimatableAttribute(const QualifiedName&) const;
#endif

    MutableStyleProperties* animatedSMILStyleProperties() const;
    MutableStyleProperties& ensureAnimatedSMILStyleProperties();
    void setUseOverrideComputedStyle(bool);

    virtual bool haveLoadedRequiredResources();

    bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;
    bool removeEventListener(const AtomString& eventType, EventListener&, const ListenerOptions&) override;

    bool hasTagName(const SVGQualifiedName& name) const { return hasLocalName(name.localName()); }

    void callClearTarget() { clearTarget(); }

    class InstanceUpdateBlocker;
    class InstanceInvalidationGuard;

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGElement>;
    virtual const SVGPropertyRegistry& propertyRegistry() const { return m_propertyRegistry; }
    void detachAllProperties() { propertyRegistry().detachAllProperties(); }

    bool isAnimatedPropertyAttribute(const QualifiedName&) const;
    bool isAnimatedAttribute(const QualifiedName&) const;
    bool isAnimatedStyleAttribute(const QualifiedName&) const;

    void synchronizeAttribute(const QualifiedName&);
    void synchronizeAllAttributes();
    static void synchronizeAllAnimatedSVGAttribute(SVGElement&);

    void commitPropertyChange(SVGProperty*) override;
    void commitPropertyChange(SVGAnimatedProperty&);

    const SVGElement* attributeContextElement() const override { return this; }
    SVGPropertyAnimatorFactory& propertyAnimatorFactory() { return *m_propertyAnimatorFactory; }
    RefPtr<SVGAttributeAnimator> createAnimator(const QualifiedName&, AnimationMode, CalcMode, bool isAccumulated, bool isAdditive);
    void animatorWillBeDeleted(const QualifiedName&);

    // These are needed for the RenderTree, animation and DOM.
    String className() const { return m_className->currentValue(); }
    SVGAnimatedString& classNameAnimated() { return m_className; }

protected:
    SVGElement(const QualifiedName&, Document&);
    virtual ~SVGElement();

    bool rendererIsNeeded(const RenderStyle&) override;
    void parseAttribute(const QualifiedName&, const AtomString&) override;

    void finishParsingChildren() override;
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason = ModifiedDirectly) override;
    bool childShouldCreateRenderer(const Node&) const override;

    SVGElementRareData& ensureSVGRareData();

    void reportAttributeParsingError(SVGParsingError, const QualifiedName&, const AtomString&);
    static CSSPropertyID cssPropertyIdForSVGAttributeName(const QualifiedName&);

    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void childrenChanged(const ChildChange&) override;
    virtual bool selfHasRelativeLengths() const { return false; }
    void updateRelativeLengthsInformation() { updateRelativeLengthsInformation(selfHasRelativeLengths(), this); }
    void updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement*);

    void willRecalcStyle(Style::Change) override;

private:
    const RenderStyle* computedStyle(PseudoId = PseudoId::None) final;

    virtual void clearTarget() { }

    void buildPendingResourcesIfNeeded();
    void accessKeyAction(bool sendMouseEvents) override;

#ifndef NDEBUG
    virtual bool filterOutAnimatableAttribute(const QualifiedName&) const;
#endif

    void invalidateInstances();

    std::unique_ptr<SVGElementRareData> m_svgRareData;

    HashSet<SVGElement*> m_elementsWithRelativeLengths;

    std::unique_ptr<SVGPropertyAnimatorFactory> m_propertyAnimatorFactory;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedString> m_className { SVGAnimatedString::create(this) };
};

class SVGElement::InstanceInvalidationGuard {
public:
    InstanceInvalidationGuard(SVGElement&);
    ~InstanceInvalidationGuard();
private:
    SVGElement& m_element;
};

class SVGElement::InstanceUpdateBlocker {
public:
    InstanceUpdateBlocker(SVGElement&);
    ~InstanceUpdateBlocker();
private:
    SVGElement& m_element;
};

struct SVGAttributeHashTranslator {
    static unsigned hash(const QualifiedName& key)
    {
        if (key.hasPrefix()) {
            QualifiedNameComponents components = { nullAtom().impl(), key.localName().impl(), key.namespaceURI().impl() };
            return hashComponents(components);
        }
        return DefaultHash<QualifiedName>::Hash::hash(key);
    }
    static bool equal(const QualifiedName& a, const QualifiedName& b) { return a.matches(b); }
};

inline SVGElement::InstanceInvalidationGuard::InstanceInvalidationGuard(SVGElement& element)
    : m_element(element)
{
}

inline SVGElement::InstanceInvalidationGuard::~InstanceInvalidationGuard()
{
    m_element.invalidateInstances();
}

inline SVGElement::InstanceUpdateBlocker::InstanceUpdateBlocker(SVGElement& element)
    : m_element(element)
{
    m_element.setInstanceUpdatesBlocked(true);
}

inline SVGElement::InstanceUpdateBlocker::~InstanceUpdateBlocker()
{
    m_element.setInstanceUpdatesBlocked(false);
}

inline bool Node::hasTagName(const SVGQualifiedName& name) const
{
    return isSVGElement() && downcast<SVGElement>(*this).hasTagName(name);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGElement)
    static bool isType(const WebCore::Node& node) { return node.isSVGElement(); }
SPECIALIZE_TYPE_TRAITS_END()

#include "SVGElementTypeHelpers.h"
