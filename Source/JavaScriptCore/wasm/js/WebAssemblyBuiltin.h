/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/WasmCallee.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/WTFString.h>

namespace JSC {

struct WebAssemblyBuiltinTypeExpectation {
    // Create an instance that expects the `i32` wasm type.
    static std::unique_ptr<WebAssemblyBuiltinTypeExpectation> i32();

    // Create an instance that expects the `externref` wasm type.
    static std::unique_ptr<WebAssemblyBuiltinTypeExpectation> externref();

    // Create an instance that expects the `ref null (array mut i16)` wasm type.
    static std::unique_ptr<WebAssemblyBuiltinTypeExpectation> refNullArrayMutI16();

    virtual ~WebAssemblyBuiltinTypeExpectation() = default;

    // Check whether the type meets this expectation.
    virtual bool isValid(const Wasm::Type&) const = 0;
};

class WebAssemblyBuiltinValueTypeExpectation : public WebAssemblyBuiltinTypeExpectation {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebAssemblyBuiltinValueTypeExpectation);
public:
    WebAssemblyBuiltinValueTypeExpectation(Wasm::Type type)
        : m_expectedType(type)
    { }

    bool isValid(const Wasm::Type& type) const { return type == m_expectedType; }

private:
    Wasm::Type m_expectedType;
};

class WebAssemblyBuiltinExternrefTypeExpectation : public WebAssemblyBuiltinTypeExpectation {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebAssemblyBuiltinExternrefTypeExpectation);
public:
    bool isValid(const Wasm::Type& type) const { return Wasm::isExternref(type); }
};

class WebAssemblyArrayMutI16TypeExpectation : public WebAssemblyBuiltinTypeExpectation {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebAssemblyArrayMutI16TypeExpectation);
public:
    bool isValid(const Wasm::Type&) const;
};

inline std::unique_ptr<WebAssemblyBuiltinTypeExpectation> WebAssemblyBuiltinTypeExpectation::i32()
{
    return WTF::makeUnique<WebAssemblyBuiltinValueTypeExpectation>(Wasm::Types::I32);
}

inline std::unique_ptr<WebAssemblyBuiltinTypeExpectation> WebAssemblyBuiltinTypeExpectation::externref()
{
    return WTF::makeUnique<WebAssemblyBuiltinExternrefTypeExpectation>();
}

inline std::unique_ptr<WebAssemblyBuiltinTypeExpectation> WebAssemblyBuiltinTypeExpectation::refNullArrayMutI16()
{
    return WTF::makeUnique<WebAssemblyArrayMutI16TypeExpectation>();
}

class WebAssemblyBuiltinSignature {
    using Expectations = Vector<std::unique_ptr<WebAssemblyBuiltinTypeExpectation>>;
public:
    WebAssemblyBuiltinSignature(Expectations&& results, Expectations&& params)
        : m_results(WTFMove(results))
        , m_params(WTFMove(params))
    { }

    size_t numParams() const { return m_params.size(); }
    size_t numResults() const { return m_results.size(); }

    bool isValid(const Wasm::FunctionSignature&) const;

private:
    Expectations m_results;
    Expectations m_params;
};

class WebAssemblyBuiltinSet;

/**
 * An individual builtin. An instance is owned by a builtin set
 * and looked up using `WebAssemblyBuiltinSet::findBuiltin`.
 */
class WebAssemblyBuiltin {
public:
    using WasmTrampolinePtr = EncodedJSValue (*)();
    friend class WebAssemblyBuiltinSet;

    template <typename WasmEntrypoint>
    WebAssemblyBuiltin(uint32_t id, ASCIILiteral name, WebAssemblyBuiltinSignature&& sig, WasmEntrypoint wasmEntrypoint, WasmTrampolinePtr wasmTrampoline, NativeFunction jsHostFunction)
        : m_id(id)
        , m_name(name)
        , m_signature(WTFMove(sig))
        , m_jsHostFunction(jsHostFunction)
    {
        ASSERT(sig.numParams() <= 4); // see generateWasmBuiltinTrampoline() for why this is the limit
        m_wasmEntrypoint = CodePtr<CFunctionPtrTag>::fromTaggedPtr(std::bit_cast<void*>(wasmEntrypoint));
        m_wasmTrampoline = CodePtr<CFunctionPtrTag>::fromTaggedPtr(std::bit_cast<void*>(wasmTrampoline)).template retagged<WasmEntryPtrTag>();
    }

