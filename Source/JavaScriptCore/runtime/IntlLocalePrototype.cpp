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

static JSC_DECLARE_HOST_FUNCTION(IntlLocalePrototypeFuncMaximize);
static JSC_DECLARE_HOST_FUNCTION(IntlLocalePrototypeFuncMinimize);
static JSC_DECLARE_HOST_FUNCTION(IntlLocalePrototypeFuncToString);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterBaseName);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterCalendar);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterCalendars);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterCaseFirst);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterCollation);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterCollations);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterHourCycle);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterHourCycles);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterNumeric);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterNumberingSystem);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterNumberingSystems);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterLanguage);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterScript);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterRegion);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterTimeZones);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterTextInfo);
static JSC_DECLARE_CUSTOM_GETTER(IntlLocalePrototypeGetterWeekInfo);

}

#include "IntlLocalePrototype.lut.h"

namespace JSC {

const ClassInfo IntlLocalePrototype::s_info = { "Intl.Locale", &Base::s_info, &localePrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlLocalePrototype) };

/* Source for IntlLocalePrototype.lut.h
@begin localePrototypeTable
  maximize         IntlLocalePrototypeFuncMaximize           DontEnum|Function 0
  minimize         IntlLocalePrototypeFuncMinimize           DontEnum|Function 0
  toString         IntlLocalePrototypeFuncToString           DontEnum|Function 0
  baseName         IntlLocalePrototypeGetterBaseName         DontEnum|ReadOnly|CustomAccessor
  calendar         IntlLocalePrototypeGetterCalendar         DontEnum|ReadOnly|CustomAccessor
  calendars        IntlLocalePrototypeGetterCalendars        DontEnum|ReadOnly|CustomAccessor
  caseFirst        IntlLocalePrototypeGetterCaseFirst        DontEnum|ReadOnly|CustomAccessor
  collation        IntlLocalePrototypeGetterCollation        DontEnum|ReadOnly|CustomAccessor
  collations       IntlLocalePrototypeGetterCollations       DontEnum|ReadOnly|CustomAccessor
  hourCycle        IntlLocalePrototypeGetterHourCycle        DontEnum|ReadOnly|CustomAccessor
  hourCycles       IntlLocalePrototypeGetterHourCycles       DontEnum|ReadOnly|CustomAccessor
  numeric          IntlLocalePrototypeGetterNumeric          DontEnum|ReadOnly|CustomAccessor
  numberingSystem  IntlLocalePrototypeGetterNumberingSystem  DontEnum|ReadOnly|CustomAccessor
  numberingSystems IntlLocalePrototypeGetterNumberingSystems DontEnum|ReadOnly|CustomAccessor
  language         IntlLocalePrototypeGetterLanguage         DontEnum|ReadOnly|CustomAccessor
  script           IntlLocalePrototypeGetterScript           DontEnum|ReadOnly|CustomAccessor
  region           IntlLocalePrototypeGetterRegion           DontEnum|ReadOnly|CustomAccessor
  timeZones        IntlLocalePrototypeGetterTimeZones        DontEnum|ReadOnly|CustomAccessor
  textInfo         IntlLocalePrototypeGetterTextInfo         DontEnum|ReadOnly|CustomAccessor
  weekInfo         IntlLocalePrototypeGetterWeekInfo         DontEnum|ReadOnly|CustomAccessor
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
JSC_DEFINE_HOST_FUNCTION(IntlLocalePrototypeFuncMaximize, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.maximize called on value that's not a Locale"_s);

    IntlLocale* newLocale = IntlLocale::create(vm, globalObject->localeStructure());
    scope.release();
    newLocale->initializeLocale(globalObject, locale->maximal(), jsUndefined());
    return JSValue::encode(newLocale);
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.minimize
JSC_DEFINE_HOST_FUNCTION(IntlLocalePrototypeFuncMinimize, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.minimize called on value that's not a Locale"_s);

    IntlLocale* newLocale = IntlLocale::create(vm, globalObject->localeStructure());
    scope.release();
    newLocale->initializeLocale(globalObject, locale->minimal(), jsUndefined());
    return JSValue::encode(newLocale);
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.toString
JSC_DEFINE_HOST_FUNCTION(IntlLocalePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, callFrame->thisValue());
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.toString called on value that's not a Locale"_s);

    const String& fullString = locale->toString();
    RELEASE_AND_RETURN(scope, JSValue::encode(fullString.isEmpty() ? jsUndefined() : jsString(vm, fullString)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.baseName
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterBaseName, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.baseName called on value that's not a Locale"_s);

    const String& baseName = locale->baseName();
    RELEASE_AND_RETURN(scope, JSValue::encode(baseName.isEmpty() ? jsUndefined() : jsString(vm, baseName)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.calendar
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterCalendar, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.calendar called on value that's not a Locale"_s);

    const String& calendar = locale->calendar();
    RELEASE_AND_RETURN(scope, JSValue::encode(calendar.isNull() ? jsUndefined() : jsString(vm, calendar)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.calendars
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterCalendars, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.calendars called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->calendars(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.caseFirst
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterCaseFirst, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.caseFirst called on value that's not a Locale"_s);

    const String& caseFirst = locale->caseFirst();
    RELEASE_AND_RETURN(scope, JSValue::encode(caseFirst.isNull() ? jsUndefined() : jsString(vm, caseFirst)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.collation
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterCollation, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.collation called on value that's not a Locale"_s);

    const String& collation = locale->collation();
    RELEASE_AND_RETURN(scope, JSValue::encode(collation.isNull() ? jsUndefined() : jsString(vm, collation)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.collations
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterCollations, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.collations called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->collations(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.hourCycle
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterHourCycle, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.hourCycle called on value that's not a Locale"_s);

    const String& hourCycle = locale->hourCycle();
    RELEASE_AND_RETURN(scope, JSValue::encode(hourCycle.isNull() ? jsUndefined() : jsString(vm, hourCycle)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.hourcycles
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterHourCycles, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.hourCycles called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->hourCycles(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numeric
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterNumeric, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.numeric called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(locale->numeric() == TriState::True)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numberingSystem
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterNumberingSystem, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.numberingSystem called on value that's not a Locale"_s);

    const String& numberingSystem = locale->numberingSystem();
    RELEASE_AND_RETURN(scope, JSValue::encode(numberingSystem.isNull() ? jsUndefined() : jsString(vm, numberingSystem)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.numberingSystems
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterNumberingSystems, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.numberingSystems called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->numberingSystems(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.language
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterLanguage, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.language called on value that's not a Locale"_s);

    const String& language = locale->language();
    RELEASE_AND_RETURN(scope, JSValue::encode(language.isEmpty() ? jsUndefined() : jsString(vm, language)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.script
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterScript, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.script called on value that's not a Locale"_s);

    const String& script = locale->script();
    RELEASE_AND_RETURN(scope, JSValue::encode(script.isEmpty() ? jsUndefined() : jsString(vm, script)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.region
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterRegion, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.region called on value that's not a Locale"_s);

    const String& region = locale->region();
    RELEASE_AND_RETURN(scope, JSValue::encode(region.isEmpty() ? jsUndefined() : jsString(vm, region)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.timezones
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterTimeZones, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.timeZones called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->timeZones(globalObject)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.textInfo
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterTextInfo, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.textInfo called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->textInfo(globalObject)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.weekInfo
JSC_DEFINE_CUSTOM_GETTER(IntlLocalePrototypeGetterWeekInfo, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.weekInfo called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->weekInfo(globalObject)));
}

} // namespace JSC
