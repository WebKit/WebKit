/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include <JavaScriptCore/AssemblyHelpers.h>
#include <JavaScriptCore/CCallHelpers.h>
#include <JavaScriptCore/CacheableIdentifier.h>
#include <JavaScriptCore/CodeOrigin.h>
#include <JavaScriptCore/JITOperationValidation.h>
#include <JavaScriptCore/JITOperations.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/RegisterSet.h>

namespace JSC {
namespace DFG {
class JITCompiler;
struct UnlinkedPropertyInlineCache;
}

class CallSiteIndex;
class CodeBlock;
class JIT;
class PropertyInlineCache;
class RepatchingPropertyInlineCache;
struct UnlinkedPropertyInlineCache;
struct BaselineUnlinkedPropertyInlineCache;

enum class AccessType : int8_t;
enum class CacheType : int8_t;
enum class JITType : uint8_t;

using CompileTimePropertyInlineCache = Variant<PropertyInlineCache*, BaselineUnlinkedPropertyInlineCache*, DFG::UnlinkedPropertyInlineCache*>;

class JITInlineCacheGenerator {
protected:
    JITInlineCacheGenerator() = default;
    JITInlineCacheGenerator(CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, AccessType);
    
public:
    PropertyInlineCache* propertyCache() const { return m_propertyCache; }

    void reportSlowPathCall(CCallHelpers::Label slowPathBegin, CCallHelpers::Call call)
    {
        m_slowPathBegin = slowPathBegin;
        m_slowPathCall = call;
    }

    CCallHelpers::Label slowPathBegin() const { return m_slowPathBegin; }

    void NODELETE finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer,
        CodeLocationLabel<JITStubRoutinePtrTag> start);

    JSC::UnlinkedPropertyInlineCache* m_unlinkedPropertyCache { nullptr };

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCacheImpl(PropertyInlineCache& propertyCache, CodeBlock* codeBlock, AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters)
    {
        propertyCache.accessType = accessType;
        propertyCache.preconfiguredCacheType = cacheType;
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.bytecodeIndex = codeOrigin.bytecodeIndex();
            UNUSED_PARAM(callSiteIndex);
        } else {
            propertyCache.codeOrigin = codeOrigin;
            propertyCache.callSiteIndex = callSiteIndex;
        }
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            downcast<RepatchingPropertyInlineCache>(propertyCache).m_usedRegisters = usedRegisters.toScalarRegisterSet();
            if (codeOrigin.inlineCallFrame())
                propertyCache.m_globalObject = baselineCodeBlockForInlineCallFrame(codeOrigin.inlineCallFrame())->globalObject();
            else
                propertyCache.m_globalObject = codeBlock->globalObject();
        } else {
            UNUSED_PARAM(codeBlock);
            UNUSED_PARAM(usedRegisters);
        }
    }

    AccessType accessType() const { return m_accessType; }

protected:
    void generateDataICFastPath(CCallHelpers&, GPRReg propertyCacheGPR);

    PropertyInlineCache* m_propertyCache { nullptr };
    AccessType m_accessType;

public:
    CCallHelpers::Label m_start;
    CCallHelpers::Label m_done;
    CCallHelpers::Label m_slowPathBegin;
    CCallHelpers::Call m_slowPathCall;
};

class JITByIdGenerator : public JITInlineCacheGenerator {
protected:
    JITByIdGenerator() = default;

    JITByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, AccessType,
        GPRReg base, GPRReg value);

public:
    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.isSet());
        return m_slowPathJump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCacheImpl(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        GPRReg baseGPR, GPRReg valueGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        propertyCache.m_identifier = propertyName;
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = baseGPR;
            registers.valueGPR = valueGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
        } else {
            UNUSED_PARAM(baseGPR);
            UNUSED_PARAM(valueGPR);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    
protected:

    void generateFastCommon(CCallHelpers&, size_t size);
    void emitDataICSlowPath(CCallHelpers&, GPRReg propertyCacheGPR);

    GPRReg m_base { InvalidGPRReg };
    GPRReg m_value { InvalidGPRReg };

public:
    CCallHelpers::Jump m_slowPathJump;
    CCallHelpers::JumpList m_dataICHandlerCases;
};

