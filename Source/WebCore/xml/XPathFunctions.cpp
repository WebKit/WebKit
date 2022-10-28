/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
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
#include "XPathFunctions.h"

#include "Attribute.h"
#include "ElementInlines.h"
#include "ProcessingInstruction.h"
#include "TreeScope.h"
#include "XMLNames.h"
#include "XPathUtil.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace XPath {

static inline bool isWhitespace(UChar c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

#define DEFINE_FUNCTION_CREATOR(Suffix) static std::unique_ptr<Function> createFunction##Suffix() { return makeUnique<Fun##Suffix>(); }

class Interval {
public:
    static const int Inf = -1;

    Interval();
    Interval(int value);
    Interval(int min, int max);

    bool contains(int value) const;

private:
    int m_min;
    int m_max;
};

class FunLast final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
public:
    FunLast() { setIsContextSizeSensitive(true); }
};

class FunPosition final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
public:
    FunPosition() { setIsContextPositionSensitive(true); }
};

class FunCount final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
};

class FunId final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NodeSetValue; }
};

class FunLocalName final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
public:
    FunLocalName() { setIsContextNodeSensitive(true); } // local-name() with no arguments uses context node. 
};

class FunNamespaceURI final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
public:
    FunNamespaceURI() { setIsContextNodeSensitive(true); } // namespace-uri() with no arguments uses context node. 
};

class FunName final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
public:
    FunName() { setIsContextNodeSensitive(true); } // name() with no arguments uses context node. 
};

class FunString final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
public:
    FunString() { setIsContextNodeSensitive(true); } // string() with no arguments uses context node. 
};

class FunConcat final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
};

class FunStartsWith final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::BooleanValue; }
};

class FunContains final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::BooleanValue; }
};

class FunSubstringBefore final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
};

class FunSubstringAfter final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
};

class FunSubstring final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
};

class FunStringLength final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
public:
    FunStringLength() { setIsContextNodeSensitive(true); } // string-length() with no arguments uses context node. 
};

class FunNormalizeSpace final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
public:
    FunNormalizeSpace() { setIsContextNodeSensitive(true); } // normalize-space() with no arguments uses context node. 
};

class FunTranslate final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::StringValue; }
};

class FunBoolean final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::BooleanValue; }
};

class FunNot : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::BooleanValue; }
};

class FunTrue final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::BooleanValue; }
};

class FunFalse final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::BooleanValue; }
};

class FunLang final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::BooleanValue; }
public:
    FunLang() { setIsContextNodeSensitive(true); } // lang() always works on context node. 
};

class FunNumber final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
public:
    FunNumber() { setIsContextNodeSensitive(true); } // number() with no arguments uses context node. 
};

class FunSum final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
};

class FunFloor final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
};

class FunCeiling final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
};

class FunRound final : public Function {
    Value evaluate() const override;
    Value::Type resultType() const override { return Value::NumberValue; }
public:
    static double round(double);
};

DEFINE_FUNCTION_CREATOR(Last)
DEFINE_FUNCTION_CREATOR(Position)
DEFINE_FUNCTION_CREATOR(Count)
DEFINE_FUNCTION_CREATOR(Id)
DEFINE_FUNCTION_CREATOR(LocalName)
DEFINE_FUNCTION_CREATOR(NamespaceURI)
DEFINE_FUNCTION_CREATOR(Name)

DEFINE_FUNCTION_CREATOR(String)
DEFINE_FUNCTION_CREATOR(Concat)
DEFINE_FUNCTION_CREATOR(StartsWith)
DEFINE_FUNCTION_CREATOR(Contains)
DEFINE_FUNCTION_CREATOR(SubstringBefore)
DEFINE_FUNCTION_CREATOR(SubstringAfter)
DEFINE_FUNCTION_CREATOR(Substring)
DEFINE_FUNCTION_CREATOR(StringLength)
DEFINE_FUNCTION_CREATOR(NormalizeSpace)
DEFINE_FUNCTION_CREATOR(Translate)

DEFINE_FUNCTION_CREATOR(Boolean)
DEFINE_FUNCTION_CREATOR(Not)
DEFINE_FUNCTION_CREATOR(True)
DEFINE_FUNCTION_CREATOR(False)
DEFINE_FUNCTION_CREATOR(Lang)

DEFINE_FUNCTION_CREATOR(Number)
DEFINE_FUNCTION_CREATOR(Sum)
DEFINE_FUNCTION_CREATOR(Floor)
DEFINE_FUNCTION_CREATOR(Ceiling)
DEFINE_FUNCTION_CREATOR(Round)

#undef DEFINE_FUNCTION_CREATOR

