/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include "CloneDeserializerBase.h"
#include "CloneSerializerBase.h"
#include "Message.h"

namespace JSCShell {

class JSCCloneSerializer final : public JSC::CloneSerializerBase<JSCCloneSerializer> {
    using Base = JSC::CloneSerializerBase<JSCCloneSerializer>;
public:
    struct Result {
        JSC::SerializationReturnCode code;
        SideChannels sideChannels;
    };

    static Result serialize(JSC::JSGlobalObject* globalObject, JSC::JSValue value, Vector<uint8_t>& out, const JSC::ArgList& transferList = JSC::ArgList())
    {
        JSCCloneSerializer serializer(globalObject, out);
        for (unsigned i = 0; i < transferList.size(); ++i)
            serializer.m_transferredArrayBuffers.add(asObject(transferList.at(i)), i);
        auto code = serializer.Base::serialize(value);
        return { code, serializer.takeSideChannels() };
    }

    bool dumpDerivedTerminal(JSC::JSObject*, JSC::SerializationReturnCode&) { return false; }

private:
    JSCCloneSerializer(JSC::JSGlobalObject* globalObject, Vector<uint8_t>& out)
        : Base(globalObject, out)
    {
        write(JSC::currentVersion());
    }
};
static_assert(JSC::StructuredCloneSerializerHandler<JSCCloneSerializer>);

class JSCCloneDeserializer final : public JSC::CloneDeserializerBase<JSCCloneDeserializer> {
    using Base = JSC::CloneDeserializerBase<JSCCloneDeserializer>;
public:
    static JSC::DeserializationResult deserialize(JSC::JSGlobalObject* globalObject, std::span<const uint8_t> data,
        JSC::CloneDeserializationSideChannels sideChannels)
    {
        JSCCloneDeserializer deserializer(globalObject, data, sideChannels);
        if (!deserializer.isValid())
            return { JSC::JSValue(), JSC::SerializationReturnCode::ValidationError };
        return deserializer.Base::deserialize();
    }

    bool isTagExposed(JSC::SerializationTag) const { return true; }
    // An empty JSValue tells CloneDeserializerBase::readTerminal() that the tag it just consumed
    // isn't a terminal after all, so it should rewind and let the walker re-read the tag as the
    // start of an Array/Object/Map/Set.
    JSC::JSValue readDerivedTerminal(JSC::SerializationTag) { return { }; }

private:
    JSCCloneDeserializer(JSC::JSGlobalObject* globalObject, std::span<const uint8_t> data, JSC::CloneDeserializationSideChannels sideChannels)
        : Base(globalObject, globalObject, data, sideChannels)
    {
        readAndStoreVersion();
    }
};
static_assert(JSC::StructuredCloneDeserializerHandler<JSCCloneDeserializer>);

JSC::EncodedJSValue throwSerializationError(JSC::JSGlobalObject*, JSC::ThrowScope&, JSC::SerializationReturnCode);

RefPtr<Message> serializeMessage(JSC::JSGlobalObject*, JSC::JSValue, JSC::JSValue transfer, ASCIILiteral apiName);

} // namespace JSCShell