class JITGetByIdGenerator final : public JITByIdGenerator {
public:
    JITGetByIdGenerator() = default;

    JITGetByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        GPRReg base, GPRReg value, GPRReg propertyCacheGPR, AccessType, CacheType);
    
    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);
    void generateDataICSlowPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        GPRReg baseGPR, GPRReg valueGPR, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseGPR, valueGPR, propertyCacheGPR);
    }

private:
    bool m_isLengthAccess;
    CacheType m_cacheType;
};

class JITGetByIdWithThisGenerator final : public JITByIdGenerator {
public:
    JITGetByIdWithThisGenerator() = default;

    JITGetByIdWithThisGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        GPRReg value, GPRReg base, GPRReg thisGPR, GPRReg propertyCacheGPR);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);
    void generateDataICSlowPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        GPRReg valueGPR, GPRReg baseGPR, GPRReg thisGPR, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseGPR, valueGPR, propertyCacheGPR);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>)
            downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers.extraGPR = thisGPR;
        else
            UNUSED_PARAM(thisGPR);
    }
};

class JITPutByIdGenerator final : public JITByIdGenerator {
public:
    JITPutByIdGenerator() = default;

    JITPutByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        GPRReg base, GPRReg value, GPRReg propertyCacheGPR, GPRReg scratch, AccessType);
    
    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);
    void generateDataICSlowPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        GPRReg baseGPR, GPRReg valueGPR, GPRReg propertyCacheGPR, GPRReg scratchGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseGPR, valueGPR, propertyCacheGPR);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>)
            downcast<RepatchingPropertyInlineCache>(propertyCache).m_usedRegisters.remove(scratchGPR);
        else
            UNUSED_PARAM(scratchGPR);
    }
};

class JITPutByValGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITPutByValGenerator() = default;

    JITPutByValGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        GPRReg base, GPRReg property, GPRReg result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters,
        GPRReg baseGPR, GPRReg propertyGPR, GPRReg valueGPR, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = baseGPR;
            registers.extraGPR = propertyGPR;
            registers.valueGPR = valueGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
            registers.arrayProfileGPR = arrayProfileGPR;
        } else {
            UNUSED_PARAM(baseGPR);
            UNUSED_PARAM(propertyGPR);
            UNUSED_PARAM(valueGPR);
            UNUSED_PARAM(propertyCacheGPR);
            UNUSED_PARAM(arrayProfileGPR);
        }
    }

    GPRReg m_base { InvalidGPRReg };
    GPRReg m_value { InvalidGPRReg };

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITDelByValGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITDelByValGenerator() = default;

    JITDelByValGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        GPRReg base, GPRReg property, GPRReg result, GPRReg propertyCacheGPR);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters,
        GPRReg baseGPR, GPRReg propertyGPR, GPRReg resultGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = baseGPR;
            registers.extraGPR = propertyGPR;
            registers.valueGPR = resultGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
        } else {
            UNUSED_PARAM(baseGPR);
            UNUSED_PARAM(propertyGPR);
            UNUSED_PARAM(resultGPR);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITDelByIdGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITDelByIdGenerator() = default;

    JITDelByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters, CacheableIdentifier,
        GPRReg base, GPRReg result, GPRReg propertyCacheGPR);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        GPRReg baseGPR, GPRReg resultGPR, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseGPR, resultGPR, propertyCacheGPR);
    }

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITInByValGenerator : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITInByValGenerator() = default;

    JITInByValGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        GPRReg base, GPRReg property, GPRReg result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters,
        GPRReg baseGPR, GPRReg propertyGPR, GPRReg resultGPR, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = baseGPR;
            registers.extraGPR = propertyGPR;
            registers.valueGPR = resultGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
            registers.arrayProfileGPR = arrayProfileGPR;
        } else {
            UNUSED_PARAM(baseGPR);
            UNUSED_PARAM(propertyGPR);
            UNUSED_PARAM(resultGPR);
            UNUSED_PARAM(propertyCacheGPR);
            UNUSED_PARAM(arrayProfileGPR);
        }
    }

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITInByIdGenerator final : public JITByIdGenerator {
public:
    JITInByIdGenerator() = default;

    JITInByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        GPRReg base, GPRReg value, GPRReg propertyCacheGPR);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);
    void generateDataICSlowPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        GPRReg baseGPR, GPRReg valueGPR, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseGPR, valueGPR, propertyCacheGPR);
    }
};

