/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef APISerializedScriptValue_h
#define APISerializedScriptValue_h

#include "APIObject.h"

#include "DataReference.h"
#include <WebCore/SerializedScriptValue.h>
#include <wtf/RefPtr.h>

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>

typedef struct _GVariant GVariant;
typedef struct _JSCContext JSCContext;
typedef struct _JSCValue JSCValue;
#endif

namespace API {

class SerializedScriptValue : public API::ObjectImpl<API::Object::Type::SerializedScriptValue> {
public:
    static Ref<SerializedScriptValue> create(Ref<WebCore::SerializedScriptValue>&& serializedValue)
    {
        return adoptRef(*new SerializedScriptValue(WTFMove(serializedValue)));
    }
    
    static RefPtr<SerializedScriptValue> create(JSContextRef context, JSValueRef value, JSValueRef* exception)
    {
        RefPtr<WebCore::SerializedScriptValue> serializedValue = WebCore::SerializedScriptValue::create(context, value, exception);
        if (!serializedValue)
            return nullptr;
        return adoptRef(*new SerializedScriptValue(serializedValue.releaseNonNull()));
    }
    
    static Ref<SerializedScriptValue> createFromWireBytes(Vector<uint8_t>&& buffer)
    {
        return adoptRef(*new SerializedScriptValue(WebCore::SerializedScriptValue::createFromWireBytes(WTFMove(buffer))));
    }
    
    JSValueRef deserialize(JSContextRef context, JSValueRef* exception)
    {
        return m_serializedScriptValue->deserialize(context, exception);
    }
    
#if PLATFORM(COCOA) && defined(__OBJC__)
    static id deserialize(WebCore::SerializedScriptValue&, JSValueRef* exception);
    static RefPtr<SerializedScriptValue> createFromNSObject(id);
#endif

#if USE(GLIB)
    static JSCContext* sharedJSCContext();
    static GRefPtr<JSCValue> deserialize(WebCore::SerializedScriptValue&);
    static RefPtr<SerializedScriptValue> createFromGVariant(GVariant*);
    static RefPtr<SerializedScriptValue> createFromJSCValue(JSCValue*);
#endif

    IPC::DataReference dataReference() const { return m_serializedScriptValue->wireBytes(); }

    WebCore::SerializedScriptValue& internalRepresentation() { return m_serializedScriptValue.get(); }

private:
    explicit SerializedScriptValue(Ref<WebCore::SerializedScriptValue>&& serializedScriptValue)
        : m_serializedScriptValue(WTFMove(serializedScriptValue))
    {
    }

    Ref<WebCore::SerializedScriptValue> m_serializedScriptValue;
};
    
}

#endif
