/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GlobalSorting.h"

#include "ASTIdentifierExpression.h"
#include "ASTVariableStatement.h"
#include "ASTVisitor.h"
#include "ContextProviderInlines.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringBuilder.h>

namespace WGSL {

template<typename ASTNode>
class Graph {
public:
    class Edge;
    struct EdgeHash;
    struct EdgeHashTraits;
    using EdgeSet = HashSet<Edge, EdgeHash, EdgeHashTraits>;

    class Node {
    public:
        Node()
            : m_astNode(nullptr)
        {
        }

        Node(ASTNode& astNode)
            : m_astNode(&astNode)
        {
        }

        ASTNode& astNode() const { return *m_astNode; }
        EdgeSet& incomingEdges() { return m_incomingEdges; }
        EdgeSet& outgoingEdges() { return m_outgoingEdges; }

    private:
        ASTNode* m_astNode;
        EdgeSet m_incomingEdges;
        EdgeSet m_outgoingEdges;
    };

    class Edge {
        friend EdgeHash;
        friend EdgeHashTraits;
    public:
        Edge()
            : m_source(nullptr)
            , m_target(nullptr)
        {
        }

        Edge(Node& source, Node& target)
            : m_source(&source)
            , m_target(&target)
        {
        }

        void remove(Graph& graph)
        {
            m_source->outgoingEdges().remove(*this);
            m_target->incomingEdges().remove(*this);
            graph.edges().remove(*this);
        }

        Node& source() const { return *m_source; }
        Node& target() const { return *m_target; }

        bool operator==(const Edge& other) const
        {
            return m_source == other.m_source && m_target == other.m_target;
        }

    private:
        Node* m_source;
        Node* m_target;
    };

    struct EdgeHashTraits : HashTraits<Edge> {
        static constexpr bool emptyValueIsZero = true;
        static void constructDeletedValue(Edge& slot) { slot.m_source = bitwise_cast<Node*>(static_cast<intptr_t>(-1)); }
        static bool isDeletedValue(const Edge& edge) { return edge.m_source == bitwise_cast<Node*>(static_cast<intptr_t>(-1)); }
    };

    struct EdgeHash {
        static unsigned hash(const Edge& edge)
        {
            return WTF::TupleHash<Node*, Node*>::hash(std::tuple(edge.m_source, edge.m_target));
        }
        static bool equal(const Edge& a, const Edge& b)
        {
            return a == b;
        }
        static constexpr bool safeToCompareToEmptyOrDeleted = true;
    };

    Graph(size_t capacity)
        : m_nodes(capacity)
    {
    }

    FixedVector<Node>& nodes() { return m_nodes; }
    Node* addNode(unsigned index, ASTNode& astNode)
    {
        if (m_nodeMap.find(astNode.name()) != m_nodeMap.end())
            return nullptr;

        m_nodes[index] = Node(astNode);
        auto* node = &m_nodes[index];
        m_nodeMap.add(astNode.name(), node);
        return node;
    }
    Node* getNode(const AST::Identifier& identifier)
    {
        auto it = m_nodeMap.find(identifier);
        if (it == m_nodeMap.end())
            return nullptr;
        return it->value;
    }

    EdgeSet& edges() { return m_edges; }
    void addEdge(Node& source, Node& target)
    {
        auto result = m_edges.add(Edge(source, target));
        Edge& edge = *result.iterator;
        source.outgoingEdges().add(edge);
        target.incomingEdges().add(edge);
    }

    void topologicalSort();

private:
    FixedVector<Node> m_nodes;
    HashMap<String, Node*> m_nodeMap;
    EdgeSet m_edges;
};

struct Empty { };

template<typename ASTNode>
class GraphBuilder : public AST::Visitor, public ContextProvider<Empty> {
public:
    static void visit(Graph<ASTNode>&, typename Graph<ASTNode>::Node&);

    using AST::Visitor::visit;

    void visit(AST::Function&) override;
    void visit(AST::VariableStatement&) override;
    void visit(AST::CompoundStatement&) override;
    void visit(AST::ForStatement&) override;
    void visit(AST::IdentifierExpression&) override;

private:
    GraphBuilder(Graph<ASTNode>&, typename Graph<ASTNode>::Node&);

    void introduceVariable(AST::Identifier&);
    void readVariable(AST::Identifier&) const;

