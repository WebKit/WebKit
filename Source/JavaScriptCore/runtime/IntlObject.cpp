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
#include "IntlObject.h"

#if ENABLE(INTL)

#include "FunctionPrototype.h"
#include "IntlCollator.h"
#include "IntlCollatorConstructor.h"
#include "IntlCollatorPrototype.h"
#include "IntlDateTimeFormat.h"
#include "IntlDateTimeFormatConstructor.h"
#include "IntlDateTimeFormatPrototype.h"
#include "IntlNumberFormat.h"
#include "IntlNumberFormatConstructor.h"
#include "IntlNumberFormatPrototype.h"
#include "JSCInlines.h"
#include "Lookup.h"
#include "ObjectPrototype.h"
#include <wtf/Assertions.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlObject);

}

namespace JSC {

const ClassInfo IntlObject::s_info = { "Object", &Base::s_info, 0, CREATE_METHOD_TABLE(IntlObject) };

IntlObject::IntlObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

IntlObject* IntlObject::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    IntlObject* object = new (NotNull, allocateCell<IntlObject>(vm.heap)) IntlObject(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

void IntlObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    // Set up Collator.
    IntlCollatorPrototype* collatorPrototype = IntlCollatorPrototype::create(vm, globalObject, IntlCollatorPrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
    Structure* collatorStructure = IntlCollator::createStructure(vm, globalObject, collatorPrototype);
    IntlCollatorConstructor* collatorConstructor = IntlCollatorConstructor::create(vm, IntlCollatorConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), collatorPrototype, collatorStructure);

    collatorPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, collatorConstructor, DontEnum);

    // Set up NumberFormat.
    IntlNumberFormatPrototype* numberFormatPrototype = IntlNumberFormatPrototype::create(vm, globalObject, IntlNumberFormatPrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
    Structure* numberFormatStructure = IntlNumberFormat::createStructure(vm, globalObject, numberFormatPrototype);
    IntlNumberFormatConstructor* numberFormatConstructor = IntlNumberFormatConstructor::create(vm, IntlNumberFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), numberFormatPrototype, numberFormatStructure);

    numberFormatPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, numberFormatConstructor, DontEnum);

    // Set up DateTimeFormat.
    IntlDateTimeFormatPrototype* dateTimeFormatPrototype = IntlDateTimeFormatPrototype::create(vm, globalObject, IntlDateTimeFormatPrototype::createStructure(vm, globalObject, globalObject->objectPrototype()));
    Structure* dateTimeFormatStructure = IntlDateTimeFormat::createStructure(vm, globalObject, dateTimeFormatPrototype);
    IntlDateTimeFormatConstructor* dateTimeFormatConstructor = IntlDateTimeFormatConstructor::create(vm, IntlDateTimeFormatConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), dateTimeFormatPrototype, dateTimeFormatStructure);

    dateTimeFormatPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, dateTimeFormatConstructor, DontEnum);

    // 8.1 Properties of the Intl Object (ECMA-402 2.0)
    putDirectWithoutTransition(vm, vm.propertyNames->Collator, collatorConstructor, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->NumberFormat, numberFormatConstructor, DontEnum);
    putDirectWithoutTransition(vm, vm.propertyNames->DateTimeFormat, dateTimeFormatConstructor, DontEnum);
}

Structure* IntlObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

} // namespace JSC

#endif // ENABLE(INTL)
