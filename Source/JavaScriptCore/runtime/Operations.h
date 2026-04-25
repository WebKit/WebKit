/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2002-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/JSExportMacros.h>
#include <wtf/Forward.h>
#include <wtf/TriState.h>

namespace JSC {

class JSBigInt;
class JSCell;
class JSGlobalObject;
class JSString;
class JSValue;
class Register;
class VM;

enum class Concurrency : uint8_t;
enum class JSBigIntComparisonMode : uint8_t;
enum class JSBigIntComparisonResult : uint8_t;

typedef int64_t EncodedJSValue;

#define InvalidPrototypeChain (std::numeric_limits<size_t>::max())

NEVER_INLINE JSValue jsAddSlowCase(JSGlobalObject*, JSValue, JSValue);
JSString* jsTypeStringForValueWithConcurrency(VM&, JSGlobalObject*, JSValue, Concurrency);
size_t normalizePrototypeChain(JSGlobalObject*, JSCell*, bool& sawPolyProto);

// All inline function definitions are in OperationsInlines.h
template<Concurrency concurrency>
TriState jsTypeofIsObjectWithConcurrency(JSGlobalObject*, JSValue);
template<Concurrency concurrency>
TriState jsTypeofIsFunctionWithConcurrency(JSGlobalObject*, JSValue);
JSString* jsTypeStringForValue(JSGlobalObject*, JSValue);
bool jsTypeofIsObject(JSGlobalObject*, JSValue);
bool jsTypeofIsFunction(JSGlobalObject*, JSValue);

JSString* jsString(JSGlobalObject*, const String&, JSString*);
JSString* jsString(JSGlobalObject*, JSString*, const String&);
JSString* jsString(JSGlobalObject*, JSString*, JSString*);
JSString* jsString(JSGlobalObject*, JSString*, JSString*, JSString*);
JSString* jsString(JSGlobalObject*, const String&, const String&);
JSString* jsString(JSGlobalObject*, const String&, const String&, const String&);
JSValue jsStringFromRegisterArray(JSGlobalObject*, Register*, unsigned count);

JSBigIntComparisonResult compareBigInt(JSValue, JSValue);
JSBigIntComparisonResult compareBigIntToOtherPrimitive(JSGlobalObject*, JSBigInt*, JSValue);
JSBigIntComparisonResult compareBigInt32ToOtherPrimitive(JSGlobalObject*, int32_t, JSValue);
bool bigIntCompareResult(JSBigIntComparisonResult, JSBigIntComparisonMode);
bool bigIntCompare(JSGlobalObject*, JSValue, JSValue, JSBigIntComparisonMode);
bool toPrimitiveNumeric(JSGlobalObject*, JSValue, JSValue&, double&);

template<bool leftFirst>
bool jsLess(JSGlobalObject*, JSValue, JSValue);
template<bool leftFirst>
bool jsLessEq(JSGlobalObject*, JSValue, JSValue);

JSValue jsAddNonNumber(JSGlobalObject*, JSValue, JSValue);
JSValue jsAdd(JSGlobalObject*, JSValue, JSValue);

template<typename DoubleOperation, typename BigIntOp>
JSValue arithmeticBinaryOp(JSGlobalObject*, JSValue, JSValue, DoubleOperation&&, BigIntOp&&, ASCIILiteral);

JSValue jsSub(JSGlobalObject*, JSValue, JSValue);
JSValue jsMul(JSGlobalObject*, JSValue, JSValue);
JSValue jsDiv(JSGlobalObject*, JSValue, JSValue);
JSValue jsRemainder(JSGlobalObject*, JSValue, JSValue);
JSValue jsPow(JSGlobalObject*, JSValue, JSValue);
JSValue jsInc(JSGlobalObject*, JSValue);
JSValue jsDec(JSGlobalObject*, JSValue);
JSValue jsBitwiseNot(JSGlobalObject*, JSValue);

template <bool isLeft>
JSValue shift(JSGlobalObject*, JSValue, JSValue);
JSValue jsLShift(JSGlobalObject*, JSValue, JSValue);
JSValue jsRShift(JSGlobalObject*, JSValue, JSValue);
JSValue jsURShift(JSGlobalObject*, JSValue, JSValue);

template<typename Int32Operation, typename BigIntOp>
JSValue bitwiseBinaryOp(JSGlobalObject*, JSValue, JSValue, Int32Operation&&, BigIntOp&&, ASCIILiteral);
JSValue jsBitwiseAnd(JSGlobalObject*, JSValue, JSValue);
JSValue jsBitwiseOr(JSGlobalObject*, JSValue, JSValue);
JSValue jsBitwiseXor(JSGlobalObject*, JSValue, JSValue);

EncodedJSValue getByValWithIndexAndThis(JSGlobalObject*, JSCell*, uint32_t, JSValue);
EncodedJSValue getByValWithIndex(JSGlobalObject*, JSCell*, uint32_t);

} // namespace JSC
