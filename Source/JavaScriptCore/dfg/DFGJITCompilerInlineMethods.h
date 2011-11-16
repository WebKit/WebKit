/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef DFGJITCompilerInlineMethods_h
#define DFGJITCompilerInlineMethods_h


#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

#if USE(JSVALUE32_64)

inline void JITCompiler::emitLoadTag(NodeIndex index, GPRReg tag)
{
    if (isConstant(index)) {
        move(Imm32(valueOfJSConstant(index).tag()), tag);
        return;
    }

    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    load32(tagFor(virtualRegister), tag);
}

inline void JITCompiler::emitLoadPayload(NodeIndex index, GPRReg payload)
{
    if (isConstant(index)) {
        move(Imm32(valueOfJSConstant(index).payload()), payload);
        return;
    }

    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    load32(payloadFor(virtualRegister), payload);
}

inline void JITCompiler::emitLoad(const JSValue& v, GPRReg tag, GPRReg payload)
{
    move(Imm32(v.payload()), payload);
    move(Imm32(v.tag()), tag);
}

inline void JITCompiler::emitLoad(NodeIndex index, GPRReg tag, GPRReg payload)
{
    ASSERT(tag != payload);
    emitLoadPayload(index, payload);
    emitLoadTag(index, tag);
    return;
}

inline void JITCompiler::emitLoad2(NodeIndex index1, GPRReg tag1, GPRReg payload1, NodeIndex index2, GPRReg tag2, GPRReg payload2)
{
    emitLoad(index2, tag2, payload2);
    emitLoad(index1, tag1, payload1);
}

inline void JITCompiler::emitLoadDouble(NodeIndex index, FPRReg value)
{
    if (isConstant(index))
        loadDouble(addressOfDoubleConstant(index), value);
    else {
        Node& node = graph()[index];
        VirtualRegister virtualRegister = node.virtualRegister();
        loadDouble(addressFor(virtualRegister), value);
    }
}

inline void JITCompiler::emitLoadInt32ToDouble(NodeIndex index, FPRReg value)
{
    if (isConstant(index)) {
        char* bytePointer = reinterpret_cast<char*>(addressOfDoubleConstant(index));
        convertInt32ToDouble(AbsoluteAddress(bytePointer + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), value);
    } else {
        Node& node = graph()[index];
        VirtualRegister virtualRegister = node.virtualRegister();
        convertInt32ToDouble(payloadFor(virtualRegister), value);
    }
}

inline void JITCompiler::emitStore(NodeIndex index, GPRReg tag, GPRReg payload)
{
    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    store32(payload, payloadFor(virtualRegister));
    store32(tag, tagFor(virtualRegister));
}

inline void JITCompiler::emitStoreInt32(NodeIndex index, GPRReg payload, bool indexIsInt32)
{
    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    store32(payload, payloadFor(virtualRegister));
    if (!indexIsInt32)
        store32(TrustedImm32(JSValue::Int32Tag), tagFor(virtualRegister));
}

inline void JITCompiler::emitStoreInt32(NodeIndex index, TrustedImm32 payload, bool indexIsInt32)
{
    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    store32(payload, payloadFor(virtualRegister));
    if (!indexIsInt32)
        store32(TrustedImm32(JSValue::Int32Tag), tagFor(virtualRegister));
}

inline void JITCompiler::emitStoreCell(NodeIndex index, GPRReg payload, bool indexIsCell)
{
    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    store32(payload, payloadFor(virtualRegister));
    if (!indexIsCell)
        store32(TrustedImm32(JSValue::CellTag), tagFor(virtualRegister));
}

inline void JITCompiler::emitStoreBool(NodeIndex index, GPRReg payload, bool indexIsBool)
{
    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    store32(payload, payloadFor(virtualRegister));
    if (!indexIsBool)
        store32(TrustedImm32(JSValue::BooleanTag), tagFor(virtualRegister));
}

inline void JITCompiler::emitStoreDouble(NodeIndex index, FPRReg value)
{
    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    storeDouble(value, addressFor(virtualRegister));
}

inline void JITCompiler::emitStore(NodeIndex index, const JSValue constant)
{
    Node& node = graph()[index];
    VirtualRegister virtualRegister = node.virtualRegister();
    store32(Imm32(constant.payload()), payloadFor(virtualRegister));
    store32(Imm32(constant.tag()), tagFor(virtualRegister));
}

#endif // USE(JSVALUE32_64)

} } // namespace JSC::DFG

#endif // ENABLE_DFG_JIT

#endif
