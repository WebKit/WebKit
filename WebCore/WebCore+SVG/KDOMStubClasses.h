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
};

#endif // KDOMStubClasses_h