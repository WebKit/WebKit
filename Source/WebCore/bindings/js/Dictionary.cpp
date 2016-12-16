/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "Dictionary.h"

#include "JSDOMConvert.h"

using namespace JSC;

namespace WebCore {

Dictionary::Dictionary()
{
}

Dictionary::Dictionary(ExecState* state, JSObject* initializerObject)
    : m_state(state)
{
    if (state && initializerObject)
        m_initializerObject = Strong<JSObject>(state->vm(), initializerObject);
}

Dictionary::Dictionary(ExecState* state, JSValue value)
    : Dictionary(state, value.isObject() ? value.getObject() : nullptr)
{
}

Dictionary::GetPropertyResult Dictionary::tryGetProperty(const char* propertyName, JSValue& finalResult) const
{
    ASSERT(isValid());

    auto scope = DECLARE_THROW_SCOPE(m_state->vm());
    auto identifier = Identifier::fromString(m_state, propertyName);
    bool propertyFound = m_initializerObject.get()->getPropertySlot(m_state, identifier, [&] (bool propertyFound, PropertySlot& slot) -> bool {
        if (!propertyFound)
            return false;
        finalResult = slot.getValue(m_state, identifier);
        return true;
    });
    RETURN_IF_EXCEPTION(scope, ExceptionThrown);
    return propertyFound ? PropertyFound : NoPropertyFound;
}

bool Dictionary::getOwnPropertyNames(Vector<String>& names) const
{
    if (!isValid())
        return false;

    PropertyNameArray propertyNames(m_state, PropertyNameMode::Strings);
    JSObject::getOwnPropertyNames(m_initializerObject.get(), m_state, propertyNames, EnumerationMode());
    for (auto& identifier : propertyNames) {
        auto stringKey = identifier.string();
        if (!stringKey.isEmpty())
            names.append(stringKey);
    }

    return true;
}

// MARK: - Value conversions

void Dictionary::convertValue(ExecState& state, JSValue value, bool& result)
{
    result = convert<IDLBoolean>(state, value);
}

void Dictionary::convertValue(ExecState& state, JSValue value, int& result)
{
    result = convert<IDLLong>(state, value);
}

void Dictionary::convertValue(ExecState& state, JSValue value, unsigned& result)
{
    result = convert<IDLUnsignedLong>(state, value);
}

void Dictionary::convertValue(ExecState& state, JSValue value, unsigned short& result)
{
    result = convert<IDLUnsignedShort>(state, value);
}

void Dictionary::convertValue(ExecState& state, JSValue value, double& result)
{
    result = convert<IDLDouble>(state, value);
}

void Dictionary::convertValue(ExecState& state, JSValue value, String& result)
{
    result = convert<IDLDOMString>(state, value);
}

void Dictionary::convertValue(ExecState& state, JSValue value, Vector<String>& result)
{
    if (value.isUndefinedOrNull())
        return;
    result = convert<IDLSequence<IDLDOMString>>(state, value);
}

void Dictionary::convertValue(ExecState& state, JSValue value, Dictionary& result)
{
    result = Dictionary(&state, value);
}

}
