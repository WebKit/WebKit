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

bool ScriptArguments::getFirstArgumentAsString(String& result) const
{
    if (!argumentCount())
        return false;

    auto* globalObject = this->globalObject();
    if (!globalObject) {
        ASSERT_NOT_REACHED();
        return false;
    }

    auto value = argumentAt(0);
    if (JSC::jsDynamicCast<JSC::ProxyObject*>(globalObject->vm(), value)) {
        result = "[object Proxy]"_s;
        return true;
    }

    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    result = value.toWTFString(globalObject);
    scope.clearException();
    return true;
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
