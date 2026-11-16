/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SideEffectCheckScope.h"

#include "ArgList.h"
#include "ArrayPrototypeInlines.h"
#include "CodeBlock.h"
#include "Error.h"
#include "FunctionExecutable.h"
#include "InternalFunction.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "NativeExecutable.h"
#include "Options.h"
#include "PropertyDescriptor.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringView.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SideEffectFreeFunctions);

// Evaluation previews are recomputed on every keystroke, so they must not block the page for long.
static constexpr Seconds timeLimit = 500_ms;

using Guard = SideEffectFreeFunctions::Guard;

struct AllowlistEntry {
    ASCIILiteral path;
    Guard guard { Guard::None };
};

// Each path is resolved from the global object of a pristine realm. A leading "get " resolves the getter of
// the last component, "@@toPrimitive" is Symbol.toPrimitive, and "%TypedArray%" is the prototype of Int8Array.
//
// Deliberately absent: anything that mutates its receiver or arguments (e.g. `push`, `sort`, `fill`), consumes
// iterators (e.g. `Array.from`, `Object.fromEntries`) since that can resume a generator defined by the page, reads
// properties of objects returned by a callback (e.g. `flat`, `flatMap`) since a JS builtin does that through inline
// caches that can call getters and Proxy traps directly, can allocate an arbitrarily large amount of memory in one
// call (e.g. `repeat`, `padStart`) since the time limit is only checked between calls,
// executes a RegExp (e.g. `match`, `replace`) since that updates `lastIndex` and `RegExp.lastMatch`, schedules
// work (e.g. `then`), forwards calls to arbitrary functions (e.g. `call`, `apply`, `bind`), or is observable
// by the garbage collector (e.g. `WeakRef.prototype.deref`).
static constexpr AllowlistEntry allowlist[] = {
    { "decodeURI"_s }, { "decodeURIComponent"_s }, { "encodeURI"_s }, { "encodeURIComponent"_s }, { "escape"_s }, { "unescape"_s },
    { "isFinite"_s }, { "isNaN"_s }, { "parseFloat"_s }, { "parseInt"_s },
    { "BigInt"_s }, { "Boolean"_s }, { "Number"_s }, { "String"_s }, { "Symbol"_s },

    { "Object.entries"_s }, { "Object.getOwnPropertyDescriptor"_s }, { "Object.getOwnPropertyDescriptors"_s }, { "Object.getOwnPropertyNames"_s },
    { "Object.getOwnPropertySymbols"_s }, { "Object.getPrototypeOf"_s }, { "Object.hasOwn"_s }, { "Object.is"_s }, { "Object.isExtensible"_s },
    { "Object.isFrozen"_s }, { "Object.isSealed"_s }, { "Object.keys"_s }, { "Object.values"_s },
    { "Object.prototype.hasOwnProperty"_s }, { "Object.prototype.isPrototypeOf"_s }, { "Object.prototype.propertyIsEnumerable"_s },
    { "Object.prototype.toLocaleString"_s }, { "Object.prototype.toString"_s }, { "Object.prototype.valueOf"_s }, { "get Object.prototype.__proto__"_s },

    { "Function.prototype.toString"_s },

    { "Array.isArray"_s },
    { "Array.prototype.at"_s, Guard::ArrayReceiver }, { "Array.prototype.concat"_s }, { "Array.prototype.every"_s, Guard::ArrayReceiver },
    { "Array.prototype.filter"_s, Guard::ArrayReceiver }, { "Array.prototype.find"_s, Guard::ArrayReceiver }, { "Array.prototype.findIndex"_s, Guard::ArrayReceiver },
    { "Array.prototype.findLast"_s, Guard::ArrayReceiver }, { "Array.prototype.findLastIndex"_s, Guard::ArrayReceiver },
    { "Array.prototype.forEach"_s, Guard::ArrayReceiver }, { "Array.prototype.includes"_s },
    { "Array.prototype.indexOf"_s }, { "Array.prototype.join"_s }, { "Array.prototype.lastIndexOf"_s }, { "Array.prototype.map"_s, Guard::ArrayReceiver },
    { "Array.prototype.reduce"_s, Guard::ArrayReceiver }, { "Array.prototype.reduceRight"_s, Guard::ArrayReceiver }, { "Array.prototype.slice"_s },
    { "Array.prototype.some"_s, Guard::ArrayReceiver }, { "Array.prototype.toReversed"_s }, { "Array.prototype.toSorted"_s }, { "Array.prototype.toSpliced"_s },
    { "Array.prototype.toString"_s }, { "Array.prototype.with"_s },

    { "String.fromCharCode"_s }, { "String.fromCodePoint"_s },
    { "String.prototype.at"_s }, { "String.prototype.charAt"_s }, { "String.prototype.charCodeAt"_s }, { "String.prototype.codePointAt"_s },
    { "String.prototype.concat"_s }, { "String.prototype.endsWith"_s }, { "String.prototype.includes"_s }, { "String.prototype.indexOf"_s },
    { "String.prototype.isWellFormed"_s }, { "String.prototype.lastIndexOf"_s }, { "String.prototype.localeCompare"_s }, { "String.prototype.normalize"_s },
    { "String.prototype.slice"_s },
    // A RegExp separator is executed directly in C++, which updates `RegExp.lastMatch`.
    { "String.prototype.split"_s, Guard::PrimitiveArguments },
    { "String.prototype.startsWith"_s }, { "String.prototype.substr"_s }, { "String.prototype.substring"_s }, { "String.prototype.toLocaleLowerCase"_s },
    { "String.prototype.toLocaleUpperCase"_s }, { "String.prototype.toLowerCase"_s }, { "String.prototype.toString"_s }, { "String.prototype.toUpperCase"_s },
    { "String.prototype.toWellFormed"_s }, { "String.prototype.trim"_s }, { "String.prototype.trimEnd"_s }, { "String.prototype.trimStart"_s },
    { "String.prototype.valueOf"_s },

    { "Number.isFinite"_s }, { "Number.isInteger"_s }, { "Number.isNaN"_s }, { "Number.isSafeInteger"_s }, { "Number.parseFloat"_s }, { "Number.parseInt"_s },
    { "Number.prototype.toExponential"_s }, { "Number.prototype.toFixed"_s }, { "Number.prototype.toLocaleString"_s }, { "Number.prototype.toPrecision"_s },
    { "Number.prototype.toString"_s }, { "Number.prototype.valueOf"_s },

    { "Boolean.prototype.toString"_s }, { "Boolean.prototype.valueOf"_s },

    { "BigInt.asIntN"_s }, { "BigInt.asUintN"_s },
    { "BigInt.prototype.toLocaleString"_s }, { "BigInt.prototype.toString"_s }, { "BigInt.prototype.valueOf"_s },

    { "Symbol.prototype.toString"_s }, { "Symbol.prototype.valueOf"_s }, { "Symbol.prototype.@@toPrimitive"_s }, { "get Symbol.prototype.description"_s },

    { "Math.abs"_s }, { "Math.acos"_s }, { "Math.acosh"_s }, { "Math.asin"_s }, { "Math.asinh"_s }, { "Math.atan"_s }, { "Math.atan2"_s }, { "Math.atanh"_s },
    { "Math.cbrt"_s }, { "Math.ceil"_s }, { "Math.clz32"_s }, { "Math.cos"_s }, { "Math.cosh"_s }, { "Math.exp"_s }, { "Math.expm1"_s }, { "Math.f16round"_s },
    { "Math.floor"_s }, { "Math.fround"_s }, { "Math.hypot"_s }, { "Math.imul"_s }, { "Math.log"_s }, { "Math.log10"_s }, { "Math.log1p"_s }, { "Math.log2"_s },
    { "Math.max"_s }, { "Math.min"_s }, { "Math.pow"_s }, { "Math.random"_s }, { "Math.round"_s }, { "Math.sign"_s }, { "Math.sin"_s }, { "Math.sinh"_s },
    { "Math.sqrt"_s }, { "Math.tan"_s }, { "Math.tanh"_s }, { "Math.trunc"_s },

    { "JSON.parse"_s }, { "JSON.stringify"_s },

    { "Date.UTC"_s }, { "Date.now"_s }, { "Date.parse"_s },
    { "Date.prototype.getDate"_s }, { "Date.prototype.getDay"_s }, { "Date.prototype.getFullYear"_s }, { "Date.prototype.getHours"_s },
    { "Date.prototype.getMilliseconds"_s }, { "Date.prototype.getMinutes"_s }, { "Date.prototype.getMonth"_s }, { "Date.prototype.getSeconds"_s },
    { "Date.prototype.getTime"_s }, { "Date.prototype.getTimezoneOffset"_s }, { "Date.prototype.getUTCDate"_s }, { "Date.prototype.getUTCDay"_s },
    { "Date.prototype.getUTCFullYear"_s }, { "Date.prototype.getUTCHours"_s }, { "Date.prototype.getUTCMilliseconds"_s }, { "Date.prototype.getUTCMinutes"_s },
    { "Date.prototype.getUTCMonth"_s }, { "Date.prototype.getUTCSeconds"_s }, { "Date.prototype.getYear"_s }, { "Date.prototype.toDateString"_s },
    { "Date.prototype.toISOString"_s }, { "Date.prototype.toJSON"_s }, { "Date.prototype.toLocaleDateString"_s }, { "Date.prototype.toLocaleString"_s },
    { "Date.prototype.toLocaleTimeString"_s }, { "Date.prototype.toString"_s }, { "Date.prototype.toTimeString"_s }, { "Date.prototype.toUTCString"_s },
    { "Date.prototype.valueOf"_s }, { "Date.prototype.@@toPrimitive"_s },

    { "Map.prototype.get"_s }, { "Map.prototype.has"_s }, { "get Map.prototype.size"_s },
    { "Set.prototype.has"_s }, { "get Set.prototype.size"_s },
    { "WeakMap.prototype.get"_s }, { "WeakMap.prototype.has"_s }, { "WeakSet.prototype.has"_s },

    { "RegExp.prototype.toString"_s }, { "get RegExp.prototype.dotAll"_s }, { "get RegExp.prototype.flags"_s }, { "get RegExp.prototype.global"_s },
    { "get RegExp.prototype.hasIndices"_s }, { "get RegExp.prototype.ignoreCase"_s }, { "get RegExp.prototype.multiline"_s }, { "get RegExp.prototype.source"_s },
    { "get RegExp.prototype.sticky"_s }, { "get RegExp.prototype.unicode"_s }, { "get RegExp.prototype.unicodeSets"_s },

    { "Error.prototype.toString"_s },

    { "get ArrayBuffer.prototype.byteLength"_s }, { "get ArrayBuffer.prototype.detached"_s }, { "get ArrayBuffer.prototype.maxByteLength"_s },
    { "get ArrayBuffer.prototype.resizable"_s }, { "get SharedArrayBuffer.prototype.byteLength"_s },
    { "get DataView.prototype.buffer"_s }, { "get DataView.prototype.byteLength"_s }, { "get DataView.prototype.byteOffset"_s },
    { "%TypedArray%.prototype.includes"_s }, { "%TypedArray%.prototype.indexOf"_s }, { "%TypedArray%.prototype.join"_s }, { "%TypedArray%.prototype.lastIndexOf"_s },
    { "get %TypedArray%.prototype.buffer"_s }, { "get %TypedArray%.prototype.byteLength"_s }, { "get %TypedArray%.prototype.byteOffset"_s },
    { "get %TypedArray%.prototype.length"_s },
};

