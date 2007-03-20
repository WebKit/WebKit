/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef XPathStep_h
#define XPathStep_h

#if ENABLE(XPATH)

#include "Node.h"
#include "XPathExpressionNode.h"
#include "XPathNodeSet.h"

namespace WebCore {

    namespace XPath {

        class Predicate;
        
        class Step : public ParseNode, Noncopyable {
        public:
            enum Axis {
                AncestorAxis, AncestorOrSelfAxis, AttributeAxis,
                ChildAxis, DescendantAxis, DescendantOrSelfAxis,
                FollowingAxis, FollowingSiblingAxis, NamespaceAxis,
                ParentAxis, PrecedingAxis, PrecedingSiblingAxis,
                SelfAxis
            };
            
            class NodeTest {
            public:
                enum Kind {
                    TextNodeTest, CommentNodeTest, ProcessingInstructionNodeTest, AnyNodeTest, NameTest,
                    ElementNodeTest // XPath 2.0
                };
                
                NodeTest(Kind kind, const String& data = String()) : m_kind(kind), m_data(data) {}
                
                Kind kind() const { return m_kind; }
                const String data() const { return m_data; }
                
            private:
                Kind m_kind;
                String m_data;
            };

            Step(Axis, const NodeTest& nodeTest, const Vector<Predicate*>& predicates = Vector<Predicate*>());
            Step(Axis, const NodeTest& nodeTest, const String& namespaceURI, const Vector<Predicate*>& predicates = Vector<Predicate*>());
            ~Step();

            NodeSet evaluate(Node* context) const;
            
            Axis axis() const { return m_axis; }
            NodeTest nodeTest() const { return m_nodeTest; }
            const String& nodeTestData() const { return m_nodeTestData; }
            const String& namespaceURI() const { return m_namespaceURI; }
            const Vector<Predicate*>& predicates() const { return m_predicates; }
            
            void setAxis(Axis axis) { m_axis = axis; }
            void setNodeTest(NodeTest nodeTest) { m_nodeTest = nodeTest; }
            void setNodeTestData(const String& nodeTestData) { m_nodeTestData = nodeTestData; }
            void setNamespaceURI(const String& namespaceURI) { m_namespaceURI = namespaceURI; }
            void setPredicates(const Vector<Predicate*>& predicates) { m_predicates = predicates; }
            
        private:
            void parseNodeTest(const String&);
            NodeSet nodesInAxis(Node* context) const;
            NodeSet nodeTestMatches(const NodeSet& nodes) const;
            String namespaceFromNodetest(const String& nodeTest) const;
            Node::NodeType primaryNodeType(Axis) const;

            Axis m_axis;
            NodeTest m_nodeTest;
            String m_nodeTestData;
            String m_namespaceURI;
            Vector<Predicate*> m_predicates;
        };

    }

}

#endif // ENABLE(XPATH)

#endif // XPath_Step_H
