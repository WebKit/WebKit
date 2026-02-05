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

#if ENABLE(B3_JIT)

#include "B3AbstractHeap.h"
#include "B3Value.h"
#include "DFGArrayMode.h"
#include "IndexingType.h"

namespace JSC::B3 {

#define FOR_EACH_ABSTRACT_HEAP(macro) \
    macro(TypedArrayProperties) \
    macro(JSCellHeaderAndNamedProperties) \
    macro(OrderedHashTableData) \

// macro(name, offset, mutability)
#define FOR_EACH_ABSTRACT_FIELD(macro) \
    macro(ArrayBuffer_data, ArrayBuffer::offsetOfData(), Mutability::Mutable) \
    macro(ArrayStorage_numValuesInVector, ArrayStorage::numValuesInVectorOffset(), Mutability::Mutable) \
    macro(Butterfly_arrayBuffer, Butterfly::offsetOfArrayBuffer(), Mutability::Mutable) \
    macro(Butterfly_publicLength, Butterfly::offsetOfPublicLength(), Mutability::Mutable) \
    macro(Butterfly_vectorLength, Butterfly::offsetOfVectorLength(), Mutability::Mutable) \
    macro(CallFrame_callerFrame, CallFrame::callerFrameOffset(), Mutability::Mutable) \
    macro(ClassInfo_parentClass, ClassInfo::offsetOfParentClass(), Mutability::Immutable) \
    macro(ClonedArguments_callee, ClonedArguments::offsetOfCallee(), Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache0_key, ConcatKeyAtomStringCache::offsetOfQuickCache0() + ConcatKeyAtomStringCache::CacheEntry::offsetOfKey(), Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache0_value, ConcatKeyAtomStringCache::offsetOfQuickCache0() + ConcatKeyAtomStringCache::CacheEntry::offsetOfValue(), Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache1_key, ConcatKeyAtomStringCache::offsetOfQuickCache1() + ConcatKeyAtomStringCache::CacheEntry::offsetOfKey(), Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache1_value, ConcatKeyAtomStringCache::offsetOfQuickCache1() + ConcatKeyAtomStringCache::CacheEntry::offsetOfValue(), Mutability::Mutable) \
    macro(DateInstance_internalNumber, DateInstance::offsetOfInternalNumber(), Mutability::Mutable) \
    macro(DateInstance_data, DateInstance::offsetOfData(), Mutability::Mutable) \
    macro(DateInstanceData_gregorianDateTimeCachedForMS, DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), Mutability::Mutable) \
    macro(DateInstanceData_gregorianDateTimeUTCCachedForMS, DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_year, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfYear(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_year, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfYear(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_month, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMonth(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_month, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMonth(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_monthDay, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMonthDay(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_monthDay, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMonthDay(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_weekDay, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfWeekDay(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_weekDay, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfWeekDay(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_hour, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfHour(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_hour, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfHour(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_minute, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMinute(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_minute, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMinute(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_second, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfSecond(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_second, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfSecond(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_utcOffsetInMinute, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfUTCOffsetInMinute(), Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_utcOffsetInMinute, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfUTCOffsetInMinute(), Mutability::Mutable) \
    macro(DirectArguments_callee, DirectArguments::offsetOfCallee(), Mutability::Mutable) \
    macro(DirectArguments_length, DirectArguments::offsetOfLength(), Mutability::Mutable) \
    macro(DirectArguments_minCapacity, DirectArguments::offsetOfMinCapacity(), Mutability::Mutable) \
    macro(DirectArguments_mappedArguments, DirectArguments::offsetOfMappedArguments(), Mutability::Mutable) \
    macro(DirectArguments_modifiedArgumentsDescriptor, DirectArguments::offsetOfModifiedArgumentsDescriptor(), Mutability::Mutable) \
    macro(FunctionExecutable_rareData, FunctionExecutable::offsetOfRareData(), Mutability::Mutable) \
    macro(FunctionExecutableRareData_asString, FunctionExecutable::RareData::offsetOfAsString(), Mutability::Mutable) \
    macro(FunctionRareData_allocator, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfAllocator(), Mutability::Mutable) \
    macro(FunctionRareData_structure, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfStructure(), Mutability::Mutable) \
    macro(FunctionRareData_prototype, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfPrototype(), Mutability::Mutable) \
    macro(FunctionRareData_allocationProfileWatchpointSet, FunctionRareData::offsetOfAllocationProfileWatchpointSet(), Mutability::Mutable) \
    macro(FunctionRareData_executable, FunctionRareData::offsetOfExecutable(), Mutability::Mutable) \
    macro(FunctionRareData_internalFunctionAllocationProfile_structureID, FunctionRareData::offsetOfInternalFunctionAllocationProfile() + InternalFunctionAllocationProfile::offsetOfStructureID(), Mutability::Mutable) \
    macro(GetterSetter_getter, GetterSetter::offsetOfGetter(), Mutability::Mutable) \
    macro(GetterSetter_setter, GetterSetter::offsetOfSetter(), Mutability::Mutable) \
    macro(JSArrayBufferView_byteOffset, JSArrayBufferView::offsetOfByteOffset(), Mutability::Mutable) \
    macro(JSArrayBufferView_length, JSArrayBufferView::offsetOfLength(), Mutability::Mutable) \
    macro(JSArrayBufferView_mode, JSArrayBufferView::offsetOfMode(), Mutability::Mutable) \
    macro(JSArrayBufferView_vector, JSArrayBufferView::offsetOfVector(), Mutability::Mutable) \
    macro(JSBigInt_length, JSBigInt::offsetOfLength(), Mutability::Immutable) \
    macro(JSBoundFunction_targetFunction, JSBoundFunction::offsetOfTargetFunction(), Mutability::Mutable) \
    macro(JSBoundFunction_boundThis, JSBoundFunction::offsetOfBoundThis(), Mutability::Mutable) \
    macro(JSBoundFunction_boundArg0, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 0, Mutability::Mutable) \
    macro(JSBoundFunction_boundArg1, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 1, Mutability::Mutable) \
    macro(JSBoundFunction_boundArg2, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 2, Mutability::Mutable) \
    macro(JSBoundFunction_nameMayBeNull, JSBoundFunction::offsetOfNameMayBeNull(), Mutability::Mutable) \
    macro(JSBoundFunction_length, JSBoundFunction::offsetOfLength(), Mutability::Mutable) \
    macro(JSBoundFunction_boundArgsLength, JSBoundFunction::offsetOfBoundArgsLength(), Mutability::Mutable) \
    macro(JSBoundFunction_canConstruct, JSBoundFunction::offsetOfCanConstruct(), Mutability::Mutable) \
    macro(JSCallee_scope, JSCallee::offsetOfScopeChain(), Mutability::Mutable) \
    macro(JSCell_cellState, JSCell::cellStateOffset(), Mutability::Mutable) \
    macro(JSCell_header, 0, Mutability::Mutable) \
    macro(JSCell_indexingTypeAndMisc, JSCell::indexingTypeAndMiscOffset(), Mutability::Mutable) \
    macro(JSCell_structureID, JSCell::structureIDOffset(), Mutability::Mutable) \
    macro(JSCell_typeInfoFlags, JSCell::typeInfoFlagsOffset(), Mutability::Mutable) \
    macro(JSCell_typeInfoType, JSCell::typeInfoTypeOffset(), Mutability::Immutable) \
    macro(JSCell_usefulBytes, JSCell::indexingTypeAndMiscOffset(), Mutability::Mutable) \
    macro(JSFunction_executableOrRareData, JSFunction::offsetOfExecutableOrRareData(), Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_lastRegExp, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfLastRegExp(), Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_lastInput, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfLastInput(), Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_result_start, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, start), Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_result_end, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, end), Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_reified, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfReified(), Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_oneCharacterMatch, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfOneCharacterMatch(), Mutability::Mutable) \
    macro(JSGlobalProxy_target, JSGlobalProxy::targetOffset(), Mutability::Mutable) \
    macro(JSObject_butterfly, JSObject::butterflyOffset(), Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_cachedInlineCapacity, JSPropertyNameEnumerator::cachedInlineCapacityOffset(), Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_cachedPropertyNamesVector, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset(), Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_cachedStructureID, JSPropertyNameEnumerator::cachedStructureIDOffset(), Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_endGenericPropertyIndex, JSPropertyNameEnumerator::endGenericPropertyIndexOffset(), Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_endStructurePropertyIndex, JSPropertyNameEnumerator::endStructurePropertyIndexOffset(), Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_indexLength, JSPropertyNameEnumerator::indexedLengthOffset(), Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_flags, JSPropertyNameEnumerator::flagsOffset(), Mutability::Mutable) \
    macro(JSRopeString_flags, JSRopeString::offsetOfFlags(), Mutability::Mutable) \
    macro(JSRopeString_length, JSRopeString::offsetOfLength(), Mutability::Immutable) \
    macro(JSRopeString_fiber0, JSRopeString::offsetOfFiber0(), Mutability::Mutable) \
    macro(JSRopeString_fiber1, JSRopeString::offsetOfFiber1(), Mutability::Mutable) \
    macro(JSRopeString_fiber2, JSRopeString::offsetOfFiber2(), Mutability::Mutable) \
    macro(JSScope_next, JSScope::offsetOfNext(), Mutability::Immutable) \
    macro(JSSymbolTableObject_symbolTable, JSSymbolTableObject::offsetOfSymbolTable(), Mutability::Mutable) \
    macro(JSWebAssemblyArray_size, JSWebAssemblyArray::offsetOfSize(), Mutability::Immutable) \
    macro(JSWebAssemblyInstance_cachedMemorySize, JSWebAssemblyInstance::offsetOfCachedMemorySize(), Mutability::Mutable) \
    macro(JSWebAssemblyInstance_cachedTable0Buffer, JSWebAssemblyInstance::offsetOfCachedTable0Buffer(), Mutability::Mutable) \
    macro(JSWebAssemblyInstance_cachedTable0Length, JSWebAssemblyInstance::offsetOfCachedTable0Length(), Mutability::Mutable) \
    macro(JSWebAssemblyInstance_moduleRecord, JSWebAssemblyInstance::offsetOfModuleRecord(), Mutability::Mutable) \
    macro(JSWebAssemblyInstance_vm, JSWebAssemblyInstance::offsetOfVM(), Mutability::Immutable) \
    macro(NativeExecutable_asString, NativeExecutable::offsetOfAsString(), Mutability::Mutable) \
    macro(RegExpObject_regExpAndFlags, RegExpObject::offsetOfRegExpAndFlags(), Mutability::Mutable) \
    macro(RegExpObject_lastIndex, RegExpObject::offsetOfLastIndex(), Mutability::Mutable) \
    macro(ShadowChicken_Packet_callee, OBJECT_OFFSETOF(ShadowChicken::Packet, callee), Mutability::Mutable) \
    macro(ShadowChicken_Packet_frame, OBJECT_OFFSETOF(ShadowChicken::Packet, frame), Mutability::Mutable) \
    macro(ShadowChicken_Packet_callerFrame, OBJECT_OFFSETOF(ShadowChicken::Packet, callerFrame), Mutability::Mutable) \
    macro(ShadowChicken_Packet_thisValue, OBJECT_OFFSETOF(ShadowChicken::Packet, thisValue), Mutability::Mutable) \
    macro(ShadowChicken_Packet_scope, OBJECT_OFFSETOF(ShadowChicken::Packet, scope), Mutability::Mutable) \
    macro(ShadowChicken_Packet_codeBlock, OBJECT_OFFSETOF(ShadowChicken::Packet, codeBlock), Mutability::Mutable) \
    macro(ShadowChicken_Packet_callSiteIndex, OBJECT_OFFSETOF(ShadowChicken::Packet, callSiteIndex), Mutability::Mutable) \
    macro(ScopedArguments_overrodeThings, ScopedArguments::offsetOfOverrodeThings(), Mutability::Mutable) \
    macro(ScopedArguments_scope, ScopedArguments::offsetOfScope(), Mutability::Mutable) \
    macro(ScopedArguments_storage, ScopedArguments::offsetOfStorage(), Mutability::Mutable) \
    macro(ScopedArguments_table, ScopedArguments::offsetOfTable(), Mutability::Mutable) \
    macro(ScopedArguments_totalLength, ScopedArguments::offsetOfTotalLength(), Mutability::Mutable) \
    macro(ScopedArgumentsTable_arguments, ScopedArgumentsTable::offsetOfArguments(), Mutability::Mutable) \
    macro(ScopedArgumentsTable_length, ScopedArgumentsTable::offsetOfLength(), Mutability::Mutable) \
    macro(StringImpl_data, StringImpl::dataOffset(), Mutability::Immutable) \
    macro(StringImpl_hashAndFlags, StringImpl::flagsOffset(), Mutability::Mutable) \
    macro(StringImpl_length, StringImpl::lengthMemoryOffset(), Mutability::Immutable) \
    macro(Structure_bitField, Structure::bitFieldOffset(), Mutability::Mutable) \
    macro(Structure_classInfo, Structure::classInfoOffset(), Mutability::Immutable) \
    macro(Structure_globalObject, Structure::globalObjectOffset(), Mutability::Immutable) \
    macro(Structure_indexingModeIncludingHistory, Structure::indexingModeIncludingHistoryOffset(), Mutability::Immutable) \
    macro(Structure_inlineCapacity, Structure::inlineCapacityOffset(), Mutability::Immutable) \
    macro(Structure_outOfLineTypeFlags, Structure::outOfLineTypeFlagsOffset(), Mutability::Immutable) \
    macro(Structure_previousOrRareData, Structure::previousOrRareDataOffset(), Mutability::Mutable) \
    macro(Structure_propertyHash, Structure::propertyHashOffset(), Mutability::Mutable) \
    macro(Structure_prototype, Structure::prototypeOffset(), Mutability::Immutable) \
    macro(Structure_seenProperties, Structure::seenPropertiesOffset(), Mutability::Mutable) \
    macro(StructureRareData_cachedEnumerableStrings, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::EnumerableStrings), Mutability::Mutable) \
    macro(StructureRareData_cachedStrings, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::Strings), Mutability::Mutable) \
    macro(StructureRareData_cachedSymbols, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::Symbols), Mutability::Mutable) \
    macro(StructureRareData_cachedStringsAndSymbols, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::StringsAndSymbols), Mutability::Mutable) \
    macro(StructureRareData_cachedPropertyNameEnumeratorAndFlag, StructureRareData::offsetOfCachedPropertyNameEnumeratorAndFlag(), Mutability::Mutable) \
    macro(StructureRareData_specialPropertyCache, StructureRareData::offsetOfSpecialPropertyCache(), Mutability::Mutable) \
    macro(SpecialPropertyCache_cachedToStringTagValue, SpecialPropertyCache::offsetOfCache(CachedSpecialPropertyKey::ToStringTag) + SpecialPropertyCacheEntry::offsetOfValue(), Mutability::Mutable) \
    macro(JSMap_storage, (JSMap::offsetOfStorage()), Mutability::Mutable) \
    macro(JSSet_storage, (JSSet::offsetOfStorage()), Mutability::Mutable) \
    macro(VM_heap_barrierThreshold, VM::offsetOfHeapBarrierThreshold(), Mutability::Mutable) \
    macro(VM_heap_mutatorShouldBeFenced, VM::offsetOfHeapMutatorShouldBeFenced(), Mutability::Mutable) \
    macro(VM_exception, VM::exceptionOffset(), Mutability::Mutable) \
    macro(WatchpointSet_state, WatchpointSet::offsetOfState(), Mutability::Mutable) \
    macro(WasmFuncRefTable_functions, Wasm::FuncRefTable::offsetOfFunctions(), Mutability::Mutable) \
    macro(WasmFuncRefTableFunction_boxedCallee, Wasm::FuncRefTable::Function::offsetOfFunction() + Wasm::WasmToWasmImportableFunction::offsetOfBoxedCallee(), Mutability::Mutable) \
    macro(WasmFuncRefTableFunction_entrypointLoadLocation, Wasm::FuncRefTable::Function::offsetOfFunction() + Wasm::WasmToWasmImportableFunction::offsetOfEntrypointLoadLocation(), Mutability::Mutable) \
    macro(WasmFuncRefTableFunction_rtt, Wasm::FuncRefTable::Function::offsetOfFunction() + Wasm::WasmToWasmImportableFunction::offsetOfRTT(), Mutability::Mutable) \
    macro(WasmFuncRefTableFunction_targetInstance, Wasm::FuncRefTable::Function::offsetOfFunction() + Wasm::WasmToWasmImportableFunction::offsetOfTargetInstance(), Mutability::Mutable) \
    macro(WasmGlobal_value, Wasm::Global::offsetOfValue(), Mutability::Mutable) \
    macro(WasmGlobal_owner, Wasm::Global::offsetOfOwner(), Mutability::Immutable) \
    macro(WasmGlobalValue_owner, Wasm::Global::Value::offsetOfOwner(), Mutability::Immutable) \
    macro(WasmGlobalValue_value, Wasm::Global::Value::offsetOfValue(), Mutability::Mutable) \
    macro(WasmWasmCallableFunctionLocation_value, Wasm::WasmCallableFunction::offsetOfValueOfLoadLocation(), Mutability::Mutable) \
    macro(WasmRTT_displaySizeExcludingThis, Wasm::RTT::offsetOfDisplaySizeExcludingThis(), Mutability::Immutable) \
    macro(WasmRTT_kind, Wasm::RTT::offsetOfKind(), Mutability::Immutable) \
    macro(WasmTable_length, Wasm::Table::offsetOfLength(), Mutability::Mutable) \
    macro(WeakMapImpl_capacity, WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfCapacity(), Mutability::Mutable) \
    macro(WeakMapImpl_buffer,  WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfBuffer(), Mutability::Mutable) \
    macro(WeakMapBucket_value, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfValue(), Mutability::Mutable) \
    macro(WeakMapBucket_key, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfKey(), Mutability::Mutable) \
    macro(WebAssemblyFunctionBase_boxedCallee, WebAssemblyFunctionBase::offsetOfBoxedCallee(), Mutability::Immutable) \
    macro(WebAssemblyFunctionBase_entrypointLoadLocation, WebAssemblyFunctionBase::offsetOfEntrypointLoadLocation(), Mutability::Immutable) \
    macro(WebAssemblyFunctionBase_rtt, WebAssemblyFunctionBase::offsetOfRTT(), Mutability::Immutable) \
    macro(WebAssemblyFunctionBase_targetInstance, WebAssemblyFunctionBase::offsetOfTargetInstance(), Mutability::Immutable) \
    macro(WebAssemblyGCObjectBase_rtt, WebAssemblyGCObjectBase::offsetOfRTT(), Mutability::Immutable) \
    macro(WebAssemblyGCStructure_rtt, WebAssemblyGCStructure::offsetOfRTT(), Mutability::Immutable) \
    macro(WebAssemblyModuleRecord_exportsObject, WebAssemblyModuleRecord::offsetOfExportsObject(), Mutability::Mutable) \
    macro(Symbol_symbolImpl, Symbol::offsetOfSymbolImpl(), Mutability::Immutable) \

#define FOR_EACH_INDEXED_ABSTRACT_HEAP(macro) \
    macro(ArrayStorage_vector, ArrayStorage::vectorOffset(), sizeof(WriteBarrier<Unknown>)) \
    macro(CompleteSubspace_allocatorForSizeStep, CompleteSubspace::offsetOfAllocatorForSizeStep(), sizeof(Allocator)) \
    macro(DirectArguments_storage, DirectArguments::storageOffset(), sizeof(EncodedJSValue)) \
    macro(JSLexicalEnvironment_variables, JSLexicalEnvironment::offsetOfVariables(), sizeof(EncodedJSValue)) \
    macro(JSPropertyNameEnumerator_cachedPropertyNamesVectorContents, 0, sizeof(WriteBarrier<JSString>)) \
    macro(JSInternalFieldObjectImpl_internalFields, JSInternalFieldObjectImpl<>::offsetOfInternalFields(), sizeof(WriteBarrier<Unknown>)) \
    macro(ScopedArguments_Storage_storage, 0, sizeof(EncodedJSValue)) \
    macro(WriteBarrierBuffer_bufferContents, 0, sizeof(JSCell*)) \
    macro(characters8, 0, sizeof(Latin1Character)) \
    macro(characters16, 0, sizeof(char16_t)) \
    macro(indexedInt32Properties, 0, sizeof(EncodedJSValue)) \
    macro(indexedDoubleProperties, 0, sizeof(double)) \
    macro(indexedContiguousProperties, 0, sizeof(EncodedJSValue)) \
    macro(scopedArgumentsTableArguments, 0, sizeof(int32_t)) \
    macro(singleCharacterStrings, 0, sizeof(JSString*)) \
    macro(structureTable, 0, sizeof(Structure*)) \
    macro(variables, 0, sizeof(Register)) \
    macro(HasOwnPropertyCache, 0, sizeof(HasOwnPropertyCache::Entry)) \
    macro(SmallIntCache, 0, sizeof(NumericStrings::StringWithJSString)) \
    macro(WasmRTT_data, Wasm::RTT::offsetOfData(), sizeof(RefPtr<const Wasm::RTT>)) \

#define FOR_EACH_NUMBERED_ABSTRACT_HEAP(macro) \
    macro(properties) \
    /* WasmGC Struct access are analyzed via field index and field type. We can include Wasm type definition to further make alias analysis better. */ \
    macro(JSWebAssemblyStruct_fields) \
    /* WasmGC Array access are analyzed via element index and element type. Not using IndexedAbstractHeap right now intentionally since large Wasm array has different base offset. */ \
    macro(JSWebAssemblyArray_i8) \
    macro(JSWebAssemblyArray_i16) \
    macro(JSWebAssemblyArray_i32) \
    macro(JSWebAssemblyArray_i64) \
    macro(JSWebAssemblyArray_f32) \
    macro(JSWebAssemblyArray_f64) \
    macro(JSWebAssemblyArray_v128) \
    macro(JSWebAssemblyArray_ref) \
    /* Embedded WasmGlobal access are analyzed via index and element type. */ \
    macro(JSWebAssemblyInstance_embeddedGlobals_i32) \
    macro(JSWebAssemblyInstance_embeddedGlobals_i64) \
    macro(JSWebAssemblyInstance_embeddedGlobals_f32) \
    macro(JSWebAssemblyInstance_embeddedGlobals_f64) \
    macro(JSWebAssemblyInstance_embeddedGlobals_v128) \
    macro(JSWebAssemblyInstance_embeddedGlobals_ref) \
    \
    macro(JSWebAssemblyInstance_portableGlobals) \
    /* WasmGC structure access are analyzed via type index */ \
    macro(JSWebAssemblyInstance_gcObjectStructureIDs) \
    macro(JSWebAssemblyInstance_importFunctionStubs) \
    macro(JSWebAssemblyInstance_tables) \

// This class is meant to be cacheable between compilations, but it doesn't have to be.
// Doing so saves on creation of nodes. But clearing it will save memory.

class AbstractHeapRepository {
    WTF_MAKE_NONCOPYABLE(AbstractHeapRepository);
public:
    AbstractHeapRepository();
    ~AbstractHeapRepository();

    AbstractHeap root;

#define ABSTRACT_HEAP_DECLARATION(name) AbstractHeap name;
    FOR_EACH_ABSTRACT_HEAP(ABSTRACT_HEAP_DECLARATION)
#undef ABSTRACT_HEAP_DECLARATION

    AbstractHeap& WebAssemblyMemory;

#define ABSTRACT_FIELD_DECLARATION(name, offset, mutability) AbstractHeap name;
    FOR_EACH_ABSTRACT_FIELD(ABSTRACT_FIELD_DECLARATION)
#undef ABSTRACT_FIELD_DECLARATION

    AbstractHeap& JSCell_freeListNext;
    AbstractHeap& ArrayStorage_publicLength;
    AbstractHeap& ArrayStorage_vectorLength;

#define INDEXED_ABSTRACT_HEAP_DECLARATION(name, offset, size) IndexedAbstractHeap name;
    FOR_EACH_INDEXED_ABSTRACT_HEAP(INDEXED_ABSTRACT_HEAP_DECLARATION)
#undef INDEXED_ABSTRACT_HEAP_DECLARATION

#define NUMBERED_ABSTRACT_HEAP_DECLARATION(name) NumberedAbstractHeap name;
    FOR_EACH_NUMBERED_ABSTRACT_HEAP(NUMBERED_ABSTRACT_HEAP_DECLARATION)
#undef NUMBERED_ABSTRACT_HEAP_DECLARATION

    AbstractHeap& JSString_value;
    AbstractHeap& JSWrapperObject_internalValue;

    AbsoluteAbstractHeap absolute;

    IndexedAbstractHeap* forIndexingType(IndexingType indexingType)
    {
        switch (indexingType) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            return nullptr;

        case ALL_INT32_INDEXING_TYPES:
            return &indexedInt32Properties;

        case ALL_DOUBLE_INDEXING_TYPES:
            return &indexedDoubleProperties;

        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return &indexedContiguousProperties;

        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return &ArrayStorage_vector;

        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    IndexedAbstractHeap& forArrayType(DFG::Array::Type type)
    {
        switch (type) {
        case DFG::Array::Int32:
            return indexedInt32Properties;
        case DFG::Array::Double:
            return indexedDoubleProperties;
        case DFG::Array::Contiguous:
            return indexedContiguousProperties;
        case DFG::Array::ArrayStorage:
        case DFG::Array::SlowPutArrayStorage:
            return ArrayStorage_vector;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return indexedInt32Properties;
        }
    }

    void decorateMemory(const AbstractHeap*, Value*);
    void decorateCCallRead(const AbstractHeap*, Value*);
    void decorateCCallWrite(const AbstractHeap*, Value*);
    void decoratePatchpointRead(const AbstractHeap*, Value*);
    void decoratePatchpointWrite(const AbstractHeap*, Value*);
    void decorateFenceRead(const AbstractHeap*, Value*);
    void decorateFenceWrite(const AbstractHeap*, Value*);
    void decorateFencedAccess(const AbstractHeap*, Value*);
    void decorateWasmStructGet(const AbstractHeap*, Value*);
    void decorateWasmStructSet(const AbstractHeap*, Value*);

    void computeRangesAndDecorateInstructions();

private:

    struct HeapForValue {
        HeapForValue()
        {
        }

        HeapForValue(const AbstractHeap* heap, Value* value)
            : heap(heap)
            , value(value)
        {
        }

        const AbstractHeap* heap { nullptr };
        Value* value { nullptr };
    };

    Vector<HeapForValue> m_heapForMemory;
    Vector<HeapForValue> m_heapForCCallRead;
    Vector<HeapForValue> m_heapForCCallWrite;
    Vector<HeapForValue> m_heapForPatchpointRead;
    Vector<HeapForValue> m_heapForPatchpointWrite;
    Vector<HeapForValue> m_heapForFenceRead;
    Vector<HeapForValue> m_heapForFenceWrite;
    Vector<HeapForValue> m_heapForFencedAccess;
    Vector<HeapForValue> m_heapForWasmStructGet;
    Vector<HeapForValue> m_heapForWasmStructSet;
};

} // namespace JSC::B3

#endif // ENABLE(B3_JIT)