class JITInstanceOfGenerator final : public JITInlineCacheGenerator {
public:
    using Base = JITInlineCacheGenerator;
    JITInstanceOfGenerator() = default;
    
    JITInstanceOfGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, GPRReg result,
        GPRReg value, GPRReg prototype, GPRReg propertyCacheGPR,
        bool prototypeIsKnownObject = false);
    
    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters,
        GPRReg resultGPR, GPRReg valueGPR, GPRReg prototypeGPR, GPRReg propertyCacheGPR, bool prototypeIsKnownObject)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        propertyCache.prototypeIsKnownObject = prototypeIsKnownObject;
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = valueGPR;
            registers.valueGPR = resultGPR;
            registers.extraGPR = prototypeGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
        } else {
            UNUSED_PARAM(valueGPR);
            UNUSED_PARAM(resultGPR);
            UNUSED_PARAM(prototypeGPR);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITGetByValGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITGetByValGenerator() = default;

    JITGetByValGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        GPRReg base, GPRReg property, GPRReg result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);
    
    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    void generateEmptyPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters,
        GPRReg baseGPR, GPRReg propertyGPR, GPRReg resultGPR, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = baseGPR;
            registers.extraGPR = propertyGPR;
            registers.valueGPR = resultGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
            registers.arrayProfileGPR = arrayProfileGPR;
        } else {
            UNUSED_PARAM(baseGPR);
            UNUSED_PARAM(propertyGPR);
            UNUSED_PARAM(resultGPR);
            UNUSED_PARAM(propertyCacheGPR);
            UNUSED_PARAM(arrayProfileGPR);
        }
    }

    GPRReg m_base { InvalidGPRReg };
    GPRReg m_result { InvalidGPRReg };

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITGetByValWithThisGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITGetByValWithThisGenerator() = default;

    JITGetByValWithThisGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        GPRReg base, GPRReg property, GPRReg thisGPR, GPRReg result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    void generateEmptyPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters,
        GPRReg baseGPR, GPRReg propertyGPR, GPRReg thisGPR, GPRReg resultGPR, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = baseGPR;
            registers.extraGPR = thisGPR;
            registers.valueGPR = resultGPR;
            registers.extra2GPR = propertyGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
            registers.arrayProfileGPR = arrayProfileGPR;
        } else {
            UNUSED_PARAM(baseGPR);
            UNUSED_PARAM(propertyGPR);
            UNUSED_PARAM(thisGPR);
            UNUSED_PARAM(resultGPR);
            UNUSED_PARAM(propertyCacheGPR);
            UNUSED_PARAM(arrayProfileGPR);
        }
    }

    GPRReg m_base { InvalidGPRReg };
    GPRReg m_result { InvalidGPRReg };

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITPrivateBrandAccessGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITPrivateBrandAccessGenerator() = default;

    JITPrivateBrandAccessGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        GPRReg base, GPRReg brand, GPRReg propertyCacheGPR);

    CCallHelpers::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);
    
    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters,
        GPRReg baseGPR, GPRReg brandGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            auto& registers = downcast<RepatchingPropertyInlineCache>(propertyCache).m_registers;
            registers.baseGPR = baseGPR;
            registers.extraGPR = brandGPR;
            registers.propertyCacheGPR = propertyCacheGPR;
        } else {
            UNUSED_PARAM(baseGPR);
            UNUSED_PARAM(brandGPR);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    CCallHelpers::PatchableJump m_slowPathJump;
};

template<typename VectorType>
void finalizeInlineCaches(VectorType& vector, LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    for (auto& entry : vector)
        entry.finalize(fastPath, slowPath);
}

template<typename VectorType>
void finalizeInlineCaches(VectorType& vector, LinkBuffer& linkBuffer)
{
    finalizeInlineCaches(vector, linkBuffer, linkBuffer);
}

} // namespace JSC

#endif // ENABLE(JIT)
