/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "IntlLocalePrototype.h"

#include "IntlLocale.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeFuncMaximize(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeFuncMinimize(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeFuncToString(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterBaseName(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterCalendar(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterCaseFirst(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterCollation(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterHourCycle(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterNumeric(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterNumberingSystem(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterLanguage(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterScript(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterRegion(JSGlobalObject*, CallFrame*);

}

#include "IntlLocalePrototype.lut.h"

namespace JSC {

const ClassInfo IntlLocalePrototype::s_info = { "Intl.Locale", &Base::s_info, &localePrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlLocalePrototype) };

/* Source for IntlLocalePrototype.lut.h
@begin localePrototypeTable
  maximize         IntlLocalePrototypeFuncMaximize           DontEnum|Function 0
  minimize         IntlLocalePrototypeFuncMinimize           DontEnum|Function 0
  toString         IntlLocalePrototypeFuncToString           DontEnum|Function 0
  baseName         IntlLocalePrototypeGetterBaseName         DontEnum|Accessor
  calendar         IntlLocalePrototypeGetterCalendar         DontEnum|Accessor
  caseFirst        IntlLocalePrototypeGetterCaseFirst        DontEnum|Accessor
  collation        IntlLocalePrototypeGetterCollation        DontEnum|Accessor
  hourCycle        IntlLocalePrototypeGetterHourCycle        DontEnum|Accessor
  numeric          IntlLocalePrototypeGetterNumeric          DontEnum|Accessor
  numberingSystem  IntlLocalePrototypeGetterNumberingSystem  DontEnum|Accessor
  language         IntlLocalePrototypeGetterLanguage         DontEnum|Accessor
  script           IntlLocalePrototypeGetterScript           DontEnum|Accessor
  region           IntlLocalePrototypeGetterRegion           DontEnum|Accessor
@end
*/

IntlLocalePrototype* IntlLocalePrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlLocalePrototype>(vm.heap)) IntlLocalePrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlLocalePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlLocalePrototype::IntlLocalePrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlLocalePrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.maximize
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeFuncMaximize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.maximize called on value that's not an object initialized as a Locale"_s);

    IntlLocale* newLocale = IntlLocale::create(vm, globalObject->localeStructure());
    scope.release();
    newLocale->initializeLocale(globalObject, locale->maximal(), jsUndefined());
    return JSValue::encode(newLocale);
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.minimize
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeFuncMinimize(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.minimize called on value that's not an object initialized as a Locale"_s);

    IntlLocale* newLocale = IntlLocale::create(vm, globalObject->localeStructure());
    scope.release();
    newLocale->initializeLocale(globalObject, locale->minimal(), jsUndefined());
    return JSValue::encode(newLocale);
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.toString
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeFuncToString(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.toString called on value that's not an object initialized as a Locale"_s);

    const String& fullString = locale->toString();
    RELEASE_AND_RETURN(scope, JSValue::encode(fullString.isEmpty() ? jsUndefined() : jsString(vm, fullString)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.baseName
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterBaseName(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.baseName called on value that's not an object initialized as a Locale"_s);

    const String& baseName = locale->baseName();
    RELEASE_AND_RETURN(scope, JSValue::encode(baseName.isEmpty() ? jsUndefined() : jsString(vm, baseName)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.calendar
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterCalendar(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.calendar called on value that's not an object initialized as a Locale"_s);

    const String& calendar = locale->calendar();
    RELEASE_AND_RETURN(scope, JSValue::encode(calendar.isNull() ? jsUndefined() : jsString(vm, calendar)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.caseFirst
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterCaseFirst(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.caseFirst called on value that's not an object initialized as a Locale"_s);

    const String& caseFirst = locale->caseFirst();
    RELEASE_AND_RETURN(scope, JSValue::encode(caseFirst.isNull() ? jsUndefined() : jsString(vm, caseFirst)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.collation
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterCollation(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.collation called on value that's not an object initialized as a Locale"_s);

    const String& collation = locale->collation();
    RELEASE_AND_RETURN(scope, JSValue::encode(collation.isNull() ? jsUndefined() : jsString(vm, collation)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.hourCycle
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterHourCycle(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.hourCycle called on value that's not an object initialized as a Locale"_s);

    const String& hourCycle = locale->hourCycle();
    RELEASE_AND_RETURN(scope, JSValue::encode(hourCycle.isNull() ? jsUndefined() : jsString(vm, hourCycle)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numeric
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterNumeric(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.numeric called on value that's not an object initialized as a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(locale->numeric() == TriState::True)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numberingSystem
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterNumberingSystem(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.numberingSystem called on value that's not an object initialized as a Locale"_s);

    const String& numberingSystem = locale->numberingSystem();
    RELEASE_AND_RETURN(scope, JSValue::encode(numberingSystem.isNull() ? jsUndefined() : jsString(vm, numberingSystem)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.language
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterLanguage(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.language called on value that's not an object initialized as a Locale"_s);

    const String& language = locale->language();
    RELEASE_AND_RETURN(scope, JSValue::encode(language.isEmpty() ? jsUndefined() : jsString(vm, language)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.script
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterScript(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.script called on value that's not an object initialized as a Locale"_s);

    const String& script = locale->script();
    RELEASE_AND_RETURN(scope, JSValue::encode(script.isEmpty() ? jsUndefined() : jsString(vm, script)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.region
EncodedJSValue JSC_HOST_CALL IntlLocalePrototypeGetterRegion(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.region called on value that's not an object initialized as a Locale"_s);

    const String& region = locale->region();
    RELEASE_AND_RETURN(scope, JSValue::encode(region.isEmpty() ? jsUndefined() : jsString(vm, region)));
}

} // namespace JSC