    WebAssemblyBuiltin(WebAssemblyBuiltin&&) = default;

    // An index used to get the builtin callee from the callee table in a Wasm instance.
    uint32_t id() const { return m_id; }

    // The name of the builtin function.
    const ASCIILiteral& name() const { return m_name; }

    // The signature that a valid import of this builtin must match.
    const WebAssemblyBuiltinSignature& signature() const { return m_signature; }

    CodePtr<CFunctionPtrTag> wasmEntrypoint() const { return m_wasmEntrypoint; }
    CodePtr<WasmEntryPtrTag> wasmTrampoline() const { return m_wasmTrampoline; }
    // A function acting as a JS entrypoint into the builtin implementation.
    JSFunction* jsWrapper(JSGlobalObject*) const;

    Wasm::WasmBuiltinCallee* callee() const { return m_callee.get(); }
    const Wasm::Name* wasmName() const { return m_wasmName; }
    RefPtr<Wasm::NameSection> nameSection() const { return m_nameSection; };

private:
    uint32_t m_id;
    ASCIILiteral m_name;
    WebAssemblyBuiltinSignature m_signature;
    CodePtr<CFunctionPtrTag> m_wasmEntrypoint;
    CodePtr<WasmEntryPtrTag> m_wasmTrampoline;
    NativeFunction m_jsHostFunction;
    // The following are set by WasmBuiltinSet::finalizeCreation()
    const Wasm::Name* m_wasmName;
    RefPtr<Wasm::NameSection> m_nameSection;
    RefPtr<Wasm::WasmBuiltinCallee> m_callee;
};

/**
 * A collection of builtins such as `wasm:js-string`.
 *
 * Sets are created and managed by a builtin registry. Use
 * `WebAssemblyBuiltinRegistry::findByQualifiedName` to get an instance.
 */
class WebAssemblyBuiltinSet {
public:
    friend class WebAssemblyBuiltinRegistry;

    WebAssemblyBuiltinSet(WebAssemblyBuiltinSet&&) = default;

    // The set name with the "wasm:" prefix.
    const ASCIILiteral& qualifiedName() const
    {
        return m_qualifiedName;
    }
    // Search in the set for a builtin with the given name.
    // Return a pointer to the builtin or nullptr if not found.
    const WebAssemblyBuiltin* findBuiltin(const String& name) const;

private:
    // Create and return the `wasm:js-string` builtin set.
    static WebAssemblyBuiltinSet jsString();

    WebAssemblyBuiltinSet(ASCIILiteral qualifiedName)
        : m_qualifiedName(qualifiedName)
        , m_nameSection(Wasm::NameSection::create())
    {
    }

    void add(WebAssemblyBuiltin&&);
    // Should be called once only after adding all builtins.
    void finalizeCreation();

    ASCIILiteral m_qualifiedName;
    Vector<WebAssemblyBuiltin> m_builtins;
    UncheckedKeyHashMap<String, WebAssemblyBuiltin*> m_builtinsByName;
    // Simulates a name section of a module so builtin callees have a name to report in a stack dump.
    Ref<Wasm::NameSection> m_nameSection;
};

/**
 * A registry of all builtin sets. The registry is a singleton.
 */
class WebAssemblyBuiltinRegistry {
    friend class NeverDestroyed<WebAssemblyBuiltinRegistry>;
public:
    static const WebAssemblyBuiltinRegistry& singleton();

    WebAssemblyBuiltinRegistry();

    // Look for a builtin set with the specified qualified name.
    // Return a pointer to the set, or nullptr if not found.
    const WebAssemblyBuiltinSet* findByQualifiedName(const String&) const;

private:
    WebAssemblyBuiltinRegistry(WebAssemblyBuiltinRegistry&&) = default;

    Vector<WebAssemblyBuiltinSet> m_builtinSets;
};

#define PASTE(x, y) x##y
#define CONCAT(x, y) PASTE(x, y)

