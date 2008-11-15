/*
*  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
*  Copyright (C) 2001 Peter Kelly (pmk@post.com)
*  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
*  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
*  Copyright (C) 2007 Maks Orlovich
*  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Library General Public
*  License as published by the Free Software Foundation; either
*  version 2 of the License, or (at your option) any later version.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Library General Public License for more details.
*
*  You should have received a copy of the GNU Library General Public License
*  along with this library; see the file COPYING.LIB.  If not, write to
*  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
*  Boston, MA 02110-1301, USA.
*
*/

#include "config.h"
#include "Nodes.h"

#include "CodeGenerator.h"
#include "ExecState.h"
#include "JSGlobalObject.h"
#include "JSStaticScopeObject.h"
#include "LabelScope.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "RegExpObject.h"
#include "SamplingTool.h"
#include "Debugger.h"
#include "Lexer.h"
#include "Operations.h"
#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/MathExtras.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/Threading.h>

using namespace WTF;

namespace JSC {

static void substitute(UString& string, const UString& substring) JSC_FAST_CALL;

// ------------------------------ NodeReleaser --------------------------------

class NodeReleaser : Noncopyable {
public:
    // Call this function inside the destructor of a class derived from Node.
    // This will traverse the tree below this node, destroying all of those nodes,
    // but without relying on recursion.
    static void releaseAllNodes(ParserRefCounted* root);

    // Call this on each node in a the releaseNodes virtual function.
    // It gives the node to the NodeReleaser, which will then release the
    // node later at the end of the releaseAllNodes process.
    template <typename T> void release(RefPtr<T>& node) { if (node) adopt(node.release()); }
    void release(RefPtr<FunctionBodyNode>& node) { if (node) adoptFunctionBodyNode(node); }

private:
    NodeReleaser() { }
    ~NodeReleaser() { }

    void adopt(PassRefPtr<ParserRefCounted>);
    void adoptFunctionBodyNode(RefPtr<FunctionBodyNode>&);

    typedef Vector<RefPtr<ParserRefCounted> > NodeReleaseVector;
    OwnPtr<NodeReleaseVector> m_vector;
};

void NodeReleaser::releaseAllNodes(ParserRefCounted* root)
{
    ASSERT(root);
    NodeReleaser releaser;
    root->releaseNodes(releaser);
    if (!releaser.m_vector)
        return;
    // Note: The call to release.m_vector->size() is intentionally inside
    // the loop, since calls to releaseNodes are expected to increase the size.
    for (size_t i = 0; i < releaser.m_vector->size(); ++i) {
        ParserRefCounted* node = (*releaser.m_vector)[i].get();
        if (node->hasOneRef())
            node->releaseNodes(releaser);
    }
}

void NodeReleaser::adopt(PassRefPtr<ParserRefCounted> node)
{
    ASSERT(node);
    if (!node->hasOneRef())
        return;
    if (!m_vector)
        m_vector.set(new NodeReleaseVector);
    m_vector->append(node);
}

void NodeReleaser::adoptFunctionBodyNode(RefPtr<FunctionBodyNode>& functionBodyNode)
{
    // This sidesteps a problem where if you assign a PassRefPtr<FunctionBodyNode>
    // to a PassRefPtr<Node> we leave the two reference counts (FunctionBodyNode
    // and ParserRefCounted) unbalanced. It would be nice to fix this problem in
    // a cleaner way -- perhaps we could remove the FunctionBodyNode reference
    // count at some point.
    RefPtr<Node> node = functionBodyNode;
    functionBodyNode = 0;
    adopt(node.release());
}

// ------------------------------ ParserRefCounted -----------------------------------------

#ifndef NDEBUG
static RefCountedLeakCounter parserRefCountedCounter("JSC::Node");
#endif

ParserRefCounted::ParserRefCounted(JSGlobalData* globalData)
    : m_globalData(globalData)
{
#ifndef NDEBUG
    parserRefCountedCounter.increment();
#endif
    if (!m_globalData->newParserObjects)
        m_globalData->newParserObjects = new HashSet<ParserRefCounted*>;
    m_globalData->newParserObjects->add(this);
    ASSERT(m_globalData->newParserObjects->contains(this));
}

ParserRefCounted::~ParserRefCounted()
{
#ifndef NDEBUG
    parserRefCountedCounter.decrement();
#endif
}

void ParserRefCounted::releaseNodes(NodeReleaser&)
{
}

void ParserRefCounted::ref()
{
    // bumping from 0 to 1 is just removing from the new nodes set
    if (m_globalData->newParserObjects) {
        HashSet<ParserRefCounted*>::iterator it = m_globalData->newParserObjects->find(this);
        if (it != m_globalData->newParserObjects->end()) {
            m_globalData->newParserObjects->remove(it);
            ASSERT(!m_globalData->parserObjectExtraRefCounts || !m_globalData->parserObjectExtraRefCounts->contains(this));
            return;
        }
    }

    ASSERT(!m_globalData->newParserObjects || !m_globalData->newParserObjects->contains(this));

    if (!m_globalData->parserObjectExtraRefCounts)
        m_globalData->parserObjectExtraRefCounts = new HashCountedSet<ParserRefCounted*>;
    m_globalData->parserObjectExtraRefCounts->add(this);
}

void ParserRefCounted::deref()
{
    ASSERT(!m_globalData->newParserObjects || !m_globalData->newParserObjects->contains(this));

    if (!m_globalData->parserObjectExtraRefCounts) {
        delete this;
        return;
    }

    HashCountedSet<ParserRefCounted*>::iterator it = m_globalData->parserObjectExtraRefCounts->find(this);
    if (it == m_globalData->parserObjectExtraRefCounts->end())
        delete this;
    else
        m_globalData->parserObjectExtraRefCounts->remove(it);
}

bool ParserRefCounted::hasOneRef()
{
    if (m_globalData->newParserObjects && m_globalData->newParserObjects->contains(this)) {
        ASSERT(!m_globalData->parserObjectExtraRefCounts || !m_globalData->parserObjectExtraRefCounts->contains(this));
        return false;
    }

    ASSERT(!m_globalData->newParserObjects || !m_globalData->newParserObjects->contains(this));

    if (!m_globalData->parserObjectExtraRefCounts)
        return true;

    return !m_globalData->parserObjectExtraRefCounts->contains(this);
}

void ParserRefCounted::deleteNewObjects(JSGlobalData* globalData)
{
    if (!globalData->newParserObjects)
        return;

#ifndef NDEBUG
    HashSet<ParserRefCounted*>::iterator end = globalData->newParserObjects->end();
    for (HashSet<ParserRefCounted*>::iterator it = globalData->newParserObjects->begin(); it != end; ++it)
        ASSERT(!globalData->parserObjectExtraRefCounts || !globalData->parserObjectExtraRefCounts->contains(*it));
#endif
    deleteAllValues(*globalData->newParserObjects);
    delete globalData->newParserObjects;
    globalData->newParserObjects = 0;
}

// ------------------------------ Node --------------------------------

Node::Node(JSGlobalData* globalData)
    : ParserRefCounted(globalData)
{
    m_line = globalData->lexer->lineNo();
}

// ------------------------------ ThrowableExpressionData --------------------------------

static void substitute(UString& string, const UString& substring)
{
    int position = string.find("%s");
    ASSERT(position != -1);
    UString newString = string.substr(0, position);
    newString.append(substring);
    newString.append(string.substr(position + 2));
    string = newString;
}

RegisterID* ThrowableExpressionData::emitThrowError(CodeGenerator& generator, ErrorType e, const char* msg)
{
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(generator.globalData(), msg));
    generator.emitThrow(exception);
    return exception;
}

RegisterID* ThrowableExpressionData::emitThrowError(CodeGenerator& generator, ErrorType e, const char* msg, const Identifier& label)
{
    UString message = msg;
    substitute(message, label.ustring());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(generator.globalData(), message));
    generator.emitThrow(exception);
    return exception;
}
    
// ------------------------------ StatementNode --------------------------------

StatementNode::StatementNode(JSGlobalData* globalData)
    : Node(globalData)
    , m_lastLine(-1)
{
}

void StatementNode::setLoc(int firstLine, int lastLine)
{
    m_line = firstLine;
    m_lastLine = lastLine;
}

// ------------------------------ SourceElements --------------------------------

void SourceElements::append(PassRefPtr<StatementNode> statement)
{
    if (statement->isEmptyStatement())
        return;

    m_statements.append(statement);
}

// ------------------------------ NullNode -------------------------------------