static JSValue resolve(JSGlobalObject* globalObject, StringView path)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool isGetter = path.startsWith("get "_s);
    if (isGetter)
        path = path.substring(4);

    Vector<StringView, 4> components;
    for (auto component : path.split('.'))
        components.append(component);

    JSValue value = globalObject;
    for (size_t i = 0; i < components.size(); ++i) {
        auto* object = value.getObject();
        if (!object)
            return { };

        auto component = components[i];
        if (component == "%TypedArray%"_s) {
            JSValue int8Array = object->get(globalObject, Identifier::fromString(vm, "Int8Array"_s));
            RETURN_IF_EXCEPTION(scope, { });
            if (!int8Array.isObject())
                return { };
            value = asObject(int8Array)->getPrototypeDirect();
            continue;
        }

        Identifier identifier = component == "@@toPrimitive"_s ? vm.propertyNames->toPrimitiveSymbol : Identifier::fromString(vm, component.toString());

        if (isGetter && i == components.size() - 1) {
            PropertyDescriptor descriptor;
            bool hasProperty = object->getOwnPropertyDescriptor(globalObject, identifier, descriptor);
            RETURN_IF_EXCEPTION(scope, { });
            if (!hasProperty || !descriptor.isAccessorDescriptor())
                return { };
            return descriptor.getter();
        }

        value = object->get(globalObject, identifier);
        RETURN_IF_EXCEPTION(scope, { });
    }
    return value;
}

