/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2009, 2014 Apple Inc. All rights reserved.
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

#ifndef SVGElement_h
#define SVGElement_h

#include "CSSPropertyNames.h"
#include "SVGAnimatedString.h"
#include "SVGLangSpace.h"
#include "SVGLocatable.h"
#include "SVGNames.h"
#include "SVGParsingError.h"
#include "SVGPropertyInfo.h"
#include "StyledElement.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

class AffineTransform;
class CSSCursorImageValue;
class CSSStyleDeclaration;
class CSSValue;
class Document;
class SVGAttributeToPropertyMap;
class SVGCursorElement;
class SVGDocumentExtensions;
class SVGElementRareData;
class SVGSVGElement;
class SVGUseElement;

void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName);

class SVGElement : public StyledElement, public SVGLangSpace {
public:
    bool isOutermostSVGSVGElement() const;

    String xmlbase() const;
    void setXmlbase(const String&, ExceptionCode&);

    SVGSVGElement* ownerSVGElement() const;
    SVGElement* viewportElement() const;

    String title() const override;
    static bool isAnimatableCSSProperty(const QualifiedName&);
    bool isPresentationAttributeWithSVGDOM(const QualifiedName&);
    bool isKnownAttribute(const QualifiedName&);
    RefPtr<CSSValue> getPresentationAttribute(const String& name);
    virtual bool supportsMarkers() const { return false; }
    bool hasRelativeLengths() const { return !m_elementsWithRelativeLengths.isEmpty(); }
    virtual bool needsPendingResourceHandling() const { return true; }
    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);
    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;

    virtual bool isSVGGraphicsElement() const { return false; }
    virtual bool isFilterEffect() const { return false; }
    virtual bool isGradientStop() const { return false; }
    virtual bool isTextContent() const { return false; }
    virtual bool isSMILElement() const { return false; }

    // For SVGTests
    virtual bool isValid() const { return true; }

    virtual void svgAttributeChanged(const QualifiedName&);

    Vector<AnimatedPropertyType> animatedPropertyTypesForAttribute(const QualifiedName&);

    void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
    void sendSVGLoadEventIfPossibleAsynchronously();
    void svgLoadEventTimerFired();
    virtual Timer* svgLoadEventTimer();

    virtual AffineTransform* supplementalTransform() { return 0; }

    void invalidateSVGAttributes() { ensureUniqueElementData().setAnimatedSVGAttributesAreDirty(true); }
    void invalidateSVGPresentationAttributeStyle()
    {
        ensureUniqueElementData().setPresentationAttributeStyleIsDirty(true);
        // Trigger style recalculation for "elements as resource" (e.g. referenced by feImage).
        setNeedsStyleRecalc(InlineStyleChange);
    }

    // The instances of an element are clones made in shadow trees to implement <use>.
    const HashSet<SVGElement*>& instances() const;

    bool getBoundingBox(FloatRect&, SVGLocatable::StyleUpdateStrategy = SVGLocatable::AllowStyleUpdate);

    void setCursorElement(SVGCursorElement*);
    void cursorElementRemoved();
    void setCursorImageValue(CSSCursorImageValue*);
    void cursorImageValueRemoved();

    SVGElement* correspondingElement() const;
    SVGUseElement* correspondingUseElement() const;

    void setCorrespondingElement(SVGElement*);

    void synchronizeAnimatedSVGAttribute(const QualifiedName&) const;
    static void synchronizeAllAnimatedSVGAttribute(SVGElement*);
 
    RefPtr<RenderStyle> customStyleForRenderer(RenderStyle& parentStyle, RenderStyle* shadowHostStyle) override;

    static void synchronizeRequiredFeatures(SVGElement* contextElement);
    static void synchronizeRequiredExtensions(SVGElement* contextElement);
    static void synchronizeSystemLanguage(SVGElement* contextElement);

    virtual void synchronizeRequiredFeatures() { }
    virtual void synchronizeRequiredExtensions() { }
    virtual void synchronizeSystemLanguage() { }

    static QualifiedName animatableAttributeForName(const AtomicString&);
#ifndef NDEBUG
    bool isAnimatableAttribute(const QualifiedName&) const;
#endif

    MutableStyleProperties* animatedSMILStyleProperties() const;
    MutableStyleProperties& ensureAnimatedSMILStyleProperties();
    void setUseOverrideComputedStyle(bool);

    virtual bool haveLoadedRequiredResources();

    bool addEventListener(const AtomicString& eventType, RefPtr<EventListener>&&, bool useCapture) override;
    bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) override;
    bool hasFocusEventListeners() const;

#if ENABLE(CSS_REGIONS)
    bool shouldMoveToFlowThread(const RenderStyle&) const override;
#endif

    bool hasTagName(const SVGQualifiedName& name) const { return hasLocalName(name.localName()); }
    short tabIndex() const override;

    void callClearTarget() { clearTarget(); }

    class InstanceUpdateBlocker;

protected:
    SVGElement(const QualifiedName&, Document&);
    virtual ~SVGElement();

    bool isMouseFocusable() const override;
    bool supportsFocus() const override { return false; }

    bool rendererIsNeeded(const RenderStyle&) override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;

    void finishParsingChildren() override;
    void attributeChanged(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason = ModifiedDirectly) override;
    bool childShouldCreateRenderer(const Node&) const override;

    SVGElementRareData& ensureSVGRareData();

    void reportAttributeParsingError(SVGParsingError, const QualifiedName&, const AtomicString&);
    static CSSPropertyID cssPropertyIdForSVGAttributeName(const QualifiedName&);

    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void removedFrom(ContainerNode&) override;
    void childrenChanged(const ChildChange&) override;
    virtual bool selfHasRelativeLengths() const { return false; }
    void updateRelativeLengthsInformation() { updateRelativeLengthsInformation(selfHasRelativeLengths(), this); }
    void updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement*);

    class InstanceInvalidationGuard;

private:
    RenderStyle* computedStyle(PseudoId = NOPSEUDO) override final;
    bool willRecalcStyle(Style::Change) override;

    virtual bool isSupported(StringImpl* feature, StringImpl* version) const;

    virtual void clearTarget() { }

    void buildPendingResourcesIfNeeded();
    void accessKeyAction(bool sendMouseEvents) override;

#ifndef NDEBUG
    virtual bool filterOutAnimatableAttribute(const QualifiedName&) const;
#endif

    void invalidateInstances();

    std::unique_ptr<SVGElementRareData> m_svgRareData;

    HashSet<SVGElement*> m_elementsWithRelativeLengths;

    BEGIN_DECLARE_ANIMATED_PROPERTIES_BASE(SVGElement)
        DECLARE_ANIMATED_STRING(ClassName, className)
    END_DECLARE_ANIMATED_PROPERTIES

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
            QualifiedNameComponents components = { nullAtom.impl(), key.localName().impl(), key.namespaceURI().impl() };
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

#endif
