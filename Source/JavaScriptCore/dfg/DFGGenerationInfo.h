
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DFGGenerationInfo_h
#define DFGGenerationInfo_h

#if ENABLE(DFG_JIT)

#include "DataFormat.h"
#include <dfg/DFGJITCompiler.h>

namespace JSC { namespace DFG {

// === GenerationInfo ===
//
// This class is used to track the current status of a live values during code generation.
// Can provide information as to whether a value is in machine registers, and if so which,
// whether a value has been spilled to the RegsiterFile, and if so may be able to provide
// details of the format in memory (all values are spilled in a boxed form, but we may be
// able to track the type of box), and tracks how many outstanding uses of a value remain,
// so that we know when the value is dead and the machine registers associated with it
// may be released.
class GenerationInfo {
public:
    GenerationInfo()
        : m_nodeIndex(NoNode)
        , m_useCount(0)
        , m_registerFormat(DataFormatNone)
        , m_spillFormat(DataFormatNone)
        , m_canFill(false)
    {
    }

    void initConstant(NodeIndex nodeIndex, uint32_t useCount)
    {
        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = DataFormatNone;
        m_spillFormat = DataFormatNone;
        m_canFill = true;
        ASSERT(m_useCount);
    }
    void initInteger(NodeIndex nodeIndex, uint32_t useCount, GPRReg gpr)
    {
        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = DataFormatInteger;
        m_spillFormat = DataFormatNone;
        m_canFill = false;
        u.gpr = gpr;
        ASSERT(m_useCount);
    }
#if USE(JSVALUE64)
    void initJSValue(NodeIndex nodeIndex, uint32_t useCount, GPRReg gpr, DataFormat format = DataFormatJS)
    {
        ASSERT(format & DataFormatJS);

        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = format;
        m_spillFormat = DataFormatNone;
        m_canFill = false;
        u.gpr = gpr;
        ASSERT(m_useCount);
    }
#elif USE(JSVALUE32_64)
    void initJSValue(NodeIndex nodeIndex, uint32_t useCount, GPRReg tagGPR, GPRReg payloadGPR, DataFormat format = DataFormatJS)
    {
        ASSERT(format & DataFormatJS);

        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = format;
        m_spillFormat = DataFormatNone;
        m_canFill = false;
        u.v.tagGPR = tagGPR;
        u.v.payloadGPR = payloadGPR;
        ASSERT(m_useCount);
    }
#endif
    void initCell(NodeIndex nodeIndex, uint32_t useCount, GPRReg gpr)
    {
        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = DataFormatCell;
        m_spillFormat = DataFormatNone;
        m_canFill = false;
        u.gpr = gpr;
        ASSERT(m_useCount);
    }
    void initBoolean(NodeIndex nodeIndex, uint32_t useCount, GPRReg gpr)
    {
        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = DataFormatBoolean;
        m_spillFormat = DataFormatNone;
        m_canFill = false;
        u.gpr = gpr;
        ASSERT(m_useCount);
    }
    void initDouble(NodeIndex nodeIndex, uint32_t useCount, FPRReg fpr)
    {
        ASSERT(fpr != InvalidFPRReg);
        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = DataFormatDouble;
        m_spillFormat = DataFormatNone;
        m_canFill = false;
        u.fpr = fpr;
        ASSERT(m_useCount);
    }
    void initStorage(NodeIndex nodeIndex, uint32_t useCount, GPRReg gpr)
    {
        m_nodeIndex = nodeIndex;
        m_useCount = useCount;
        m_registerFormat = DataFormatStorage;
        m_spillFormat = DataFormatNone;
        m_canFill = false;
        u.gpr = gpr;
        ASSERT(m_useCount);
    }

    // Get the index of the node that produced this value.
    NodeIndex nodeIndex() { return m_nodeIndex; }

    // Mark the value as having been used (decrement the useCount).
    // Returns true if this was the last use of the value, and any
    // associated machine registers may be freed.
    bool use()
    {
        ASSERT(m_useCount);
        return !--m_useCount;
    }

    // Used to check the operands of operations to see if they are on
    // their last use; in some cases it may be safe to reuse the same
    // machine register for the result of the operation.
    bool canReuse()
    {
        ASSERT(m_useCount);
        return m_useCount == 1;
    }

    // Get the format of the value in machine registers (or 'none').
    DataFormat registerFormat() { return m_registerFormat; }
    // Get the format of the value as it is spilled in the RegisterFile (or 'none').
    DataFormat spillFormat() { return m_spillFormat; }
    
    bool isJSFormat(DataFormat expectedFormat)
    {
        return JSC::isJSFormat(registerFormat(), expectedFormat) || JSC::isJSFormat(spillFormat(), expectedFormat);
    }
    
    bool isJSInteger()
    {
        return isJSFormat(DataFormatJSInteger);
    }
    
    bool isJSDouble()
    {
        return isJSFormat(DataFormatJSDouble);
    }
    
    bool isJSCell()
    {
        return isJSFormat(DataFormatJSCell);
    }
    
