/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSRegExpStringIterator.h"

#include "Error.h"
#include "IteratorOperations.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSRegExpStringIteratorInlines.h"
#include "RegExpObjectInlines.h"
#include "RegExpPrototypeInlines.h"

namespace JSC {

const ClassInfo JSRegExpStringIterator::s_info = { "RegExpStringIterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSRegExpStringIterator) };

JSRegExpStringIterator* JSRegExpStringIterator::createWithInitialValues(VM& vm, Structure* structure)
{
    JSRegExpStringIterator* iterator = new (NotNull, allocateCell<JSRegExpStringIterator>(vm)) JSRegExpStringIterator(vm, structure);
    iterator->finishCreation(vm);
    return iterator;
}

void JSRegExpStringIterator::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSRegExpStringIterator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSRegExpStringIterator>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSRegExpStringIterator);

// https://tc39.es/ecma262/#sec-%regexpstringiteratorprototype%.next
JSValue JSRegExpStringIterator::nextImpl(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 5. Let R be O.[[IteratingRegExp]].
    // 6. Let S be O.[[IteratedString]].
    // 7. Let global be O.[[Global]].
    // 8. Let fullUnicode be O.[[Unicode]].
    JSObject* regExp = this->regExp();
    JSString* string = iteratedString();
    bool global = isGlobal();
    bool fullUnicode = isFullUnicode();

    // 9. Let match be ? RegExpExec(R, S).
    // When R's `exec` is still the primordial RegExp.prototype.exec, RegExpExec is equivalent to
    // RegExpBuiltinExec, which RegExpObject::execInline implements without observable lookups, and
    // emptiness is known from the MatchResult instead of ToString(Get(match, "0")).
    auto* regExpObject = dynamicDowncast<RegExpObject>(regExp);
    JSValue match;
    bool isEmptyMatch = false;
    if (regExpObject && regExpExecWatchpointIsValid(vm, regExpObject)) [[likely]] {
        MatchResult result;
        match = regExpObject->execInline(globalObject, string, result);
        RETURN_IF_EXCEPTION(scope, { });
        isEmptyMatch = !match.isNull() && result.empty();
    } else {
        match = regExpExec(globalObject, regExp, string);
        RETURN_IF_EXCEPTION(scope, { });
        // 11.a.i. ToString(Get(match, "0")) is only observed to detect an empty match.
        if (!match.isNull() && global) {
            JSValue matchValue = asObject(match)->get(globalObject, static_cast<unsigned>(0));
            RETURN_IF_EXCEPTION(scope, { });
            JSString* matchString = matchValue.toString(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            isEmptyMatch = !matchString->length();
        }
    }

    // 10. If match is null, set O.[[Done]] to true and return.
    if (match.isNull()) {
        setDone(true);
        return jsNull();
    }

    // 11.b. If global is false, set O.[[Done]] to true.
    if (!global) {
        setDone(true);
        return match;
    }

    // 11.a.ii. If matchStr is the empty String, advance R's lastIndex past it. R's lastIndex is a
    // non-configurable data property whenever R is a RegExpObject, so the field accessors observe
    // exactly what Get/Set(R, "lastIndex") would.
    if (isEmptyMatch) {
        // 11.a.ii.1. Let thisIndex be ℝ(? ToLength(? Get(R, "lastIndex"))).
        uint64_t thisIndex;
        if (regExpObject)
            thisIndex = regExpObject->getLastIndex().toLength(globalObject);
        else {
            JSValue lastIndexValue = regExp->get(globalObject, vm.propertyNames->lastIndex);
            RETURN_IF_EXCEPTION(scope, { });
            thisIndex = lastIndexValue.toLength(globalObject);
        }
        RETURN_IF_EXCEPTION(scope, { });

        // 11.a.ii.2. Let nextIndex be AdvanceStringIndex(S, thisIndex, fullUnicode).
        auto stringView = string->view(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        uint64_t nextIndex = advanceStringIndex(stringView, stringView->length(), thisIndex, fullUnicode);

        // 11.a.ii.3. Perform ? Set(R, "lastIndex", 𝔽(nextIndex), true).
        if (regExpObject)
            regExpObject->setLastIndex(globalObject, jsNumber(nextIndex), true);
        else {
            PutPropertySlot slot(regExp, true);
            regExp->methodTable()->put(regExp, globalObject, vm.propertyNames->lastIndex, jsNumber(nextIndex), slot);
        }
        RETURN_IF_EXCEPTION(scope, { });
    }

    return match;
}

// https://tc39.es/ecma262/#sec-%regexpstringiteratorprototype%.next
// Steps 1-3 (the brand check on the receiver) are handled by the caller.
JSObject* JSRegExpStringIterator::next(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 4. If O.[[Done]] is true, return CreateIteratorResultObject(undefined, true).
    if (isDone()) {
        scope.release();
        return createIteratorResultObject(globalObject, jsUndefined(), true);
    }

    JSValue match = nextImpl(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    scope.release();
    if (match.isNull())
        return createIteratorResultObject(globalObject, jsUndefined(), true);
    return createIteratorResultObject(globalObject, match, false);
}

JSC_DEFINE_HOST_FUNCTION(regExpStringIteratorPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    ASSERT(callFrame->argument(0).isCell());
    ASSERT(callFrame->argument(1).isString());
    ASSERT(callFrame->argument(2).isBoolean());
    ASSERT(callFrame->argument(3).isBoolean());

    VM& vm = globalObject->vm();

    auto* regExpStringIterator = JSRegExpStringIterator::createWithInitialValues(vm, globalObject->regExpStringIteratorStructure());

    regExpStringIterator->setRegExp(vm, asObject(callFrame->uncheckedArgument(0)));
    regExpStringIterator->setString(vm, callFrame->uncheckedArgument(1));
    regExpStringIterator->setFlags(callFrame->argument(2).asBoolean(), callFrame->argument(3).asBoolean());

    return JSValue::encode(regExpStringIterator);
}

} // namespace JSC