RegisterID* NullNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult())
        return 0;
    return generator.emitLoad(dst, jsNull());
}

// ------------------------------ BooleanNode ----------------------------------

RegisterID* BooleanNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult())
        return 0;
    return generator.emitLoad(dst, m_value);
}

// ------------------------------ NumberNode -----------------------------------

RegisterID* NumberNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult())
        return 0;
    return generator.emitLoad(dst, m_double);
}

// ------------------------------ StringNode -----------------------------------

RegisterID* StringNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult())
        return 0;
    return generator.emitLoad(dst, m_value);
}

// ------------------------------ RegExpNode -----------------------------------

RegisterID* RegExpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegExp> regExp = RegExp::create(generator.globalData(), m_pattern, m_flags);
    if (!regExp->isValid())
        return emitThrowError(generator, SyntaxError, ("Invalid regular expression: " + UString(regExp->errorMessage())).UTF8String().c_str());
    if (dst == ignoredResult())
        return 0;
    return generator.emitNewRegExp(generator.finalDestination(dst), regExp.get());
}

// ------------------------------ ThisNode -------------------------------------

RegisterID* ThisNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult())
        return 0;
    return generator.moveToDestinationIfNeeded(dst, generator.thisRegister());
}

// ------------------------------ ResolveNode ----------------------------------

bool ResolveNode::isPure(CodeGenerator& generator) const
{
    return generator.isLocal(m_ident);
}

RegisterID* ResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (dst == ignoredResult())
            return 0;
        return generator.moveToDestinationIfNeeded(dst, local);
    }
    
    generator.emitExpressionInfo(m_startOffset + m_ident.size(), m_ident.size(), 0);
    return generator.emitResolve(generator.finalDestination(dst), m_ident);
}

// ------------------------------ ElementNode ------------------------------------

ElementNode::~ElementNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ElementNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_next);
    releaser.release(m_node);
}

// ------------------------------ ArrayNode ------------------------------------

ArrayNode::~ArrayNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ArrayNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_element);
}

RegisterID* ArrayNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    // FIXME: Should we put all of this code into emitNewArray?

    unsigned length = 0;
    ElementNode* firstPutElement;
    for (firstPutElement = m_element.get(); firstPutElement; firstPutElement = firstPutElement->next()) {
        if (firstPutElement->elision())
            break;
        ++length;
    }

    if (!firstPutElement && !m_elision)
        return generator.emitNewArray(generator.finalDestination(dst), m_element.get());

    RefPtr<RegisterID> array = generator.emitNewArray(generator.tempDestination(dst), m_element.get());

    for (ElementNode* n = firstPutElement; n; n = n->next()) {
        RegisterID* value = generator.emitNode(n->value());
        length += n->elision();
        generator.emitPutByIndex(array.get(), length++, value);
    }

    if (m_elision) {
        RegisterID* value = generator.emitLoad(0, jsNumber(generator.globalData(), m_elision + length));
        generator.emitPutById(array.get(), generator.propertyNames().length, value);
    }

    return generator.moveToDestinationIfNeeded(dst, array.get());
}

// ------------------------------ PropertyNode ----------------------------

PropertyNode::~PropertyNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PropertyNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_assign);
}

// ------------------------------ ObjectLiteralNode ----------------------------

ObjectLiteralNode::~ObjectLiteralNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ObjectLiteralNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_list);
}

RegisterID* ObjectLiteralNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
     if (!m_list) {
         if (dst == ignoredResult())
             return 0;
         return generator.emitNewObject(generator.finalDestination(dst));
     }
     return generator.emitNode(dst, m_list.get());
}

// ------------------------------ PropertyListNode -----------------------------

PropertyListNode::~PropertyListNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PropertyListNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_node);
    releaser.release(m_next);
}

RegisterID* PropertyListNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newObj = generator.tempDestination(dst);
    
    generator.emitNewObject(newObj.get());
    
    for (PropertyListNode* p = this; p; p = p->m_next.get()) {
        RegisterID* value = generator.emitNode(p->m_node->m_assign.get());
        
        switch (p->m_node->m_type) {
            case PropertyNode::Constant: {
                generator.emitPutById(newObj.get(), p->m_node->name(), value);
                break;
            }
            case PropertyNode::Getter: {
                generator.emitPutGetter(newObj.get(), p->m_node->name(), value);
                break;
            }
            case PropertyNode::Setter: {
                generator.emitPutSetter(newObj.get(), p->m_node->name(), value);
                break;
            }
            default:
                ASSERT_NOT_REACHED();
        }
    }
    
    return generator.moveToDestinationIfNeeded(dst, newObj.get());
}

// ------------------------------ BracketAccessorNode --------------------------------

BracketAccessorNode::~BracketAccessorNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void BracketAccessorNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_subscript);
}

RegisterID* BracketAccessorNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments, m_subscript->isPure(generator));
    RegisterID* property = generator.emitNode(m_subscript.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitGetByVal(generator.finalDestination(dst), base.get(), property);
}

// ------------------------------ DotAccessorNode --------------------------------

DotAccessorNode::~DotAccessorNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void DotAccessorNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
}

RegisterID* DotAccessorNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* base = generator.emitNode(m_base.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitGetById(generator.finalDestination(dst), base, m_ident);
}

// ------------------------------ ArgumentListNode -----------------------------

ArgumentListNode::~ArgumentListNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ArgumentListNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_next);
    releaser.release(m_expr);
}

RegisterID* ArgumentListNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    return generator.emitNode(dst, m_expr.get());
}

// ------------------------------ ArgumentsNode -----------------------------

ArgumentsNode::~ArgumentsNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ArgumentsNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_listNode);
}

// ------------------------------ NewExprNode ----------------------------------

NewExprNode::~NewExprNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void NewExprNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
    releaser.release(m_args);
}

RegisterID* NewExprNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> func = generator.emitNode(m_expr.get());
    return generator.emitConstruct(generator.finalDestination(dst), func.get(), m_args.get(), divot(), startOffset(), endOffset());
}

// ------------------------------ EvalFunctionCallNode ----------------------------------

EvalFunctionCallNode::~EvalFunctionCallNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void EvalFunctionCallNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_args);
}

RegisterID* EvalFunctionCallNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> func = generator.tempDestination(dst);
    RefPtr<RegisterID> thisRegister = generator.newTemporary();
    generator.emitResolveWithBase(thisRegister.get(), func.get(), generator.propertyNames().eval);
    return generator.emitCallEval(generator.finalDestination(dst, func.get()), func.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
}

// ------------------------------ FunctionCallValueNode ----------------------------------

FunctionCallValueNode::~FunctionCallValueNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void FunctionCallValueNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
    releaser.release(m_args);
}

RegisterID* FunctionCallValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> func = generator.emitNode(m_expr.get());
    RefPtr<RegisterID> thisRegister = generator.emitLoad(generator.newTemporary(), jsNull());
    return generator.emitCall(generator.finalDestination(dst, func.get()), func.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
}

// ------------------------------ FunctionCallResolveNode ----------------------------------

FunctionCallResolveNode::~FunctionCallResolveNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void FunctionCallResolveNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_args);
}

RegisterID* FunctionCallResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RefPtr<RegisterID> local = generator.registerFor(m_ident)) {
        RefPtr<RegisterID> thisRegister = generator.emitLoad(generator.newTemporary(), jsNull());
        return generator.emitCall(generator.finalDestination(dst, thisRegister.get()), local.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
    }

    int index = 0;
    size_t depth = 0;
    JSObject* globalObject = 0;
    if (generator.findScopedProperty(m_ident, index, depth, false, globalObject) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> func = generator.emitGetScopedVar(generator.newTemporary(), depth, index, globalObject);
        RefPtr<RegisterID> thisRegister = generator.emitLoad(generator.newTemporary(), jsNull());
        return generator.emitCall(generator.finalDestination(dst, func.get()), func.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
    }

    RefPtr<RegisterID> func = generator.tempDestination(dst);
    RefPtr<RegisterID> thisRegister = generator.newTemporary();
    int identifierStart = divot() - startOffset();
    generator.emitExpressionInfo(identifierStart + m_ident.size(), m_ident.size(), 0);
    generator.emitResolveFunction(thisRegister.get(), func.get(), m_ident);
    return generator.emitCall(generator.finalDestination(dst, func.get()), func.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
}

// ------------------------------ FunctionCallBracketNode ----------------------------------

FunctionCallBracketNode::~FunctionCallBracketNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void FunctionCallBracketNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_subscript);
    releaser.release(m_args);
}

