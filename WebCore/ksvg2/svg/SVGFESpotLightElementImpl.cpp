#include "SVGFESpotLightElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFESpotLightElementImpl::SVGFESpotLightElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : 
SVGFELightElementImpl(tagName, doc)
{
}

SVGFESpotLightElementImpl::~SVGFESpotLightElementImpl()
{
}

KCLightSource *SVGFESpotLightElementImpl::lightSource() const
{
    KCanvasPoint3F pos(x()->baseVal(), y()->baseVal(), z()->baseVal());
    //convert lookAt to a direction
    KCanvasPoint3F direction(pointsAtX()->baseVal() - pos.x(), 
                             pointsAtY()->baseVal() - pos.y(), 
                             pointsAtZ()->baseVal() - pos.z());
    direction.normalize();
    return new KCSpotLightSource(pos, direction, specularExponent()->baseVal(), limitingConeAngle()->baseVal());
}
