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

#ifndef DFGValueSource_h
#define DFGValueSource_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGCommon.h"
#include "DataFormat.h"
#include "SpeculatedType.h"
#include "ValueRecovery.h"

namespace JSC { namespace DFG {

enum ValueSourceKind {
    SourceNotSet,
    ValueInRegisterFile,
    Int32InRegisterFile,
    CellInRegisterFile,
    BooleanInRegisterFile,
    DoubleInRegisterFile,
    ArgumentsSource,
    SourceIsDead,
    HaveNode
};

static inline ValueSourceKind dataFormatToValueSourceKind(DataFormat dataFormat)
{
    switch (dataFormat) {
    case DataFormatInteger:
        return Int32InRegisterFile;
    case DataFormatDouble:
        return DoubleInRegisterFile;
    case DataFormatBoolean:
        return BooleanInRegisterFile;
    case DataFormatCell:
        return CellInRegisterFile;
    case DataFormatDead:
        return SourceIsDead;
    case DataFormatArguments:
        return ArgumentsSource;
    default:
        ASSERT(dataFormat & DataFormatJS);
        return ValueInRegisterFile;
    }
}

static inline DataFormat valueSourceKindToDataFormat(ValueSourceKind kind)
{
    switch (kind) {
    case ValueInRegisterFile:
        return DataFormatJS;
    case Int32InRegisterFile:
        return DataFormatInteger;
    case CellInRegisterFile:
        return DataFormatCell;
    case BooleanInRegisterFile:
        return DataFormatBoolean;
    case DoubleInRegisterFile:
        return DataFormatDouble;
    case ArgumentsSource:
        return DataFormatArguments;
    case SourceIsDead:
        return DataFormatDead;
    default:
        return DataFormatNone;
    }
}

static inline bool isInRegisterFile(ValueSourceKind kind)
{
    DataFormat format = valueSourceKindToDataFormat(kind);
    return format != DataFormatNone && format < DataFormatOSRMarker;
}

// Can this value be recovered without having to look at register allocation state or
// DFG node liveness?
static inline bool isTriviallyRecoverable(ValueSourceKind kind)
{
    return valueSourceKindToDataFormat(kind) != DataFormatNone;
}

class ValueSource {
public:
    ValueSource()
        : m_nodeIndex(nodeIndexFromKind(SourceNotSet))
    {
    }
    
    explicit ValueSource(ValueSourceKind valueSourceKind)
        : m_nodeIndex(nodeIndexFromKind(valueSourceKind))
    {
        ASSERT(kind() != SourceNotSet);
        ASSERT(kind() != HaveNode);
    }
    
    explicit ValueSource(NodeIndex nodeIndex)
        : m_nodeIndex(nodeIndex)
    {
        ASSERT(nodeIndex != NoNode);
        ASSERT(kind() == HaveNode);
    }
    
    static ValueSource forSpeculation(SpeculatedType prediction)
    {
        if (isInt32Speculation(prediction))
            return ValueSource(Int32InRegisterFile);
        if (isArraySpeculation(prediction))
            return ValueSource(CellInRegisterFile);
        if (isBooleanSpeculation(prediction))
            return ValueSource(BooleanInRegisterFile);
        return ValueSource(ValueInRegisterFile);
    }
    
    static ValueSource forDataFormat(DataFormat dataFormat)
    {
        return ValueSource(dataFormatToValueSourceKind(dataFormat));
    }
    
    bool isSet() const
    {
        return kindFromNodeIndex(m_nodeIndex) != SourceNotSet;
    }
    
    ValueSourceKind kind() const
    {
        return kindFromNodeIndex(m_nodeIndex);
    }
    
    bool isInRegisterFile() const { return JSC::DFG::isInRegisterFile(kind()); }
    bool isTriviallyRecoverable() const { return JSC::DFG::isTriviallyRecoverable(kind()); }
    
    DataFormat dataFormat() const
    {
        return valueSourceKindToDataFormat(kind());
    }
    
    ValueRecovery valueRecovery() const
    {
        ASSERT(isTriviallyRecoverable());
        switch (kind()) {
        case ValueInRegisterFile:
            return ValueRecovery::alreadyInRegisterFile();
            
        case Int32InRegisterFile:
            return ValueRecovery::alreadyInRegisterFileAsUnboxedInt32();
            
        case CellInRegisterFile:
            return ValueRecovery::alreadyInRegisterFileAsUnboxedCell();
            
        case BooleanInRegisterFile:
            return ValueRecovery::alreadyInRegisterFileAsUnboxedBoolean();
            
        case DoubleInRegisterFile:
            return ValueRecovery::alreadyInRegisterFileAsUnboxedDouble();
            
        case SourceIsDead:
            return ValueRecovery::constant(jsUndefined());
            
        case ArgumentsSource:
            return ValueRecovery::argumentsThatWereNotCreated();
            
        default:
            ASSERT_NOT_REACHED();
            return ValueRecovery();
        }
    }
    
    NodeIndex nodeIndex() const
    {
        ASSERT(kind() == HaveNode);
        return m_nodeIndex;
    }
    
    void dump(FILE* out) const;
    
private:
    static NodeIndex nodeIndexFromKind(ValueSourceKind kind)
    {
        ASSERT(kind >= SourceNotSet && kind < HaveNode);
        return NoNode - kind;
    }
    
    static ValueSourceKind kindFromNodeIndex(NodeIndex nodeIndex)
    {
        unsigned kind = static_cast<unsigned>(NoNode - nodeIndex);
        if (kind >= static_cast<unsigned>(HaveNode))
            return HaveNode;
        return static_cast<ValueSourceKind>(kind);
    }
    
    NodeIndex m_nodeIndex;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGValueSource_h

