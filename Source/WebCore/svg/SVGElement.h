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
#include "SVGLocatable.h"
#include "SVGNames.h"
#include "SVGParsingError.h"
#include "SVGPropertyOwnerRegistry.h"
#include "SVGRenderStyleDefs.h"
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
class SVGResourceElementClient;
class SVGSVGElement;
class SVGUseElement;
class Timer;

class SVGElement : public StyledElement, public SVGPropertyOwner {
    WTF_MAKE_ISO_ALLOCATED(SVGElement);
public:
    bool isInnerSVGSVGElement() const;
    bool isOutermostSVGSVGElement() const;

    SVGSVGElement* ownerSVGElement() const;
    SVGElement* viewportElement() const;

    String title() const override;
    virtual bool supportsMarkers() const { return false; }
    bool hasRelativeLengths() const { return m_selfHasRelativeLengths || !m_childElementsWithRelativeLengths.isEmptyIgnoringNullReferences(); }
    virtual bool needsPendingResourceHandling() const { return true; }
    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);
    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;

    bool hasPendingResources() const { return hasEventTargetFlag(EventTargetFlag::HasPendingResources); }
    void setHasPendingResources() { setEventTargetFlag(EventTargetFlag::HasPendingResources, true); }
    void clearHasPendingResources() { setEventTargetFlag(EventTargetFlag::HasPendingResources, false); }
    virtual void buildPendingResource() { }

    virtual bool isSVGGraphicsElement() const { return false; }
    virtual bool isSVGGeometryElement() const { return false; }
    virtual bool isFilterEffect() const { return false; }
    virtual bool isGradientStop() const { return false; }
    virtual bool isTextContent() const { return false; }
    virtual bool isSMILElement() const { return false; }

    // For SVGTests
    virtual bool isValid() const { return true; }

    virtual void svgAttributeChanged(const QualifiedName&);

    void sendLoadEventIfPossible();
    void loadEventTimerFired();
    virtual Timer* loadEventTimer();

    virtual AffineTransform* ensureSupplementalTransform() { return nullptr; }
    virtual AffineTransform* supplementalTransform() const { return nullptr; }

    inline void setAnimatedSVGAttributesAreDirty();
    inline void setPresentationalHintStyleIsDirty();
    void updateSVGRendererForElementChange();

    // The instances of an element are clones made in shadow trees to implement <use>.
    const WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData>& instances() const;

    std::optional<FloatRect> getBoundingBox() const;

    Vector<Ref<SVGElement>> referencingElements() const;
    void addReferencingElement(SVGElement&);
    void removeReferencingElement(SVGElement&);
    void removeElementReference();

    Vector<WeakPtr<SVGResourceElementClient>> referencingCSSClients() const;
    void addReferencingCSSClient(SVGResourceElementClient&);
    void removeReferencingCSSClient(SVGResourceElementClient&);


    SVGElement* correspondingElement() const;
    RefPtr<SVGUseElement> correspondingUseElement() const;

    void setCorrespondingElement(SVGElement*);

    std::optional<Style::ResolvedStyle> resolveCustomStyle(const Style::ResolutionContext&, const RenderStyle* shadowHostStyle) override;

    static QualifiedName animatableAttributeForName(const AtomString&);
#ifndef NDEBUG
    bool isAnimatableAttribute(const QualifiedName&) const;