RegisterID* FunctionCallBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RegisterID* property = generator.emitNode(m_subscript.get());
    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> function = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property);
    RefPtr<RegisterID> thisRegister = generator.emitMove(generator.newTemporary(), base.get());
    return generator.emitCall(generator.finalDestination(dst, function.get()), function.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
}

// ------------------------------ FunctionCallDotNode ----------------------------------

FunctionCallDotNode::~FunctionCallDotNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void FunctionCallDotNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_args);
}

RegisterID* FunctionCallDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> function = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RefPtr<RegisterID> thisRegister = generator.emitMove(generator.newTemporary(), base.get());
    return generator.emitCall(generator.finalDestination(dst, function.get()), function.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
}

// ------------------------------ PostfixResolveNode ----------------------------------

static RegisterID* emitPreIncOrDec(CodeGenerator& generator, RegisterID* srcDst, Operator oper)
{
    return (oper == OpPlusPlus) ? generator.emitPreInc(srcDst) : generator.emitPreDec(srcDst);
}

static RegisterID* emitPostIncOrDec(CodeGenerator& generator, RegisterID* dst, RegisterID* srcDst, Operator oper)
{
    return (oper == OpPlusPlus) ? generator.emitPostInc(dst, srcDst) : generator.emitPostDec(dst, srcDst);
}

RegisterID* PostfixResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            if (dst == ignoredResult())
                return 0;
            return generator.emitToJSNumber(generator.finalDestination(dst), local);
        }

        if (dst == ignoredResult())
            return emitPreIncOrDec(generator, local, m_operator);
        return emitPostIncOrDec(generator, generator.finalDestination(dst), local, m_operator);
    }

    int index = 0;
    size_t depth = 0;
    JSObject* globalObject = 0;
    if (generator.findScopedProperty(m_ident, index, depth, true, globalObject) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> value = generator.emitGetScopedVar(generator.newTemporary(), depth, index, globalObject);
        RegisterID* oldValue;
        if (dst == ignoredResult()) {
            oldValue = 0;
            emitPreIncOrDec(generator, value.get(), m_operator);
        } else {
            oldValue = emitPostIncOrDec(generator, generator.finalDestination(dst), value.get(), m_operator);
        }
        generator.emitPutScopedVar(depth, index, value.get(), globalObject);
        return oldValue;
    }

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RefPtr<RegisterID> value = generator.newTemporary();
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), value.get(), m_ident);
    RegisterID* oldValue;
    if (dst == ignoredResult()) {
        oldValue = 0;
        emitPreIncOrDec(generator, value.get(), m_operator);
    } else {
        oldValue = emitPostIncOrDec(generator, generator.finalDestination(dst), value.get(), m_operator);
    }
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

// ------------------------------ PostfixBracketNode ----------------------------------

PostfixBracketNode::~PostfixBracketNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PostfixBracketNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_subscript);
}

RegisterID* PostfixBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue;
    if (dst == ignoredResult()) {
        oldValue = 0;
        if (m_operator == OpPlusPlus)
            generator.emitPreInc(value.get());
        else
            generator.emitPreDec(value.get());
    } else {
        oldValue = (m_operator == OpPlusPlus) ? generator.emitPostInc(generator.finalDestination(dst), value.get()) : generator.emitPostDec(generator.finalDestination(dst), value.get());
    }
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitPutByVal(base.get(), property.get(), value.get());
    return oldValue;
}

// ------------------------------ PostfixDotNode ----------------------------------

PostfixDotNode::~PostfixDotNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PostfixDotNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
}

RegisterID* PostfixDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    RegisterID* oldValue;
    if (dst == ignoredResult()) {
        oldValue = 0;
        if (m_operator == OpPlusPlus)
            generator.emitPreInc(value.get());
        else
            generator.emitPreDec(value.get());
    } else {
        oldValue = (m_operator == OpPlusPlus) ? generator.emitPostInc(generator.finalDestination(dst), value.get()) : generator.emitPostDec(generator.finalDestination(dst), value.get());
    }
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

// ------------------------------ PostfixErrorNode -----------------------------------

PostfixErrorNode::~PostfixErrorNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PostfixErrorNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* PostfixErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, m_operator == OpPlusPlus ? "Postfix ++ operator applied to value that is not a reference." : "Postfix -- operator applied to value that is not a reference.");
}

// ------------------------------ DeleteResolveNode -----------------------------------

RegisterID* DeleteResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.registerFor(m_ident))
        return generator.emitUnexpectedLoad(generator.finalDestination(dst), false);

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* base = generator.emitResolveBase(generator.tempDestination(dst), m_ident);
    return generator.emitDeleteById(generator.finalDestination(dst, base), base, m_ident);
}

// ------------------------------ DeleteBracketNode -----------------------------------

DeleteBracketNode::~DeleteBracketNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void DeleteBracketNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_subscript);
}

RegisterID* DeleteBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_base.get());
    RegisterID* r1 = generator.emitNode(m_subscript.get());

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitDeleteByVal(generator.finalDestination(dst), r0.get(), r1);
}

// ------------------------------ DeleteDotNode -----------------------------------

DeleteDotNode::~DeleteDotNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void DeleteDotNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
}

RegisterID* DeleteDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* r0 = generator.emitNode(m_base.get());

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitDeleteById(generator.finalDestination(dst), r0, m_ident);
}

// ------------------------------ DeleteValueNode -----------------------------------

DeleteValueNode::~DeleteValueNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void DeleteValueNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* DeleteValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(ignoredResult(), m_expr.get());

    // delete on a non-location expression ignores the value and returns true
    return generator.emitUnexpectedLoad(generator.finalDestination(dst), true);
}

// ------------------------------ VoidNode -------------------------------------

VoidNode::~VoidNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void VoidNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* VoidNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult()) {
        generator.emitNode(ignoredResult(), m_expr.get());
        return 0;
    }
    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    return generator.emitLoad(dst, jsUndefined());
}

// ------------------------------ TypeOfValueNode -----------------------------------

RegisterID* TypeOfResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (dst == ignoredResult())
            return 0;
        return generator.emitTypeOf(generator.finalDestination(dst), local);
    }

    RefPtr<RegisterID> scratch = generator.emitResolveBase(generator.tempDestination(dst), m_ident);
    generator.emitGetById(scratch.get(), scratch.get(), m_ident);
    if (dst == ignoredResult())
        return 0;
    return generator.emitTypeOf(generator.finalDestination(dst, scratch.get()), scratch.get());
}

// ------------------------------ TypeOfValueNode -----------------------------------

TypeOfValueNode::~TypeOfValueNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void TypeOfValueNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* TypeOfValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult()) {
        generator.emitNode(ignoredResult(), m_expr.get());
        return 0;
    }
    RefPtr<RegisterID> src = generator.emitNode(m_expr.get());
    return generator.emitTypeOf(generator.finalDestination(dst), src.get());
}

// ------------------------------ PrefixResolveNode ----------------------------------

RegisterID* PrefixResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            if (dst == ignoredResult())
                return 0;
            RefPtr<RegisterID> r0 = generator.emitUnexpectedLoad(generator.finalDestination(dst), (m_operator == OpPlusPlus) ? 1.0 : -1.0);
            return generator.emitBinaryOp(op_add, r0.get(), local, r0.get(), OperandTypes());
        }

        emitPreIncOrDec(generator, local, m_operator);
        return generator.moveToDestinationIfNeeded(dst, local);
    }

    int index = 0;
    size_t depth = 0;
    JSObject* globalObject = 0;
    if (generator.findScopedProperty(m_ident, index, depth, false, globalObject) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> propDst = generator.emitGetScopedVar(generator.tempDestination(dst), depth, index, globalObject);
        emitPreIncOrDec(generator, propDst.get(), m_operator);
        generator.emitPutScopedVar(depth, index, propDst.get(), globalObject);
        return generator.moveToDestinationIfNeeded(dst, propDst.get());
    }

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), propDst.get(), m_ident);
    emitPreIncOrDec(generator, propDst.get(), m_operator);
    generator.emitPutById(base.get(), m_ident, propDst.get());
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

// ------------------------------ PrefixBracketNode ----------------------------------

PrefixBracketNode::~PrefixBracketNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PrefixBracketNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_subscript);
}