inline Interval::Interval()
    : m_min(Inf), m_max(Inf)
{
}

inline Interval::Interval(int value)
    : m_min(value), m_max(value)
{
}

inline Interval::Interval(int min, int max)
    : m_min(min), m_max(max)
{
}

inline bool Interval::contains(int value) const
{
    if (m_min == Inf && m_max == Inf)
        return true;

    if (m_min == Inf)
        return value <= m_max;

    if (m_max == Inf)
        return value >= m_min;

    return value >= m_min && value <= m_max;
}

void Function::setArguments(const String& name, Vector<std::unique_ptr<Expression>> arguments)
{
    ASSERT(!subexpressionCount());

    // Functions that use the context node as an implicit argument are context node sensitive when they
    // have no arguments, but when explicit arguments are added, they are no longer context node sensitive.
    // As of this writing, the only exception to this is the "lang" function.
    if (name != "lang"_s && !arguments.isEmpty())
        setIsContextNodeSensitive(false);

    setSubexpressions(WTFMove(arguments));
}

Value FunLast::evaluate() const
{
    return Expression::evaluationContext().size;
}

Value FunPosition::evaluate() const
{
    return Expression::evaluationContext().position;
}

Value FunId::evaluate() const
{
    Value a = argument(0).evaluate();
    StringBuilder idList; // A whitespace-separated list of IDs

    if (!a.isNodeSet())
        idList.append(a.toString());
    else {
        for (auto& node : a.toNodeSet()) {
            idList.append(stringValue(node.get()));
            idList.append(' ');
        }
    }
    
    TreeScope& contextScope = evaluationContext().node->treeScope();
    NodeSet result;
    HashSet<Ref<Node>> resultSet;

    unsigned startPos = 0;
    unsigned length = idList.length();
    while (true) {
        while (startPos < length && isWhitespace(idList[startPos]))
            ++startPos;
        
        if (startPos == length)
            break;

        size_t endPos = startPos;
        while (endPos < length && !isWhitespace(idList[endPos]))
            ++endPos;

        // If there are several nodes with the same id, id() should return the first one.
        // In WebKit, getElementById behaves so, too, although its behavior in this case is formally undefined.
        Node* node = contextScope.getElementById(StringView(idList).substring(startPos, endPos - startPos));
        if (node && resultSet.add(*node).isNewEntry)
            result.append(node);
        
        startPos = endPos;
    }
    
    result.markSorted(false);
    
    return Value(WTFMove(result));
}

static inline String expandedNameLocalPart(Node* node)
{
    if (is<ProcessingInstruction>(*node))
        return downcast<ProcessingInstruction>(*node).target();
    return node->localName().string();
}

static inline String expandedName(Node* node)
{
    const AtomString& prefix = node->prefix();
    return prefix.isEmpty() ? expandedNameLocalPart(node) : prefix + ":" + expandedNameLocalPart(node);
}

Value FunLocalName::evaluate() const
{
    if (argumentCount() > 0) {
        Value a = argument(0).evaluate();
        if (!a.isNodeSet())
            return emptyString();

        Node* node = a.toNodeSet().firstNode();
        return node ? expandedNameLocalPart(node) : emptyString();
    }

    return expandedNameLocalPart(evaluationContext().node.get());
}

Value FunNamespaceURI::evaluate() const
{
    if (argumentCount() > 0) {
        Value a = argument(0).evaluate();
        if (!a.isNodeSet())
            return emptyString();

        Node* node = a.toNodeSet().firstNode();
        return node ? node->namespaceURI().string() : emptyString();
    }

    return evaluationContext().node->namespaceURI().string();
}

Value FunName::evaluate() const
{
    if (argumentCount() > 0) {
        Value a = argument(0).evaluate();
        if (!a.isNodeSet())
            return emptyString();

        Node* node = a.toNodeSet().firstNode();
        return node ? expandedName(node) : emptyString();
    }

    return expandedName(evaluationContext().node.get());
}

Value FunCount::evaluate() const
{
    Value a = argument(0).evaluate();
    
    return double(a.toNodeSet().size());
}

Value FunString::evaluate() const
{
    if (!argumentCount())
        return Value(Expression::evaluationContext().node.get()).toString();
    return argument(0).evaluate().toString();
}

Value FunConcat::evaluate() const
{
    StringBuilder result;
    result.reserveCapacity(1024);

    for (unsigned i = 0, count = argumentCount(); i < count; ++i) {
        EvaluationContext clonedContext(Expression::evaluationContext());
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext);
        result.append(argument(i).evaluate().toString());
    }

    return result.toString();
}

