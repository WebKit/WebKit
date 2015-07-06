/*
 *  Copyright (C) 2003, 2007, 2009 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef CommonIdentifiers_h
#define CommonIdentifiers_h

#include "BytecodeIntrinsicRegistry.h"
#include "Identifier.h"
#include <wtf/Noncopyable.h>

// MarkedArgumentBuffer of property names, passed to a macro so we can do set them up various
// ways without repeating the list.
#define JSC_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    macro(Array) \
    macro(ArrayBuffer) \
    macro(ArrayIterator) \
    macro(BYTES_PER_ELEMENT) \
    macro(Boolean) \
    macro(Date) \
    macro(Error) \
    macro(EvalError) \
    macro(Function) \
    macro(Infinity) \
    macro(Intl) \
    macro(JSON) \
    macro(Map)\
    macro(MapIterator)\
    macro(Math) \
    macro(NaN) \
    macro(Number) \
    macro(Object) \
    macro(Promise) \
    macro(RangeError) \
    macro(ReferenceError) \
    macro(RegExp) \
    macro(Set)\
    macro(SetIterator)\
    macro(String) \
    macro(Symbol) \
    macro(SyntaxError) \
    macro(TypeError) \
    macro(URIError) \
    macro(UTC) \
    macro(WeakMap)\
    macro(WeakSet)\
    macro(__defineGetter__) \
    macro(__defineSetter__) \
    macro(__lookupGetter__) \
    macro(__lookupSetter__) \
    macro(add) \
    macro(additionalJettisonReason) \
    macro(anonymous) \
    macro(arguments) \
    macro(assign) \
    macro(back) \
    macro(bind) \
    macro(blur) \
    macro(buffer) \
    macro(byteLength) \
    macro(byteOffset) \
    macro(bytecode) \
    macro(bytecodeIndex) \
    macro(bytecodes) \
    macro(bytecodesID) \
    macro(callee) \
    macro(caller) \
    macro(clear) \
    macro(close) \
    macro(closed) \
    macro(column) \
    macro(compilationKind) \
    macro(compilations) \
    macro(compile) \
    macro(configurable) \
    macro(constructor) \
    macro(count) \
    macro(counters) \
    macro(defineProperty) \
    macro(description) \
    macro(descriptions) \
    macro(displayName) \
    macro(document) \
    macro(done) \
    macro(entries) \
    macro(enumerable) \
    macro(eval) \
    macro(exec) \
    macro(executionCount) \
    macro(exitKind) \
    macro(flags) \
    macro(focus) \
    macro(forEach) \
    macro(forward) \
    macro(fromCharCode) \
    macro(get) \
    macro(global) \
    macro(go) \
    macro(has) \
    macro(hasOwnProperty) \
    macro(hash) \
    macro(header) \
    macro(href) \
    macro(id) \
    macro(ignoreCase) \
    macro(index) \
    macro(indexedDB) \
    macro(inferredName) \
    macro(input) \
    macro(instructionCount) \
    macro(isArray) \
    macro(isPrototypeOf) \
    macro(isView) \
    macro(isWatchpoint) \
    macro(jettisonReason) \
    macro(join) \
    macro(keys) \
    macro(lastIndex) \
    macro(length) \
    macro(line) \
    macro(message) \
    macro(multiline) \
    macro(name) \
    macro(next) \
    macro(now) \
    macro(numInlinedCalls) \
    macro(numInlinedGetByIds) \
    macro(numInlinedPutByIds) \
    macro(of) \
    macro(opcode) \
    macro(origin) \
    macro(osrExitSites) \
    macro(osrExits) \
    macro(parse) \
    macro(parseInt) \
    macro(postMessage) \
    macro(profiledBytecodes) \
    macro(propertyIsEnumerable) \
    macro(prototype) \
    macro(raw) \
    macro(reload) \
    macro(replace) \
    macro(resolve) \
    macro(set) \
    macro(showModalDialog) \
    macro(size) \
    macro(slice) \
    macro(source) \
    macro(sourceURL) \
    macro(sourceCode) \
    macro(stack) \
    macro(subarray) \
    macro(test) \
    macro(then) \
    macro(toExponential) \
    macro(toFixed) \
    macro(toISOString) \
    macro(toJSON) \
    macro(toLocaleString) \
    macro(toPrecision) \
    macro(toString) \
    macro(value) \
    macro(valueOf) \
    macro(values) \
    macro(webkit) \
    macro(webkitIndexedDB) \
    macro(window) \
    macro(writable)

#define JSC_COMMON_IDENTIFIERS_EACH_KEYWORD(macro) \
    macro(break) \
    macro(case) \
    macro(catch) \
    macro(class) \
    macro(const) \
    macro(continue) \
    macro(debugger) \
    macro(default) \
    macro(delete) \
    macro(do) \
    macro(else) \
    macro(enum) \
    macro(export) \
    macro(extends) \
    macro(false) \
    macro(finally) \
    macro(for) \
    macro(function) \
    macro(if) \
    macro(implements) \
    macro(import) \
    macro(in) \
    macro(instanceof) \
    macro(interface) \
    macro(let) \
    macro(new) \
    macro(null) \
    macro(package) \
    macro(private) \
    macro(protected) \
    macro(public) \
    macro(return) \
    macro(static) \
    macro(super) \
    macro(switch) \
    macro(this) \
    macro(throw) \
    macro(true) \
    macro(try) \
    macro(typeof) \
    macro(undefined) \
    macro(var) \
    macro(void) \
    macro(while) \
    macro(with) \
    macro(yield)

#define JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL_NOT_IMPLEMENTED_YET(macro)\
    macro(hasInstance) \
    macro(isConcatSpreadable) \
    macro(match) \
    macro(replace) \
    macro(search) \
    macro(species) \
    macro(split) \
    macro(toPrimitive) \
    macro(toStringTag)

#define JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(macro) \
    macro(iterator) \
    macro(unscopables)

#define JSC_COMMON_BYTECODE_INTRINSICS_EACH_NAME(macro) \
    macro(putByValDirect) \
    macro(toString)

#define JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    JSC_COMMON_BYTECODE_INTRINSICS_EACH_NAME(macro) \
    macro(symbolIterator) \
    macro(iteratedObject) \
    macro(arrayIteratorNextIndex) \
    macro(arrayIterationKind) \
    macro(arrayIterationKindKey) \
    macro(arrayIterationKindValue) \
    macro(arrayIterationKindKeyValue) \
    macro(charCodeAt) \
    macro(iteratedString) \
    macro(stringIteratorNextIndex) \
    macro(promise) \
    macro(fulfillmentHandler) \
    macro(rejectionHandler) \
    macro(index) \
    macro(values) \
    macro(deferred) \
    macro(countdownHolder) \
    macro(Object) \
    macro(objectKeys) \
    macro(objectGetOwnPropertyDescriptor) \
    macro(objectGetOwnPropertySymbols) \
    macro(Number) \
    macro(Array) \
    macro(String) \
    macro(Promise) \
    macro(abs) \
    macro(floor) \
    macro(isFinite) \
    macro(getPrototypeOf) \
    macro(getOwnPropertyNames) \
    macro(TypeError) \
    macro(undefined) \
    macro(BuiltinLog) \
    macro(homeObject) \
    macro(getTemplateObject) \
    macro(enqueueJob) \
    macro(handler) \
    macro(promiseState) \
    macro(promisePending) \
    macro(promiseFulfilled) \
    macro(promiseRejected) \
    macro(promiseFulfillReactions) \
    macro(promiseRejectReactions) \
    macro(promiseResult) \
    macro(capabilities) \


namespace JSC {
    
    class BuiltinNames;
    
    class CommonIdentifiers {
        WTF_MAKE_NONCOPYABLE(CommonIdentifiers); WTF_MAKE_FAST_ALLOCATED;
    private:
        CommonIdentifiers(VM*);
        ~CommonIdentifiers();
        friend class VM;
        
    public:
        const BuiltinNames& builtinNames() const { return *m_builtinNames; }
        const Identifier nullIdentifier;
        const Identifier emptyIdentifier;
        const Identifier underscoreProto;
        const Identifier thisIdentifier;
        const Identifier useStrictIdentifier;
    private:
        std::unique_ptr<BuiltinNames> m_builtinNames;

    public:
        
#define JSC_IDENTIFIER_DECLARE_KEYWORD_NAME_GLOBAL(name) const Identifier name##Keyword;
        JSC_COMMON_IDENTIFIERS_EACH_KEYWORD(JSC_IDENTIFIER_DECLARE_KEYWORD_NAME_GLOBAL)
#undef JSC_IDENTIFIER_DECLARE_KEYWORD_NAME_GLOBAL
        
#define JSC_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL(name) const Identifier name;
        JSC_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(JSC_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL)
#undef JSC_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL

#define JSC_IDENTIFIER_DECLARE_PRIVATE_PROPERTY_NAME_GLOBAL(name) const Identifier name##PrivateName;
        JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(JSC_IDENTIFIER_DECLARE_PRIVATE_PROPERTY_NAME_GLOBAL)
#undef JSC_IDENTIFIER_DECLARE_PRIVATE_PROPERTY_NAME_GLOBAL

#define JSC_IDENTIFIER_DECLARE_PRIVATE_WELL_KNOWN_SYMBOL_GLOBAL(name) const Identifier name##Symbol;
        JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(JSC_IDENTIFIER_DECLARE_PRIVATE_WELL_KNOWN_SYMBOL_GLOBAL)
#undef JSC_IDENTIFIER_DECLARE_PRIVATE_WELL_KNOWN_SYMBOL_GLOBAL

        bool isPrivateName(SymbolImpl& uid) const;
        bool isPrivateName(UniquedStringImpl& uid) const;
        bool isPrivateName(const Identifier&) const;

        const Identifier* getPrivateName(const Identifier&) const;
        Identifier getPublicName(const Identifier&) const;

        const BytecodeIntrinsicRegistry& bytecodeIntrinsicRegistry() const { return m_bytecodeIntrinsicRegistry; }

    private:
        BytecodeIntrinsicRegistry m_bytecodeIntrinsicRegistry;
    };

} // namespace JSC

#endif // CommonIdentifiers_h