RegisterID* PrefixBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);

    generator.emitExpressionInfo(divot() + m_subexpressionDivotOffset, m_subexpressionStartOffset, endOffset() - m_subexpressionDivotOffset);
    RegisterID* value = generator.emitGetByVal(propDst.get(), base.get(), property.get());
    if (m_operator == OpPlusPlus)
        generator.emitPreInc(value);
    else
        generator.emitPreDec(value);
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitPutByVal(base.get(), property.get(), value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

// ------------------------------ PrefixDotNode ----------------------------------

PrefixDotNode::~PrefixDotNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PrefixDotNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
}

RegisterID* PrefixDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);

    generator.emitExpressionInfo(divot() + m_subexpressionDivotOffset, m_subexpressionStartOffset, endOffset() - m_subexpressionDivotOffset);
    RegisterID* value = generator.emitGetById(propDst.get(), base.get(), m_ident);
    if (m_operator == OpPlusPlus)
        generator.emitPreInc(value);
    else
        generator.emitPreDec(value);
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitPutById(base.get(), m_ident, value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

// ------------------------------ PrefixErrorNode -----------------------------------

PrefixErrorNode::~PrefixErrorNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void PrefixErrorNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* PrefixErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, m_operator == OpPlusPlus ? "Prefix ++ operator applied to value that is not a reference." : "Prefix -- operator applied to value that is not a reference.");
}

// ------------------------------ Unary Operation Nodes -----------------------------------

UnaryOpNode::~UnaryOpNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void UnaryOpNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* UnaryOpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitUnaryOp(opcode(), generator.finalDestination(dst), src, m_expr->resultDescriptor());
}

// ------------------------------ Binary Operation Nodes -----------------------------------

BinaryOpNode::~BinaryOpNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void BinaryOpNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr1);
    releaser.release(m_expr2);
}

RegisterID* BinaryOpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    OpcodeID opcode = this->opcode();
    if (opcode == op_neq) {
        if (m_expr1->isNull() || m_expr2->isNull()) {
            RefPtr<RegisterID> src = generator.emitNode(dst, m_expr1->isNull() ? m_expr2.get() : m_expr1.get());
            return generator.emitUnaryOp(op_neq_null, generator.finalDestination(dst, src.get()), src.get(), ResultType::unknown());
        }
    }

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitBinaryOp(opcode, generator.finalDestination(dst, src1.get()), src1.get(), src2, OperandTypes(m_expr1->resultDescriptor(), m_expr2->resultDescriptor()));
}

RegisterID* EqualNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (m_expr1->isNull() || m_expr2->isNull()) {
        RefPtr<RegisterID> src = generator.emitNode(dst, m_expr1->isNull() ? m_expr2.get() : m_expr1.get());
        return generator.emitUnaryOp(op_eq_null, generator.finalDestination(dst, src.get()), src.get(), ResultType::unknown());
    }

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitEqualityOp(op_eq, generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

RegisterID* StrictEqualNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitEqualityOp(op_stricteq, generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

RegisterID* ReverseBinaryOpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitBinaryOp(opcode(), generator.finalDestination(dst, src1.get()), src2, src1.get(), OperandTypes(m_expr2->resultDescriptor(), m_expr1->resultDescriptor()));
}

RegisterID* ThrowableBinaryOpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitBinaryOp(opcode(), generator.finalDestination(dst, src1.get()), src1.get(), src2, OperandTypes(m_expr1->resultDescriptor(), m_expr2->resultDescriptor()));
}

RegisterID* InstanceOfNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> src2 = generator.emitNode(m_expr2.get());

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* src2Prototype = generator.emitGetById(generator.newTemporary(), src2.get(), generator.globalData()->propertyNames->prototype);

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitInstanceOf(generator.finalDestination(dst, src1.get()), src1.get(), src2.get(), src2Prototype);
}

// ------------------------------ LogicalOpNode ----------------------------

LogicalOpNode::~LogicalOpNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void LogicalOpNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr1);
    releaser.release(m_expr2);
}

RegisterID* LogicalOpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    RefPtr<LabelID> target = generator.newLabel();
    
    generator.emitNode(temp.get(), m_expr1.get());
    if (m_operator == OpLogicalAnd)
        generator.emitJumpIfFalse(temp.get(), target.get());
    else
        generator.emitJumpIfTrue(temp.get(), target.get());
    generator.emitNode(temp.get(), m_expr2.get());
    generator.emitLabel(target.get());

    return generator.moveToDestinationIfNeeded(dst, temp.get());
}

// ------------------------------ ConditionalNode ------------------------------

ConditionalNode::~ConditionalNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ConditionalNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_logical);
    releaser.release(m_expr1);
    releaser.release(m_expr2);
}

RegisterID* ConditionalNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newDst = generator.finalDestination(dst);
    RefPtr<LabelID> beforeElse = generator.newLabel();
    RefPtr<LabelID> afterElse = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_logical.get());
    generator.emitJumpIfFalse(cond, beforeElse.get());

    generator.emitNode(newDst.get(), m_expr1.get());
    generator.emitJump(afterElse.get());

    generator.emitLabel(beforeElse.get());
    generator.emitNode(newDst.get(), m_expr2.get());

    generator.emitLabel(afterElse.get());

    return newDst.get();
}

// ------------------------------ ReadModifyResolveNode -----------------------------------

ReadModifyResolveNode::~ReadModifyResolveNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ReadModifyResolveNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_right);
}

// FIXME: should this be moved to be a method on CodeGenerator?
static ALWAYS_INLINE RegisterID* emitReadModifyAssignment(CodeGenerator& generator, RegisterID* dst, RegisterID* src1, RegisterID* src2, Operator oper, OperandTypes types)
{
    OpcodeID opcode;
    switch (oper) {
        case OpMultEq:
            opcode = op_mul;
            break;
        case OpDivEq:
            opcode = op_div;
            break;
        case OpPlusEq:
            opcode = op_add;
            break;
        case OpMinusEq:
            opcode = op_sub;
            break;
        case OpLShift:
            opcode = op_lshift;
            break;
        case OpRShift:
            opcode = op_rshift;
            break;
        case OpURShift:
            opcode = op_urshift;
            break;
        case OpAndEq:
            opcode = op_bitand;
            break;
        case OpXOrEq:
            opcode = op_bitxor;
            break;
        case OpOrEq:
            opcode = op_bitor;
            break;
        case OpModEq:
            opcode = op_mod;
            break;
        default:
            ASSERT_NOT_REACHED();
            return dst;
    }
    
    return generator.emitBinaryOp(opcode, dst, src1, src2, types);
}

RegisterID* ReadModifyResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RegisterID* src2 = generator.emitNode(m_right.get());
            return emitReadModifyAssignment(generator, generator.finalDestination(dst), local, src2, m_operator, OperandTypes(ResultType::unknown(), m_right->resultDescriptor()));
        }
        
        if (generator.leftHandSideNeedsCopy(m_rightHasAssignments, m_right->isPure(generator))) {
            RefPtr<RegisterID> result = generator.newTemporary();
            generator.emitMove(result.get(), local);
            RegisterID* src2 = generator.emitNode(m_right.get());
            emitReadModifyAssignment(generator, result.get(), result.get(), src2, m_operator, OperandTypes(ResultType::unknown(), m_right->resultDescriptor()));
            generator.emitMove(local, result.get());
            return generator.moveToDestinationIfNeeded(dst, result.get());
        }
        
        RegisterID* src2 = generator.emitNode(m_right.get());
        RegisterID* result = emitReadModifyAssignment(generator, local, local, src2, m_operator, OperandTypes(ResultType::unknown(), m_right->resultDescriptor()));
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    int index = 0;
    size_t depth = 0;
    JSObject* globalObject = 0;
    if (generator.findScopedProperty(m_ident, index, depth, true, globalObject) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> src1 = generator.emitGetScopedVar(generator.tempDestination(dst), depth, index, globalObject);
        RegisterID* src2 = generator.emitNode(m_right.get());
        RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst, src1.get()), src1.get(), src2, m_operator, OperandTypes(ResultType::unknown(), m_right->resultDescriptor()));
        generator.emitPutScopedVar(depth, index, result, globalObject);
        return result;
    }

    RefPtr<RegisterID> src1 = generator.tempDestination(dst);
    generator.emitExpressionInfo(divot() - startOffset() + m_ident.size(), m_ident.size(), 0);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), src1.get(), m_ident);
    RegisterID* src2 = generator.emitNode(m_right.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst, src1.get()), src1.get(), src2, m_operator, OperandTypes(ResultType::unknown(), m_right->resultDescriptor()));
    return generator.emitPutById(base.get(), m_ident, result);
}

// ------------------------------ AssignResolveNode -----------------------------------

AssignResolveNode::~AssignResolveNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void AssignResolveNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_right);
}

