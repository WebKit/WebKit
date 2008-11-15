/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CodeBlock_h
#define CodeBlock_h

#include "EvalCodeCache.h"
#include "Instruction.h"
#include "JSGlobalObject.h"
#include "Nodes.h"
#include "RegExp.h"
#include "UString.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {

    class ExecState;

    enum CodeType { GlobalCode, EvalCode, FunctionCode };

    static ALWAYS_INLINE int missingThisObjectMarker() { return std::numeric_limits<int>::max(); }

    struct HandlerInfo {
        uint32_t start;
        uint32_t end;
        uint32_t target;
        uint32_t scopeDepth;
        void* nativeCode;
    };

    struct ExpressionRangeInfo {
        enum {
            MaxOffset = (1 << 7) - 1, 
            MaxDivot = (1 << 25) - 1
        };
        uint32_t instructionOffset : 25;
        uint32_t divotPoint : 25;
        uint32_t startOffset : 7;
        uint32_t endOffset : 7;
    };

    struct LineInfo {
        uint32_t instructionOffset;
        int32_t lineNumber;
    };

    struct OffsetLocation {
        int32_t branchOffset;
#if ENABLE(CTI)
        void* ctiOffset;
#endif
    };

    struct StructureStubInfo {
        StructureStubInfo(unsigned bytecodeIndex)
            : bytecodeIndex(bytecodeIndex)
            , stubRoutine(0)
            , callReturnLocation(0)
            , hotPathBegin(0)
        {
        }
    
        unsigned bytecodeIndex;
        void* stubRoutine;
        void* callReturnLocation;
        void* hotPathBegin;
    };

    struct CallLinkInfo {
        CallLinkInfo()
            : callReturnLocation(0)
            , hotPathBegin(0)
            , hotPathOther(0)
            , coldPathOther(0)
            , callee(0)
        {
        }
    
        unsigned bytecodeIndex;
        void* callReturnLocation;
        void* hotPathBegin;
        void* hotPathOther;
        void* coldPathOther;
        CodeBlock* callee;
        unsigned position;
        
        void setUnlinked() { callee = 0; }
        bool isLinked() { return callee; }
    };

    inline void* getStructureStubInfoReturnLocation(StructureStubInfo* structureStubInfo)
    {
        return structureStubInfo->callReturnLocation;
    }

    inline void* getCallLinkInfoReturnLocation(CallLinkInfo* callLinkInfo)
    {
        return callLinkInfo->callReturnLocation;
    }

    // Binary chop algorithm, calls valueAtPosition on pre-sorted elements in array,
    // compares result with key (KeyTypes should be comparable with '--', '<', '>').
    // Optimized for cases where the array contains the key, checked by assertions.
    template<typename ArrayType, typename KeyType, KeyType(*valueAtPosition)(ArrayType*)>
    inline ArrayType* binaryChop(ArrayType* array, size_t size, KeyType key)
    {
        // The array must contain at least one element (pre-condition, array does conatin key).
        // If the array only contains one element, no need to do the comparison.
        while (size > 1) {
            // Pick an element to check, half way through the array, and read the value.
            int pos = (size - 1) >> 1;
            KeyType val = valueAtPosition(&array[pos]);
            
            // If the key matches, success!
            if (val == key)
                return &array[pos];
            // The item we are looking for is smaller than the item being check; reduce the value of 'size',
            // chopping off the right hand half of the array.
            else if (key < val)
                size = pos;
            // Discard all values in the left hand half of the array, up to and including the item at pos.
            else {
                size -= (pos + 1);
                array += (pos + 1);
            }

            // 'size' should never reach zero.
            ASSERT(size);
        }
        
        // If we reach this point we've chopped down to one element, no need to check it matches
        ASSERT(size == 1);
        ASSERT(key == valueAtPosition(&array[0]));
        return &array[0];
    }

    struct StringJumpTable {
        typedef HashMap<RefPtr<UString::Rep>, OffsetLocation> StringOffsetTable;
        StringOffsetTable offsetTable;
#if ENABLE(CTI)
        void* ctiDefault; // FIXME: it should not be necessary to store this.
#endif

        inline int32_t offsetForValue(UString::Rep* value, int32_t defaultOffset)
        {
            StringOffsetTable::const_iterator end = offsetTable.end();
            StringOffsetTable::const_iterator loc = offsetTable.find(value);
            if (loc == end)
                return defaultOffset;
            return loc->second.branchOffset;
        }

#if ENABLE(CTI)
        inline void* ctiForValue(UString::Rep* value)
        {
            StringOffsetTable::const_iterator end = offsetTable.end();
            StringOffsetTable::const_iterator loc = offsetTable.find(value);
            if (loc == end)
                return ctiDefault;
            return loc->second.ctiOffset;
        }
#endif
    };

    struct SimpleJumpTable {
        // FIXME: The two Vectors can be combind into one Vector<OffsetLocation>
        Vector<int32_t> branchOffsets;
        int32_t min;
#if ENABLE(CTI)
        Vector<void*> ctiOffsets;
        void* ctiDefault;
#endif

        int32_t offsetForValue(int32_t value, int32_t defaultOffset);
        void add(int32_t key, int32_t offset)
        {
            if (!branchOffsets[key])
                branchOffsets[key] = offset;
        }

#if ENABLE(CTI)
        inline void* ctiForValue(int32_t value)
        {
            if (value >= min && static_cast<uint32_t>(value - min) < ctiOffsets.size())
                return ctiOffsets[value - min];
            return ctiDefault;
        }
#endif
    };

    struct CodeBlock {
        CodeBlock(ScopeNode* ownerNode, CodeType codeType, PassRefPtr<SourceProvider> sourceProvider, unsigned sourceOffset)
            : ownerNode(ownerNode)
            , globalData(0)
#if ENABLE(CTI)
            , ctiCode(0)
#endif
            , numCalleeRegisters(0)
            , numConstants(0)
            , numVars(0)
            , numParameters(0)
            , needsFullScopeChain(ownerNode->needsActivation())
            , usesEval(ownerNode->usesEval())
            , codeType(codeType)
            , source(sourceProvider)
            , sourceOffset(sourceOffset)
        {
            ASSERT(source);
        }

        ~CodeBlock();

#if ENABLE(CTI) 
        void unlinkCallers();
#endif

        void addCaller(CallLinkInfo* caller)
        {
            caller->callee = this;
            caller->position = linkedCallerList.size();
            linkedCallerList.append(caller);
        }

        void removeCaller(CallLinkInfo* caller)
        {
            unsigned pos = caller->position;
            unsigned lastPos = linkedCallerList.size() - 1;

            if (pos != lastPos) {
                linkedCallerList[pos] = linkedCallerList[lastPos];
                linkedCallerList[pos]->position = pos;
            }
            linkedCallerList.shrink(lastPos);
        }

        inline bool isKnownNotImmediate(int index)
        {
            if (index == thisRegister)
                return true;

            if (isConstantRegisterIndex(index))
                return !JSImmediate::isImmediate(getConstant(index));

            return false;
        }

        ALWAYS_INLINE bool isConstantRegisterIndex(int index)
        {
            return index >= numVars && index < numVars + numConstants;
        }

        ALWAYS_INLINE JSValue* getConstant(int index)
        {
            return constantRegisters[index - numVars].getJSValue();
        }

        ALWAYS_INLINE bool isTemporaryRegisterIndex(int index)
        {
            return index >= numVars + numConstants;
        }

#if !defined(NDEBUG) || ENABLE_OPCODE_SAMPLING
        void dump(ExecState*) const;
        void printStructureIDs(const Instruction*) const;
        void printStructureID(const char* name, const Instruction*, int operand) const;
#endif
        int expressionRangeForVPC(const Instruction*, int& divot, int& startOffset, int& endOffset);
        int lineNumberForVPC(const Instruction* vPC);
        bool getHandlerForVPC(const Instruction* vPC, Instruction*& target, int& scopeDepth);
        void* nativeExceptionCodeForHandlerVPC(const Instruction* handlerVPC);

        void mark();
        void refStructureIDs(Instruction* vPC) const;
        void derefStructureIDs(Instruction* vPC) const;

        StructureStubInfo& getStubInfo(void* returnAddress)
        {
            return *(binaryChop<StructureStubInfo, void*, getStructureStubInfoReturnLocation>(propertyAccessInstructions.begin(), propertyAccessInstructions.size(), returnAddress));
        }

        CallLinkInfo& getCallLinkInfo(void* returnAddress)
        {
            return *(binaryChop<CallLinkInfo, void*, getCallLinkInfoReturnLocation>(callLinkInfos.begin(), callLinkInfos.size(), returnAddress));
        }

        ScopeNode* ownerNode;
        JSGlobalData* globalData;
#if ENABLE(CTI)
        void* ctiCode;
#endif

        int numCalleeRegisters;

        // NOTE: numConstants holds the number of constant registers allocated
        // by the code generator, not the number of constant registers used.
        // (Duplicate constants are uniqued during code generation, and spare
        // constant registers may be allocated.)
        int numConstants;
        int numVars;
        int numParameters;
        int thisRegister;
        bool needsFullScopeChain;
        bool usesEval;
        bool usesArguments;
        CodeType codeType;
        RefPtr<SourceProvider> source;
        unsigned sourceOffset;

        Vector<Instruction> instructions;
        Vector<unsigned> globalResolveInstructions;
        Vector<StructureStubInfo> propertyAccessInstructions;
        Vector<CallLinkInfo> callLinkInfos;
        Vector<CallLinkInfo*> linkedCallerList;

        // Constant pool
        Vector<Identifier> identifiers;
        Vector<RefPtr<FuncDeclNode> > functions;
        Vector<RefPtr<FuncExprNode> > functionExpressions;
        Vector<Register> constantRegisters;
        Vector<JSValue*> unexpectedConstants;
        Vector<RefPtr<RegExp> > regexps;
        Vector<HandlerInfo> exceptionHandlers;
        Vector<ExpressionRangeInfo> expressionInfo;
        Vector<LineInfo> lineInfo;

        Vector<SimpleJumpTable> immediateSwitchJumpTables;
        Vector<SimpleJumpTable> characterSwitchJumpTables;
        Vector<StringJumpTable> stringSwitchJumpTables;

#if ENABLE(CTI)
        HashMap<void*, unsigned> ctiReturnAddressVPCMap;
#endif

        Vector<unsigned> jumpTargets;

        EvalCodeCache evalCodeCache;

        SymbolTable symbolTable;
    private:
#if !defined(NDEBUG) || ENABLE(OPCODE_SAMPLING)
        void dump(ExecState*, const Vector<Instruction>::const_iterator& begin, Vector<Instruction>::const_iterator&) const;
#endif

    };

    // Program code is not marked by any function, so we make the global object
    // responsible for marking it.

    struct ProgramCodeBlock : public CodeBlock {
        ProgramCodeBlock(ScopeNode* ownerNode, CodeType codeType, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider)
            : CodeBlock(ownerNode, codeType, sourceProvider, 0)
            , globalObject(globalObject)
        {
            globalObject->codeBlocks().add(this);
        }

        ~ProgramCodeBlock()
        {
            if (globalObject)
                globalObject->codeBlocks().remove(this);
        }

        JSGlobalObject* globalObject; // For program and eval nodes, the global object that marks the constant pool.
    };

    struct EvalCodeBlock : public ProgramCodeBlock {
        EvalCodeBlock(ScopeNode* ownerNode, JSGlobalObject* globalObject, PassRefPtr<SourceProvider> sourceProvider)
            : ProgramCodeBlock(ownerNode, EvalCode, globalObject, sourceProvider)
        {
        }
    };

} // namespace JSC

#endif // CodeBlock_h
