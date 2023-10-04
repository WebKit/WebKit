/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptArguments.h"

#include "CatchScope.h"
#include "ProxyObject.h"
#include "StrongInlines.h"

namespace Inspector {

static inline String argumentAsString(JSC::JSGlobalObject* globalObject, JSC::JSValue argument)
{
    if (JSC::jsDynamicCast<JSC::ProxyObject*>(argument))
        return "[object Proxy]"_s;

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    auto result = argument.toWTFString(globalObject);
    scope.clearException();
    return result;
}

Ref<ScriptArguments> ScriptArguments::create(JSC::JSGlobalObject* globalObject, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    return adoptRef(*new ScriptArguments(globalObject, WTFMove(arguments)));
}

ScriptArguments::ScriptArguments(JSC::JSGlobalObject* globalObject, Vector<JSC::Strong<JSC::Unknown>>&& arguments)
    : m_globalObject(globalObject->vm(), globalObject)
    , m_arguments(WTFMove(arguments))
{
}

ScriptArguments::~ScriptArguments() = default;

JSC::JSValue ScriptArguments::argumentAt(size_t index) const
{
    ASSERT(m_arguments.size() > index);
    return m_arguments[index].get();
}

JSC::JSGlobalObject* ScriptArguments::globalObject() const
{
    return m_globalObject.get();
}

std::optional<String> ScriptArguments::getArgumentAtIndexAsString(size_t argumentIndex) const
{
    if (argumentIndex >= argumentCount())
        return std::nullopt;

    auto* globalObject = this->globalObject();
    if (!globalObject) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return argumentAsString(globalObject, argumentAt(argumentIndex));
}

bool ScriptArguments::getFirstArgumentAsString(String& result) const
{
    auto argument = getArgumentAtIndexAsString(0);
    if (!argument)
        return false;

    result = *argument;
    return true;
}

Vector<String> ScriptArguments::getArgumentsAsStrings() const
{
    auto* globalObject = this->globalObject();
    ASSERT(globalObject);
    if (!globalObject)
        return { };

    return WTF::map(m_arguments, [globalObject](auto& argument) {
        return argumentAsString(globalObject, argument.get());
    });
}

bool ScriptArguments::isEqual(const ScriptArguments& other) const
{
    auto size = m_arguments.size();

    if (size != other.m_arguments.size())
        return false;

    if (!size)
        return true;

    auto* globalObject = this->globalObject();
    if (!globalObject)
        return false;

    for (size_t i = 0; i < size; ++i) {
        auto a = m_arguments[i].get();
        auto b = other.m_arguments[i].get();
        if (!a || !b) {
            if (a != b)
                return false;
        } else {
            auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
            bool result = JSC::JSValue::strictEqual(globalObject, a, b);
            scope.clearException();
            if (!result)
                return false;
        }
    }

    return true;
}

} // namespace Inspector
