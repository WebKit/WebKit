#include "SVGFEPointLightElementImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGFEPointLightElementImpl::SVGFEPointLightElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : 
SVGFELightElementImpl(tagName, doc)
{
}

SVGFEPointLightElementImpl::~SVGFEPointLightElementImpl()
{
}

KCLightSource *SVGFEPointLightElementImpl::lightSource() const
{
    KCanvasPoint3F pos(x()->baseVal(), y()->baseVal(), z()->baseVal());
    return new KCPointLightSource(pos);
}
