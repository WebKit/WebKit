#ifndef KSVG_SVGFESpotLightElementImpl_H
#define KSVG_SVGFESpotLightElementImpl_H

#include "SVGFELightElementImpl.h"

namespace KSVG
{
    class SVGFESpotLightElementImpl : public SVGFELightElementImpl
    {
    public:
        SVGFESpotLightElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFESpotLightElementImpl();
        virtual KCLightSource *lightSource() const;
    };
};

#endif
