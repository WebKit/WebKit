/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2007-2019 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "MathObject.h"

#include "JSCInlines.h"
#include "MathCommon.h"
#include "ObjectPrototype.h"
#include <time.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(MathObject);

EncodedJSValue JSC_HOST_CALL mathProtoFuncACos(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncACosh(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncASin(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncASinh(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncATan(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncATanh(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncATan2(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncCbrt(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncCeil(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncClz32(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncCos(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncCosh(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncExp(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncExpm1(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncFround(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncHypot(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncLog(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncLog1p(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncLog10(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncLog2(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncMax(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncMin(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncPow(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncRandom(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncRound(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncSign(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncSin(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncSinh(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncSqrt(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncTan(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncTanh(ExecState*);
EncodedJSValue JSC_HOST_CALL mathProtoFuncIMul(ExecState*);

const ClassInfo MathObject::s_info = { "Math", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(MathObject) };

MathObject::MathObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void MathObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));

    putDirectWithoutTransition(vm, Identifier::fromString(vm, "E"), jsNumber(Math::exp(1.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LN2"), jsNumber(Math::log(2.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LN10"), jsNumber(Math::log(10.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LOG2E"), jsNumber(1.0 / Math::log(2.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LOG10E"), jsNumber(0.4342944819032518), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "PI"), jsNumber(piDouble), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "SQRT1_2"), jsNumber(sqrt(0.5)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "SQRT2"), jsNumber(sqrt(2.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsString(vm, "Math"), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "abs"), 1, mathProtoFuncAbs, AbsIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "acos"), 1, mathProtoFuncACos, ACosIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "asin"), 1, mathProtoFuncASin, ASinIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "atan"), 1, mathProtoFuncATan, ATanIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "acosh"), 1, mathProtoFuncACosh, ACoshIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "asinh"), 1, mathProtoFuncASinh, ASinhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "atanh"), 1, mathProtoFuncATanh, ATanhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "atan2"), 2, mathProtoFuncATan2, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "cbrt"), 1, mathProtoFuncCbrt, CbrtIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "ceil"), 1, mathProtoFuncCeil, CeilIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "clz32"), 1, mathProtoFuncClz32, Clz32Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "cos"), 1, mathProtoFuncCos, CosIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "cosh"), 1, mathProtoFuncCosh, CoshIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "exp"), 1, mathProtoFuncExp, ExpIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "expm1"), 1, mathProtoFuncExpm1, Expm1Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "floor"), 1, mathProtoFuncFloor, FloorIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "fround"), 1, mathProtoFuncFround, FRoundIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "hypot"), 2, mathProtoFuncHypot, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log"), 1, mathProtoFuncLog, LogIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log10"), 1, mathProtoFuncLog10, Log10Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log1p"), 1, mathProtoFuncLog1p, Log1pIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log2"), 1, mathProtoFuncLog2, Log2Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "max"), 2, mathProtoFuncMax, MaxIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "min"), 2, mathProtoFuncMin, MinIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "pow"), 2, mathProtoFuncPow, PowIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "random"), 0, mathProtoFuncRandom, RandomIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "round"), 1, mathProtoFuncRound, RoundIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sign"), 1, mathProtoFuncSign, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sin"), 1, mathProtoFuncSin, SinIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sinh"), 1, mathProtoFuncSinh, SinhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sqrt"), 1, mathProtoFuncSqrt, SqrtIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "tan"), 1, mathProtoFuncTan, TanIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "tanh"), 1, mathProtoFuncTanh, TanhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "trunc"), 1, mathProtoFuncTrunc, TruncIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "imul"), 2, mathProtoFuncIMul, IMulIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// ------------------------------ Functions --------------------------------

