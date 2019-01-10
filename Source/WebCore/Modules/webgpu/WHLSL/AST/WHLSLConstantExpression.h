/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include "WHLSLBooleanLiteral.h"
#include "WHLSLConstantExpressionEnumerationMemberReference.h"
#include "WHLSLFloatLiteral.h"
#include "WHLSLIntegerLiteral.h"
#include "WHLSLNullLiteral.h"
#include "WHLSLUnsignedIntegerLiteral.h"
#include <wtf/Variant.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

// FIXME: macOS Sierra doesn't seem to support putting Variants inside Variants,
// so this is a wrapper class to make sure that doesn't happen. As soon as we don't
// have to support Sierra, this can be migrated to a Variant proper.
class ConstantExpression {
public:
    ConstantExpression(IntegerLiteral&& integerLiteral)
        : m_variant(WTFMove(integerLiteral))
    {
    }

    ConstantExpression(UnsignedIntegerLiteral&& unsignedIntegerLiteral)
        : m_variant(WTFMove(unsignedIntegerLiteral))
    {
    }

    ConstantExpression(FloatLiteral&& floatLiteral)
        : m_variant(WTFMove(floatLiteral))
    {
    }

    ConstantExpression(NullLiteral&& nullLiteral)
        : m_variant(WTFMove(nullLiteral))
    {
    }

    ConstantExpression(BooleanLiteral&& booleanLiteral)
        : m_variant(WTFMove(booleanLiteral))
    {
    }

    ConstantExpression(ConstantExpressionEnumerationMemberReference&& constantExpressionEnumerationMemberReference)
        : m_variant(WTFMove(constantExpressionEnumerationMemberReference))
    {
    }

    ConstantExpression(const ConstantExpression&) = delete;
    ConstantExpression(ConstantExpression&&) = default;

    ConstantExpression& operator=(const ConstantExpression&) = delete;
    ConstantExpression& operator=(ConstantExpression&&) = default;

    IntegerLiteral& integerLiteral()
    {
        ASSERT(WTF::holds_alternative<IntegerLiteral>(m_variant));
        return WTF::get<IntegerLiteral>(m_variant);
    }

    template<typename T> void visit(T&& t)
    {
        WTF::visit(WTFMove(t), m_variant);
    }

    template<typename T> void visit(T&& t) const
    {
        WTF::visit(WTFMove(t), m_variant);
    }

    ConstantExpression clone() const
    {
        return WTF::visit(WTF::makeVisitor([&](const IntegerLiteral& integerLiteral) -> ConstantExpression {
            return integerLiteral.clone();
        }, [&](const UnsignedIntegerLiteral& unsignedIntegerLiteral) -> ConstantExpression {
            return unsignedIntegerLiteral.clone();
        }, [&](const FloatLiteral& floatLiteral) -> ConstantExpression {
            return floatLiteral.clone();
        }, [&](const NullLiteral& nullLiteral) -> ConstantExpression {
            return nullLiteral.clone();
        }, [&](const BooleanLiteral& booleanLiteral) -> ConstantExpression {
            return booleanLiteral.clone();
        }, [&](const ConstantExpressionEnumerationMemberReference& constantExpressionEnumerationMemberReference) -> ConstantExpression {
            return constantExpressionEnumerationMemberReference.clone();
        }), m_variant);
    }

    bool matches(const ConstantExpression& other) const
    {
        Optional<bool> result;
        double value;
        visit(WTF::makeVisitor([&](const IntegerLiteral& integerLiteral) {
            value = integerLiteral.value();
        }, [&](const UnsignedIntegerLiteral& unsignedIntegerLiteral) {
            value = unsignedIntegerLiteral.value();
        }, [&](const FloatLiteral& floatLiteral) {
            value = floatLiteral.value();
        }, [&](const NullLiteral&) {
            result = WTF::holds_alternative<NullLiteral>(other.m_variant);
        }, [&](const BooleanLiteral& booleanLiteral) {
            if (WTF::holds_alternative<BooleanLiteral>(other.m_variant)) {
                const auto& otherBooleanLiteral = WTF::get<BooleanLiteral>(other.m_variant);
                result = booleanLiteral.value() == otherBooleanLiteral.value();
            } else
                result = false;
        }, [&](const ConstantExpressionEnumerationMemberReference& constantExpressionEnumerationMemberReference) {
            if (WTF::holds_alternative<ConstantExpressionEnumerationMemberReference>(other.m_variant)) {
                const auto& otherMemberReference = WTF::get<ConstantExpressionEnumerationMemberReference>(other.m_variant);
                result = constantExpressionEnumerationMemberReference.enumerationMember() == otherMemberReference.enumerationMember();
            } else
                result = false;
        }));

        if (result)
            return *result;

        other.visit(WTF::makeVisitor([&](const IntegerLiteral& integerLiteral) {
            result = value == integerLiteral.value();
        }, [&](const UnsignedIntegerLiteral& unsignedIntegerLiteral) {
            result = value == unsignedIntegerLiteral.value();
        }, [&](const FloatLiteral& floatLiteral) {
            result = value == floatLiteral.value();
        }, [&](const NullLiteral&) {
            result = false;
        }, [&](const BooleanLiteral&) {
            result = false;
        }, [&](const ConstantExpressionEnumerationMemberReference&) {
            result = false;
        }));

        ASSERT(result);
        return *result;
    }

private:
    Variant<
        IntegerLiteral,
        UnsignedIntegerLiteral,
        FloatLiteral,
        NullLiteral,
        BooleanLiteral,
        ConstantExpressionEnumerationMemberReference
        > m_variant;
};

} // namespace AST

}

}

#endif