Value FunStartsWith::evaluate() const
{
    auto clonedContext = Expression::evaluationContext();

    String s1 = argument(0).evaluate().toString();
    String s2;
    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext);
        s2 = argument(1).evaluate().toString();
    }

    if (s2.isEmpty())
        return true;

    return s1.startsWith(s2);
}

Value FunContains::evaluate() const
{
    auto clonedContext = Expression::evaluationContext();

    String s1 = argument(0).evaluate().toString();
    String s2;
    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext);
        s2 = argument(1).evaluate().toString();
    }

    if (s2.isEmpty()) 
        return true;

    return s1.contains(s2) != 0;
}

Value FunSubstringBefore::evaluate() const
{
    auto clonedContext = Expression::evaluationContext();

    String s1 = argument(0).evaluate().toString();
    String s2;

    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext);
        s2 = argument(1).evaluate().toString();
    }

    if (s2.isEmpty())
        return emptyString();

    size_t i = s1.find(s2);

    if (i == notFound)
        return emptyString();

    return s1.left(i);
}

Value FunSubstringAfter::evaluate() const
{
    auto clonedContext = Expression::evaluationContext();

    String s1 = argument(0).evaluate().toString();
    String s2;

    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext);
        s2 = argument(1).evaluate().toString();
    }

    size_t i = s1.find(s2);
    if (i == notFound)
        return emptyString();

    return s1.substring(i + s2.length());
}

Value FunSubstring::evaluate() const
{
    EvaluationContext clonedContext1(Expression::evaluationContext());
    EvaluationContext clonedContext2(Expression::evaluationContext());

    String s;
    double doublePos;

    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext1);
        s = argument(0).evaluate().toString();
    }
    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext2);
        doublePos = argument(1).evaluate().toNumber();
    }

    if (std::isnan(doublePos))
        return emptyString();
    long pos = static_cast<long>(FunRound::round(doublePos));
    bool haveLength = argumentCount() == 3;
    long len = -1;
    if (haveLength) {
        double doubleLen = argument(2).evaluate().toNumber();
        if (std::isnan(doubleLen))
            return emptyString();
        len = static_cast<long>(FunRound::round(doubleLen));
    }

    if (pos > long(s.length())) 
        return emptyString();

    if (pos < 1) {
        if (haveLength) {
            len -= 1 - pos;
            if (len < 1)
                return emptyString();
        }
        pos = 1;
    }

    return s.substring(pos - 1, len);
}

Value FunStringLength::evaluate() const
{
    if (!argumentCount())
        return Value(Expression::evaluationContext().node.get()).toString().length();
    return argument(0).evaluate().toString().length();
}

Value FunNormalizeSpace::evaluate() const
{
    if (!argumentCount()) {
        String s = Value(Expression::evaluationContext().node.get()).toString();
        return s.simplifyWhiteSpace();
    }

    String s = argument(0).evaluate().toString();
    return s.simplifyWhiteSpace();
}

Value FunTranslate::evaluate() const
{
    EvaluationContext clonedContext1(Expression::evaluationContext());
    EvaluationContext clonedContext2(Expression::evaluationContext());

    String s1 = argument(0).evaluate().toString();
    String s2, s3;
    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext1);
        s2 = argument(1).evaluate().toString();
    }
    {
        SetForScope<EvaluationContext> contextForScope(Expression::evaluationContext(), clonedContext2);
        s3 = argument(2).evaluate().toString();
    }

    StringBuilder result;

    for (unsigned i1 = 0; i1 < s1.length(); ++i1) {
        UChar ch = s1[i1];
        size_t i2 = s2.find(ch);
        
        if (i2 == notFound)
            result.append(ch);
        else if (i2 < s3.length())
            result.append(s3[i2]);
    }

    return result.toString();
}

Value FunBoolean::evaluate() const
{
    return argument(0).evaluate().toBoolean();
}

Value FunNot::evaluate() const
{
    return !argument(0).evaluate().toBoolean();
}

Value FunTrue::evaluate() const
{
    return true;
}

Value FunLang::evaluate() const
{
    String lang = argument(0).evaluate().toString();

    const Attribute* languageAttribute = nullptr;
    Node* node = evaluationContext().node.get();
    while (node) {
        if (is<Element>(*node)) {
            Element& element = downcast<Element>(*node);
            if (element.hasAttributes())
                languageAttribute = element.findAttributeByName(XMLNames::langAttr);
        }
        if (languageAttribute)
            break;
        node = node->parentNode();
    }

    if (!languageAttribute)
        return false;

    String langValue = languageAttribute->value();
    while (true) {
        if (equalIgnoringASCIICase(langValue, lang))
            return true;

        // Remove suffixes one by one.
        size_t index = langValue.reverseFind('-');
        if (index == notFound)
            break;
        langValue = langValue.left(index);
    }

    return false;
}

