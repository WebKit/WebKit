#include "SVGFEDistantLightElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEDistantLightElementImpl::SVGFEDistantLightElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : 
SVGFELightElementImpl(tagName, doc)
{
}

SVGFEDistantLightElementImpl::~SVGFEDistantLightElementImpl()
{
}

KCLightSource *SVGFEDistantLightElementImpl::lightSource() const
{
    return new KCDistantLightSource(azimuth()->baseVal(), elevation()->baseVal());
}
