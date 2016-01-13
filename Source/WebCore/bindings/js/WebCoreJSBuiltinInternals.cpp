/*
 *  Copyright (c) 2015, Canon Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1.  Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *  2.  Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *  3.  Neither the name of Canon Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *  THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreJSBuiltinInternals.h"

#include "JSDOMGlobalObject.h"
#include "JSReadableStreamPrivateConstructors.h"
#include "WebCoreJSClientData.h"
#include <heap/SlotVisitorInlines.h>
#include <runtime/JSCJSValueInlines.h>
#include <runtime/StructureInlines.h>

using namespace JSC;

namespace WebCore {

JSBuiltinInternalFunctions::JSBuiltinInternalFunctions(JSC::VM& v)
    : vm(v)
#if ENABLE(MEDIA_STREAM)
    , m_rtcPeerConnectionInternalsFunctions(vm)
#endif
#if ENABLE(STREAMS_API)
    , m_readableStreamInternalsFunctions(vm)
    , m_streamInternalsFunctions(vm)
    , m_writableStreamInternalsFunctions(vm)
#endif
{
}

void JSBuiltinInternalFunctions::visit(JSC::SlotVisitor& visitor)
{
#if ENABLE(MEDIA_STREAM)
    m_rtcPeerConnectionInternalsFunctions.visit(visitor);
#endif
#if ENABLE(STREAMS_API)
    m_readableStreamInternalsFunctions.visit(visitor);
    m_streamInternalsFunctions.visit(visitor);
    m_writableStreamInternalsFunctions.visit(visitor);
#endif
#ifndef SKIP_UNUSED_PARAM
    UNUSED_PARAM(visitor);
#endif
}

void JSBuiltinInternalFunctions::initialize(JSDOMGlobalObject& globalObject, VM& vm)
{
#if ENABLE(MEDIA_STREAM)
    m_rtcPeerConnectionInternalsFunctions.init(globalObject);
#endif
#if ENABLE(STREAMS_API)
    m_readableStreamInternalsFunctions.init(globalObject);
    m_streamInternalsFunctions.init(globalObject);
    m_writableStreamInternalsFunctions.init(globalObject);
#endif

#if ENABLE(STREAMS_API) || ENABLE(MEDIA_STREAM)
    JSVMClientData& clientData = *static_cast<JSVMClientData*>(vm.clientData);

    JSDOMGlobalObject::GlobalPropertyInfo staticGlobals[] = {
#if ENABLE(MEDIA_STREAM)
#define DECLARE_GLOBAL_STATIC(name)\
        JSDOMGlobalObject::GlobalPropertyInfo(\
            clientData.builtinFunctions().rtcPeerConnectionInternalsBuiltins().name##PrivateName(), rtcPeerConnectionInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_RTCPEERCONNECTIONINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#endif
#if ENABLE(STREAMS_API)
#define DECLARE_GLOBAL_STATIC(name)\
        JSDOMGlobalObject::GlobalPropertyInfo(\
            clientData.builtinFunctions().readableStreamInternalsBuiltins().name##PrivateName(), readableStreamInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_READABLESTREAMINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#define DECLARE_GLOBAL_STATIC(name)\
        JSDOMGlobalObject::GlobalPropertyInfo(\
            clientData.builtinFunctions().streamInternalsBuiltins().name##PrivateName(), streamInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_STREAMINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#define DECLARE_GLOBAL_STATIC(name)\
        JSDOMGlobalObject::GlobalPropertyInfo(\
            clientData.builtinFunctions().writableStreamInternalsBuiltins().name##PrivateName(), writableStreamInternals().m_##name##Function.get() , DontDelete | ReadOnly),
        WEBCORE_FOREACH_WRITABLESTREAMINTERNALS_BUILTIN_FUNCTION_NAME(DECLARE_GLOBAL_STATIC)
#undef DECLARE_GLOBAL_STATIC
#endif
    };

    globalObject.addStaticGlobals(staticGlobals, WTF_ARRAY_LENGTH(staticGlobals));
#endif
#ifndef SKIP_UNUSED_PARAM
    UNUSED_PARAM(globalObject);
    UNUSED_PARAM(vm);
#endif
}

}
