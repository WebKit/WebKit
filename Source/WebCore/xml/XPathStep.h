/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc.
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

#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class Node;

namespace XPath {

class Expression;
class NodeSet;

class Step {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Axis {
        AncestorAxis, AncestorOrSelfAxis, AttributeAxis,
        ChildAxis, DescendantAxis, DescendantOrSelfAxis,
        FollowingAxis, FollowingSiblingAxis, NamespaceAxis,
        ParentAxis, PrecedingAxis, PrecedingSiblingAxis,
        SelfAxis
    };

    class NodeTest {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        enum Kind { TextNodeTest, CommentNodeTest, ProcessingInstructionNodeTest, AnyNodeTest, NameTest };

        explicit NodeTest(Kind kind) : m_kind(kind) { }
        NodeTest(Kind kind, const AtomString& data) : m_kind(kind), m_data(data) { }
        NodeTest(Kind kind, const AtomString& data, const AtomString& namespaceURI) : m_kind(kind), m_data(data), m_namespaceURI(namespaceURI) { }

    private:
        friend class Step;
        friend void optimizeStepPair(Step&, Step&, bool&);
        friend bool nodeMatchesBasicTest(Node&, Axis, const NodeTest&);
        friend bool nodeMatches(Node&, Axis, const NodeTest&);

        Kind m_kind;
        AtomString m_data;
        AtomString m_namespaceURI;
        Vector<std::unique_ptr<Expression>> m_mergedPredicates;
    };

    Step(Axis, NodeTest);
    Step(Axis, NodeTest, Vector<std::unique_ptr<Expression>>);
    ~Step();

    void optimize();

    void evaluate(Node& context, NodeSet&) const;

    Axis axis() const { return m_axis; }

private:
    friend void optimizeStepPair(Step&, Step&, bool&);

    bool predicatesAreContextListInsensitive() const;

    void parseNodeTest(const String&);
    void nodesInAxis(Node& context, NodeSet&) const;

    Axis m_axis;
    NodeTest m_nodeTest;
    Vector<std::unique_ptr<Expression>> m_predicates;
};

void optimizeStepPair(Step&, Step&, bool& dropSecondStep);

} // namespace XPath
} // namespace WebCore