// The naming scheme. Changes should be matched in InPlaceInterpreter.asm
// macros wasmBuiltinCallTrampoline and defineWasmBuiltinCallTrampoline.

#define BUILTIN_FULL_NAME(setName, builtinName) setName ## __ ## builtinName
#define BUILTIN_WASM_ENTRY_NAME(setName, builtinName) CONCAT(wasm_builtin__, BUILTIN_FULL_NAME(setName, builtinName))

#define EXPECTATIONS(...) Vector<std::unique_ptr<WebAssemblyBuiltinTypeExpectation>>::from(__VA_ARGS__)

#define BUILTIN_SIG_R_R \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref()))

#define BUILTIN_SIG_I_R \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::i32()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref()))

#define BUILTIN_SIG_R_I \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::i32()))

#define BUILTIN_SIG_R_RR \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref(), WebAssemblyBuiltinTypeExpectation::externref()))

#define BUILTIN_SIG_I_RR \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::i32()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref(), WebAssemblyBuiltinTypeExpectation::externref()))

#define BUILTIN_SIG_I_RI \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::i32()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref(), WebAssemblyBuiltinTypeExpectation::i32()))

#define BUILTIN_SIG_R_RII \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref(), WebAssemblyBuiltinTypeExpectation::i32(), WebAssemblyBuiltinTypeExpectation::i32()))

#define BUILTIN_SIG_R_AII \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::refNullArrayMutI16(), WebAssemblyBuiltinTypeExpectation::i32(), WebAssemblyBuiltinTypeExpectation::i32()))

#define BUILTIN_SIG_I_RAI \
    WebAssemblyBuiltinSignature( \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::i32()), \
        EXPECTATIONS(WebAssemblyBuiltinTypeExpectation::externref(), WebAssemblyBuiltinTypeExpectation::refNullArrayMutI16(), WebAssemblyBuiltinTypeExpectation::i32()))

// Enumerates builtins of the `js-string` set.
// For ease of tracking, builtins are listed in the order they appear in the spec.
#define FOR_EACH_WASM_JS_STRING_BUILTIN(m) \
    m(jsstring, cast, BUILTIN_SIG_R_R) \
    m(jsstring, test, BUILTIN_SIG_I_R) \
    m(jsstring, fromCharCodeArray, BUILTIN_SIG_R_AII) \
    m(jsstring, intoCharCodeArray, BUILTIN_SIG_I_RAI) \
    m(jsstring, fromCharCode, BUILTIN_SIG_R_I) \
    m(jsstring, fromCodePoint, BUILTIN_SIG_R_I) \
    m(jsstring, charCodeAt, BUILTIN_SIG_I_RI) \
    m(jsstring, codePointAt, BUILTIN_SIG_I_RI) \
    m(jsstring, length, BUILTIN_SIG_I_R) \
    m(jsstring, concat, BUILTIN_SIG_R_RR) \
    m(jsstring, substring, BUILTIN_SIG_R_RII) \
    m(jsstring, equals, BUILTIN_SIG_I_RR) \
    m(jsstring, compare, BUILTIN_SIG_I_RR)

#define FOR_EACH_WASM_BUILTIN(m) \
    FOR_EACH_WASM_JS_STRING_BUILTIN(m)
    // additional builtin sets go here

enum class WasmBuiltinID {
#define BUILTIN_ENTRY(setName, builtinName, type) BUILTIN_FULL_NAME(setName, builtinName),
    FOR_EACH_WASM_BUILTIN(BUILTIN_ENTRY)
#undef BUILTIN_ENTRY
    _last
};

constexpr std::size_t WASM_BUILTIN_COUNT = static_cast<std::size_t>(WasmBuiltinID::_last);

/**
 * A struct with the same layout as the array of callee pointers in the wasm instance.
 * Digested by LLIntOffsetImporter so that trampoline asm code can use symbolic names.
*/
struct WasmBuiltinCalleeOffsets {
#define CALLEE_ENTRY(setName, builtinName, type) void* BUILTIN_FULL_NAME(setName, builtinName);
    FOR_EACH_WASM_BUILTIN(CALLEE_ENTRY)
#undef CALLEE_ENTRY
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
