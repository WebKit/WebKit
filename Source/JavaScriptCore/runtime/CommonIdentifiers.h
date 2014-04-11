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

#include "Identifier.h"
#include <wtf/Noncopyable.h>

// MarkedArgumentBuffer of property names, passed to a macro so we can do set them up various
// ways without repeating the list.
#define JSC_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    macro(ArgumentsIterator) \
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
    macro(SyntaxError) \
    macro(TypeError) \
    macro(URIError) \
    macro(UTC) \
    macro(WeakMap)\
    macro(__defineGetter__) \
    macro(__defineSetter__) \
    macro(__lookupGetter__) \
    macro(__lookupSetter__) \
    macro(add) \
    macro(anonymous) \
    macro(arguments) \
    macro(bind) \
    macro(buffer) \
    macro(byteLength) \
    macro(byteOffset) \
    macro(bytecode) \
    macro(bytecodeIndex) \
    macro(bytecodes) \
    macro(bytecodesID) \
    macro(callee) \
    macro(caller) \
    macro(cast) \
    macro(clear) \
    macro(compilationKind) \
    macro(compilations) \
    macro(compile) \
    macro(configurable) \
    macro(constructor) \
    macro(count) \
    macro(counters) \
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
    macro(forEach) \
    macro(fromCharCode) \
    macro(get) \
    macro(global) \
    macro(has) \
    macro(hasOwnProperty) \
    macro(hash) \
    macro(header) \
    macro(id) \
    macro(ignoreCase) \
    macro(index) \
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
    macro(profiledBytecodes) \
    macro(propertyIsEnumerable) \
    macro(prototype) \
    macro(set) \
    macro(size) \
    macro(slice) \
    macro(source) \
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
    macro(values) \
    macro(valueOf) \
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

#define JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    macro(iterator) \
    macro(iteratorNext) \
    macro(resolve) \
    macro(reject) \
    macro(promise) \
    macro(fulfillmentHandler) \
    macro(rejectionHandler) \
    macro(index) \
    macro(values) \
    macro(deferred) \
    macro(countdownHolder) \
    macro(Object) \
    macro(TypeError) \
    macro(undefined) \
    macro(BuiltinLog)

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
        
        const Identifier* getPrivateName(const Identifier&) const;
        Identifier getPublicName(const Identifier&) const;
    };

} // namespace JSC

#endif // CommonIdentifiers_h
