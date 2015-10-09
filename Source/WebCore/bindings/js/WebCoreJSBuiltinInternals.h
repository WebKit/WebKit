/*
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebCoreJSBuiltinInternals_h
#define WebCoreJSBuiltinInternals_h

#if ENABLE(STREAMS_API)
#include "ReadableStreamInternalsBuiltinsWrapper.h"
#endif

namespace WebCore {

class JSBuiltinInternalFunctions {
public:
explicit JSBuiltinInternalFunctions(JSC::VM& v)
        : vm(v)
#if ENABLE(STREAMS_API)
        , m_readableStreamInternalsFunctions(vm)
#endif
    { }

#if ENABLE(STREAMS_API)
    ReadableStreamInternalsBuiltinFunctions readableStreamInternals() { return m_readableStreamInternalsFunctions; }
#endif
    void visit(JSC::SlotVisitor& visitor) {
#if ENABLE(STREAMS_API)
        m_readableStreamInternalsFunctions.visit(visitor);
#else
        UNUSED_PARAM(visitor);
#endif
    }
    void init(JSC::JSGlobalObject& globalObject) {
#if ENABLE(STREAMS_API)
        m_readableStreamInternalsFunctions.init(globalObject);
#else
        UNUSED_PARAM(globalObject);
#endif
    }

private:
    JSC::VM& vm;
#if ENABLE(STREAMS_API)
     ReadableStreamInternalsBuiltinFunctions m_readableStreamInternalsFunctions;
#endif

};

}
#endif
