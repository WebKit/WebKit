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

#if ENABLE(IPC_TESTING_API)

#include "IPCConnectionTesterIdentifier.h"
#include <optional>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

// Types that IPCTester sends to IPCTesterReceiver to check that its Swift receiver decodes and
// encodes them exactly as C++ does, whether it takes them as C++ types or as Swift types.

enum class IPCTesterColor : uint8_t {
    Red,
    Green,
    Blue,
};

enum class IPCTesterFlag : uint8_t {
    First = 1 << 0,
    Second = 1 << 1,
    Third = 1 << 2,
};

enum class IPCTesterChoice : bool {
    No,
    Yes,
};

struct IPCTesterInner {
    int32_t value { 0 };
    String label;

    friend bool operator==(const IPCTesterInner&, const IPCTesterInner&) = default;
};

using IPCTesterVariant = Variant<uint64_t, String, IPCTesterInner>;

struct IPCTesterPayload {
    bool flag { false };
    uint8_t u8 { 0 };
    uint16_t u16 { 0 };
    uint32_t u32 { 0 };
    uint64_t u64 { 0 };
    int8_t i8 { 0 };
    int16_t i16 { 0 };
    int32_t i32 { 0 };
    int64_t i64 { 0 };
    float f { 0 };
    double d { 0 };
    String text { };
    String nullText { };
    Vector<uint32_t> numbers { };
    Vector<String> strings { };
    std::optional<IPCTesterColor> color { };
    OptionSet<IPCTesterFlag> flags { };
    IPCTesterChoice choice { IPCTesterChoice::No };
    IPCTesterInner inner { };
    Vector<IPCTesterInner> inners { };
    IPCTesterVariant variant { };
    Markable<IPCConnectionTesterIdentifier> identifier { };

    friend bool operator==(const IPCTesterPayload&, const IPCTesterPayload&) = default;
};

class IPCTesterNode : public RefCounted<IPCTesterNode> {
public:
    static Ref<IPCTesterNode> create(String&& name, Vector<Ref<IPCTesterNode>>&& children, RefPtr<IPCTesterNode>&& link)
    {
        return adoptRef(*new IPCTesterNode(WTF::move(name), WTF::move(children), WTF::move(link)));
    }

    const String& name() const { return m_name; }
    const Vector<Ref<IPCTesterNode>>& children() const { return m_children; }
    const RefPtr<IPCTesterNode>& link() const { return m_link; }

private:
    IPCTesterNode(String&& name, Vector<Ref<IPCTesterNode>>&& children, RefPtr<IPCTesterNode>&& link)
        : m_name(WTF::move(name))
        , m_children(WTF::move(children))
        , m_link(WTF::move(link))
    {
    }

    String m_name;
    Vector<Ref<IPCTesterNode>> m_children;
    RefPtr<IPCTesterNode> m_link;
};

// What IPCTesterReceiver replies, which its Swift (IPCTesterReceiver.swift) and C++ implementations
// compute identically. Each changes every field, so that a field decoded or encoded in the wrong place
// shows up even though it went through both a decoder and an encoder.

inline String ipcTesterExclaimed(const String& string)
{
    return string.isNull() ? String() : makeString(string, '!');
}

inline IPCTesterInner ipcTesterTransformed(const IPCTesterInner& inner)
{
    return { static_cast<int32_t>(0u - static_cast<uint32_t>(inner.value)), ipcTesterExclaimed(inner.label) };
}

