#ifndef KSVG_SVGFEPointLightElementImpl_H
#define KSVG_SVGFEPointLightElementImpl_H

#include "SVGFELightElementImpl.h"

namespace KSVG
{
    class SVGFEPointLightElementImpl : public SVGFELightElementImpl
    {
        public:
        SVGFEPointLightElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFEPointLightElementImpl();
        virtual KCLightSource *lightSource() const;
    };
};

#endif
