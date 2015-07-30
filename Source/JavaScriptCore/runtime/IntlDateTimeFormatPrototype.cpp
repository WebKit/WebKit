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
#include "IntlDateTimeFormatPrototype.h"

#if ENABLE(INTL)

#include "Error.h"
#include "IntlDateTimeFormat.h"
#include "JSBoundFunction.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSObject.h"
#include "ObjectConstructor.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlDateTimeFormatPrototype);

static EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeGetterFormat(ExecState*);
static EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeFuncResolvedOptions(ExecState*);

}

#include "IntlDateTimeFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlDateTimeFormatPrototype::s_info = { "Object", &IntlDateTimeFormat::s_info, &dateTimeFormatPrototypeTable, CREATE_METHOD_TABLE(IntlDateTimeFormatPrototype) };

/* Source for IntlDateTimeFormatPrototype.lut.h
@begin dateTimeFormatPrototypeTable
  format           IntlDateTimeFormatPrototypeGetterFormat         DontEnum|Accessor
  resolvedOptions  IntlDateTimeFormatPrototypeFuncResolvedOptions  DontEnum|Function 0
@end
*/

IntlDateTimeFormatPrototype* IntlDateTimeFormatPrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    IntlDateTimeFormatPrototype* object = new (NotNull, allocateCell<IntlDateTimeFormatPrototype>(vm.heap)) IntlDateTimeFormatPrototype(vm, structure);
    object->finishCreation(vm, structure);
    return object;
}

Structure* IntlDateTimeFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDateTimeFormatPrototype::IntlDateTimeFormatPrototype(VM& vm, Structure* structure)
    : IntlDateTimeFormat(vm, structure)
{
}

void IntlDateTimeFormatPrototype::finishCreation(VM& vm, Structure*)
{
    Base::finishCreation(vm);
}

bool IntlDateTimeFormatPrototype::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, dateTimeFormatPrototypeTable, jsCast<IntlDateTimeFormatPrototype*>(object), propertyName, slot);
}

EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeGetterFormat(ExecState* exec)
{
    // 12.3.3 Intl.DateTimeFormat.prototype.format (ECMA-402 2.0)
    // 1. Let dtf be this DateTimeFormat object.
    IntlDateTimeFormat* dtf = jsDynamicCast<IntlDateTimeFormat*>(exec->thisValue());
    // 2. ReturnIfAbrupt(dtf).
    if (!dtf)
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Intl.DateTimeFormat.prototype.format called on value that's not an object initialized as a DateTimeFormat")));
    
    JSBoundFunction* boundFormat = dtf->boundFormat();
    // 3. If the [[boundFormat]] internal slot of this DateTimeFormat object is undefined,
    if (!boundFormat) {
        VM& vm = exec->vm();
        JSGlobalObject* globalObject = dtf->globalObject();
        // a. Let F be a new built-in function object as defined in 12.3.4.
        // b. The value of F’s length property is 1. (Note: F’s length property was 0 in ECMA-402 1.0)
        JSFunction* targetObject = JSFunction::create(vm, globalObject, 1, ASCIILiteral("format"), IntlDateTimeFormatFuncFormatDateTime, NoIntrinsic);
        JSArray* boundArgs = JSArray::tryCreateUninitialized(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
        if (!boundArgs)
            return JSValue::encode(throwOutOfMemoryError(exec));
        
        // c. Let bf be BoundFunctionCreate(F, «this value»).
        boundFormat = JSBoundFunction::create(vm, globalObject, targetObject, dtf, boundArgs, 1, ASCIILiteral("format"));
        // d. Set dtf.[[boundFormat]] to bf.
        dtf->setBoundFormat(vm, boundFormat);
    }
    // 4. Return dtf.[[boundFormat]].
    return JSValue::encode(boundFormat);
}

EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeFuncResolvedOptions(ExecState* exec)
{
    // 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions() (ECMA-402 2.0)
    IntlDateTimeFormat* dtf = jsDynamicCast<IntlDateTimeFormat*>(exec->thisValue());
    if (!dtf)
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Intl.DateTimeFormat.prototype.resolvedOptions called on value that's not an object initialized as a DateTimeFormat")));

    // The function returns a new object whose properties and attributes are set as if constructed by an object literal assigning to each of the following properties the value of the corresponding internal slot of this DateTimeFormat object (see 12.4): locale, calendar, numberingSystem, timeZone, hour12, weekday, era, year, month, day, hour, minute, second, and timeZoneName. Properties whose corresponding internal slots are not present are not assigned.
    // Note: In this version of the ECMAScript 2015 Internationalization API, the timeZone property will be the name of the default time zone if no timeZone property was provided in the options object provided to the Intl.DateTimeFormat constructor. The previous version left the timeZone property undefined in this case.

    JSObject* options = constructEmptyObject(exec);

    // FIXME: Populate object from internal slots.

    return JSValue::encode(options);
}

} // namespace JSC

#endif // ENABLE(INTL)
