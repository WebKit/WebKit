/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Rob Buis <buis@kde.org>
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

#ifndef SVGSVGElement_h
#define SVGSVGElement_h

#if ENABLE(SVG)
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedRect.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
#include "SVGStyledLocatableElement.h"
#include "SVGTests.h"
#include "SVGZoomAndPan.h"

namespace WebCore {

class SVGAngle;
class SVGMatrix;
class SVGTransform;
class SVGViewSpec;
class SVGViewElement;
class SMILTimeContainer;

class SVGSVGElement : public SVGStyledLocatableElement,
                      public SVGTests,
                      public SVGLangSpace,
                      public SVGExternalResourcesRequired,
                      public SVGFitToViewBox,
                      public SVGZoomAndPan {
public:
    static PassRefPtr<SVGSVGElement> create(const QualifiedName&, Document*);

    virtual bool isValid() const { return SVGTests::isValid(); }
    virtual bool supportsFocus() const { return true; }

    // 'SVGSVGElement' functions
    const AtomicString& contentScriptType() const;
    void setContentScriptType(const AtomicString& type);

    const AtomicString& contentStyleType() const;
    void setContentStyleType(const AtomicString& type);

    FloatRect viewport() const;

    float pixelUnitToMillimeterX() const;
    float pixelUnitToMillimeterY() const;
    float screenPixelToMillimeterX() const;
    float screenPixelToMillimeterY() const;

    bool useCurrentView() const;
    void setUseCurrentView(bool currentView);

    SVGViewSpec* currentView() const;

    // RenderSVGRoot wants to query the intrinsic size, by only examining the width/height attributes.
    Length intrinsicWidthIgnoringCSSProperties() const;
    Length intrinsicHeightIgnoringCSSProperties() const;
    Length intrinsicWidth() const;
    Length intrinsicHeight() const;
    FloatSize currentViewportSize() const;
    FloatRect currentViewBoxRect() const;

    float currentScale() const;
    void setCurrentScale(float scale);

    FloatPoint& currentTranslate() { return m_translation; }
    void setCurrentTranslate(const FloatPoint&);

    // Only used from the bindings.
    void updateCurrentTranslate();

    SMILTimeContainer* timeContainer() const { return m_timeContainer.get(); }
    
    void pauseAnimations();
    void unpauseAnimations();
    bool animationsPaused() const;

    float getCurrentTime() const;
    void setCurrentTime(float seconds);

    unsigned suspendRedraw(unsigned maxWaitMilliseconds);
    void unsuspendRedraw(unsigned suspendHandleId);
    void unsuspendRedrawAll();
    void forceRedraw();

    PassRefPtr<NodeList> getIntersectionList(const FloatRect&, SVGElement* referenceElement) const;
    PassRefPtr<NodeList> getEnclosureList(const FloatRect&, SVGElement* referenceElement) const;
    bool checkIntersection(SVGElement*, const FloatRect&) const;
    bool checkEnclosure(SVGElement*, const FloatRect&) const;
    void deselectAll();

    static float createSVGNumber();
    static SVGLength createSVGLength();
    static SVGAngle createSVGAngle();
    static FloatPoint createSVGPoint();
    static SVGMatrix createSVGMatrix();
    static FloatRect createSVGRect();
    static SVGTransform createSVGTransform();
    static SVGTransform createSVGTransformFromMatrix(const SVGMatrix&);

    AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

    void setupInitialView(const String& fragmentIdentifier, Element* anchorNode);

    bool isOutermostSVG() const;

    Element* getElementById(const AtomicString&) const;

    bool widthAttributeEstablishesViewport() const;
    bool heightAttributeEstablishesViewport() const;

protected:
    virtual void didMoveToNewDocument(Document* oldDocument) OVERRIDE;

private:
    SVGSVGElement(const QualifiedName&, Document*);
    virtual ~SVGSVGElement();

    virtual bool isSVG() const { return true; }
    
    virtual void parseMappedAttribute(Attribute*);

    virtual bool rendererIsNeeded(const NodeRenderingContext& context) { return StyledElement::rendererIsNeeded(context); }
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual void svgAttributeChanged(const QualifiedName&);

    virtual bool selfHasRelativeLengths() const;

    void inheritViewAttributes(SVGViewElement*);

    enum CollectIntersectionOrEnclosure {
        CollectIntersectionList,
        CollectEnclosureList
    };

    PassRefPtr<NodeList> collectIntersectionOrEnclosureList(const FloatRect&, SVGElement*, CollectIntersectionOrEnclosure) const;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGSVGElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_LENGTH(Width, width)
        DECLARE_ANIMATED_LENGTH(Height, height)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
        DECLARE_ANIMATED_RECT(ViewBox, viewBox)
        DECLARE_ANIMATED_PRESERVEASPECTRATIO(PreserveAspectRatio, preserveAspectRatio)
    END_DECLARE_ANIMATED_PROPERTIES

    // SVGTests
    virtual void synchronizeRequiredFeatures() { SVGTests::synchronizeRequiredFeatures(this); }
    virtual void synchronizeRequiredExtensions() { SVGTests::synchronizeRequiredExtensions(this); }
    virtual void synchronizeSystemLanguage() { SVGTests::synchronizeSystemLanguage(this); }

    virtual void documentWillSuspendForPageCache();
    virtual void documentDidResumeFromPageCache();

    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;

    bool m_useCurrentView;
    RefPtr<SMILTimeContainer> m_timeContainer;
    FloatPoint m_translation;
    mutable OwnPtr<SVGViewSpec> m_viewSpec;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
