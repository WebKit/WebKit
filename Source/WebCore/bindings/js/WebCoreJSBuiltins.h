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

#ifndef WebCoreJSBuiltins_h
#define WebCoreJSBuiltins_h

#if ENABLE(STREAMS_API)
#include "ByteLengthQueuingStrategyBuiltinsWrapper.h"
#include "CountQueuingStrategyBuiltinsWrapper.h"
#include "ReadableStreamBuiltinsWrapper.h"
#include "ReadableStreamControllerBuiltinsWrapper.h"
#include "ReadableStreamInternalsBuiltinsWrapper.h"
#include "ReadableStreamReaderBuiltinsWrapper.h"
#endif


#include <runtime/VM.h>

namespace WebCore {

class JSBuiltinFunctions {
public:
    explicit JSBuiltinFunctions(JSC::VM& v)
        : vm(v)
#if ENABLE(STREAMS_API)
        , m_byteLengthQueuingStrategyBuiltins(&vm)
        , m_countQueuingStrategyBuiltins(&vm)
        , m_readableStreamBuiltins(&vm)
        , m_readableStreamControllerBuiltins(&vm)
        , m_readableStreamInternalsBuiltins(&vm)
        , m_readableStreamReaderBuiltins(&vm)
#endif
    {
#if ENABLE(STREAMS_API)
        m_readableStreamInternalsBuiltins.exportNames();
#endif
    }
#if ENABLE(STREAMS_API)
    ByteLengthQueuingStrategyBuiltinsWrapper& byteLengthQueuingStrategyBuiltins() { return m_byteLengthQueuingStrategyBuiltins;}
    CountQueuingStrategyBuiltinsWrapper& countQueuingStrategyBuiltins() { return m_countQueuingStrategyBuiltins;}
    ReadableStreamBuiltinsWrapper& readableStreamBuiltins() { return m_readableStreamBuiltins;}
    ReadableStreamControllerBuiltinsWrapper& readableStreamControllerBuiltins() { return m_readableStreamControllerBuiltins;}
    ReadableStreamInternalsBuiltinsWrapper& readableStreamInternalsBuiltins() { return m_readableStreamInternalsBuiltins;}
    ReadableStreamReaderBuiltinsWrapper& readableStreamReaderBuiltins() { return m_readableStreamReaderBuiltins;}
#endif

private:
    JSC::VM& vm;
#if ENABLE(STREAMS_API)
    ByteLengthQueuingStrategyBuiltinsWrapper m_byteLengthQueuingStrategyBuiltins;
    CountQueuingStrategyBuiltinsWrapper m_countQueuingStrategyBuiltins;
    ReadableStreamBuiltinsWrapper m_readableStreamBuiltins;
    ReadableStreamControllerBuiltinsWrapper m_readableStreamControllerBuiltins;
    ReadableStreamInternalsBuiltinsWrapper m_readableStreamInternalsBuiltins;
    ReadableStreamReaderBuiltinsWrapper m_readableStreamReaderBuiltins;
#endif

};

}

#endif
