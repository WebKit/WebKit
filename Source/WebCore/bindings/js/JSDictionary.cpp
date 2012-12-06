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
#include "JSUint8Array.h"
#include "ScriptValue.h"
#include "SerializedScriptValue.h"
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/text/AtomicString.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "JSMediaKeyError.h"
#endif

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

bool JSDictionary::convertValue(ExecState* exec, JSValue value, bool& result)
{
    result = value.toBoolean(exec);
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, int& result)
{
    result = value.toInt32(exec);
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned& result)
{
    result = value.toUInt32(exec);
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned short& result)
{
    result = static_cast<unsigned short>(value.toUInt32(exec));
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned long& result)
{
    result = static_cast<unsigned long>(value.toUInt32(exec));
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned long long& result)
{
    double d = value.toNumber(exec);
    doubleToInteger(d, result);
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, double& result)
{
    result = value.toNumber(exec);
    return true;
}

bool JSDictionary::convertValue(JSC::ExecState* exec, JSC::JSValue value, Dictionary& result)
{
    result = Dictionary(exec, value);
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, String& result)
{
    result = value.toString(exec)->value(exec);
    return !exec->hadException();
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, Vector<String>& result)
{
    if (value.isUndefinedOrNull())
        return false;

    if (!value.isObject())
        return false;
    JSObject* object = asObject(value);
    if (!isJSArray(object) && !object->inherits(&JSArray::s_info))
        return false;

    unsigned length = 0;
    object = toJSSequence(exec, value, length);
    if (exec->hadException())
        return false;

    for (unsigned i = 0 ; i < length; ++i) {
        JSValue itemValue = object->get(exec, i);
        if (exec->hadException())
            return false;
        result.append(itemValue.toString(exec)->value(exec));
    }

    return !exec->hadException();
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, ScriptValue& result)
{
    result = ScriptValue(exec->globalData(), value);
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, RefPtr<SerializedScriptValue>& result)
{
    result = SerializedScriptValue::create(exec, value, 0, 0);
    return !exec->hadException();
}

bool JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<DOMWindow>& result)
{
    result = toDOMWindow(value);
    return true;
}

bool JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<EventTarget>& result)
{
    result = toEventTarget(value);
    return true;
}

bool JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<Node>& result)
{
    result = toNode(value);
    return true;
}

bool JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<Storage>& result)
{
    result = toStorage(value);
    return true;
}

bool JSDictionary::convertValue(ExecState* exec, JSValue value, MessagePortArray& result)
{
    ArrayBufferArray arrayBuffers;
    fillMessagePortArray(exec, value, result, arrayBuffers);
    return true;
}

#if ENABLE(VIDEO_TRACK)
bool JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<TrackBase>& result)
{
    result = toTrack(value);
    return true;
}
#endif

#if ENABLE(MUTATION_OBSERVERS) || ENABLE(WEB_INTENTS)
bool JSDictionary::convertValue(ExecState* exec, JSValue value, HashSet<AtomicString>& result)
{
    result.clear();

    if (value.isUndefinedOrNull())
        return false;

    unsigned length = 0;
    JSObject* object = toJSSequence(exec, value, length);
    if (exec->hadException())
        return false;

    for (unsigned i = 0 ; i < length; ++i) {
        JSValue itemValue = object->get(exec, i);
        if (exec->hadException())
            return false;
        result.add(itemValue.toString(exec)->value(exec));
    }

    return !exec->hadException();
}
#endif

bool JSDictionary::convertValue(ExecState* exec, JSValue value, ArrayValue& result)
{
    if (value.isUndefinedOrNull())
        return false;

    result = ArrayValue(exec, value);
    return true;
}

bool JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<Uint8Array>& result)
{
    result = toUint8Array(value);
    return true;
}

#if ENABLE(ENCRYPTED_MEDIA)
bool JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<MediaKeyError>& result)
{
    result = toMediaKeyError(value);
    return true;
}
#endif

bool JSDictionary::getWithUndefinedOrNullCheck(const String& propertyName, String& result) const
{
    ASSERT(isValid());
    JSValue value;
    if (tryGetProperty(propertyName.utf8().data(), value) != PropertyFound || value.isUndefinedOrNull())
        return false;

    result = value.toWTFString(m_exec);
    return true;
}

} // namespace WebCore