SideEffectFreeFunctions::SideEffectFreeFunctions() = default;
SideEffectFreeFunctions::~SideEffectFreeFunctions() = default;

void SideEffectFreeFunctions::addHostFunction(NativeFunction function, Guard guard)
{
    m_hostFunctions.add(function.untaggedPtr(), guard);
}

void SideEffectFreeFunctions::add(JSValue value, Guard guard)
{
    if (auto* function = dynamicDowncast<JSFunction>(value)) {
        if (auto* nativeExecutable = dynamicDowncast<NativeExecutable>(function->executable())) {
            // Natives read their receiver and arguments in C++, where any call back into JS is itself checked.
            m_hostFunctions.add(nativeExecutable->function().untaggedPtr(), guard == Guard::ArrayReceiver ? Guard::None : guard);
            return;
        }

        auto* functionExecutable = dynamicDowncast<FunctionExecutable>(function->executable());
        if (!functionExecutable || !functionExecutable->isBuiltinFunction())
            return;

        // Every JSC builtin is a range of the same source, so its offset identifies it in every realm, and
        // remains valid even after `VM::deleteAllCode` discards the UnlinkedFunctionExecutable.
        const SourceCode& source = functionExecutable->source();
        if (!m_builtinSourceProvider)
            m_builtinSourceProvider = source.provider();
        if (source.provider() != m_builtinSourceProvider)
            return;

        // JS builtins read properties of their receiver and arguments without going through C++.
        m_builtins.add(static_cast<unsigned>(source.startOffset()), guard == Guard::None ? Guard::PrimitiveArguments : guard);
        return;
    }

    if (auto* internalFunction = dynamicDowncast<InternalFunction>(value))
        m_hostFunctions.add(internalFunction->nativeFunctionFor(CodeSpecializationKind::CodeForCall).untaggedPtr(), guard == Guard::ArrayReceiver ? Guard::None : guard);
}