RegisterID* AssignResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (generator.isLocalConstant(m_ident))
            return generator.emitNode(dst, m_right.get());
        
        RegisterID* result = generator.emitNode(local, m_right.get());
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    int index = 0;
    size_t depth = 0;
    JSObject* globalObject = 0;
    if (generator.findScopedProperty(m_ident, index, depth, true, globalObject) && index != missingSymbolMarker()) {
        if (dst == ignoredResult())
            dst = 0;
        RegisterID* value = generator.emitNode(dst, m_right.get());
        generator.emitPutScopedVar(depth, index, value, globalObject);
        return value;
    }

    RefPtr<RegisterID> base = generator.emitResolveBase(generator.newTemporary(), m_ident);
    if (dst == ignoredResult())
        dst = 0;
    RegisterID* value = generator.emitNode(dst, m_right.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitPutById(base.get(), m_ident, value);
}

// ------------------------------ AssignDotNode -----------------------------------

AssignDotNode::~AssignDotNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void AssignDotNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_right);
}

RegisterID* AssignDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RegisterID* result = generator.emitNode(value.get(), m_right.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitPutById(base.get(), m_ident, result);
    return generator.moveToDestinationIfNeeded(dst, result);
}

// ------------------------------ ReadModifyDotNode -----------------------------------

ReadModifyDotNode::~ReadModifyDotNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ReadModifyDotNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_right);
}

RegisterID* ReadModifyDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator, OperandTypes(ResultType::unknown(), m_right->resultDescriptor()));

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitPutById(base.get(), m_ident, updatedValue);
}

// ------------------------------ AssignErrorNode -----------------------------------

AssignErrorNode::~AssignErrorNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void AssignErrorNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_left);
    releaser.release(m_right);
}

RegisterID* AssignErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, "Left side of assignment is not a reference.");
}

// ------------------------------ AssignBracketNode -----------------------------------

AssignBracketNode::~AssignBracketNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void AssignBracketNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_subscript);
    releaser.release(m_right);
}

RegisterID* AssignBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript.get(), m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RegisterID* result = generator.emitNode(value.get(), m_right.get());

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitPutByVal(base.get(), property.get(), result);
    return generator.moveToDestinationIfNeeded(dst, result);
}

// ------------------------------ ReadModifyBracketNode -----------------------------------

ReadModifyBracketNode::~ReadModifyBracketNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ReadModifyBracketNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_base);
    releaser.release(m_subscript);
    releaser.release(m_right);
}

RegisterID* ReadModifyBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript.get(), m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator, OperandTypes(ResultType::unknown(), m_right->resultDescriptor()));

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitPutByVal(base.get(), property.get(), updatedValue);

    return updatedValue;
}

// ------------------------------ CommaNode ------------------------------------

CommaNode::~CommaNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void CommaNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr1);
    releaser.release(m_expr2);
}

RegisterID* CommaNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(ignoredResult(), m_expr1.get());
    return generator.emitNode(dst, m_expr2.get());
}

// ------------------------------ ConstDeclNode ------------------------------------

ConstDeclNode::~ConstDeclNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ConstDeclNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_next);
    releaser.release(m_init);
}

ConstDeclNode::ConstDeclNode(JSGlobalData* globalData, const Identifier& ident, ExpressionNode* init)
    : ExpressionNode(globalData)
    , m_ident(ident)
    , m_init(init)
{
}

RegisterID* ConstDeclNode::emitCodeSingle(CodeGenerator& generator)
{
    if (RegisterID* local = generator.constRegisterFor(m_ident)) {
        if (!m_init)
            return local;

        return generator.emitNode(local, m_init.get());
    }
    
    // FIXME: While this code should only be hit in eval code, it will potentially
    // assign to the wrong base if m_ident exists in an intervening dynamic scope.
    RefPtr<RegisterID> base = generator.emitResolveBase(generator.newTemporary(), m_ident);
    RegisterID* value = m_init ? generator.emitNode(m_init.get()) : generator.emitLoad(0, jsUndefined());
    return generator.emitPutById(base.get(), m_ident, value);
}

RegisterID* ConstDeclNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    RegisterID* result = 0;
    for (ConstDeclNode* n = this; n; n = n->m_next.get())
        result = n->emitCodeSingle(generator);

    return result;
}

// ------------------------------ ConstStatementNode -----------------------------

ConstStatementNode::~ConstStatementNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ConstStatementNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_next);
}

RegisterID* ConstStatementNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return generator.emitNode(m_next.get());
}

// ------------------------------ Helper functions for handling Vectors of StatementNode -------------------------------

static inline RegisterID* statementListEmitCode(const StatementVector& statements, CodeGenerator& generator, RegisterID* dst)
{
    StatementVector::const_iterator end = statements.end();
    for (StatementVector::const_iterator it = statements.begin(); it != end; ++it) {
        StatementNode* n = it->get();
        if (!n->isLoop())
            generator.emitDebugHook(WillExecuteStatement, n->firstLine(), n->lastLine());
        generator.emitNode(dst, n);
    }
    return 0;
}

// ------------------------------ BlockNode ------------------------------------

BlockNode::~BlockNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void BlockNode::releaseNodes(NodeReleaser& releaser)
{
    size_t size = m_children.size();
    for (size_t i = 0; i < size; ++i)
        releaser.release(m_children[i]);
}

BlockNode::BlockNode(JSGlobalData* globalData, SourceElements* children)
    : StatementNode(globalData)
{
    if (children)
        children->releaseContentsIntoVector(m_children);
}

RegisterID* BlockNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return statementListEmitCode(m_children, generator, dst);
}

// ------------------------------ EmptyStatementNode ---------------------------

RegisterID* EmptyStatementNode::emitCode(CodeGenerator&, RegisterID* dst)
{
    return dst;
}

// ------------------------------ DebuggerStatementNode ---------------------------

RegisterID* DebuggerStatementNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(DidReachBreakpoint, firstLine(), lastLine());
    return dst;
}

// ------------------------------ ExprStatementNode ----------------------------

RegisterID* ExprStatementNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    return generator.emitNode(dst, m_expr.get());
}

// ------------------------------ VarStatementNode ----------------------------

VarStatementNode::~VarStatementNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void VarStatementNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* VarStatementNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    ASSERT(m_expr);
    return generator.emitNode(m_expr.get());
}

// ------------------------------ IfNode ---------------------------------------

IfNode::~IfNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void IfNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_condition);
    releaser.release(m_ifBlock);
}

RegisterID* IfNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> afterThen = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, afterThen.get());

    if (!m_ifBlock->isBlock())
        generator.emitDebugHook(WillExecuteStatement, m_ifBlock->firstLine(), m_ifBlock->lastLine());

    generator.emitNode(dst, m_ifBlock.get());
    generator.emitLabel(afterThen.get());

    // FIXME: This should return the last statement executed so that it can be returned as a Completion.
    return 0;
}

// ------------------------------ IfElseNode ---------------------------------------

IfElseNode::~IfElseNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void IfElseNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_elseBlock);
    IfNode::releaseNodes(releaser);
}

RegisterID* IfElseNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> beforeElse = generator.newLabel();
    RefPtr<LabelID> afterElse = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, beforeElse.get());

    if (!m_ifBlock->isBlock())
        generator.emitDebugHook(WillExecuteStatement, m_ifBlock->firstLine(), m_ifBlock->lastLine());

    generator.emitNode(dst, m_ifBlock.get());
    generator.emitJump(afterElse.get());

    generator.emitLabel(beforeElse.get());

    if (!m_elseBlock->isBlock())
        generator.emitDebugHook(WillExecuteStatement, m_elseBlock->firstLine(), m_elseBlock->lastLine());

    generator.emitNode(dst, m_elseBlock.get());

    generator.emitLabel(afterElse.get());

    // FIXME: This should return the last statement executed so that it can be returned as a Completion.
    return 0;
}

// ------------------------------ DoWhileNode ----------------------------------

DoWhileNode::~DoWhileNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void DoWhileNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_statement);
    releaser.release(m_expr);
}

RegisterID* DoWhileNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    RefPtr<LabelID> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());

    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

    if (!m_statement->isBlock())
        generator.emitDebugHook(WillExecuteStatement, m_statement->firstLine(), m_statement->lastLine());
        
    RefPtr<RegisterID> result = generator.emitNode(dst, m_statement.get());

    generator.emitLabel(scope->continueTarget());
    generator.emitDebugHook(WillExecuteStatement, m_expr->lineNo(), m_expr->lineNo());
    RegisterID* cond = generator.emitNode(m_expr.get());
    generator.emitJumpIfTrue(cond, topOfLoop.get());

    generator.emitLabel(scope->breakTarget());
    return result.get();
}

