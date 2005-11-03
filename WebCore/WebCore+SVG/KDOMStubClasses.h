/*
 *  KDOMStubClasses.h
 *  WebCore
 *
 *  Created by Eric Seidel on 10/29/05.
 *  Copyright 2005 Apple Computer, Inc.. All rights reserved.
 *
 */

#ifndef KDOMStubClasses_h
#define KDOMStubClasses_h

#define XMLElementImpl StyledElementImpl

#include "dom_elementimpl.h"
#include "KWQKPartsPart.h"

namespace KDOM
{
    typedef NodeImpl EventTargetImpl;
    
    // FIXME: should be replaced by SharedPtr use.
    template<class T>
    inline void KDOM_SAFE_SET(T *&a, T *b)
    {
        if (b) b->ref();
        if (a) a->deref();
        a = b;
    }
    
    /**
     * The CDFInterface class has to be implemented by 'layers on top'
     * of kdom (ie. ksvg2/khtml2). This class connects kdom with the
     * layer on top in following ways:
     *
     * - CSS values/properties access / render-style
     * - EcmaScript global object / EcmaInterface object
     *
     **/
    class GlobalObject;
    class EcmaInterface;
    class CDFInterface
    {
    public:
        CDFInterface();
        virtual ~CDFInterface();

        // CSS values/properties
        virtual const char *getValueName(unsigned short) const = 0;
        virtual const char *getPropertyName(unsigned short) const = 0;
        
        virtual int getValueID(const char *valStr, int len) const = 0;
        virtual int getPropertyID(const char *propStr, int len) const = 0;

        virtual RenderStyle *renderStyle() const = 0;
        virtual bool cssPropertyApplyFirst(int) const = 0;

        // EcmaScript interface
        virtual EcmaInterface *ecmaInterface() const = 0;
        virtual GlobalObject *globalObject(DocumentImpl *doc) const = 0;
    };

};

#endif // KDOMStubClasses_h