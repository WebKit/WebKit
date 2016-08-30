/*
 * Copyright (C) 2011, 2016 Apple Inc. All rights reserved.
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
#include "DOMWindow.h"
#include "Dictionary.h"
#include "JSCSSFontFaceRule.h"
#include "JSDOMError.h"
#include "JSDOMWindow.h"
#include "JSEventTarget.h"
#include "JSMessagePortCustom.h"
#include "JSNode.h"
#include "JSStorage.h"
#include "JSTrackCustom.h"
#include "JSVoidCallback.h"
#include "SerializedScriptValue.h"
#include <runtime/JSTypedArrays.h>
#include <wtf/HashSet.h>
#include <wtf/MathExtras.h>
#include <wtf/text/AtomicString.h>

#if ENABLE(ENCRYPTED_MEDIA)
#include "JSMediaKeyError.h"
#endif

#if ENABLE(FETCH_API)
#include "JSFetchHeaders.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "JSMediaStream.h"
#include "JSMediaStreamTrack.h"
#include "JSOverconstrainedError.h"
#endif

#if ENABLE(WEB_RTC)
#include "JSRTCRtpReceiver.h"
#include "JSRTCRtpTransceiver.h"
#endif


#if ENABLE(GAMEPAD)
#include "JSGamepad.h"
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(TOUCH_EVENTS)
#include "JSTouchList.h"
#endif

using namespace JSC;

namespace WebCore {

JSDictionary::GetPropertyResult JSDictionary::tryGetProperty(const char* propertyName, JSValue& finalResult) const
{
    ASSERT(isValid());
    Identifier identifier = Identifier::fromString(m_exec, propertyName);
    bool propertyFound = m_initializerObject.get()->getPropertySlot(m_exec, identifier, [&] (bool propertyFound, PropertySlot& slot) -> bool {
        if (!propertyFound)
            return false;
        finalResult = slot.getValue(m_exec, identifier);
        return true;
    });
    if (m_exec->hadException())
        return ExceptionThrown;
    return propertyFound ? PropertyFound : NoPropertyFound;
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, bool& result)
{
    result = value.toBoolean(exec);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, int& result)
{
    result = value.toInt32(exec);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, long int& result)
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

void JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned long& result)
{
    result = static_cast<unsigned long>(value.toUInt32(exec));
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, unsigned long long& result)
{
    double d = value.toNumber(exec);
    doubleToInteger(d, result);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, long long& result)
{
    double d = value.toNumber(exec);
    result = llrint(d);
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
    result = value.toString(exec)->value(exec);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, Vector<String>& result)
{
    ASSERT(exec);
    if (value.isUndefinedOrNull())
        return;

    unsigned length = 0;
    JSObject* object = toJSSequence(*exec, value, length);
    if (exec->hadException())
        return;

    for (unsigned i = 0 ; i < length; ++i) {
        JSValue itemValue = object->get(exec, i);
        if (exec->hadException())
            return;
        result.append(itemValue.toString(exec)->value(exec));
    }
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, Deprecated::ScriptValue& result)
{
    result = Deprecated::ScriptValue(exec->vm(), value);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, RefPtr<SerializedScriptValue>& result)
{
    result = SerializedScriptValue::create(exec, value, 0, 0);
}

void JSDictionary::convertValue(ExecState* state, JSValue value, RefPtr<DOMWindow>& result)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* window = JSDOMWindow::toWrapped(*state, value);
    if (UNLIKELY(!window) && !value.isUndefinedOrNull()) {
        throwVMTypeError(state, scope, "Dictionary member is not of type Window");
        return;
    }
    result = window;
}

void JSDictionary::convertValue(ExecState* state, JSValue value, RefPtr<EventTarget>& result)
{
    result = JSEventTarget::toWrapped(*state, value);
}

void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<Node>& result)
{
    result = JSNode::toWrapped(value);
}

void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<Storage>& result)
{
    result = JSStorage::toWrapped(value);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, MessagePortArray& result)
{
    ArrayBufferArray arrayBuffers;
    fillMessagePortArray(*exec, value, result, arrayBuffers);
}

#if ENABLE(VIDEO_TRACK)
void JSDictionary::convertValue(ExecState*, JSValue value, RefPtr<TrackBase>& result)
{
    result = toTrack(value);
}
#endif

void JSDictionary::convertValue(ExecState* exec, JSValue value, HashSet<AtomicString>& result)
{
    ASSERT(exec);
    result.clear();

    if (value.isUndefinedOrNull())
        return;

    unsigned length = 0;
    JSObject* object = toJSSequence(*exec, value, length);
    if (exec->hadException())
        return;

    for (unsigned i = 0 ; i < length; ++i) {
        JSValue itemValue = object->get(exec, i);
        if (exec->hadException())
            return;
        result.add(itemValue.toString(exec)->value(exec));
    }
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, ArrayValue& result)
{
    if (value.isUndefinedOrNull())
        return;

    result = ArrayValue(exec, value);
}

void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<Uint8Array>& result)
{
    result = toUint8Array(value);
}

#if ENABLE(ENCRYPTED_MEDIA)
void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<MediaKeyError>& result)
{
    result = JSMediaKeyError::toWrapped(value);
}
#endif

#if ENABLE(FETCH_API)
void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<FetchHeaders>& result)
{
    result = JSFetchHeaders::toWrapped(value);
}
#endif

#if ENABLE(MEDIA_STREAM)
void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<MediaStream>& result)
{
    result = JSMediaStream::toWrapped(value);
}

void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<MediaStreamTrack>& result)
{
    result = JSMediaStreamTrack::toWrapped(value);
}

void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<OverconstrainedError>& result)
{
    result = JSOverconstrainedError::toWrapped(value);
}
#endif

#if ENABLE(WEB_RTC)
void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<RTCRtpReceiver>& result)
{
    result = JSRTCRtpReceiver::toWrapped(value);
}

void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<RTCRtpTransceiver>& result)
{
    result = JSRTCRtpTransceiver::toWrapped(value);
}

void JSDictionary::convertValue(ExecState* exec, JSValue value, Vector<RefPtr<MediaStream>>& result)
{
    ASSERT(exec);
    if (value.isUndefinedOrNull())
        return;

    unsigned length = 0;
    JSObject* object = toJSSequence(*exec, value, length);
    if (exec->hadException())
        return;

    for (unsigned i = 0 ; i < length; ++i) {
        JSValue itemValue = object->get(exec, i);
        if (exec->hadException())
            return;

        auto stream = JSMediaStream::toWrapped(itemValue);
        if (!stream) {
            setDOMException(exec, TypeError);
            return;
        }
        result.append(stream);
    }
}
#endif

#if ENABLE(FONT_LOAD_EVENTS)
void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<CSSFontFaceRule>& result)
{
    result = JSCSSFontFaceRule::toWrapped(value);
}

void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<DOMError>& result)
{
    result = JSDOMError::toWrapped(value);
}

void JSDictionary::convertValue(JSC::ExecState* exec, JSC::JSValue value, RefPtr<VoidCallback>& result)
{
    if (!value.isFunction())
        return;

    result = JSVoidCallback::create(asObject(value), jsCast<JSDOMGlobalObject*>(exec->lexicalGlobalObject()));
}
#endif

#if ENABLE(GAMEPAD)
void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<Gamepad>& result)
{
    result = JSGamepad::toWrapped(value);
}
#endif

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(TOUCH_EVENTS)
void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, RefPtr<TouchList>& result)
{
    result = JSTouchList::toWrapped(value);
}
#endif

void JSDictionary::convertValue(JSC::ExecState*, JSC::JSValue value, JSC::JSFunction*& result)
{
    result = jsDynamicCast<JSC::JSFunction*>(value);
}

bool JSDictionary::getWithUndefinedOrNullCheck(const char* propertyName, String& result) const
{
    ASSERT(isValid());
    JSValue value;
    if (tryGetProperty(propertyName, value) != PropertyFound || value.isUndefinedOrNull())
        return false;

    result = value.toWTFString(m_exec);
    return true;
}

} // namespace WebCore