// ------------------------------ WhileNode ------------------------------------

WhileNode::~WhileNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void WhileNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
    releaser.release(m_statement);
}

RegisterID* WhileNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    generator.emitJump(scope->continueTarget());

    RefPtr<LabelID> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());

    if (!m_statement->isBlock())
        generator.emitDebugHook(WillExecuteStatement, m_statement->firstLine(), m_statement->lastLine());
 
    generator.emitNode(dst, m_statement.get());

    generator.emitLabel(scope->continueTarget());
    generator.emitDebugHook(WillExecuteStatement, m_expr->lineNo(), m_expr->lineNo());
    RegisterID* cond = generator.emitNode(m_expr.get());
    generator.emitJumpIfTrue(cond, topOfLoop.get());

    generator.emitLabel(scope->breakTarget());
    
    // FIXME: This should return the last statement executed so that it can be returned as a Completion
    return 0;
}

// ------------------------------ ForNode --------------------------------------

ForNode::~ForNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ForNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr1);
    releaser.release(m_expr2);
    releaser.release(m_expr3);
    releaser.release(m_statement);
}

RegisterID* ForNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult())
        dst = 0;

    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

    if (m_expr1)
        generator.emitNode(ignoredResult(), m_expr1.get());

    RefPtr<LabelID> condition = generator.newLabel();
    generator.emitJump(condition.get());

    RefPtr<LabelID> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());

    if (!m_statement->isBlock())
        generator.emitDebugHook(WillExecuteStatement, m_statement->firstLine(), m_statement->lastLine());
    RefPtr<RegisterID> result = generator.emitNode(dst, m_statement.get());

    generator.emitLabel(scope->continueTarget());
    if (m_expr3)
        generator.emitNode(ignoredResult(), m_expr3.get());

    generator.emitLabel(condition.get());
    if (m_expr2) {
        RegisterID* cond = generator.emitNode(m_expr2.get());
        generator.emitJumpIfTrue(cond, topOfLoop.get());
    } else
        generator.emitJump(topOfLoop.get());

    generator.emitLabel(scope->breakTarget());
    return result.get();
}

// ------------------------------ ForInNode ------------------------------------

ForInNode::~ForInNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ForInNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_init);
    releaser.release(m_lexpr);
    releaser.release(m_expr);
    releaser.release(m_statement);
}

ForInNode::ForInNode(JSGlobalData* globalData, ExpressionNode* l, ExpressionNode* expr, StatementNode* statement)
    : StatementNode(globalData)
    , m_init(0L)
    , m_lexpr(l)
    , m_expr(expr)
    , m_statement(statement)
    , m_identIsVarDecl(false)
{
}

ForInNode::ForInNode(JSGlobalData* globalData, const Identifier& ident, ExpressionNode* in, ExpressionNode* expr, StatementNode* statement, int divot, int startOffset, int endOffset)
    : StatementNode(globalData)
    , m_ident(ident)
    , m_lexpr(new ResolveNode(globalData, ident, divot - startOffset))
    , m_expr(expr)
    , m_statement(statement)
    , m_identIsVarDecl(true)
{
    if (in) {
        AssignResolveNode* node = new AssignResolveNode(globalData, ident, in, true);
        node->setExceptionSourceCode(divot, divot - startOffset, endOffset - divot);
        m_init = node;
    }
    // for( var foo = bar in baz )
}

RegisterID* ForInNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    if (!m_lexpr->isLocation())
        return emitThrowError(generator, ReferenceError, "Left side of for-in statement is not a reference.");

    RefPtr<LabelID> continueTarget = generator.newLabel(); 

    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

    if (m_init)
        generator.emitNode(ignoredResult(), m_init.get());
    RegisterID* forInBase = generator.emitNode(m_expr.get());
    RefPtr<RegisterID> iter = generator.emitGetPropertyNames(generator.newTemporary(), forInBase);
    generator.emitJump(scope->continueTarget());

    RefPtr<LabelID> loopStart = generator.newLabel();
    generator.emitLabel(loopStart.get());

    RegisterID* propertyName;
    if (m_lexpr->isResolveNode()) {
        const Identifier& ident = static_cast<ResolveNode*>(m_lexpr.get())->identifier();
        propertyName = generator.registerFor(ident);
        if (!propertyName) {
            propertyName = generator.newTemporary();
            RefPtr<RegisterID> protect = propertyName;
            RegisterID* base = generator.emitResolveBase(generator.newTemporary(), ident);

            generator.emitExpressionInfo(divot(), startOffset(), endOffset());
            generator.emitPutById(base, ident, propertyName);
        }
    } else if (m_lexpr->isDotAccessorNode()) {
        DotAccessorNode* assignNode = static_cast<DotAccessorNode*>(m_lexpr.get());
        const Identifier& ident = assignNode->identifier();
        propertyName = generator.newTemporary();
        RefPtr<RegisterID> protect = propertyName;
        RegisterID* base = generator.emitNode(assignNode->base());

        generator.emitExpressionInfo(assignNode->divot(), assignNode->startOffset(), assignNode->endOffset());
        generator.emitPutById(base, ident, propertyName);
    } else {
        ASSERT(m_lexpr->isBracketAccessorNode());
        BracketAccessorNode* assignNode = static_cast<BracketAccessorNode*>(m_lexpr.get());
        propertyName = generator.newTemporary();
        RefPtr<RegisterID> protect = propertyName;
        RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
        RegisterID* subscript = generator.emitNode(assignNode->subscript());
        
        generator.emitExpressionInfo(assignNode->divot(), assignNode->startOffset(), assignNode->endOffset());
        generator.emitPutByVal(base.get(), subscript, propertyName);
    }   

    if (!m_statement->isBlock())
        generator.emitDebugHook(WillExecuteStatement, m_statement->firstLine(), m_statement->lastLine());
    generator.emitNode(dst, m_statement.get());

    generator.emitLabel(scope->continueTarget());
    generator.emitNextPropertyName(propertyName, iter.get(), loopStart.get());
    generator.emitLabel(scope->breakTarget());
    return dst;
}

// ------------------------------ ContinueNode ---------------------------------

// ECMA 12.7
RegisterID* ContinueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    LabelScope* scope = generator.continueTarget(m_ident);

    if (!scope)
        return m_ident.isEmpty()
            ? emitThrowError(generator, SyntaxError, "Invalid continue statement.")
            : emitThrowError(generator, SyntaxError, "Undefined label: '%s'.", m_ident);

    generator.emitJumpScopes(scope->continueTarget(), scope->scopeDepth());
    return dst;
}

// ------------------------------ BreakNode ------------------------------------

// ECMA 12.8
RegisterID* BreakNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    LabelScope* scope = generator.breakTarget(m_ident);
    
    if (!scope)
        return m_ident.isEmpty()
            ? emitThrowError(generator, SyntaxError, "Invalid break statement.")
            : emitThrowError(generator, SyntaxError, "Undefined label: '%s'.", m_ident);

    generator.emitJumpScopes(scope->breakTarget(), scope->scopeDepth());
    return dst;
}

// ------------------------------ ReturnNode -----------------------------------

ReturnNode::~ReturnNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ReturnNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_value);
}

RegisterID* ReturnNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.codeType() != FunctionCode)
        return emitThrowError(generator, SyntaxError, "Invalid return statement.");

    if (dst == ignoredResult())
        dst = 0;
    RegisterID* r0 = m_value ? generator.emitNode(dst, m_value.get()) : generator.emitLoad(dst, jsUndefined());
    if (generator.scopeDepth()) {
        RefPtr<LabelID> l0 = generator.newLabel();
        generator.emitJumpScopes(l0.get(), 0);
        generator.emitLabel(l0.get());
    }
    generator.emitDebugHook(WillLeaveCallFrame, firstLine(), lastLine());
    return generator.emitReturn(r0);
}

// ------------------------------ WithNode -------------------------------------

WithNode::~WithNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void WithNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
    releaser.release(m_statement);
}

RegisterID* WithNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> scope = generator.emitNode(m_expr.get()); // scope must be protected until popped
    generator.emitExpressionInfo(m_divot, m_expressionLength, 0);
    generator.emitPushScope(scope.get());
    RegisterID* result = generator.emitNode(dst, m_statement.get());
    generator.emitPopScope();
    return result;
}