#endif

    MutableStyleProperties* animatedSMILStyleProperties() const;
    MutableStyleProperties& ensureAnimatedSMILStyleProperties();
    void setUseOverrideComputedStyle(bool);

    virtual bool haveLoadedRequiredResources();

    bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;
    bool removeEventListener(const AtomString& eventType, EventListener&, const EventListenerOptions&) override;

    bool hasTagName(const SVGQualifiedName& name) const { return hasLocalName(name.localName()); }

    void callClearTarget() { clearTarget(); }

    class InstanceUpdateBlocker;
    class InstanceInvalidationGuard;

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGElement>;
    const SVGPropertyRegistry& propertyRegistry() const { return m_propertyRegistry.get(); }
    void detachAllProperties() { propertyRegistry().detachAllProperties(); }

    bool isAnimatedPropertyAttribute(const QualifiedName&) const;
    bool isAnimatedAttribute(const QualifiedName&) const;
    bool isAnimatedStyleAttribute(const QualifiedName&) const;

    void synchronizeAttribute(const QualifiedName&);
    void synchronizeAllAttributes();

    void commitPropertyChange(SVGProperty*) override;
    void commitPropertyChange(SVGAnimatedProperty&);

    const SVGElement* attributeContextElement() const override { return this; }
    SVGPropertyAnimatorFactory& propertyAnimatorFactory() { return *m_propertyAnimatorFactory; }
    RefPtr<SVGAttributeAnimator> createAnimator(const QualifiedName&, AnimationMode, CalcMode, bool isAccumulated, bool isAdditive);
    void animatorWillBeDeleted(const QualifiedName&);

    using Node::computedStyle;
    const RenderStyle* computedStyle(const std::optional<Style::PseudoElementIdentifier>&) final;

    ColorInterpolation colorInterpolation() const;

    // These are needed for the RenderTree, animation and DOM.
    AtomString className() const { return AtomString { m_className->currentValue() }; }
    SVGAnimatedString& classNameAnimated() { return m_className; }

    SVGConditionalProcessingAttributes& conditionalProcessingAttributes();
    SVGConditionalProcessingAttributes* conditionalProcessingAttributesIfExists() const;

    bool hasAssociatedSVGLayoutBox() const;

protected:
    SVGElement(const QualifiedName&, Document&, UniqueRef<SVGPropertyRegistry>&&, OptionSet<TypeFlag> = { });
    virtual ~SVGElement();

    bool rendererIsNeeded(const RenderStyle&) override;

    void finishParsingChildren() override;
    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason = AttributeModificationReason::Directly) override;
    bool childShouldCreateRenderer(const Node&) const override;

    SVGElementRareData& ensureSVGRareData();

    void reportAttributeParsingError(SVGParsingError, const QualifiedName&, const AtomString&);
    static CSSPropertyID cssPropertyIdForSVGAttributeName(const QualifiedName&);

    bool hasPresentationalHintsForAttribute(const QualifiedName&) const override;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void didFinishInsertingNode() override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void childrenChanged(const ChildChange&) override;
    virtual bool selfHasRelativeLengths() const { return false; }
    void updateRelativeLengthsInformation();
    void updateRelativeLengthsInformationForChild(bool hasRelativeLengths, SVGElement&);

    void willRecalcStyle(Style::Change) override;

private:
    virtual void clearTarget() { }

    void buildPendingResourcesIfNeeded();
    bool accessKeyAction(bool sendMouseEvents) override;

#ifndef NDEBUG
    virtual bool filterOutAnimatableAttribute(const QualifiedName&) const;
#endif

    void invalidateInstances();

    std::unique_ptr<SVGElementRareData> m_svgRareData;

    WeakHashSet<SVGElement, WeakPtrImplWithEventTargetData> m_childElementsWithRelativeLengths;
    bool m_hasRegisteredWithParentForRelativeLengths { false };
    bool m_selfHasRelativeLengths { false };
    bool m_hasInitializedRelativeLengthsState { false };

    std::unique_ptr<SVGPropertyAnimatorFactory> m_propertyAnimatorFactory;

    UniqueRef<SVGPropertyRegistry> m_propertyRegistry;
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



} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGElement)
    static bool isType(const WebCore::EventTarget& eventTarget) { return eventTarget.isNode() && static_cast<const WebCore::Node&>(eventTarget).isSVGElement(); }
    static bool isType(const WebCore::Node& node) { return node.isSVGElement(); }
SPECIALIZE_TYPE_TRAITS_END()

