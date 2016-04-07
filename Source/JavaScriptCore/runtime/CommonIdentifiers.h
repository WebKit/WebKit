/*
 *  Copyright (C) 2003, 2007, 2009, 2016 Apple Inc. All rights reserved.
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
    macro(AnimationTimeline) \
    macro(Array) \
    macro(ArrayBuffer) \
    macro(ArrayIterator) \
    macro(Audio) \
    macro(BYTES_PER_ELEMENT) \
    macro(Boolean) \
    macro(Collator) \
    macro(Date) \
    macro(DateTimeFormat) \
    macro(DocumentTimeline) \
    macro(Error) \
    macro(EvalError) \
    macro(Function) \
    macro(Gamepad) \
    macro(GamepadButton) \
    macro(GamepadEvent) \
    macro(GeneratorFunction) \
    macro(Headers) \
    macro(HTMLAudioElement) \
    macro(HTMLSlotElement) \
    macro(IDBCursor) \
    macro(IDBCursorWithValue) \
    macro(IDBDatabase) \
    macro(IDBFactory) \
    macro(IDBIndex) \
    macro(IDBKeyRange) \
    macro(IDBObjectStore) \
    macro(IDBOpenDBRequest) \
    macro(IDBRequest) \
    macro(IDBTransaction) \
    macro(IDBVersionChangeEvent) \
    macro(Infinity) \
    macro(Intl) \
    macro(JSON) \
    macro(Loader) \
    macro(Map)\
    macro(MapIterator)\
    macro(Math) \
    macro(NaN) \
    macro(Number) \
    macro(NumberFormat) \
    macro(Object) \
    macro(Promise) \
    macro(Proxy) \
    macro(RangeError) \
    macro(ReferenceError) \
    macro(Reflect) \
    macro(RegExp) \
    macro(Response) \
    macro(Request) \
    macro(Set)\
    macro(SetIterator)\
    macro(ShadowRoot) \
    macro(String) \
    macro(Symbol) \
    macro(SyntaxError) \
    macro(TypeError) \
    macro(URIError) \
    macro(UTC) \
    macro(WeakMap)\
    macro(WeakSet)\
    macro(WebGL2RenderingContext) \
    macro(WebGLVertexArrayObject) \
    macro(WebSocket) \
    macro(__defineGetter__) \
    macro(__defineSetter__) \
    macro(__lookupGetter__) \
    macro(__lookupSetter__) \
    macro(add) \
    macro(additionalJettisonReason) \
    macro(anonymous) \
    macro(arguments) \
    macro(as) \
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
    macro(calendar) \
    macro(callee) \
    macro(caller) \
    macro(caseFirst) \
    macro(clear) \
    macro(close) \
    macro(closed) \
    macro(collation) \
    macro(column) \
    macro(compilationKind) \
    macro(compilations) \
    macro(compile) \
    macro(configurable) \
    macro(constructor) \
    macro(count) \
    macro(counters) \
    macro(day) \
    macro(defineProperty) \
    macro(description) \
    macro(descriptions) \
    macro(displayName) \
    macro(document) \
    macro(done) \
    macro(entries) \
    macro(enumerable) \
    macro(era) \
    macro(eval) \
    macro(exec) \
    macro(executionCount) \
    macro(exitKind) \
    macro(fetch) \
    macro(flags) \
    macro(focus) \
    macro(forEach) \
    macro(formatMatcher) \
    macro(forward) \
    macro(frames) \
    macro(from) \
    macro(fromCharCode) \
    macro(get) \
    macro(global) \
    macro(go) \
    macro(has) \
    macro(hasOwnProperty) \
    macro(hash) \
    macro(header) \
    macro(hour) \
    macro(hour12) \
    macro(href) \
    macro(id) \
    macro(ignoreCase) \
    macro(ignorePunctuation) \
    macro(index) \
    macro(indexedDB) \
    macro(inferredName) \
    macro(input) \
    macro(instructionCount) \
    macro(isArray) \
    macro(isEnabled) \
    macro(isPrototypeOf) \
    macro(isView) \
    macro(isWatchpoint) \
    macro(jettisonReason) \
    macro(join) \
    macro(keys) \
    macro(lastIndex) \
    macro(length) \
    macro(line) \
    macro(locale) \
    macro(localeMatcher) \
    macro(location) \
    macro(message) \
    macro(minute) \
    macro(month) \
    macro(multiline) \
    macro(name) \
    macro(next) \
    macro(now) \
    macro(numInlinedCalls) \
    macro(numInlinedGetByIds) \
    macro(numInlinedPutByIds) \
    macro(numberingSystem) \
    macro(numeric) \
    macro(of) \
    macro(opcode) \
    macro(opener) \
    macro(origin) \
    macro(osrExitSites) \
    macro(osrExits) \
    macro(parent) \
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
    macro(second) \
    macro(self) \
    macro(sensitivity) \
    macro(set) \
    macro(showModalDialog) \
    macro(size) \
    macro(slice) \
    macro(source) \
    macro(sourceCode) \
    macro(sourceURL) \
    macro(stack) \
    macro(sticky) \
    macro(subarray) \
    macro(target) \
    macro(test) \
    macro(then) \
    macro(timeZone) \
    macro(timeZoneName) \
    macro(toExponential) \
    macro(toFixed) \
    macro(toISOString) \
    macro(toJSON) \
    macro(toLocaleString) \
    macro(toPrecision) \
    macro(toString) \
    macro(top) \
    macro(unicode) \
    macro(usage) \
    macro(value) \
    macro(valueOf) \
    macro(values) \
    macro(webkit) \
    macro(webkitIDBCursor) \
    macro(webkitIDBDatabase) \
    macro(webkitIDBFactory) \
    macro(webkitIDBIndex) \
    macro(webkitIDBKeyRange) \
    macro(webkitIDBObjectStore) \
    macro(webkitIDBRequest) \
    macro(webkitIDBTransaction) \
    macro(webkitIndexedDB) \
    macro(weekday) \
    macro(window) \
    macro(writable) \
    macro(year)

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
    macro(isConcatSpreadable) \
    macro(replace) \
    macro(split)

#define JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_WELL_KNOWN_SYMBOL(macro) \
    macro(hasInstance) \
    macro(iterator) \
    macro(match) \
    macro(search) \
    macro(species) \
    macro(toPrimitive) \
    macro(toStringTag) \
    macro(unscopables)

#define JSC_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    JSC_COMMON_BYTECODE_INTRINSIC_FUNCTIONS_EACH_NAME(macro) \
    JSC_COMMON_BYTECODE_INTRINSIC_CONSTANTS_EACH_NAME(macro) \
    macro(iteratedObject) \
    macro(arrayIteratorNextIndex) \
    macro(arrayIterationKind) \
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
    macro(ownEnumerablePropertyKeys) \
    macro(Number) \
    macro(Array) \
    macro(String) \
    macro(RegExp) \
    macro(Map) \
    macro(Promise) \
    macro(Reflect) \
    macro(InternalPromise) \
    macro(abs) \
    macro(floor) \
    macro(trunc) \
    macro(isFinite) \
    macro(isNaN) \
    macro(create) \
    macro(defineProperty) \
    macro(getPrototypeOf) \
    macro(getOwnPropertyDescriptor) \
    macro(getOwnPropertyNames) \
    macro(ownKeys) \
    macro(Error) \
    macro(RangeError) \
    macro(TypeError) \
    macro(typedArrayLength) \
    macro(typedArraySort) \
    macro(typedArrayGetOriginalConstructor) \
    macro(BuiltinLog) \
    macro(homeObject) \
    macro(getTemplateObject) \
    macro(enqueueJob) \
    macro(handler) \
    macro(promiseState) \
    macro(promiseFulfillReactions) \
    macro(promiseRejectReactions) \
    macro(promiseResult) \
    macro(push) \
    macro(repeatCharacter) \
    macro(capabilities) \
    macro(starDefault) \
    macro(InspectorInstrumentation) \
    macro(get) \
    macro(set) \
    macro(shift) \
    macro(allocateTypedArray) \
    macro(Int8Array) \
    macro(Int16Array) \
    macro(Int32Array) \
    macro(Uint8Array) \
    macro(Uint8ClampedArray) \
    macro(Uint16Array) \
    macro(Uint32Array) \
    macro(Float32Array) \
    macro(Float64Array) \
    macro(exec) \
    macro(generator) \
    macro(generatorNext) \
    macro(generatorState) \
    macro(generatorFrame) \
    macro(generatorValue) \
    macro(generatorThis) \
    macro(generatorResumeMode) \
    macro(Collator) \
    macro(DateTimeFormat) \
    macro(NumberFormat) \
    macro(intlSubstituteValue) \
    macro(thisTimeValue) \
    macro(newTargetLocal) \
    macro(derivedConstructor) \
    macro(isBoundFunction) \
    macro(hasInstanceBoundFunction) \
    macro(instanceOf) \
    macro(isArray) \
    macro(isArrayConstructor) \
    macro(isSet) \
    macro(isMap) \
    macro(regExpCreate) \
    macro(SetIterator) \
    macro(setIteratorNext) \
    macro(MapIterator) \
    macro(mapIteratorNext) \


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
        const Identifier timesIdentifier;
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

        const Identifier* lookUpPrivateName(const Identifier&) const;
        Identifier lookUpPublicName(const Identifier&) const;

        // Callers of this method should make sure that identifiers given to this method 
        // survive the lifetime of CommonIdentifiers and related VM.
        JS_EXPORT_PRIVATE void appendExternalName(const Identifier& publicName, const Identifier& privateName);
    };

} // namespace JSC

#endif // CommonIdentifiers_h
