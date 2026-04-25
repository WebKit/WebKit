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

    void reportBaselineDataICSlowPathBegin(CCallHelpers::Label slowPathBegin)
    {
        m_slowPathBegin = slowPathBegin;
    }

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
            UNUSED_PARAM(usedRegisters);
            UNUSED_PARAM(codeBlock);
        } else {
            propertyCache.codeOrigin = codeOrigin;
            propertyCache.callSiteIndex = callSiteIndex;
            propertyCache.usedRegisters = usedRegisters.toScalarRegisterSet();
        }
        if constexpr (std::is_same_v<std::decay_t<PropertyInlineCache>, JSC::PropertyInlineCache>) {
            if (codeOrigin.inlineCallFrame())
                propertyCache.m_globalObject = baselineCodeBlockForInlineCallFrame(codeOrigin.inlineCallFrame())->globalObject();
            else
                propertyCache.m_globalObject = codeBlock->globalObject();
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
        JSValueRegs base, JSValueRegs value);

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
        JSValueRegs baseRegs, JSValueRegs valueRegs, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        propertyCache.m_identifier = propertyName;
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = baseRegs.payloadGPR();
            propertyCache.m_valueGPR = valueRegs.payloadGPR();
            propertyCache.m_extraGPR = InvalidGPRReg;
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = baseRegs.tagGPR();
            propertyCache.m_valueTagGPR = valueRegs.tagGPR();
            propertyCache.m_extraTagGPR = InvalidGPRReg;
#endif
        } else {
            UNUSED_PARAM(baseRegs);
            UNUSED_PARAM(valueRegs);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    
protected:
    
    void generateFastCommon(CCallHelpers&, size_t size);
    
    JSValueRegs m_base;
    JSValueRegs m_value;

public:
    CCallHelpers::Jump m_slowPathJump;
};

class JITGetByIdGenerator final : public JITByIdGenerator {
public:
    JITGetByIdGenerator() = default;

    JITGetByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs value, GPRReg propertyCacheGPR, AccessType, CacheType);
    
    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        JSValueRegs baseRegs, JSValueRegs valueRegs, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseRegs, valueRegs, propertyCacheGPR);
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
        JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs, GPRReg propertyCacheGPR);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        JSValueRegs valueRegs, JSValueRegs baseRegs, JSValueRegs thisRegs, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseRegs, valueRegs, propertyCacheGPR);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_extraGPR = thisRegs.payloadGPR();
#if USE(JSVALUE32_64)
            propertyCache.m_extraTagGPR = thisRegs.tagGPR();
#endif
        } else
            UNUSED_PARAM(thisRegs);
    }
};

class JITPutByIdGenerator final : public JITByIdGenerator {
public:
    JITPutByIdGenerator() = default;

    JITPutByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs value, GPRReg propertyCacheGPR, GPRReg scratch, AccessType);
    
    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        JSValueRegs baseRegs, JSValueRegs valueRegs, GPRReg propertyCacheGPR, GPRReg scratchGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseRegs, valueRegs, propertyCacheGPR);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>)
            propertyCache.usedRegisters.remove(scratchGPR);
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
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

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
        JSValueRegs baseRegs, JSValueRegs propertyRegs, JSValueRegs valueRegs, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = baseRegs.payloadGPR();
            propertyCache.m_extraGPR = propertyRegs.payloadGPR();
            propertyCache.m_valueGPR = valueRegs.payloadGPR();
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
            if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, DFG::UnlinkedPropertyInlineCache>)
                propertyCache.m_arrayProfileGPR = arrayProfileGPR;
            else
                UNUSED_PARAM(arrayProfileGPR);
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = baseRegs.tagGPR();
            propertyCache.m_valueTagGPR = valueRegs.tagGPR();
            propertyCache.m_extraTagGPR = propertyRegs.tagGPR();
#endif
        } else {
            UNUSED_PARAM(baseRegs);
            UNUSED_PARAM(propertyRegs);
            UNUSED_PARAM(valueRegs);
            UNUSED_PARAM(propertyCacheGPR);
            UNUSED_PARAM(arrayProfileGPR);
        }
    }

    JSValueRegs m_base;
    JSValueRegs m_value;

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITDelByValGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITDelByValGenerator() = default;

    JITDelByValGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg propertyCacheGPR);

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
        JSValueRegs baseRegs, JSValueRegs propertyRegs, JSValueRegs resultRegs, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = baseRegs.payloadGPR();
            propertyCache.m_extraGPR = propertyRegs.payloadGPR();
            propertyCache.m_valueGPR = resultRegs.payloadGPR();
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = baseRegs.tagGPR();
            propertyCache.m_valueTagGPR = resultRegs.tagGPR();
            propertyCache.m_extraTagGPR = propertyRegs.tagGPR();
#endif
        } else {
            UNUSED_PARAM(baseRegs);
            UNUSED_PARAM(propertyRegs);
            UNUSED_PARAM(resultRegs);
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
        JSValueRegs base, JSValueRegs result, GPRReg propertyCacheGPR);

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
        JSValueRegs baseRegs, JSValueRegs resultRegs, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseRegs, resultRegs, propertyCacheGPR);
    }

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITInByValGenerator : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITInByValGenerator() = default;

    JITInByValGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

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
        JSValueRegs baseRegs, JSValueRegs propertyRegs, JSValueRegs resultRegs, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = baseRegs.payloadGPR();
            propertyCache.m_extraGPR = propertyRegs.payloadGPR();
            propertyCache.m_valueGPR = resultRegs.payloadGPR();
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
            if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, DFG::UnlinkedPropertyInlineCache>)
                propertyCache.m_arrayProfileGPR = arrayProfileGPR;
            else
                UNUSED_PARAM(arrayProfileGPR);
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = baseRegs.tagGPR();
            propertyCache.m_valueTagGPR = resultRegs.tagGPR();
            propertyCache.m_extraTagGPR = propertyRegs.tagGPR();
