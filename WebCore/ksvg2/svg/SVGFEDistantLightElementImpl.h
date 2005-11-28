#ifndef KSVG_SVGFEDistantLightElementImpl_H
#define KSVG_SVGFEDistantLightElementImpl_H

#include "SVGFELightElementImpl.h"

namespace KSVG
{
    class SVGFEDistantLightElementImpl : public SVGFELightElementImpl
    { 
    public:
        SVGFEDistantLightElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFEDistantLightElementImpl();
        virtual KCLightSource *lightSource() const;
    };
};

#endif