Value FunFalse::evaluate() const
{
    return false;
}

Value FunNumber::evaluate() const
{
    if (!argumentCount())
        return Value(Expression::evaluationContext().node.get()).toNumber();
    return argument(0).evaluate().toNumber();
}

Value FunSum::evaluate() const
{
    Value a = argument(0).evaluate();
    if (!a.isNodeSet())
        return 0.0;

    double sum = 0.0;
    const NodeSet& nodes = a.toNodeSet();
    // To be really compliant, we should sort the node-set, as floating point addition is not associative.
    // However, this is unlikely to ever become a practical issue, and sorting is slow.

    for (auto& node : nodes)
        sum += Value(stringValue(node.get())).toNumber();
    
    return sum;
}

Value FunFloor::evaluate() const
{
    return floor(argument(0).evaluate().toNumber());
}

Value FunCeiling::evaluate() const
{
    return ceil(argument(0).evaluate().toNumber());
}

double FunRound::round(double val)
{
    if (!std::isnan(val) && !std::isinf(val)) {
        if (std::signbit(val) && val >= -0.5)
            val *= 0; // negative zero
        else
            val = floor(val + 0.5);
    }
    return val;
}

Value FunRound::evaluate() const
{
    return round(argument(0).evaluate().toNumber());
}

struct FunctionMapValue {
    std::unique_ptr<Function> (*creationFunction)();
    Interval argumentCountInterval;
};

static MemoryCompactLookupOnlyRobinHoodHashMap<String, FunctionMapValue> createFunctionMap()
{
    struct FunctionMapping {
        ASCIILiteral name;
        FunctionMapValue function;
    };

    static const FunctionMapping functions[] = {
        { "boolean"_s, { createFunctionBoolean, 1 } },
        { "ceiling"_s, { createFunctionCeiling, 1 } },
        { "concat"_s, { createFunctionConcat, Interval(2, Interval::Inf) } },
        { "contains"_s, { createFunctionContains, 2 } },
        { "count"_s, { createFunctionCount, 1 } },
        { "false"_s, { createFunctionFalse, 0 } },
        { "floor"_s, { createFunctionFloor, 1 } },
        { "id"_s, { createFunctionId, 1 } },
        { "lang"_s, { createFunctionLang, 1 } },
        { "last"_s, { createFunctionLast, 0 } },
        { "local-name"_s, { createFunctionLocalName, Interval(0, 1) } },
        { "name"_s, { createFunctionName, Interval(0, 1) } },
        { "namespace-uri"_s, { createFunctionNamespaceURI, Interval(0, 1) } },
        { "normalize-space"_s, { createFunctionNormalizeSpace, Interval(0, 1) } },
        { "not"_s, { createFunctionNot, 1 } },
        { "number"_s, { createFunctionNumber, Interval(0, 1) } },
        { "position"_s, { createFunctionPosition, 0 } },
        { "round"_s, { createFunctionRound, 1 } },
        { "starts-with"_s, { createFunctionStartsWith, 2 } },
        { "string"_s, { createFunctionString, Interval(0, 1) } },
        { "string-length"_s, { createFunctionStringLength, Interval(0, 1) } },
        { "substring"_s, { createFunctionSubstring, Interval(2, 3) } },
        { "substring-after"_s, { createFunctionSubstringAfter, 2 } },
        { "substring-before"_s, { createFunctionSubstringBefore, 2 } },
        { "sum"_s, { createFunctionSum, 1 } },
        { "translate"_s, { createFunctionTranslate, 3 } },
        { "true"_s, { createFunctionTrue, 0 } },
    };

    MemoryCompactLookupOnlyRobinHoodHashMap<String, FunctionMapValue> map;
    for (auto& function : functions)
        map.add(function.name, function.function);
    return map;
}

std::unique_ptr<Function> Function::create(const String& name, unsigned numArguments)
{
    static NeverDestroyed functionMap = createFunctionMap();

    auto it = functionMap.get().find(name);
    if (it == functionMap.get().end())
        return nullptr;

    if (!it->value.argumentCountInterval.contains(numArguments))
        return nullptr;

    return it->value.creationFunction();
}

std::unique_ptr<Function> Function::create(const String& name)
{
    return create(name, 0);
}

std::unique_ptr<Function> Function::create(const String& name, Vector<std::unique_ptr<Expression>> arguments)
{
    auto function = create(name, arguments.size());
    if (function)
        function->setArguments(name, WTFMove(arguments));
    return function;
}

}
}
