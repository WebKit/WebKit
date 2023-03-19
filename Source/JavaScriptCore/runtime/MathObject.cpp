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
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(MathObject);

static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncACos);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncACosh);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncASin);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncASinh);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncATan);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncATanh);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncATan2);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncCbrt);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncCeil);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncClz32);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncCos);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncCosh);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncExp);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncExpm1);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncFround);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncHypot);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncLog);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncLog1p);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncLog10);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncLog2);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncMax);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncPow);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncRandom);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncRound);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncSign);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncSin);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncSinh);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncSqrt);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncTan);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncTanh);
static JSC_DECLARE_HOST_FUNCTION(mathProtoFuncIMul);

const ClassInfo MathObject::s_info = { "Math"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(MathObject) };

MathObject::MathObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void MathObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, Identifier::fromString(vm, "E"_s), jsNumber(Math::exp(1.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LN2"_s), jsNumber(Math::log(2.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LN10"_s), jsNumber(Math::log(10.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LOG2E"_s), jsNumber(1.0 / Math::log(2.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "LOG10E"_s), jsNumber(0.4342944819032518), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "PI"_s), jsNumber(piDouble), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "SQRT1_2"_s), jsNumber(sqrt(0.5)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, Identifier::fromString(vm, "SQRT2"_s), jsNumber(sqrt(2.0)), PropertyAttribute::DontDelete | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();

    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "abs"_s), 1, mathProtoFuncAbs, ImplementationVisibility::Public, AbsIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "acos"_s), 1, mathProtoFuncACos, ImplementationVisibility::Public, ACosIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "asin"_s), 1, mathProtoFuncASin, ImplementationVisibility::Public, ASinIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "atan"_s), 1, mathProtoFuncATan, ImplementationVisibility::Public, ATanIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "acosh"_s), 1, mathProtoFuncACosh, ImplementationVisibility::Public, ACoshIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "asinh"_s), 1, mathProtoFuncASinh, ImplementationVisibility::Public, ASinhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "atanh"_s), 1, mathProtoFuncATanh, ImplementationVisibility::Public, ATanhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "atan2"_s), 2, mathProtoFuncATan2, ImplementationVisibility::Public, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "cbrt"_s), 1, mathProtoFuncCbrt, ImplementationVisibility::Public, CbrtIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "ceil"_s), 1, mathProtoFuncCeil, ImplementationVisibility::Public, CeilIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "clz32"_s), 1, mathProtoFuncClz32, ImplementationVisibility::Public, Clz32Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "cos"_s), 1, mathProtoFuncCos, ImplementationVisibility::Public, CosIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "cosh"_s), 1, mathProtoFuncCosh, ImplementationVisibility::Public, CoshIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "exp"_s), 1, mathProtoFuncExp, ImplementationVisibility::Public, ExpIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "expm1"_s), 1, mathProtoFuncExpm1, ImplementationVisibility::Public, Expm1Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "floor"_s), 1, mathProtoFuncFloor, ImplementationVisibility::Public, FloorIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "fround"_s), 1, mathProtoFuncFround, ImplementationVisibility::Public, FRoundIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "hypot"_s), 2, mathProtoFuncHypot, ImplementationVisibility::Public, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log"_s), 1, mathProtoFuncLog, ImplementationVisibility::Public, LogIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log10"_s), 1, mathProtoFuncLog10, ImplementationVisibility::Public, Log10Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log1p"_s), 1, mathProtoFuncLog1p, ImplementationVisibility::Public, Log1pIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "log2"_s), 1, mathProtoFuncLog2, ImplementationVisibility::Public, Log2Intrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "max"_s), 2, mathProtoFuncMax, ImplementationVisibility::Public, MaxIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "min"_s), 2, mathProtoFuncMin, ImplementationVisibility::Public, MinIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "pow"_s), 2, mathProtoFuncPow, ImplementationVisibility::Public, PowIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "random"_s), 0, mathProtoFuncRandom, ImplementationVisibility::Public, RandomIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "round"_s), 1, mathProtoFuncRound, ImplementationVisibility::Public, RoundIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sign"_s), 1, mathProtoFuncSign, ImplementationVisibility::Public, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sin"_s), 1, mathProtoFuncSin, ImplementationVisibility::Public, SinIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sinh"_s), 1, mathProtoFuncSinh, ImplementationVisibility::Public, SinhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "sqrt"_s), 1, mathProtoFuncSqrt, ImplementationVisibility::Public, SqrtIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "tan"_s), 1, mathProtoFuncTan, ImplementationVisibility::Public, TanIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "tanh"_s), 1, mathProtoFuncTanh, ImplementationVisibility::Public, TanhIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "trunc"_s), 1, mathProtoFuncTrunc, ImplementationVisibility::Public, TruncIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunctionWithoutTransition(vm, globalObject, Identifier::fromString(vm, "imul"_s), 2, mathProtoFuncIMul, ImplementationVisibility::Public, IMulIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// ------------------------------ Functions --------------------------------

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncAbs, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsNumber(std::abs(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncACos, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::acos(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncASin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::asin(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncATan, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::atan(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncATan2, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    double arg0 = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    double arg1 = callFrame->argument(1).toNumber(globalObject);
    return JSValue::encode(jsDoubleNumber(atan2(arg0, arg1)));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncCeil, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsNumber(ceil(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncClz32, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    uint32_t value = callFrame->argument(0).toUInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(JSValue(clz(value)));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncCos, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::cos(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncExp, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::exp(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncFloor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsNumber(floor(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncHypot, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned argsCount = callFrame->argumentCount();
    Vector<double, 8> args;
    args.reserveInitialCapacity(argsCount);
    for (unsigned i = 0; i < argsCount; ++i) {
        double argument = callFrame->uncheckedArgument(i).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        args.uncheckedAppend(argument);
    }

    double max = 0;
    for (double argument : args) {
        if (std::isinf(argument))
            return JSValue::encode(jsDoubleNumber(+std::numeric_limits<double>::infinity()));
        max = std::max(std::abs(argument), max);
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

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncLog, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::log(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncMax, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned argsCount = callFrame->argumentCount();
    double result = -std::numeric_limits<double>::infinity();
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = callFrame->uncheckedArgument(k).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (std::isnan(val)) {
            result = PNaN;
        } else if (val > result || (!val && !result && !std::signbit(val)))
            result = val;
    }
    return JSValue::encode(jsNumber(result));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncMin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned argsCount = callFrame->argumentCount();
    double result = +std::numeric_limits<double>::infinity();
    for (unsigned k = 0; k < argsCount; ++k) {
        double val = callFrame->uncheckedArgument(k).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (std::isnan(val)) {
            result = PNaN;
        } else if (val < result || (!val && !result && std::signbit(val)))
            result = val;
    }
    return JSValue::encode(jsNumber(result));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncPow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    // ECMA 15.8.2.1.13

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    double arg = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    double arg2 = callFrame->argument(1).toNumber(globalObject);

    return JSValue::encode(JSValue(operationMathPow(arg, arg2)));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncRandom, (JSGlobalObject* globalObject, CallFrame*))
{
    return JSValue::encode(jsDoubleNumber(globalObject->weakRandomNumber()));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncRound, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsNumber(Math::roundDouble(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncSign, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    double arg = callFrame->argument(0).toNumber(globalObject);
    if (std::isnan(arg))
        return JSValue::encode(jsNaN());
    if (!arg)
        return JSValue::encode(std::signbit(arg) ? jsNumber(-0.0) : jsNumber(0));
    return JSValue::encode(jsNumber(std::signbit(arg) ? -1 : 1));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncSin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::sin(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncSqrt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(sqrt(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncTan, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::tan(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncIMul, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    int32_t left = callFrame->argument(0).toInt32(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    scope.release();
    int32_t right = callFrame->argument(1).toInt32(globalObject);
    return JSValue::encode(jsNumber(left * right));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncACosh, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::acosh(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncASinh, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::asinh(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncATanh, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::atanh(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncCbrt, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::cbrt(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncCosh, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::cosh(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncExpm1, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::expm1(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncFround, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(static_cast<float>(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncLog1p, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::log1p(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncLog10, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::log10(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncLog2, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::log2(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncSinh, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::sinh(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncTanh, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsDoubleNumber(Math::tanh(callFrame->argument(0).toNumber(globalObject))));
}

JSC_DEFINE_HOST_FUNCTION(mathProtoFuncTrunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(jsNumber(callFrame->argument(0).toIntegerPreserveNaN(globalObject)));
}

} // namespace JSC