void SideEffectFreeFunctions::ensureBuiltinsAdded(VM& vm)
{
    if (m_didAddBuiltins)
        return;
    m_didAddBuiltins = true;

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    // Object spread (e.g. `({...a})`) calls these. They only read from their source, and copy into a new object.
    addHostFunction(globalFuncCloneObject);
    addHostFunction(globalFuncCopyDataProperties);

    // The page could have replaced any of these properties, including with a different native function of the
    // same name (e.g. `String.prototype.normalize = Node.prototype.normalize`), so use a realm it can't reach.
    auto* globalObject = JSGlobalObject::create(vm, JSGlobalObject::createStructure(vm, jsNull()));
    for (auto& entry : allowlist) {
        JSValue value = resolve(globalObject, entry.path);
        if (scope.exception()) [[unlikely]] {
            scope.clearException();
            continue;
        }
        if (value)
            add(value, entry.guard);
    }
}

std::optional<Guard> SideEffectFreeFunctions::guardForHostFunction(TaggedNativeFunction function) const
{
    auto it = m_hostFunctions.find(function.untaggedPtr());
    if (it == m_hostFunctions.end())
        return std::nullopt;
    return it->value;
}

std::optional<Guard> SideEffectFreeFunctions::guardForBuiltin(const FunctionExecutable& executable) const
{
    const SourceCode& source = executable.source();
    if (!m_builtinSourceProvider || source.provider() != m_builtinSourceProvider)
        return std::nullopt;

    auto it = m_builtins.find(static_cast<unsigned>(source.startOffset()));
    if (it == m_builtins.end())
        return std::nullopt;
    return it->value;
}

bool SideEffectCheckScope::isSupported()
{
    return Options::useLLInt();
}

SideEffectCheckScope::SideEffectCheckScope(VM& vm, const SourceCode& source)
    : m_vm(vm)
    , m_previousScope(vm.m_sideEffectCheckScope)
    , m_sourceProvider(source.provider())
    , m_deadline(MonotonicTime::now() + timeLimit)
{
    vm.sideEffectFreeFunctions().ensureBuiltinsAdded(vm);
    vm.m_sideEffectCheckScope = this;
}

SideEffectCheckScope::~SideEffectCheckScope()
{
    ASSERT(m_vm.m_sideEffectCheckScope == this);
    m_vm.m_sideEffectCheckScope = m_previousScope;
}

bool SideEffectCheckScope::isEvaluatedCode(const ScriptExecutable* executable) const
{
    return executable && isEvaluatedSource(executable->source().provider());
}

bool SideEffectCheckScope::isEvaluatedCode(const CodeBlock* codeBlock) const
{
    return codeBlock && isEvaluatedCode(codeBlock->ownerExecutable());
}

