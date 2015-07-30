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
#include "IntlNumberFormat.h"

#if ENABLE(INTL)

#include "Error.h"
#include "IntlNumberFormatConstructor.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo IntlNumberFormat::s_info = { "Object", &Base::s_info, 0, CREATE_METHOD_TABLE(IntlNumberFormat) };

IntlNumberFormat* IntlNumberFormat::create(VM& vm, IntlNumberFormatConstructor* constructor)
{
    IntlNumberFormat* format = new (NotNull, allocateCell<IntlNumberFormat>(vm.heap)) IntlNumberFormat(vm, constructor->numberFormatStructure());
    format->finishCreation(vm);
    return format;
}

Structure* IntlNumberFormat::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlNumberFormat::IntlNumberFormat(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
{
}

void IntlNumberFormat::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

void IntlNumberFormat::destroy(JSCell* cell)
{
    static_cast<IntlNumberFormat*>(cell)->IntlNumberFormat::~IntlNumberFormat();
}

void IntlNumberFormat::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlNumberFormat* thisObject = jsCast<IntlNumberFormat*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_boundFormat);
}

void IntlNumberFormat::setBoundFormat(VM& vm, JSBoundFunction* format)
{
    m_boundFormat.set(vm, this, format);
}

EncodedJSValue JSC_HOST_CALL IntlNumberFormatFuncFormatNumber(ExecState* exec)
{
    // 11.3.4 Format Number Functions (ECMA-402 2.0)
    // 1. Let nf be the this value.
    IntlNumberFormat* format = jsDynamicCast<IntlNumberFormat*>(exec->thisValue());
    // 2. Assert: Type(nf) is Object and nf has an [[initializedNumberFormat]] internal slot whose value is true.
    if (!format)
        return JSValue::encode(throwTypeError(exec));

    // 3. If value is not provided, let value be undefined.
    // 4. Let x be ToNumber(value).
    double value = exec->argument(0).toNumber(exec);
    // 5. ReturnIfAbrupt(x).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    // 6. Return FormatNumber(nf, x).
    
    // 11.3.4 FormatNumber abstract operation (ECMA-402 2.0)
    // FIXME: Implement FormatNumber.

    return JSValue::encode(jsNumber(value).toString(exec));
}

} // namespace JSC

#endif // ENABLE(INTL)
