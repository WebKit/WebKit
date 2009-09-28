/*
 * Copyright (C) 2009 Google, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DOMObjectWithSVGContext_h
#define DOMObjectWithSVGContext_h

#if ENABLE(SVG)

#include "JSDOMBinding.h"
#include "SVGElement.h"

namespace WebCore {

    // FIXME: This class (and file) should be removed once all SVG bindings
    // have moved context() onto the various impl() pointers.
    class DOMObjectWithSVGContext : public DOMObject {
    public:
        SVGElement* context() const { return m_context.get(); }

    protected:
        DOMObjectWithSVGContext(NonNullPassRefPtr<JSC::Structure> structure, JSDOMGlobalObject*, SVGElement* context)
            : DOMObject(structure)
            , m_context(context)
        {
            // No space to store the JSDOMGlobalObject w/o hitting the CELL_SIZE limit.
        }

    protected: // FIXME: Many custom bindings use m_context directly.  Making this protected to temporariliy reduce code churn.
        RefPtr<SVGElement> m_context;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // DOMObjectWithSVGContext_h
