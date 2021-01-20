/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include "StringConstructor.h"

#include "JSCInlines.h"
#include "StringPrototype.h"
#include <wtf/text/StringBuilder.h>

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(stringFromCharCode);
static JSC_DECLARE_HOST_FUNCTION(stringFromCodePoint);

}

#include "StringConstructor.lut.h"

namespace JSC {

const ClassInfo StringConstructor::s_info = { "Function", &InternalFunction::s_info, &stringConstructorTable, nullptr, CREATE_METHOD_TABLE(StringConstructor) };

/* Source for StringConstructor.lut.h
@begin stringConstructorTable
  fromCharCode          stringFromCharCode         DontEnum|Function 1 FromCharCodeIntrinsic
  fromCodePoint         stringFromCodePoint        DontEnum|Function 1
  raw                   JSBuiltin                  DontEnum|Function 1
@end
*/

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(StringConstructor);


static JSC_DECLARE_HOST_FUNCTION(callStringConstructor);
static JSC_DECLARE_HOST_FUNCTION(constructWithStringConstructor);

StringConstructor::StringConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callStringConstructor, constructWithStringConstructor)
{
}

void StringConstructor::finishCreation(VM& vm, StringPrototype* stringPrototype)
{
    Base::finishCreation(vm, 1, vm.propertyNames->String.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, stringPrototype, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);
}

// ------------------------------ Functions --------------------------------

JSC_DEFINE_HOST_FUNCTION(stringFromCharCode, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = callFrame->argumentCount();
    if (LIKELY(length == 1)) {
        scope.release();
        unsigned code = callFrame->uncheckedArgument(0).toUInt32(globalObject);
        // Not checking for an exception here is ok because jsSingleCharacterString will just fetch an unused string if there's an exception.
        return JSValue::encode(jsSingleCharacterString(vm, code));
    }

    LChar* buf8Bit;
    auto impl8Bit = StringImpl::createUninitialized(length, buf8Bit);
    for (unsigned i = 0; i < length; ++i) {
        UChar character = static_cast<UChar>(callFrame->uncheckedArgument(i).toUInt32(globalObject));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (UNLIKELY(!isLatin1(character))) {
            UChar* buf16Bit;
            auto impl16Bit = StringImpl::createUninitialized(length, buf16Bit);
            StringImpl::copyCharacters(buf16Bit, buf8Bit, i);
            buf16Bit[i] = character;
            ++i;
            for (; i < length; ++i) {
                buf16Bit[i] = static_cast<UChar>(callFrame->uncheckedArgument(i).toUInt32(globalObject));
                RETURN_IF_EXCEPTION(scope, encodedJSValue());
            }
            RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, WTFMove(impl16Bit))));
        }
        buf8Bit[i] = static_cast<LChar>(character);
    }
    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, WTFMove(impl8Bit))));
}

JSString* stringFromCharCode(JSGlobalObject* globalObject, int32_t arg)
{
    return jsSingleCharacterString(globalObject->vm(), arg);
}

JSC_DEFINE_HOST_FUNCTION(stringFromCodePoint, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    unsigned length = callFrame->argumentCount();
    StringBuilder builder;
    builder.reserveCapacity(length);

    for (unsigned i = 0; i < length; ++i) {
        double codePointAsDouble = callFrame->uncheckedArgument(i).toNumber(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());

        uint32_t codePoint = static_cast<uint32_t>(codePointAsDouble);

        if (codePoint != codePointAsDouble || codePoint > UCHAR_MAX_VALUE)
            return throwVMError(globalObject, scope, createRangeError(globalObject, "Arguments contain a value that is out of range of code points"_s));

        if (U_IS_BMP(codePoint))
            builder.append(static_cast<UChar>(codePoint));
        else {
            builder.append(U16_LEAD(codePoint));
            builder.append(U16_TRAIL(codePoint));
        }
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(jsString(vm, builder.toString())));
}

JSC_DEFINE_HOST_FUNCTION(constructWithStringConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->stringObjectStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->stringObjectStructure());
    RETURN_IF_EXCEPTION(scope, { });

    if (!callFrame->argumentCount())
        return JSValue::encode(StringObject::create(vm, structure));
    JSString* str = callFrame->uncheckedArgument(0).toString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    return JSValue::encode(StringObject::create(vm, structure, str));
}

JSString* stringConstructor(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    if (argument.isSymbol())
        return jsNontrivialString(vm, asSymbol(argument)->descriptiveString());
    return argument.toString(globalObject);
}

JSC_DEFINE_HOST_FUNCTION(callStringConstructor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    if (!callFrame->argumentCount())
        return JSValue::encode(jsEmptyString(vm));
    return JSValue::encode(stringConstructor(globalObject, callFrame->uncheckedArgument(0)));
}

} // namespace JSC