// ------------------------------ CaseClauseNode --------------------------------

CaseClauseNode::~CaseClauseNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void CaseClauseNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

// ------------------------------ ClauseListNode --------------------------------

ClauseListNode::~ClauseListNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ClauseListNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_clause);
    releaser.release(m_next);
}

// ------------------------------ CaseBlockNode --------------------------------

CaseBlockNode::~CaseBlockNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void CaseBlockNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_list1);
    releaser.release(m_defaultClause);
    releaser.release(m_list2);
}

enum SwitchKind { 
    SwitchUnset = 0,
    SwitchNumber = 1, 
    SwitchString = 2, 
    SwitchNeither = 3 
};

static void processClauseList(ClauseListNode* list, Vector<ExpressionNode*, 8>& literalVector, SwitchKind& typeForTable, bool& singleCharacterSwitch, int32_t& min_num, int32_t& max_num)
{
    for (; list; list = list->getNext()) {
        ExpressionNode* clauseExpression = list->getClause()->expr();
        literalVector.append(clauseExpression);
        if (clauseExpression->isNumber()) {
            double value = static_cast<NumberNode*>(clauseExpression)->value();
            if ((typeForTable & ~SwitchNumber) || !JSImmediate::from(value)) {
                typeForTable = SwitchNeither;
                break;
            }
            int32_t intVal = static_cast<int32_t>(value);
            ASSERT(intVal == value);
            if (intVal < min_num)
                min_num = intVal;
            if (intVal > max_num)
                max_num = intVal;
            typeForTable = SwitchNumber;
            continue;
        }
        if (clauseExpression->isString()) {
            if (typeForTable & ~SwitchString) {
                typeForTable = SwitchNeither;
                break;
            }
            const UString& value = static_cast<StringNode*>(clauseExpression)->value().ustring();
            if (singleCharacterSwitch &= value.size() == 1) {
                int32_t intVal = value.rep()->data()[0];
                if (intVal < min_num)
                    min_num = intVal;
                if (intVal > max_num)
                    max_num = intVal;
            }
            typeForTable = SwitchString;
            continue;
        }
        typeForTable = SwitchNeither;
        break;        
    }
}
    
SwitchInfo::SwitchType CaseBlockNode::tryOptimizedSwitch(Vector<ExpressionNode*, 8>& literalVector, int32_t& min_num, int32_t& max_num)
{
    SwitchKind typeForTable = SwitchUnset;
    bool singleCharacterSwitch = true;
    
    processClauseList(m_list1.get(), literalVector, typeForTable, singleCharacterSwitch, min_num, max_num);
    processClauseList(m_list2.get(), literalVector, typeForTable, singleCharacterSwitch, min_num, max_num);
    
    if (typeForTable == SwitchUnset || typeForTable == SwitchNeither)
        return SwitchInfo::SwitchNone;
    
    if (typeForTable == SwitchNumber) {
        int32_t range = max_num - min_num;
        if (min_num <= max_num && range <= 1000 && (range / literalVector.size()) < 10)
            return SwitchInfo::SwitchImmediate;
        return SwitchInfo::SwitchNone;
    } 
    
    ASSERT(typeForTable == SwitchString);
    
    if (singleCharacterSwitch) {
        int32_t range = max_num - min_num;
        if (min_num <= max_num && range <= 1000 && (range / literalVector.size()) < 10)
            return SwitchInfo::SwitchCharacter;
    }

    return SwitchInfo::SwitchString;
}

RegisterID* CaseBlockNode::emitCodeForBlock(CodeGenerator& generator, RegisterID* switchExpression, RegisterID* dst)
{
    RefPtr<LabelID> defaultLabel;
    Vector<RefPtr<LabelID>, 8> labelVector;
    Vector<ExpressionNode*, 8> literalVector;
    int32_t min_num = std::numeric_limits<int32_t>::max();
    int32_t max_num = std::numeric_limits<int32_t>::min();
    SwitchInfo::SwitchType switchType = tryOptimizedSwitch(literalVector, min_num, max_num);

    if (switchType != SwitchInfo::SwitchNone) {
        // Prepare the various labels
        for (uint32_t i = 0; i < literalVector.size(); i++)
            labelVector.append(generator.newLabel());
        defaultLabel = generator.newLabel();
        generator.beginSwitch(switchExpression, switchType);
    } else {
        // Setup jumps
        for (ClauseListNode* list = m_list1.get(); list; list = list->getNext()) {
            RefPtr<RegisterID> clauseVal = generator.newTemporary();
            generator.emitNode(clauseVal.get(), list->getClause()->expr());
            generator.emitBinaryOp(op_stricteq, clauseVal.get(), clauseVal.get(), switchExpression, OperandTypes());
            labelVector.append(generator.newLabel());
            generator.emitJumpIfTrue(clauseVal.get(), labelVector[labelVector.size() - 1].get());
        }
        
        for (ClauseListNode* list = m_list2.get(); list; list = list->getNext()) {
            RefPtr<RegisterID> clauseVal = generator.newTemporary();
            generator.emitNode(clauseVal.get(), list->getClause()->expr());
            generator.emitBinaryOp(op_stricteq, clauseVal.get(), clauseVal.get(), switchExpression, OperandTypes());
            labelVector.append(generator.newLabel());
            generator.emitJumpIfTrue(clauseVal.get(), labelVector[labelVector.size() - 1].get());
        }
        defaultLabel = generator.newLabel();
        generator.emitJump(defaultLabel.get());
    }

    RegisterID* result = 0;

    size_t i = 0;
    for (ClauseListNode* list = m_list1.get(); list; list = list->getNext()) {
        generator.emitLabel(labelVector[i++].get());
        result = statementListEmitCode(list->getClause()->children(), generator, dst);
    }

    if (m_defaultClause) {
        generator.emitLabel(defaultLabel.get());
        result = statementListEmitCode(m_defaultClause->children(), generator, dst);
    }

    for (ClauseListNode* list = m_list2.get(); list; list = list->getNext()) {
        generator.emitLabel(labelVector[i++].get());
        result = statementListEmitCode(list->getClause()->children(), generator, dst);
    }
    if (!m_defaultClause)
        generator.emitLabel(defaultLabel.get());

    ASSERT(i == labelVector.size());
    if (switchType != SwitchInfo::SwitchNone) {
        ASSERT(labelVector.size() == literalVector.size());
        generator.endSwitch(labelVector.size(), labelVector.data(), literalVector.data(), defaultLabel.get(), min_num, max_num);
    }
    return result;
}

// ------------------------------ SwitchNode -----------------------------------

SwitchNode::~SwitchNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void SwitchNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
    releaser.release(m_block);
}

RegisterID* SwitchNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Switch);

    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    RegisterID* r1 = m_block->emitCodeForBlock(generator, r0.get(), dst);

    generator.emitLabel(scope->breakTarget());
    return r1;
}

// ------------------------------ LabelNode ------------------------------------

LabelNode::~LabelNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void LabelNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_statement);
}

RegisterID* LabelNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.breakTarget(m_name))
        return emitThrowError(generator, SyntaxError, "Duplicate label: %s.", m_name);

    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::NamedLabel, &m_name);
    RegisterID* r0 = generator.emitNode(dst, m_statement.get());

    generator.emitLabel(scope->breakTarget());
    return r0;
}

// ------------------------------ ThrowNode ------------------------------------

ThrowNode::~ThrowNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ThrowNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_expr);
}

RegisterID* ThrowNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (dst == ignoredResult())
        dst = 0;
    RefPtr<RegisterID> expr = generator.emitNode(dst, m_expr.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitThrow(expr.get());
    return dst;
}

// ------------------------------ TryNode --------------------------------------

TryNode::~TryNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void TryNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_tryBlock);
    releaser.release(m_catchBlock);
    releaser.release(m_finallyBlock);
}

