/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

static JSC_DECLARE_HOST_FUNCTION(intlLocalePrototypeFuncMaximize);
static JSC_DECLARE_HOST_FUNCTION(intlLocalePrototypeFuncMinimize);
static JSC_DECLARE_HOST_FUNCTION(intlLocalePrototypeFuncToString);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterBaseName);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterCalendar);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterCalendars);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterCaseFirst);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterCollation);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterCollations);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterHourCycle);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterHourCycles);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterNumeric);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterNumberingSystem);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterNumberingSystems);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterLanguage);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterScript);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterRegion);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterTimeZones);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterTextInfo);
static JSC_DECLARE_CUSTOM_GETTER(intlLocalePrototypeGetterWeekInfo);

}

#include "IntlLocalePrototype.lut.h"

namespace JSC {

const ClassInfo IntlLocalePrototype::s_info = { "Intl.Locale", &Base::s_info, &localePrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlLocalePrototype) };

/* Source for IntlLocalePrototype.lut.h
@begin localePrototypeTable
  maximize         intlLocalePrototypeFuncMaximize           DontEnum|Function 0
  minimize         intlLocalePrototypeFuncMinimize           DontEnum|Function 0
  toString         intlLocalePrototypeFuncToString           DontEnum|Function 0
  baseName         intlLocalePrototypeGetterBaseName         DontEnum|ReadOnly|CustomAccessor
  calendar         intlLocalePrototypeGetterCalendar         DontEnum|ReadOnly|CustomAccessor
  calendars        intlLocalePrototypeGetterCalendars        DontEnum|ReadOnly|CustomAccessor
  caseFirst        intlLocalePrototypeGetterCaseFirst        DontEnum|ReadOnly|CustomAccessor
  collation        intlLocalePrototypeGetterCollation        DontEnum|ReadOnly|CustomAccessor
  collations       intlLocalePrototypeGetterCollations       DontEnum|ReadOnly|CustomAccessor
  hourCycle        intlLocalePrototypeGetterHourCycle        DontEnum|ReadOnly|CustomAccessor
  hourCycles       intlLocalePrototypeGetterHourCycles       DontEnum|ReadOnly|CustomAccessor
  numeric          intlLocalePrototypeGetterNumeric          DontEnum|ReadOnly|CustomAccessor
  numberingSystem  intlLocalePrototypeGetterNumberingSystem  DontEnum|ReadOnly|CustomAccessor
  numberingSystems intlLocalePrototypeGetterNumberingSystems DontEnum|ReadOnly|CustomAccessor
  language         intlLocalePrototypeGetterLanguage         DontEnum|ReadOnly|CustomAccessor
  script           intlLocalePrototypeGetterScript           DontEnum|ReadOnly|CustomAccessor
  region           intlLocalePrototypeGetterRegion           DontEnum|ReadOnly|CustomAccessor
  timeZones        intlLocalePrototypeGetterTimeZones        DontEnum|ReadOnly|CustomAccessor
  textInfo         intlLocalePrototypeGetterTextInfo         DontEnum|ReadOnly|CustomAccessor
  weekInfo         intlLocalePrototypeGetterWeekInfo         DontEnum|ReadOnly|CustomAccessor
@end
*/

IntlLocalePrototype* IntlLocalePrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlLocalePrototype>(vm)) IntlLocalePrototype(vm, structure);
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
JSC_DEFINE_HOST_FUNCTION(intlLocalePrototypeFuncMaximize, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
JSC_DEFINE_HOST_FUNCTION(intlLocalePrototypeFuncMinimize, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
JSC_DEFINE_HOST_FUNCTION(intlLocalePrototypeFuncToString, (JSGlobalObject* globalObject, CallFrame* callFrame))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterBaseName, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterCalendar, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterCalendars, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.calendars called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->calendars(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.caseFirst
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterCaseFirst, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterCollation, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterCollations, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.collations called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->collations(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.hourCycle
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterHourCycle, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterHourCycles, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.hourCycles called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->hourCycles(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numeric
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterNumeric, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.numeric called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(locale->numeric() == TriState::True)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.numberingSystem
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterNumberingSystem, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterNumberingSystems, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.numberingSystems called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->numberingSystems(globalObject)));
}

// https://tc39.es/ecma402/#sec-Intl.Locale.prototype.language
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterLanguage, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterScript, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterRegion, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
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
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterTimeZones, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.timeZones called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->timeZones(globalObject)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.textInfo
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterTextInfo, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.textInfo called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->textInfo(globalObject)));
}

// https://tc39.es/proposal-intl-locale-info/#sec-Intl.Locale.prototype.weekInfo
JSC_DEFINE_CUSTOM_GETTER(intlLocalePrototypeGetterWeekInfo, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* locale = jsDynamicCast<IntlLocale*>(vm, JSValue::decode(thisValue));
    if (!locale)
        return throwVMTypeError(globalObject, scope, "Intl.Locale.prototype.weekInfo called on value that's not a Locale"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(locale->weekInfo(globalObject)));
}

} // namespace JSC