EncodedJSValue JSC_HOST_CALL mathProtoFuncAbs(ExecState* exec)
{
    return JSValue::encode(jsNumber(fabs(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncACos(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::acos(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncASin(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::asin(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncATan(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::atan(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncATan2(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    double arg0 = exec->argument(0).toNumber(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    double arg1 = exec->argument(1).toNumber(exec);
    return JSValue::encode(jsDoubleNumber(atan2(arg0, arg1)));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncCeil(ExecState* exec)
{
    return JSValue::encode(jsNumber(ceil(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncClz32(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint32_t value = exec->argument(0).toUInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(JSValue(clz(value)));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncCos(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::cos(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncExp(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::exp(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncFloor(ExecState* exec)
{
    return JSValue::encode(jsNumber(floor(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncHypot(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned argsCount = exec->argumentCount();
    double max = 0;
    Vector<double, 8> args;
    args.reserveInitialCapacity(argsCount);
    for (unsigned i = 0; i < argsCount; ++i) {
        args.uncheckedAppend(exec->uncheckedArgument(i).toNumber(exec));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (std::isinf(args[i]))
            return JSValue::encode(jsDoubleNumber(+std::numeric_limits<double>::infinity()));
        max = std::max(fabs(args[i]), max);
    }
    if (!max)
        max = 1;
    // Kahan summation algorithm significantly reduces the numerical error in the total obtained.
    double sum = 0;
    double compensation = 0;
    for (double argument : args) {
        double scaledArgument = argument / max;
        double summand = scaledArgument * scaledArgument - compensation;
        double preliminary = sum + summand;
        compensation = (preliminary - sum) - summand;
        sum = preliminary;
    }
    return JSValue::encode(jsDoubleNumber(sqrt(sum) * max));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncLog(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::log(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncMax(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned argsCount = exec->argumentCount();
    double result = -std::numeric_limits<double>::infinity();
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = exec->uncheckedArgument(k).toNumber(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (std::isnan(val)) {
            result = PNaN;
        } else if (val > result || (!val && !result && !std::signbit(val)))
            result = val;
    }
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncMin(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned argsCount = exec->argumentCount();
    double result = +std::numeric_limits<double>::infinity();
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = exec->uncheckedArgument(k).toNumber(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (std::isnan(val)) {
            result = PNaN;
        } else if (val < result || (!val && !result && std::signbit(val)))
            result = val;
    }
    return JSValue::encode(jsNumber(result));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncPow(ExecState* exec)
{
    // ECMA 15.8.2.1.13

    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double arg = exec->argument(0).toNumber(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    double arg2 = exec->argument(1).toNumber(exec);

    return JSValue::encode(JSValue(operationMathPow(arg, arg2)));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncRandom(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(exec->lexicalGlobalObject()->weakRandomNumber()));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncRound(ExecState* exec)
{
    return JSValue::encode(jsNumber(jsRound(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncSign(ExecState* exec)
{
    double arg = exec->argument(0).toNumber(exec);
    if (std::isnan(arg))
        return JSValue::encode(jsNaN());
    if (!arg)
        return JSValue::encode(std::signbit(arg) ? jsNumber(-0.0) : jsNumber(0));
    return JSValue::encode(jsNumber(std::signbit(arg) ? -1 : 1));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncSin(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::sin(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncSqrt(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(sqrt(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncTan(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::tan(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncIMul(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    int32_t left = exec->argument(0).toInt32(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    int32_t right = exec->argument(1).toInt32(exec);
    return JSValue::encode(jsNumber(left * right));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncACosh(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::acosh(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncASinh(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::asinh(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncATanh(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::atanh(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncCbrt(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::cbrt(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncCosh(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::cosh(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncExpm1(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::expm1(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncFround(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(static_cast<float>(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncLog1p(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::log1p(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncLog10(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::log10(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncLog2(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::log2(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncSinh(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::sinh(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncTanh(ExecState* exec)
{
    return JSValue::encode(jsDoubleNumber(Math::tanh(exec->argument(0).toNumber(exec))));
}

EncodedJSValue JSC_HOST_CALL mathProtoFuncTrunc(ExecState*exec)
{
    return JSValue::encode(jsNumber(exec->argument(0).toIntegerPreserveNaN(exec)));
}

} // namespace JSC