RegisterID* TryNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> tryStartLabel = generator.newLabel();
    RefPtr<LabelID> tryEndLabel = generator.newLabel();
    RefPtr<LabelID> finallyStart;
    RefPtr<RegisterID> finallyReturnAddr;
    if (m_finallyBlock) {
        finallyStart = generator.newLabel();
        finallyReturnAddr = generator.newTemporary();
        generator.pushFinallyContext(finallyStart.get(), finallyReturnAddr.get());
    }
    generator.emitLabel(tryStartLabel.get());
    generator.emitNode(dst, m_tryBlock.get());
    generator.emitLabel(tryEndLabel.get());

    if (m_catchBlock) {
        RefPtr<LabelID> handlerEndLabel = generator.newLabel();
        generator.emitJump(handlerEndLabel.get());
        RefPtr<RegisterID> exceptionRegister = generator.emitCatch(generator.newTemporary(), tryStartLabel.get(), tryEndLabel.get());
        generator.emitPushNewScope(exceptionRegister.get(), m_exceptionIdent, exceptionRegister.get());
        generator.emitNode(dst, m_catchBlock.get());
        generator.emitPopScope();
        generator.emitLabel(handlerEndLabel.get());
    }

    if (m_finallyBlock) {
        generator.popFinallyContext();
        // there may be important registers live at the time we jump
        // to a finally block (such as for a return or throw) so we
        // ref the highest register ever used as a conservative
        // approach to not clobbering anything important
        RefPtr<RegisterID> highestUsedRegister = generator.highestUsedRegister();
        RefPtr<LabelID> finallyEndLabel = generator.newLabel();
        generator.emitJumpSubroutine(finallyReturnAddr.get(), finallyStart.get());
        // Use a label to record the subtle fact that sret will return to the
        // next instruction. sret is the only way to jump without an explicit label.
        generator.emitLabel(generator.newLabel().get());
        generator.emitJump(finallyEndLabel.get());

        // Finally block for exception path
        RefPtr<RegisterID> tempExceptionRegister = generator.emitCatch(generator.newTemporary(), tryStartLabel.get(), generator.emitLabel(generator.newLabel().get()).get());
        generator.emitJumpSubroutine(finallyReturnAddr.get(), finallyStart.get());
        // Use a label to record the subtle fact that sret will return to the
        // next instruction. sret is the only way to jump without an explicit label.
        generator.emitLabel(generator.newLabel().get());
        generator.emitThrow(tempExceptionRegister.get());

        // emit the finally block itself
        generator.emitLabel(finallyStart.get());
        generator.emitNode(dst, m_finallyBlock.get());
        generator.emitSubroutineReturn(finallyReturnAddr.get());

        generator.emitLabel(finallyEndLabel.get());
    }

    return dst;
}

// ------------------------------ ParameterNode -----------------------------

ParameterNode::~ParameterNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ParameterNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_next);
}

// ------------------------------ ScopeNode -----------------------------

ScopeNode::ScopeNode(JSGlobalData* globalData, const SourceCode& source, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, CodeFeatures features, int numConstants)
    : BlockNode(globalData, children)
    , m_source(source)
    , m_features(features)
    , m_numConstants(numConstants)
{
    if (varStack)
        m_varStack = *varStack;
    if (funcStack)
        m_functionStack = *funcStack;
#if ENABLE(OPCODE_SAMPLING)
    globalData->machine->sampler()->notifyOfScope(this);
#endif
}

// ------------------------------ ProgramNode -----------------------------

ProgramNode::ProgramNode(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& source, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, source, children, varStack, funcStack, features, numConstants)
{
}

ProgramNode* ProgramNode::create(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& source, CodeFeatures features, int numConstants)
{
    return new ProgramNode(globalData, children, varStack, funcStack, source, features, numConstants);
}

// ------------------------------ EvalNode -----------------------------

EvalNode::EvalNode(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& source, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, source, children, varStack, funcStack, features, numConstants)
{
}

RegisterID* EvalNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteProgram, firstLine(), lastLine());

    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    statementListEmitCode(children(), generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, firstLine(), lastLine());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void EvalNode::generateCode(ScopeChainNode* scopeChainNode)
{
    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    m_code.set(new EvalCodeBlock(this, globalObject, source().provider()));

    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_code->symbolTable, m_code.get());
    generator.generate();
}

EvalNode* EvalNode::create(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& source, CodeFeatures features, int numConstants)
{
    return new EvalNode(globalData, children, varStack, funcStack, source, features, numConstants);
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionBodyNode::FunctionBodyNode(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& sourceCode, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, sourceCode, children, varStack, funcStack, features, numConstants)
    , m_parameters(0)
    , m_parameterCount(0)
    , m_refCount(0)
{
}

FunctionBodyNode::~FunctionBodyNode()
{
    ASSERT(!m_refCount);
    fastFree(m_parameters);
}

void FunctionBodyNode::finishParsing(const SourceCode& source, ParameterNode* firstParameter)
{
    Vector<Identifier> parameters;
    for (ParameterNode* parameter = firstParameter; parameter; parameter = parameter->nextParam())
        parameters.append(parameter->ident());
    size_t count = parameters.size();

    setSource(source);
    finishParsing(parameters.releaseBuffer(), count);
}

void FunctionBodyNode::finishParsing(Identifier* parameters, size_t parameterCount)
{
    ASSERT(!source().isNull());
    m_parameters = parameters;
    m_parameterCount = parameterCount;
}

void FunctionBodyNode::mark()
{
    if (m_code)
        m_code->mark();
}

FunctionBodyNode* FunctionBodyNode::create(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, CodeFeatures features, int numConstants)
{
    return new FunctionBodyNode(globalData, children, varStack, funcStack, SourceCode(), features, numConstants);
}

FunctionBodyNode* FunctionBodyNode::create(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& sourceCode, CodeFeatures features, int numConstants)
{
    return new FunctionBodyNode(globalData, children, varStack, funcStack, sourceCode, features, numConstants);
}

void FunctionBodyNode::generateCode(ScopeChainNode* scopeChainNode)
{
    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    m_code.set(new CodeBlock(this, FunctionCode, source().provider(), source().startOffset()));

    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_code->symbolTable, m_code.get());
    generator.generate();
}

RegisterID* FunctionBodyNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(DidEnterCallFrame, firstLine(), lastLine());
    statementListEmitCode(children(), generator, ignoredResult());
    if (!children().size() || !children().last()->isReturnNode()) {
        RegisterID* r0 = generator.emitLoad(0, jsUndefined());
        generator.emitDebugHook(WillLeaveCallFrame, firstLine(), lastLine());
        generator.emitReturn(r0);
    }
    return 0;
}

RegisterID* ProgramNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteProgram, firstLine(), lastLine());

    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    statementListEmitCode(children(), generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, firstLine(), lastLine());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void ProgramNode::generateCode(ScopeChainNode* scopeChainNode)
{
    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    m_code.set(new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider()));
    
    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_code.get(), m_varStack, m_functionStack);
    generator.generate();
}

UString FunctionBodyNode::paramString() const
{
    UString s("");
    for (size_t pos = 0; pos < m_parameterCount; ++pos) {
        if (!s.isEmpty())
            s += ", ";
        s += parameters()[pos].ustring();
    }

    return s;
}

Identifier* FunctionBodyNode::copyParameters()
{
    Identifier* parameters = static_cast<Identifier*>(fastMalloc(m_parameterCount * sizeof(Identifier)));
    VectorCopier<false, Identifier>::uninitializedCopy(m_parameters, m_parameters + m_parameterCount, parameters);
    return parameters;
}

// ------------------------------ FuncDeclNode ---------------------------------

FuncDeclNode::~FuncDeclNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void FuncDeclNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_parameter);
    releaser.release(m_body);
}

JSFunction* FuncDeclNode::makeFunction(ExecState* exec, ScopeChainNode* scopeChain)
{
    return new (exec) JSFunction(exec, m_ident, m_body.get(), scopeChain);
}

RegisterID* FuncDeclNode::emitCode(CodeGenerator&, RegisterID* dst)
{
    return dst;
}

// ------------------------------ FuncExprNode ---------------------------------

FuncExprNode::~FuncExprNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void FuncExprNode::releaseNodes(NodeReleaser& releaser)
{
    releaser.release(m_parameter);
    releaser.release(m_body);
}

RegisterID* FuncExprNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitNewFunctionExpression(generator.finalDestination(dst), this);
}

JSFunction* FuncExprNode::makeFunction(ExecState* exec, ScopeChainNode* scopeChain)
{
    JSFunction* func = new (exec) JSFunction(exec, m_ident, m_body.get(), scopeChain);

    /* 
        The Identifier in a FunctionExpression can be referenced from inside
        the FunctionExpression's FunctionBody to allow the function to call
        itself recursively. However, unlike in a FunctionDeclaration, the
        Identifier in a FunctionExpression cannot be referenced from and
        does not affect the scope enclosing the FunctionExpression.
     */

    if (!m_ident.isNull()) {
        JSStaticScopeObject* functionScopeObject = new (exec) JSStaticScopeObject(exec, m_ident, func, ReadOnly | DontDelete);
        func->scope().push(functionScopeObject);
    }

    return func;
}

} // namespace JSC