inline IPCTesterPayload ipcTesterTransformed(const IPCTesterPayload& payload)
{
    IPCTesterPayload result = payload;
    result.flag = !payload.flag;
    result.u8 = payload.u8 + 1;
    result.u16 = payload.u16 + 1;
    result.u32 = payload.u32 + 1;
    result.u64 = payload.u64 + 1;
    result.i8 = static_cast<int8_t>(static_cast<uint8_t>(payload.i8) - 1);
    result.i16 = static_cast<int16_t>(static_cast<uint16_t>(payload.i16) - 1);
    result.i32 = static_cast<int32_t>(static_cast<uint32_t>(payload.i32) - 1);
    result.i64 = static_cast<int64_t>(static_cast<uint64_t>(payload.i64) - 1);
    result.f = payload.f * 2;
    result.d = payload.d * 2;
    result.text = ipcTesterExclaimed(payload.text);
    result.numbers.reverse();
    result.strings.reverse();
    if (payload.color)
        result.color = *payload.color == IPCTesterColor::Red ? IPCTesterColor::Green : *payload.color == IPCTesterColor::Green ? IPCTesterColor::Blue : IPCTesterColor::Red;
    result.flags = payload.flags ^ OptionSet<IPCTesterFlag> { IPCTesterFlag::First, IPCTesterFlag::Second, IPCTesterFlag::Third };
    result.choice = payload.choice == IPCTesterChoice::Yes ? IPCTesterChoice::No : IPCTesterChoice::Yes;
    result.inner = ipcTesterTransformed(payload.inner);
    result.inners = payload.inners.map([](auto& inner) {
        return ipcTesterTransformed(inner);
    });
    result.variant = WTF::switchOn(payload.variant, [](uint64_t value) -> IPCTesterVariant {
        return value + 1;
    }, [](const String& value) -> IPCTesterVariant {
        return ipcTesterExclaimed(value);
    }, [](const IPCTesterInner& value) -> IPCTesterVariant {
        return ipcTesterTransformed(value);
    });
    return result;
}

inline Ref<IPCTesterNode> ipcTesterMirrored(const IPCTesterNode& node)
{
    auto children = node.children().map([](auto& child) {
        return ipcTesterMirrored(child);
    });
    children.reverse();
    RefPtr<IPCTesterNode> link;
    if (RefPtr nodeLink = node.link())
        link = ipcTesterMirrored(*nodeLink);
    return IPCTesterNode::create(ipcTesterExclaimed(node.name()), WTF::move(children), WTF::move(link));
}

inline uint32_t ipcTesterNodeCount(const IPCTesterNode& node)
{
    uint32_t count = 1;
    for (auto& child : node.children())
        count += ipcTesterNodeCount(child);
    if (RefPtr link = node.link())
        count += ipcTesterNodeCount(*link);
    return count;
}

inline String ipcTesterDescription(IPCTesterColor color, OptionSet<IPCTesterFlag> flags, IPCTesterChoice choice, IPCConnectionTesterIdentifier identifier, const IPCTesterVariant& variant, bool flag)
{
    auto describeString = [](const String& string) -> String {
        return string.isNull() ? "null"_s : string;
    };
    StringBuilder builder;
    builder.append("color="_s, color == IPCTesterColor::Red ? "red"_s : color == IPCTesterColor::Green ? "green"_s : "blue"_s);
    builder.append(" flags="_s);
    if (flags.isEmpty())
        builder.append("none"_s);
    bool needsSeparator = false;
    for (auto [value, name] : { std::pair { IPCTesterFlag::First, "first"_s }, std::pair { IPCTesterFlag::Second, "second"_s }, std::pair { IPCTesterFlag::Third, "third"_s } }) {
        if (!flags.contains(value))
            continue;
        if (needsSeparator)
            builder.append('|');
        builder.append(name);
        needsSeparator = true;
    }
    builder.append(" choice="_s, choice == IPCTesterChoice::Yes ? "yes"_s : "no"_s);
    builder.append(" identifier="_s, identifier.toUInt64());
    builder.append(" variant="_s);
    WTF::switchOn(variant, [&](uint64_t value) {
        builder.append("uint64:"_s, value);
    }, [&](const String& value) {
        builder.append("string:"_s, describeString(value));
    }, [&](const IPCTesterInner& value) {
        builder.append("inner:"_s, value.value, ':', describeString(value.label));
    });
    builder.append(" flag="_s, flag ? "true"_s : "false"_s);
    return builder.toString();
}

} // namespace WebKit

#endif // ENABLE(IPC_TESTING_API)
