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

#include "config.h"
#include "XPathValue.h"

#include "CommonAtomStrings.h"
#include "XPathExpressionNode.h"
#include "XPathUtil.h"
#include <limits>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore::XPath {

const NodeSet& Value::toNodeSet() const
{
    if (!isNodeSet()) {
        Expression::evaluationContext().hadTypeConversionError = true;
        static NeverDestroyed<NodeSet> emptyNodeSet;
        return emptyNodeSet;
    }

    return std::get<Ref<NodeSetHolder>>(m_value)->nodeSet;
}

NodeSet& Value::modifiableNodeSet()
{
    if (!isNodeSet()) {
        Expression::evaluationContext().hadTypeConversionError = true;
        m_value = NodeSetHolder::create(NodeSet { });
    }

    return std::get<Ref<NodeSetHolder>>(m_value)->nodeSet;
}

bool Value::toBoolean() const
{
    return switchOn(
        [](bool value) {
            return value;
        },
        [](double value) {
            return value && !std::isnan(value);
        },
        [](const String& string) {
            return !string.isEmpty();
        },
        [](const NodeSet& nodeSet) {
            return !nodeSet.isEmpty();
        }
    );
}

double Value::toNumber() const
{
    return switchOn(
        [](bool value) -> double {
            return value;
        },
        [](double value) {
            return value;
        },
        [](const String& string) -> double {
            auto simplified = string.simplifyWhiteSpace(deprecatedIsSpaceOrNewline);

            // String::toDouble() supports exponential notation, which is not allowed in XPath.
            unsigned len = simplified.length();
            for (unsigned i = 0; i < len; ++i) {
                char16_t c = simplified[i];
                if (!isASCIIDigit(c) && c != '.' && c != '-')
                    return std::numeric_limits<double>::quiet_NaN();
            }

            bool canConvert;
            auto value = simplified.toDouble(&canConvert);
            if (canConvert)
                return value;
            return std::numeric_limits<double>::quiet_NaN();
        },
        [this](const NodeSet&) {
            return Value(toString()).toNumber();
        }
    );
}

String Value::toString() const
{
    return switchOn(
        [](bool value) -> String {
            return value ? trueAtom() : falseAtom();
        },
        [](double value) -> String {
            if (std::isnan(value))
                return "NaN"_s;
            if (!value)
                return "0"_s;
            if (std::isinf(value))
                return std::signbit(value) ? "-Infinity"_s : "Infinity"_s;
            return String::number(value);
        },
        [](const String& string) {
            return string;
        },
        [](const NodeSet& nodeSet) -> String {
            if (nodeSet.isEmpty())
                return emptyString();
            return stringValue(Ref { *nodeSet.firstNode() });
        }
    );
}

} // namespace WebCore::XPath
