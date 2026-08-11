/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/XPathNodeSet.h>
#include <wtf/Variant.h>

namespace WebCore::XPath {

class Value {
public:
    enum class Type : uint8_t { NodeSet, Boolean, Number, String };

    Value() = delete;

    Value(bool value)
        : m_value(value)
    { }
    Value(unsigned value)
        : m_value(static_cast<double>(value))
    { }
    Value(double value)
        : m_value(value)
    { }

    Value(const String& value)
        : m_value(value)
    { }

    explicit Value(NodeSet&& value)
        : m_value(NodeSetHolder::create(WTF::move(value)))
    { }

    bool isBoolean() const { return WTF::holdsAlternative<bool>(m_value); }
    bool isNumber() const { return WTF::holdsAlternative<double>(m_value); }
    bool isString() const { return WTF::holdsAlternative<String>(m_value); }
    bool isNodeSet() const { return WTF::holdsAlternative<Ref<NodeSetHolder>>(m_value); }

    const NodeSet& toNodeSet() const;
    bool toBoolean() const;
    double toNumber() const;
    String toString() const;

    // Note that the NodeSet is shared with other Values that this one was copied from or that are copies of this one.
    NodeSet& modifiableNodeSet();

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        return WTF::switchOn(m_value,
            [&](bool value) {
                return visitor(value);
            },
            [&](double value) {
                return visitor(value);
            },
            [&](const String& string) {
                return visitor(string);
            },
            [&](const Ref<NodeSetHolder>& holder) {
                return visitor(holder->nodeSet);
            }
        );
    }

private:
    // This constructor creates ambiguity so that we don't accidentally call the boolean overload for pointer types.
    Value(void*) = delete;

    struct NodeSetHolder : public RefCounted<NodeSetHolder> {
        static Ref<NodeSetHolder> create(NodeSet&& nodeSet) { return adoptRef(*new NodeSetHolder(WTF::move(nodeSet))); }
        NodeSet nodeSet;
    private:
        explicit NodeSetHolder(NodeSet&& ns)
            : nodeSet(WTF::move(ns))
        { }
    };

    Variant<bool, double, String, Ref<NodeSetHolder>> m_value;
};

} // namespace WebCore::XPath
