/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "JSCBuiltins.h"

namespace JSC {

class CodeBlock;
class JSGlobalObject;

#define JSC_FOREACH_LINK_TIME_CONSTANTS(v) \
    JSC_FOREACH_BUILTIN_FUNCTION_PRIVATE_GLOBAL_NAME(v) \
    v(throwTypeErrorFunction, nullptr) \
    v(importModule, nullptr) \
    v(mapBucketHead, nullptr) \
    v(mapBucketNext, nullptr) \
    v(mapBucketKey, nullptr) \
    v(mapBucketValue, nullptr) \
    v(setBucketHead, nullptr) \
    v(setBucketNext, nullptr) \
    v(setBucketKey, nullptr) \
    v(propertyIsEnumerable, nullptr) \
    v(ownKeys, nullptr) \
    v(enqueueJob, nullptr) \
    v(makeTypeError, nullptr) \
    v(typedArrayLength, nullptr) \
    v(typedArrayGetOriginalConstructor, nullptr) \
    v(typedArraySort, nullptr) \
    v(isTypedArrayView, nullptr) \
    v(typedArraySubarrayCreate, nullptr) \
    v(isBoundFunction, nullptr) \
    v(hasInstanceBoundFunction, nullptr) \
    v(instanceOf, nullptr) \
    v(BuiltinLog, nullptr) \
    v(BuiltinDescribe, nullptr) \
    v(RegExp, nullptr) \
    v(trunc, nullptr) \
    v(Promise, nullptr) \
    v(InternalPromise, nullptr) \
    v(defaultPromiseThen, nullptr) \
    v(repeatCharacter, nullptr) \
    v(arraySpeciesCreate, nullptr) \
    v(isArray, nullptr) \
    v(isArraySlow, nullptr) \
    v(concatMemcpy, nullptr) \
    v(appendMemcpy, nullptr) \
    v(hostPromiseRejectionTracker, nullptr) \
    v(InspectorInstrumentation, nullptr) \
    v(Set, nullptr) \
    v(thisTimeValue, nullptr) \
    v(isConstructor, nullptr) \
    v(regExpProtoFlagsGetter, nullptr) \
    v(regExpProtoGlobalGetter, nullptr) \
    v(regExpProtoIgnoreCaseGetter, nullptr) \
    v(regExpProtoMultilineGetter, nullptr) \
    v(regExpProtoSourceGetter, nullptr) \
    v(regExpProtoStickyGetter, nullptr) \
    v(regExpProtoUnicodeGetter, nullptr) \
    v(regExpBuiltinExec, nullptr) \
    v(regExpCreate, nullptr) \
    v(isRegExp, nullptr) \
    v(regExpMatchFast, nullptr) \
    v(regExpSearchFast, nullptr) \
    v(regExpSplitFast, nullptr) \
    v(regExpPrototypeSymbolReplace, nullptr) \
    v(regExpTestFast, nullptr) \
    v(stringIncludesInternal, nullptr) \
    v(stringSplitFast, nullptr) \
    v(stringSubstrInternal, nullptr) \
    v(makeBoundFunction, nullptr) \
    v(hasOwnLengthProperty, nullptr) \
    v(dateTimeFormat, nullptr) \
    v(webAssemblyCompileStreamingInternal, nullptr) \
    v(webAssemblyInstantiateStreamingInternal, nullptr) \
    v(Object, nullptr) \
    v(Array, nullptr) \
    v(applyFunction, nullptr) \
    v(callFunction, nullptr) \


#define DECLARE_LINK_TIME_CONSTANT(name, code) name,
enum class LinkTimeConstant : int32_t {
    JSC_FOREACH_LINK_TIME_CONSTANTS(DECLARE_LINK_TIME_CONSTANT)
};
#undef DECLARE_LINK_TIME_CONSTANT
#define COUNT_LINK_TIME_CONSTANT(name, code) 1 +
static constexpr unsigned numberOfLinkTimeConstants = JSC_FOREACH_LINK_TIME_CONSTANTS(COUNT_LINK_TIME_CONSTANT) 0;
#undef COUNT_LINK_TIME_CONSTANT

} // namespace JSC

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::LinkTimeConstant);

} // namespace WTF
