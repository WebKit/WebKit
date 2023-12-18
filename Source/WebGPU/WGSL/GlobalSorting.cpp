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

#include "ASTFunction.h"
#include "ASTIdentifierExpression.h"
#include "ASTVariableStatement.h"
#include "ASTVisitor.h"
#include "ContextProviderInlines.h"
#include "WGSLShaderModule.h"
#include <wtf/DataLog.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/StringBuilder.h>

namespace WGSL {

constexpr bool shouldLogGlobalSorting = false;

class Graph {
public:
    class Edge;
    class Node;
    struct EdgeHash;
    struct EdgeHashTraits;
    using EdgeSet = ListHashSet<Edge, EdgeHash>;

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

    class Node {
    public:
        Node()
            : m_astNode(nullptr)
        {
        }

        Node(unsigned index, AST::Declaration& astNode)
            : m_index(index)
            , m_astNode(&astNode)
        {
        }

        unsigned index() const { return m_index; }
        AST::Declaration& astNode() const { return *m_astNode; }
        EdgeSet& incomingEdges() { return m_incomingEdges; }
        EdgeSet& outgoingEdges() { return m_outgoingEdges; }

    private:
        unsigned m_index;
        AST::Declaration* m_astNode;
        EdgeSet m_incomingEdges;
        EdgeSet m_outgoingEdges;
    };


    Graph(size_t capacity)
        : m_nodes(capacity)
    {
    }

    FixedVector<Node>& nodes() { return m_nodes; }
    Node* addNode(unsigned index, AST::Declaration& astNode)
    {
        if (m_nodeMap.find(astNode.name()) != m_nodeMap.end())
            return nullptr;

        m_nodes[index] = Node(index, astNode);
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
        dataLogLnIf(shouldLogGlobalSorting, "addEdge: source: ", source.astNode().name(), ", target: ", target.astNode().name());
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

class GraphBuilder : public AST::Visitor, public ContextProvider<Empty> {
public:
    static void visit(Graph&, Graph::Node&);

    using AST::Visitor::visit;

    void visit(AST::Function&) override;
    void visit(AST::VariableStatement&) override;
    void visit(AST::CompoundStatement&) override;
    void visit(AST::ForStatement&) override;
    void visit(AST::IdentifierExpression&) override;

private:
    GraphBuilder(Graph&, Graph::Node&);

    void introduceVariable(AST::Identifier&);
    void readVariable(AST::Identifier&) const;

    Graph& m_graph;
    Graph::Node& m_currentNode;
};

void GraphBuilder::visit(Graph& graph, Graph::Node& node)
{
    GraphBuilder(graph, node).visit(node.astNode());
}

GraphBuilder::GraphBuilder(Graph& graph, Graph::Node& node)
    : m_graph(graph)
    , m_currentNode(node)
{
}

void GraphBuilder::visit(AST::Function& function)
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

void GraphBuilder::visit(AST::VariableStatement& variable)
{
    introduceVariable(variable.variable().name());
    AST::Visitor::visit(variable);
}

void GraphBuilder::visit(AST::CompoundStatement& statement)
{
    ContextScope blockScope(this);
    AST::Visitor::visit(statement);
}

void GraphBuilder::visit(AST::ForStatement& statement)
{
    ContextScope forScope(this);
    AST::Visitor::visit(statement);
}

void GraphBuilder::visit(AST::IdentifierExpression& identifier)
{
    readVariable(identifier.identifier());
}

void GraphBuilder::introduceVariable(AST::Identifier& name)
{
    ContextProvider::introduceVariable(name, { });
}

void GraphBuilder::readVariable(AST::Identifier& name) const
{
    if (ContextProvider::readVariable(name))
        return;
    if (auto* node = m_graph.getNode(name))
        m_graph.addEdge(m_currentNode, *node);
}


static std::optional<FailedCheck> reorder(AST::Declaration::List& list)
{
    Graph graph(list.size());
    Vector<Graph::Node*> graphNodeList;
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
        GraphBuilder::visit(graph, *graphNode);

    list.clear();
    Deque<Graph::Node> queue;

    std::function<void(Graph::Node&, unsigned)> processNode;
    processNode = [&](Graph::Node& node, unsigned currentIndex) {
        dataLogLnIf(shouldLogGlobalSorting, "Process: ", node.astNode().name());
        list.append(node.astNode());
        for (auto edge : node.incomingEdges()) {
            auto& source = edge.source();
            source.outgoingEdges().remove(edge);
            graph.edges().remove(edge);
            if (source.outgoingEdges().isEmpty() && source.index() < currentIndex)
                processNode(source, currentIndex);
        }
    };

    for (auto& node : graph.nodes()) {
        if (node.outgoingEdges().isEmpty())
            processNode(node, node.index());
    }

    if (graph.edges().isEmpty())
        return std::nullopt;

    dataLogLnIf(shouldLogGlobalSorting, "=== CYCLE ===");
    Graph::Node* cycleNode = nullptr;
    for (auto& node : graph.nodes()) {
        if (!node.outgoingEdges().isEmpty()) {
            cycleNode = &node;
            break;
        }
    }
    ASSERT(cycleNode);
    StringBuilder error;
    auto* node = cycleNode;
    HashSet<Graph::Node*> visited;
    while (true) {
        dataLogLnIf(shouldLogGlobalSorting, "cycle node: ", node->astNode().name());
        ASSERT(!node->outgoingEdges().isEmpty());
        visited.add(node);
        node = &node->outgoingEdges().first().target();
        if (visited.contains(node)) {
            cycleNode = node;
            break;
        }
    }
    error.append("encountered a dependency cycle: ", cycleNode->astNode().name());
    do {
        ASSERT(!node->outgoingEdges().isEmpty());
        node = &node->outgoingEdges().first().target();
        error.append(" -> ", node->astNode().name());
    } while (node != cycleNode);
    return FailedCheck { Vector<Error> { Error(error.toString(), cycleNode->astNode().span()) }, { } };
}

std::optional<FailedCheck> reorderGlobals(ShaderModule& module)
{
    if (auto maybeError = reorder(module.declarations()))
        return *maybeError;
    return std::nullopt;
}

} // namespace WGSL