    Graph<ASTNode>& m_graph;
    typename Graph<ASTNode>::Node& m_currentNode;
};

template<typename ASTNode>
void GraphBuilder<ASTNode>::visit(Graph<ASTNode>& graph, typename Graph<ASTNode>::Node& node)
{
    GraphBuilder(graph, node).visit(node.astNode());
}

template<typename ASTNode>
GraphBuilder<ASTNode>::GraphBuilder(Graph<ASTNode>& graph, typename Graph<ASTNode>::Node& node)
    : m_graph(graph)
    , m_currentNode(node)
{
}

template<typename ASTNode>
void GraphBuilder<ASTNode>::visit(AST::Function& function)
{
    ContextScope functionScope(this);

    for (auto& parameter : function.parameters()) {
        AST::Visitor::visit(parameter.typeName());
        introduceVariable(parameter.name());
    }

    AST::Visitor::visit(function.body());

    if (function.maybeReturnType())
        AST::Visitor::visit(*function.maybeReturnType());
}

template<typename ASTNode>
void GraphBuilder<ASTNode>::visit(AST::VariableStatement& variable)
{
    introduceVariable(variable.variable().name());
    AST::Visitor::visit(variable);
}

template<typename ASTNode>
void GraphBuilder<ASTNode>::visit(AST::CompoundStatement& statement)
{
    ContextScope blockScope(this);
    AST::Visitor::visit(statement);
}

template<typename ASTNode>
void GraphBuilder<ASTNode>::visit(AST::ForStatement& statement)
{
    ContextScope forScope(this);
    AST::Visitor::visit(statement);
}

template<typename ASTNode>
void GraphBuilder<ASTNode>::visit(AST::IdentifierExpression& identifier)
{
    readVariable(identifier.identifier());
}

template<typename ASTNode>
void GraphBuilder<ASTNode>::introduceVariable(AST::Identifier& name)
{
    ContextProvider::introduceVariable(name, { });
}

template<typename ASTNode>
void GraphBuilder<ASTNode>::readVariable(AST::Identifier& name) const
{
    if (ContextProvider::readVariable(name))
        return;
    if (auto* node = m_graph.getNode(name))
        m_graph.addEdge(*node, m_currentNode);
}


template<typename T>
std::optional<FailedCheck> reorder(typename T::List& list)
{
    Graph<T> graph(list.size());
    Vector<typename Graph<T>::Node*> graphNodeList;
    graphNodeList.reserveCapacity(list.size());
    unsigned index = 0;
    for (auto& node : list) {
        auto* graphNode = graph.addNode(index++, node);
        if (!graphNode) {
            // This is unfortunately duplicated between this pass and the type checker
            // since here we only cover redeclarations of the same type (e.g. two
            // variables with the same name), while the type checker will also identify
            // redeclarations of different types (e.g. a variable and a struct with the
            // same name)
            return FailedCheck { Vector<Error> { Error(makeString("redeclaration of '", node.name(), "'"), node.span()) }, { } };
        }
        graphNodeList.append(graphNode);
    }

    for (auto* graphNode : graphNodeList)
        GraphBuilder<T>::visit(graph, *graphNode);

    list.clear();
    Deque<typename Graph<T>::Node> queue;
    for (auto& node : graph.nodes()) {
        if (node.incomingEdges().isEmpty())
            queue.append(node);
    }
    while (!queue.isEmpty()) {
        auto node = queue.takeFirst();
        list.append(node.astNode());
        for (auto edge : node.outgoingEdges()) {
            auto& target = edge.target();
            edge.remove(graph);
            if (target.incomingEdges().isEmpty())
                queue.append(target);
        }
    }
    if (graph.edges().isEmpty())
        return std::nullopt;

    typename Graph<T>::Node* cycleNode = nullptr;
    for (auto& node : graph.nodes()) {
        if (!node.incomingEdges().isEmpty()) {
            cycleNode = &node;
            break;
        }
    }
    ASSERT(cycleNode);
    StringBuilder error;
    auto* node = cycleNode;
    HashSet<typename Graph<T>::Node*> visited;
    while (true) {
        ASSERT(!node->incomingEdges().isEmpty());
        visited.add(node);
        node = &node->incomingEdges().random()->source();
        if (visited.contains(node)) {
            cycleNode = node;
            break;
        }
    }
    error.append("encountered a dependency cycle: ", cycleNode->astNode().name());
    do {
        ASSERT(!node->incomingEdges().isEmpty());
        node = &node->incomingEdges().random()->source();
        error.append(" -> ", node->astNode().name());
    } while (node != cycleNode);
    return FailedCheck { Vector<Error> { Error(error.toString(), cycleNode->astNode().span()) }, { } };
}

std::optional<FailedCheck> reorderGlobals(ShaderModule& module)
{
    if (auto maybeError = reorder<AST::Structure>(module.structures()))
        return *maybeError;
    if (auto maybeError = reorder<AST::Variable>(module.variables()))
        return *maybeError;
    if (auto maybeError = reorder<AST::Function>(module.functions()))
        return *maybeError;
    return std::nullopt;
}

} // namespace WGSL
