/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "JSDictionary.h"

#include "ArrayValue.h"
#include "Dictionary.h"
#include "JSDOMWindow.h"
#include "JSEventTarget.h"
#include "JSMessagePortCustom.h"
#include "JSNode.h"
#include "JSStorage.h"
#include "JSTrackCustom.h"
#include "ScriptValue.h"
#include "SerializedScriptValue.h"
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/text/AtomicString.h>

using namespace JSC;

namespace WebCore {

JSDictionary::GetPropertyResult JSDictionary::tryGetProperty(const char* propertyName, JSValue& finalResult) const
{
    ASSERT(isValid());
    Identifier identifier(m_exec, propertyName);
    PropertySlot slot(m_initializerObject.get());

    if (!m_initializerObject.get()->getPropertySlot(m_exec, identifier, slot))
        return NoPropertyFound;

    if (m_exec->hadException())
        return ExceptionThrown;

    finalResult = slot.getValue(m_exec, identifier);
    if (m_exec->hadException())
        return ExceptionThrown;

    return PropertyFound;
}

void JSDictionary::convertValue(ExecState*, JSValue value, bool& result)
{
    result = value.toBoolean();
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, int& result)
{
    result = value.toInt32(exec);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned& result)
{
    result = value.toUInt32(exec);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned short& result)
{
    result = static_cast<unsigned short>(value.toUInt32(exec));
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned long long& result)
{
    double d = value.toNumber(exec);
    doubleToInteger(d, result);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, double& result)
{
    result = value.toNumber(exec);
}

void JSDictionary::convertValue(JSC::ExecState* exec, JSC::JSValue value, Dictionary& result)
{
    result = Dictionary(exec, value);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, String& result)
{
    result = ustringToString(value.toString(exec)->value(exec));
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, Vector<String>& result)
{
    if (value.isUndefinedOrNull())
        return;

    unsigned length = 0;
    JSObject* object = toJSSequence(exec, value, length);
    if (exec->hadException())
        return;

    for (unsigned i = 0 ; i < length; ++i) {
        JSValue itemValue = object->get(exec, i);
        if (exec->hadException())
            return;
        result.append(ustringToString(itemValue.toString(exec)->value(exec)));
    }
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, ScriptValue& result)
{
    result = ScriptValue(exec->globalData(), value);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, RefPtr<SerializedScriptValue>& result)
{
    result = SerializedScriptValue::create(exec, value, 0, 0);
}

void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<DOMWindow>& result)
{
    result = toDOMWindow(value);
}

void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<EventTarget>& result)
{
    result = toEventTarget(value);
}

void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<Node>& result)
{
    result = toNode(value);
}

void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<Storage>& result)
{
    result = toStorage(value);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, MessagePortArray& result)
{
    ArrayBufferArray arrayBuffers;
    fillMessagePortArray(exec, value, result, arrayBuffers);
}

#if ENABLE(VIDEO_TRACK)
void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<TrackBase>& result)
{
    result = toTrack(value);
}
#endif

#if ENABLE(MUTATION_OBSERVERS) || ENABLE(WEB_INTENTS)
void JSDictionary::convertValue(ExecState* exec, JSValue value, HashSet<AtomicString>& result)
{
    result.clear();

    if (value.isUndefinedOrNull())
        return;

    unsigned length = 0;
    JSObject* object = toJSSequence(exec, value, length);
    if (exec->hadException())
        return;

    for (unsigned i = 0 ; i < length; ++i) {
        JSValue itemValue = object->get(exec, i);
        if (exec->hadException())
            return;
        result.add(ustringToAtomicString(itemValue.toString(exec)->value(exec)));
    }
}
#endif

void JSDictionary::convertValue(ExecState* exec, JSValue value, ArrayValue& result)
{
    if (value.isUndefinedOrNull())
        return;

    result = ArrayValue(exec, value);
}

bool JSDictionary::getWithUndefinedOrNullCheck(const String& propertyName, String& result) const
{
    ASSERT(isValid());
    JSValue value;
    if (tryGetProperty(propertyName.utf8().data(), value) != PropertyFound || value.isUndefinedOrNull())
        return false;

    result = ustringToString(value.toString(m_exec)->value(m_exec));
    return true;
}

} // namespace WebCore
