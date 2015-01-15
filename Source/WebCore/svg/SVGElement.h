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
#include "SVGElementTypeHelpers.h"
#include "SVGLangSpace.h"
#include "SVGLocatable.h"
#include "SVGParsingError.h"
#include "SVGPropertyInfo.h"
#include "StyledElement.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class AffineTransform;
class CSSCursorImageValue;
class CSSStyleDeclaration;
class CSSValue;
class Document;
class SVGAttributeToPropertyMap;
class SVGCursorElement;
class SVGDocumentExtensions;
class SVGElementInstance;
class SVGElementRareData;
class SVGSVGElement;

void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName);

class SVGElement : public StyledElement, public SVGLangSpace {
public:
    bool isOutermostSVGSVGElement() const;

    String xmlbase() const;
    void setXmlbase(const String&, ExceptionCode&);

    SVGSVGElement* ownerSVGElement() const;
    SVGElement* viewportElement() const;

    virtual String title() const override;
    static bool isAnimatableCSSProperty(const QualifiedName&);
    bool isKnownAttribute(const QualifiedName&);
    PassRefPtr<CSSValue> getPresentationAttribute(const String& name);
    virtual bool supportsMarkers() const { return false; }
    bool hasRelativeLengths() const { return !m_elementsWithRelativeLengths.isEmpty(); }
    virtual bool needsPendingResourceHandling() const { return true; }
    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);
    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;

    SVGDocumentExtensions* accessDocumentSVGExtensions();

    virtual bool isSVGGraphicsElement() const { return false; }
    virtual bool isFilterEffect() const { return false; }
    virtual bool isGradientStop() const { return false; }
    virtual bool isTextContent() const { return false; }
    virtual bool isSMILElement() const { return false; }

    // For SVGTests
    virtual bool isValid() const { return true; }

    virtual void svgAttributeChanged(const QualifiedName&);

    virtual void animatedPropertyTypeForAttribute(const QualifiedName&, Vector<AnimatedPropertyType>&);

    void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
    void sendSVGLoadEventIfPossibleAsynchronously();
    void svgLoadEventTimerFired(Timer*);
    virtual Timer* svgLoadEventTimer();

    virtual AffineTransform* supplementalTransform() { return 0; }

    void invalidateSVGAttributes() { ensureUniqueElementData().setAnimatedSVGAttributesAreDirty(true); }
    void invalidateSVGPresentationAttributeStyle() { ensureUniqueElementData().setPresentationAttributeStyleIsDirty(true); }

    const HashSet<SVGElementInstance*>& instancesForElement() const;

    bool getBoundingBox(FloatRect&, SVGLocatable::StyleUpdateStrategy = SVGLocatable::AllowStyleUpdate);

    void setCursorElement(SVGCursorElement*);
    void cursorElementRemoved();
    void setCursorImageValue(CSSCursorImageValue*);
    void cursorImageValueRemoved();

    SVGElement* correspondingElement();
    void setCorrespondingElement(SVGElement*);

    void synchronizeAnimatedSVGAttribute(const QualifiedName&) const;
    static void synchronizeAllAnimatedSVGAttribute(SVGElement*);
 
    virtual PassRefPtr<RenderStyle> customStyleForRenderer(RenderStyle& parentStyle) override;

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

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) override;
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) override;
    bool hasFocusEventListeners() const;

#if ENABLE(CSS_REGIONS)
    virtual bool shouldMoveToFlowThread(const RenderStyle&) const override;
#endif

    bool hasTagName(const SVGQualifiedName& name) const { return hasLocalName(name.localName()); }
    virtual short tabIndex() const override;

    void callClearTarget() { clearTarget(); }

protected:
    SVGElement(const QualifiedName&, Document&);
    virtual ~SVGElement();

    virtual bool isMouseFocusable() const override;
    virtual bool supportsFocus() const override { return false; }

    virtual bool rendererIsNeeded(const RenderStyle&) override;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;

    virtual void finishParsingChildren() override;
    virtual void attributeChanged(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason = ModifiedDirectly) override;
    virtual bool childShouldCreateRenderer(const Node&) const override;

    SVGElementRareData& ensureSVGRareData();

    void reportAttributeParsingError(SVGParsingError, const QualifiedName&, const AtomicString&);
    static CSSPropertyID cssPropertyIdForSVGAttributeName(const QualifiedName&);

    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;
    virtual void childrenChanged(const ChildChange&) override;
    virtual bool selfHasRelativeLengths() const { return false; }
    void updateRelativeLengthsInformation() { updateRelativeLengthsInformation(selfHasRelativeLengths(), this); }
    void updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement*);

private:
    friend class SVGElementInstance;

    virtual RenderStyle* computedStyle(PseudoId = NOPSEUDO) override final;
    virtual bool willRecalcStyle(Style::Change) override;

    virtual bool isSupported(StringImpl* feature, StringImpl* version) const;

    virtual void clearTarget() { }

    void mapInstanceToElement(SVGElementInstance*);
    void removeInstanceMapping(SVGElementInstance*);

    void buildPendingResourcesIfNeeded();
    virtual void accessKeyAction(bool sendMouseEvents) override;

#ifndef NDEBUG
    virtual bool filterOutAnimatableAttribute(const QualifiedName&) const;
#endif

    std::unique_ptr<SVGElementRareData> m_svgRareData;

    HashSet<SVGElement*> m_elementsWithRelativeLengths;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGElement)
        DECLARE_ANIMATED_STRING(ClassName, className)
    END_DECLARE_ANIMATED_PROPERTIES

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

void isSVGElement(const SVGElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isSVGElement(const Node& node) { return node.isSVGElement(); }
template <> inline bool isElementOfType<const SVGElement>(const Element& element) { return element.isSVGElement(); }

NODE_TYPE_CASTS(SVGElement)

inline bool Node::hasTagName(const SVGQualifiedName& name) const
{
    return isSVGElement() && toSVGElement(*this).hasTagName(name);
}

}

#endif
