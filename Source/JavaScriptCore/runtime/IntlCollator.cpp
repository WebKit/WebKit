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
#include "IntlCollator.h"

#if ENABLE(INTL)

#include "Error.h"
#include "IntlCollatorConstructor.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo IntlCollator::s_info = { "Object", &Base::s_info, 0, CREATE_METHOD_TABLE(IntlCollator) };

IntlCollator* IntlCollator::create(VM& vm, IntlCollatorConstructor* constructor)
{
    IntlCollator* format = new (NotNull, allocateCell<IntlCollator>(vm.heap)) IntlCollator(vm, constructor->collatorStructure());
    format->finishCreation(vm);
    return format;
}

Structure* IntlCollator::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlCollator::IntlCollator(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
{
}

void IntlCollator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

void IntlCollator::destroy(JSCell* cell)
{
    static_cast<IntlCollator*>(cell)->IntlCollator::~IntlCollator();
}

void IntlCollator::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    IntlCollator* thisObject = jsCast<IntlCollator*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_boundCompare);
}

void IntlCollator::setBoundCompare(VM& vm, JSBoundFunction* format)
{
    m_boundCompare.set(vm, this, format);
}

EncodedJSValue JSC_HOST_CALL IntlCollatorFuncCompare(ExecState* state)
{
    // 10.3.4 Collator Compare Functions (ECMA-402 2.0)
    // 1. Let collator be the this value.
    IntlCollator* collator = jsDynamicCast<IntlCollator*>(state->thisValue());

    // 2. Assert: Type(collator) is Object and collator has an [[initializedCollator]] internal slot whose value is true.
    if (!collator)
        return JSValue::encode(throwTypeError(state));

    // 3. If x is not provided, let x be undefined.
    // 4. If y is not provided, let y be undefined.
    // 5. Let X be ToString(x).
    JSString* a = state->argument(0).toString(state);
    // 6. ReturnIfAbrupt(X).
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    // 7. Let Y be ToString(y).
    JSString* b = state->argument(1).toString(state);
    // 8. ReturnIfAbrupt(Y).
    if (state->hadException())
        return JSValue::encode(jsUndefined());

    // 9. Return CompareStrings(collator, X, Y).

    // 10.3.4 CompareStrings abstract operation (ECMA-402 2.0)
    // FIXME: Implement CompareStrings.

    // Return simple check until properly implemented.
    return JSValue::encode(jsNumber(codePointCompare(a->value(state).impl(), b->value(state).impl())));
}

} // namespace JSC

#endif // ENABLE(INTL)