    bool isJSBoolean()
    {
        return isJSFormat(DataFormatJSBoolean);
    }
    
    bool isUnknownJS()
    {
        return spillFormat() == DataFormatNone
            ? registerFormat() == DataFormatJS || registerFormat() == DataFormatNone
            : spillFormat() == DataFormatJS;
    }

    // Get the machine resister currently holding the value.
#if USE(JSVALUE64)
    GPRReg gpr() { ASSERT(m_registerFormat && m_registerFormat != DataFormatDouble); return u.gpr; }
    FPRReg fpr() { ASSERT(m_registerFormat == DataFormatDouble); return u.fpr; }
    JSValueRegs jsValueRegs() { ASSERT(m_registerFormat & DataFormatJS); return JSValueRegs(u.gpr); }
#elif USE(JSVALUE32_64)
    GPRReg gpr() { ASSERT(!(m_registerFormat & DataFormatJS) && m_registerFormat != DataFormatDouble); return u.gpr; }
    GPRReg tagGPR() { ASSERT(m_registerFormat & DataFormatJS); return u.v.tagGPR; }
    GPRReg payloadGPR() { ASSERT(m_registerFormat & DataFormatJS); return u.v.payloadGPR; }
    FPRReg fpr() { ASSERT(m_registerFormat == DataFormatDouble || m_registerFormat == DataFormatJSDouble); return u.fpr; }
    JSValueRegs jsValueRegs() { ASSERT(m_registerFormat & DataFormatJS); return JSValueRegs(u.v.tagGPR, u.v.payloadGPR); }
#endif

    // Check whether a value needs spilling in order to free up any associated machine registers.
    bool needsSpill()
    {
        // This should only be called on values that are currently in a register.
        ASSERT(m_registerFormat != DataFormatNone);
        // Constants do not need spilling, nor do values that have already been
        // spilled to the RegisterFile.
        return !m_canFill;
    }

    // Called when a VirtualRegister is being spilled to the RegisterFile for the first time.
    void spill(DataFormat spillFormat)
    {
        // We shouldn't be spill values that don't need spilling.
        ASSERT(!m_canFill);
        ASSERT(m_spillFormat == DataFormatNone);
        // We should only be spilling values that are currently in machine registers.
        ASSERT(m_registerFormat != DataFormatNone);

        m_registerFormat = DataFormatNone;
        m_spillFormat = spillFormat;
        m_canFill = true;
    }

    // Called on values that don't need spilling (constants and values that have
    // already been spilled), to mark them as no longer being in machine registers.
    void setSpilled()
    {
        // Should only be called on values that don't need spilling, and are currently in registers.
        ASSERT(m_canFill && m_registerFormat != DataFormatNone);
        m_registerFormat = DataFormatNone;
    }
    
    void killSpilled()
    {
        m_spillFormat = DataFormatNone;
        m_canFill = false;
    }

    // Record that this value is filled into machine registers,
    // tracking which registers, and what format the value has.
#if USE(JSVALUE64)
    void fillJSValue(GPRReg gpr, DataFormat format = DataFormatJS)
    {
        ASSERT(format & DataFormatJS);
        m_registerFormat = format;
        u.gpr = gpr;
    }
#elif USE(JSVALUE32_64)
    void fillJSValue(GPRReg tagGPR, GPRReg payloadGPR, DataFormat format = DataFormatJS)
    {
        ASSERT(format & DataFormatJS);
        m_registerFormat = format;
        u.v.tagGPR = tagGPR; // FIXME: for JSValues with known type (boolean, integer, cell etc.) no tagGPR is needed?
        u.v.payloadGPR = payloadGPR;
    }
    void fillCell(GPRReg gpr)
    {
        m_registerFormat = DataFormatCell;
        u.gpr = gpr;
    }
#endif
    void fillInteger(GPRReg gpr)
    {
        m_registerFormat = DataFormatInteger;
        u.gpr = gpr;
    }
    void fillBoolean(GPRReg gpr)
    {
        m_registerFormat = DataFormatBoolean;
        u.gpr = gpr;
    }
    void fillDouble(FPRReg fpr)
    {
        ASSERT(fpr != InvalidFPRReg);
        m_registerFormat = DataFormatDouble;
        u.fpr = fpr;
    }
    void fillStorage(GPRReg gpr)
    {
        m_registerFormat = DataFormatStorage;
        u.gpr = gpr;
    }

    bool alive()
    {
        return m_useCount;
    }

private:
    // The index of the node whose result is stored in this virtual register.
    NodeIndex m_nodeIndex;
    uint32_t m_useCount;
    DataFormat m_registerFormat;
    DataFormat m_spillFormat;
    bool m_canFill;
    union {
        GPRReg gpr;
        FPRReg fpr;
#if USE(JSVALUE32_64)
        struct {
            GPRReg tagGPR;
            GPRReg payloadGPR;
        } v;
#endif
    } u;
};

} } // namespace JSC::DFG

#endif
#endif