bool SideEffectCheckScope::isAllowedCallback(JSValue value)
{
    if (!value.isCallable())
        return true;

    if (auto* function = dynamicDowncast<JSFunction>(value)) {
        if (auto* nativeExecutable = dynamicDowncast<NativeExecutable>(function->executable()))
            return m_vm.sideEffectFreeFunctions().guardForHostFunction(nativeExecutable->function()) == Guard::None;
        // A builtin calls its callback directly, so the callback's own guard would never be checked.
        return isEvaluatedCode(function->jsExecutable());
    }

    if (auto* internalFunction = dynamicDowncast<InternalFunction>(value))
        return m_vm.sideEffectFreeFunctions().guardForHostFunction(internalFunction->nativeFunctionFor(CodeSpecializationKind::CodeForCall)) == Guard::None;

    return false;
}

bool SideEffectCheckScope::passesGuard(Guard guard, JSValue thisValue, const ArgList& arguments)
{
    switch (guard) {
    case Guard::None:
        return true;

    case Guard::PrimitiveArguments:
        if (thisValue.isObject())
            return false;
        for (size_t i = 0; i < arguments.size(); ++i) {
            if (arguments.at(i).isObject())
                return false;
        }
        return true;

    case Guard::ArrayReceiver: {
        auto* array = dynamicDowncast<JSArray>(thisValue);
        if (!array)
            return false;

        // Rule out anything that could make the builtin call a getter, e.g. accessors on the array itself or
        // on its prototype chain, or a `constructor`/`Symbol.species` used to create the result.
        auto* globalObject = array->realmMayBeNull();
        if (!globalObject || !globalObject->isOriginalArrayStructure(array->structure()) || hasAnyArrayStorage(array->indexingType()))
            return false;
        if (!globalObject->arrayPrototypeChainIsSane() || !arraySpeciesWatchpointIsValid(m_vm, array))
            return false;

        for (size_t i = 0; i < arguments.size(); ++i) {
            if (!isAllowedCallback(arguments.at(i)))
                return false;
        }
        return true;
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

bool SideEffectCheckScope::isAllowedToRun(JSObject* callee, CodeSpecializationKind kind, JSValue thisValue, const ArgList& arguments)
{
    auto& functions = m_vm.sideEffectFreeFunctions();

    if (auto* function = dynamicDowncast<JSFunction>(callee)) {
        if (auto* functionExecutable = dynamicDowncast<FunctionExecutable>(function->executable())) {
            if (isEvaluatedCode(functionExecutable))
                return true;

            if (kind != CodeSpecializationKind::CodeForCall || !functionExecutable->isBuiltinFunction())
                return false;

            auto guard = functions.guardForBuiltin(*functionExecutable);
            return guard && passesGuard(*guard, thisValue, arguments);
        }

        auto* nativeExecutable = dynamicDowncast<NativeExecutable>(function->executable());
        if (!nativeExecutable || kind != CodeSpecializationKind::CodeForCall)
            return false;

        auto guard = functions.guardForHostFunction(nativeExecutable->function());
        return guard && passesGuard(*guard, thisValue, arguments);
    }

    if (auto* internalFunction = dynamicDowncast<InternalFunction>(callee)) {
        if (kind != CodeSpecializationKind::CodeForCall)
            return false;

        auto guard = functions.guardForHostFunction(internalFunction->nativeFunctionFor(kind));
        return guard && passesGuard(*guard, thisValue, arguments);
    }

    return false;
}

bool SideEffectCheckScope::throwUnlessAllowed(JSGlobalObject* globalObject, bool isAllowed)
{
    auto scope = DECLARE_THROW_SCOPE(m_vm);

    if (MonotonicTime::now() > m_deadline) {
        throwException(globalObject, scope, createEvalError(globalObject, "Possible side effect: time limit exceeded"_s));
        return false;
    }

    if (!isAllowed) {
        throwException(globalObject, scope, createEvalError(globalObject, "Possible side effect"_s));
        return false;
    }

    return true;
}

bool SideEffectCheckScope::checkCall(JSGlobalObject* globalObject, JSObject* callee, CodeSpecializationKind kind, JSValue thisValue, const ArgList& arguments)
{
    return throwUnlessAllowed(globalObject, isAllowedToRun(callee, kind, thisValue, arguments));
}

bool SideEffectCheckScope::checkCachedCall(JSGlobalObject* globalObject, JSFunction* function)
{
    // A CachedCall is prepared before its receiver and arguments are known, so no guard can be checked.
    return throwUnlessAllowed(globalObject, isEvaluatedCode(function->jsExecutable()));
}

} // namespace JSC