#endif
        } else {
            UNUSED_PARAM(baseRegs);
            UNUSED_PARAM(propertyRegs);
            UNUSED_PARAM(resultRegs);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITInByIdGenerator final : public JITByIdGenerator {
public:
    JITInByIdGenerator() = default;

    JITInByIdGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs value, GPRReg propertyCacheGPR);

    void generateFastPath(CCallHelpers&);
    void generateDataICFastPath(CCallHelpers&);

    template<typename PropertyInlineCache>
    static void setUpPropertyInlineCache(PropertyInlineCache& propertyCache, CodeBlock* codeBlock,
        AccessType accessType, CacheType cacheType, CodeOrigin codeOrigin, CallSiteIndex callSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier propertyName,
        JSValueRegs baseRegs, JSValueRegs valueRegs, GPRReg propertyCacheGPR)
    {
        JITByIdGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters, propertyName, baseRegs, valueRegs, propertyCacheGPR);
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
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = valueGPR;
            propertyCache.m_valueGPR = resultGPR;
            propertyCache.m_extraGPR = prototypeGPR;
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = InvalidGPRReg;
            propertyCache.m_valueTagGPR = InvalidGPRReg;
            propertyCache.m_extraTagGPR = InvalidGPRReg;
#endif
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
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

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
        JSValueRegs baseRegs, JSValueRegs propertyRegs, JSValueRegs resultRegs, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = baseRegs.payloadGPR();
            propertyCache.m_extraGPR = propertyRegs.payloadGPR();
            propertyCache.m_valueGPR = resultRegs.payloadGPR();
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
            if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, DFG::UnlinkedPropertyInlineCache>)
                propertyCache.m_arrayProfileGPR = arrayProfileGPR;
            else
                UNUSED_PARAM(arrayProfileGPR);
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = baseRegs.tagGPR();
            propertyCache.m_valueTagGPR = resultRegs.tagGPR();
            propertyCache.m_extraTagGPR = propertyRegs.tagGPR();
#endif
        } else {
            UNUSED_PARAM(baseRegs);
            UNUSED_PARAM(propertyRegs);
            UNUSED_PARAM(resultRegs);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    JSValueRegs m_base;
    JSValueRegs m_result;

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITGetByValWithThisGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITGetByValWithThisGenerator() = default;

    JITGetByValWithThisGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs property, JSValueRegs thisRegs, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR);

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
        JSValueRegs baseRegs, JSValueRegs propertyRegs, JSValueRegs thisRegs, JSValueRegs resultRegs, GPRReg arrayProfileGPR, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = baseRegs.payloadGPR();
            propertyCache.m_extraGPR = thisRegs.payloadGPR();
            propertyCache.m_valueGPR = resultRegs.payloadGPR();
            propertyCache.m_extra2GPR = propertyRegs.payloadGPR();
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
            if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, DFG::UnlinkedPropertyInlineCache>)
                propertyCache.m_arrayProfileGPR = arrayProfileGPR;
            else
                UNUSED_PARAM(arrayProfileGPR);
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = baseRegs.tagGPR();
            propertyCache.m_valueTagGPR = resultRegs.tagGPR();
            propertyCache.m_extraTagGPR = thisRegs.tagGPR();
            propertyCache.m_extra2TagGPR = propertyRegs.tagGPR();
#endif
        } else {
            UNUSED_PARAM(baseRegs);
            UNUSED_PARAM(propertyRegs);
            UNUSED_PARAM(thisRegs);
            UNUSED_PARAM(resultRegs);
            UNUSED_PARAM(propertyCacheGPR);
        }
    }

    JSValueRegs m_base;
    JSValueRegs m_result;

    CCallHelpers::PatchableJump m_slowPathJump;
};

class JITPrivateBrandAccessGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITPrivateBrandAccessGenerator() = default;

    JITPrivateBrandAccessGenerator(
        CodeBlock*, CompileTimePropertyInlineCache, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs brand, GPRReg propertyCacheGPR);

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
        JSValueRegs baseRegs, JSValueRegs brandRegs, GPRReg propertyCacheGPR)
    {
        JITInlineCacheGenerator::setUpPropertyInlineCacheImpl(propertyCache, codeBlock, accessType, cacheType, codeOrigin, callSiteIndex, usedRegisters);
        if constexpr (!std::is_same_v<std::decay_t<PropertyInlineCache>, BaselineUnlinkedPropertyInlineCache>) {
            propertyCache.m_baseGPR = baseRegs.payloadGPR();
            propertyCache.m_extraGPR = brandRegs.payloadGPR();
            propertyCache.m_valueGPR = InvalidGPRReg;
            propertyCache.m_propertyCacheGPR = propertyCacheGPR;
#if USE(JSVALUE32_64)
            propertyCache.m_baseTagGPR = baseRegs.tagGPR();
            propertyCache.m_extraTagGPR = brandRegs.tagGPR();
            propertyCache.m_valueTagGPR = InvalidGPRReg;
#endif
        } else {
            UNUSED_PARAM(baseRegs);
            UNUSED_PARAM(brandRegs);
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
