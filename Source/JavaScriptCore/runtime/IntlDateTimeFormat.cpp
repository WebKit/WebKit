/*
 * Copyright (C) 2015 Andy VanWagoner (thetalecrafter@gmail.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "IntlDateTimeFormat.h"

#if ENABLE(INTL)

#include "DateConstructor.h"
#include "DateInstance.h"
#include "Error.h"
#include "IntlDateTimeFormatConstructor.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo IntlDateTimeFormat::s_info = { "Object", &Base::s_info, 0, CREATE_METHOD_TABLE(IntlDateTimeFormat) };

IntlDateTimeFormat* IntlDateTimeFormat::create(VM& vm, IntlDateTimeFormatConstructor* constructor)
{
    IntlDateTimeFormat* format = new (NotNull, allocateCell<IntlDateTimeFormat>(vm.heap)) IntlDateTimeFormat(vm, constructor->dateTimeFormatStructure());
    format->finishCreation(vm);
    return format;
}

Structure* IntlDateTimeFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDateTimeFormat::IntlDateTimeFormat(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
{
}

void IntlDateTimeFormat::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

void IntlDateTimeFormat::destroy(JSCell* cell)
{
    static_cast<IntlDateTimeFormat*>(cell)->IntlDateTimeFormat::~IntlDateTimeFormat();
}

void IntlDateTimeFormat::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlDateTimeFormat* thisObject = jsCast<IntlDateTimeFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_boundFormat);
}

void IntlDateTimeFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatFuncFormatDateTime(ExecState* exec)
{
    // 12.3.4 DateTime Format Functions (ECMA-402 2.0)
    // 1. Let dtf be the this value.
    IntlDateTimeFormat* format = jsDynamicCast<IntlDateTimeFormat*>(exec->thisValue());
    // 2. Assert: Type(dtf) is Object and dtf has an [[initializedDateTimeFormat]] internal slot whose value is true.
    if (!format)
        return JSValue::encode(throwTypeError(exec));

    JSValue date = exec->argument(0);
    double value;

    // 3. If date is not provided or is undefined, then
    if (date.isUndefined()) {
        // a. Let x be %Date_now%().
        value = JSValue::decode(dateNow(exec)).toNumber(exec);
    } else {
        // 4. Else
        // a. Let x be ToNumber(date).
        value = date.toNumber(exec);
        // b. ReturnIfAbrupt(x).
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
    }

    // 5. Return FormatDateTime(dtf, x).

    // 12.3.4 FormatDateTime abstract operation (ECMA-402 2.0)

    // 1. If x is not a finite Number, then throw a RangeError exception.
    if (!std::isfinite(value))
        return JSValue::encode(throwRangeError(exec, ASCIILiteral("date value is not finite in DateTimeFormat.format()")));

    // FIXME: implement 2 - 9

    // Return new Date(value).toString() until properly implemented.
    VM& vm = exec->vm();
    JSGlobalObject* globalObject = exec->callee()->globalObject();
    DateInstance* d = DateInstance::create(vm, globalObject->dateStructure(), value);
    return JSValue::encode(JSValue(d).toString(exec));
}

} // namespace JSC

#endif // ENABLE(INTL)
