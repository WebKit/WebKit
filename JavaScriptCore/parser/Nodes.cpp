/*
*  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
*  Copyright (C) 2001 Peter Kelly (pmk@post.com)
*  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "NodeConstructors.h"

#include "BytecodeGenerator.h"
#include "CallFrame.h"
#include "Debugger.h"
#include "JIT.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSStaticScopeObject.h"
#include "LabelScope.h"
#include "Lexer.h"
#include "Operations.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "RegExpObject.h"
#include "SamplingTool.h"
#include <wtf/Assertions.h>
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

ALWAYS_INLINE void NodeReleaser::releaseAllNodes(ParserRefCounted* root)
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

ALWAYS_INLINE void NodeReleaser::adopt(PassRefPtr<ParserRefCounted> node)
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

ALWAYS_INLINE ParserRefCounted::ParserRefCounted(JSGlobalData* globalData)
{
    globalData->parserObjects.append(adoptRef(this));
    parserRefCountedCounter.increment();
}

ALWAYS_INLINE ParserRefCounted::~ParserRefCounted()
{
    parserRefCountedCounter.decrement();
}

#endif

void ParserRefCounted::releaseNodes(NodeReleaser&)
{
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

RegisterID* ThrowableExpressionData::emitThrowError(BytecodeGenerator& generator, ErrorType e, const char* msg)
{
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(generator.globalData(), msg));
    generator.emitThrow(exception);
    return exception;
}

RegisterID* ThrowableExpressionData::emitThrowError(BytecodeGenerator& generator, ErrorType e, const char* msg, const Identifier& label)
{
    UString message = msg;
    substitute(message, label.ustring());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(generator.globalData(), message));
    generator.emitThrow(exception);
    return exception;
}
    
// ------------------------------ StatementNode --------------------------------

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

RegisterID* NullNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitLoad(dst, jsNull());
}

// ------------------------------ BooleanNode ----------------------------------

RegisterID* BooleanNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitLoad(dst, m_value);
}

// ------------------------------ NumberNode -----------------------------------

RegisterID* NumberNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitLoad(dst, m_double);
}

// ------------------------------ StringNode -----------------------------------

RegisterID* StringNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitLoad(dst, m_value);
}

// ------------------------------ RegExpNode -----------------------------------

RegisterID* RegExpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegExp> regExp = RegExp::create(generator.globalData(), m_pattern, m_flags);
    if (!regExp->isValid())
        return emitThrowError(generator, SyntaxError, ("Invalid regular expression: " + UString(regExp->errorMessage())).UTF8String().c_str());
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitNewRegExp(generator.finalDestination(dst), regExp.get());
}

// ------------------------------ ThisNode -------------------------------------

RegisterID* ThisNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;
    return generator.moveToDestinationIfNeeded(dst, generator.thisRegister());
}

// ------------------------------ ResolveNode ----------------------------------

bool ResolveNode::isPure(BytecodeGenerator& generator) const
{
    return generator.isLocal(m_ident);
}

RegisterID* ResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (dst == generator.ignoredResult())
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

RegisterID* ArrayNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

bool ArrayNode::isSimpleArray() const
{
    if (m_elision || m_optional)
        return false;
    for (ElementNode* ptr = m_element.get(); ptr; ptr = ptr->next()) {
        if (ptr->elision())
            return false;
    }
    return true;
}

PassRefPtr<ArgumentListNode> ArrayNode::toArgumentList(JSGlobalData* globalData) const
{
    ASSERT(!m_elision && !m_optional);
    RefPtr<ArgumentListNode> head;
    ElementNode* ptr = m_element.get();
    if (!ptr)
        return head;
    head = new ArgumentListNode(globalData, ptr->value());
    ArgumentListNode* tail = head.get();
    ptr = ptr->next();
    for (; ptr; ptr = ptr->next()) {
        ASSERT(!ptr->elision());
        tail = new ArgumentListNode(globalData, tail, ptr->value());
    }
    return head.release();
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

RegisterID* ObjectLiteralNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
     if (!m_list) {
         if (dst == generator.ignoredResult())
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

RegisterID* PropertyListNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* BracketAccessorNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* DotAccessorNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* ArgumentListNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* NewExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* EvalFunctionCallNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> func = generator.tempDestination(dst);
    RefPtr<RegisterID> thisRegister = generator.newTemporary();
    generator.emitExpressionInfo(divot() - startOffset() + 4, 4, 0);
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

RegisterID* FunctionCallValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* FunctionCallResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

    RefPtr<RegisterID> func = generator.newTemporary();
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

RegisterID* FunctionCallBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* FunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> function = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RefPtr<RegisterID> thisRegister = generator.emitMove(generator.newTemporary(), base.get());
    return generator.emitCall(generator.finalDestination(dst, function.get()), function.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
}

RegisterID* CallFunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<Label> realCall = generator.newLabel();
    RefPtr<Label> end = generator.newLabel();
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> function = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RefPtr<RegisterID> finalDestination = generator.finalDestination(dst, function.get());
    generator.emitJumpIfNotFunctionCall(function.get(), realCall.get());
    {
        RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
        RefPtr<RegisterID> thisRegister = generator.newTemporary();
        RefPtr<ArgumentListNode> oldList = m_args->m_listNode;
        if (m_args->m_listNode && m_args->m_listNode->m_expr) {
            generator.emitNode(thisRegister.get(), m_args->m_listNode->m_expr.get());
            m_args->m_listNode = m_args->m_listNode->m_next;
        } else
            generator.emitLoad(thisRegister.get(), jsNull());

        generator.emitCall(finalDestination.get(), realFunction.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
        generator.emitJump(end.get());
        m_args->m_listNode = oldList;
    }
    generator.emitLabel(realCall.get());
    {
        RefPtr<RegisterID> thisRegister = generator.emitMove(generator.newTemporary(), base.get());
        generator.emitCall(finalDestination.get(), function.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
    }
    generator.emitLabel(end.get());
    return finalDestination.get();
}
    
static bool areTrivialApplyArguments(ArgumentsNode* args)
{
    return !args->m_listNode || !args->m_listNode->m_expr || !args->m_listNode->m_next
        || (!args->m_listNode->m_next->m_next && args->m_listNode->m_next->m_expr->isSimpleArray());
}

RegisterID* ApplyFunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    // A few simple cases can be trivially handled as ordinary functions calls
    // function.apply(), function.apply(arg) -> identical to function.call
    // function.apply(thisArg, [arg0, arg1, ...]) -> can be trivially coerced into function.call(thisArg, arg0, arg1, ...) and saves object allocation
    bool mayBeCall = areTrivialApplyArguments(m_args.get());

    RefPtr<Label> realCall = generator.newLabel();
    RefPtr<Label> end = generator.newLabel();
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> function = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RefPtr<RegisterID> finalDestination = generator.finalDestination(dst, function.get());
    generator.emitJumpIfNotFunctionApply(function.get(), realCall.get());
    {
        if (mayBeCall) {
            RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
            RefPtr<RegisterID> thisRegister = generator.newTemporary();
            RefPtr<ArgumentListNode> oldList = m_args->m_listNode;
            if (m_args->m_listNode && m_args->m_listNode->m_expr) {
                generator.emitNode(thisRegister.get(), m_args->m_listNode->m_expr.get());
                m_args->m_listNode = m_args->m_listNode->m_next;
                if (m_args->m_listNode) {
                    ASSERT(m_args->m_listNode->m_expr->isSimpleArray());
                    ASSERT(!m_args->m_listNode->m_next);
                    m_args->m_listNode = static_cast<ArrayNode*>(m_args->m_listNode->m_expr.get())->toArgumentList(generator.globalData());
                }
            } else
                generator.emitLoad(thisRegister.get(), jsNull());
            generator.emitCall(finalDestination.get(), realFunction.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
            m_args->m_listNode = oldList;
        } else {
            ASSERT(m_args->m_listNode && m_args->m_listNode->m_next);
            RefPtr<RegisterID> realFunction = generator.emitMove(generator.newTemporary(), base.get());
            RefPtr<RegisterID> argsCountRegister = generator.newTemporary();
            RefPtr<RegisterID> thisRegister = generator.newTemporary();
            RefPtr<RegisterID> argsRegister = generator.newTemporary();
            generator.emitNode(thisRegister.get(), m_args->m_listNode->m_expr.get());
            ArgumentListNode* args = m_args->m_listNode->m_next.get();
            generator.emitNode(argsRegister.get(), args->m_expr.get());
            while ((args = args->m_next.get()))
                generator.emitNode(args->m_expr.get());

            generator.emitLoadVarargs(argsCountRegister.get(), argsRegister.get());
            generator.emitCallVarargs(finalDestination.get(), realFunction.get(), thisRegister.get(), argsCountRegister.get(), divot(), startOffset(), endOffset());
        }
        generator.emitJump(end.get());
    }
    generator.emitLabel(realCall.get());
    {
        RefPtr<RegisterID> thisRegister = generator.emitMove(generator.newTemporary(), base.get());
        generator.emitCall(finalDestination.get(), function.get(), thisRegister.get(), m_args.get(), divot(), startOffset(), endOffset());
    }
    generator.emitLabel(end.get());
    return finalDestination.get();
}

// ------------------------------ PostfixResolveNode ----------------------------------

static RegisterID* emitPreIncOrDec(BytecodeGenerator& generator, RegisterID* srcDst, Operator oper)
{
    return (oper == OpPlusPlus) ? generator.emitPreInc(srcDst) : generator.emitPreDec(srcDst);
}

static RegisterID* emitPostIncOrDec(BytecodeGenerator& generator, RegisterID* dst, RegisterID* srcDst, Operator oper)
{
    return (oper == OpPlusPlus) ? generator.emitPostInc(dst, srcDst) : generator.emitPostDec(dst, srcDst);
}

RegisterID* PostfixResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            if (dst == generator.ignoredResult())
                return 0;
            return generator.emitToJSNumber(generator.finalDestination(dst), local);
        }

        if (dst == generator.ignoredResult())
            return emitPreIncOrDec(generator, local, m_operator);
        return emitPostIncOrDec(generator, generator.finalDestination(dst), local, m_operator);
    }

    int index = 0;
    size_t depth = 0;
    JSObject* globalObject = 0;
    if (generator.findScopedProperty(m_ident, index, depth, true, globalObject) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> value = generator.emitGetScopedVar(generator.newTemporary(), depth, index, globalObject);
        RegisterID* oldValue;
        if (dst == generator.ignoredResult()) {
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
    if (dst == generator.ignoredResult()) {
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

RegisterID* PostfixBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue;
    if (dst == generator.ignoredResult()) {
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

RegisterID* PostfixDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    RegisterID* oldValue;
    if (dst == generator.ignoredResult()) {
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

RegisterID* PostfixErrorNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, m_operator == OpPlusPlus ? "Postfix ++ operator applied to value that is not a reference." : "Postfix -- operator applied to value that is not a reference.");
}

// ------------------------------ DeleteResolveNode -----------------------------------

RegisterID* DeleteResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* DeleteBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* DeleteDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* DeleteValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(generator.ignoredResult(), m_expr.get());

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

RegisterID* VoidNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult()) {
        generator.emitNode(generator.ignoredResult(), m_expr.get());
        return 0;
    }
    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    return generator.emitLoad(dst, jsUndefined());
}

// ------------------------------ TypeOfValueNode -----------------------------------

RegisterID* TypeOfResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (dst == generator.ignoredResult())
            return 0;
        return generator.emitTypeOf(generator.finalDestination(dst), local);
    }

    RefPtr<RegisterID> scratch = generator.emitResolveBase(generator.tempDestination(dst), m_ident);
    generator.emitGetById(scratch.get(), scratch.get(), m_ident);
    if (dst == generator.ignoredResult())
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

RegisterID* TypeOfValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult()) {
        generator.emitNode(generator.ignoredResult(), m_expr.get());
        return 0;
    }
    RefPtr<RegisterID> src = generator.emitNode(m_expr.get());
    return generator.emitTypeOf(generator.finalDestination(dst), src.get());
}

// ------------------------------ PrefixResolveNode ----------------------------------

RegisterID* PrefixResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            if (dst == generator.ignoredResult())
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

RegisterID* PrefixBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* PrefixDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* PrefixErrorNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
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

RegisterID* UnaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitUnaryOp(opcodeID(), generator.finalDestination(dst), src);
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

// BinaryOpNode::emitStrcat:
//
// This node generates an op_strcat operation.  This opcode can handle concatenation of three or
// more values, where we can determine a set of separate op_add operations would be operating on
// string values.
//
// This function expects to be operating on a graph of AST nodes looking something like this:
//
//     (a)...     (b)
//          \   /
//           (+)     (c)
//              \   /
//      [d]     ((+))
//         \    /
//          [+=]
//
// The assignment operation is optional, if it exists the register holding the value on the
// lefthand side of the assignment should be passing as the optional 'lhs' argument.
//
// The method should be called on the node at the root of the tree of regular binary add
// operations (marked in the diagram with a double set of parentheses).  This node must
// be performing a string concatenation (determined by statically detecting that at least
// one child must be a string).  
//
// Since the minimum number of values being concatenated together is expected to be 3, if
// a lhs to a concatenating assignment is not provided then the  root add should have at
// least one left child that is also an add that can be determined to be operating on strings.
//
// Fixme: This method should correctly support concatenation on read-modify nodes (+=)
//        but disabling this due to performance degradation (op_strcat needs to be able
//        to append to the existing string).
//
RegisterID* BinaryOpNode::emitStrcat(BytecodeGenerator& generator, RegisterID* dst, RegisterID* lhs)
{
    ASSERT(isAdd());
    ASSERT(resultDescriptor().definitelyIsString());

    // Create a list of expressions for all the adds in the tree of nodes we can convert into
    // a string concatenation.  The rightmost node (c) is added first.  The rightmost node is
    // added first, and the leftmost child is never added, so the vector produced for the
    // example above will be [ c, b ].
    Vector<ExpressionNode*, 16> reverseExpressionList;
    reverseExpressionList.append(m_expr2.get());

    // Examine the left child of the add.  So long as this is a string add, add its right-child
    // to the list, and keep processing along the left fork.
    ExpressionNode* leftMostAddChild = m_expr1.get();
    while (leftMostAddChild->isAdd() && leftMostAddChild->resultDescriptor().definitelyIsString()) {
        reverseExpressionList.append(static_cast<AddNode*>(leftMostAddChild)->m_expr2.get());
        leftMostAddChild = static_cast<AddNode*>(leftMostAddChild)->m_expr1.get();
    }

    Vector<RefPtr<RegisterID>, 16> temporaryRegisters;

    // If there is an assignment, allocate a temporary to hold the lhs after conversion.
    // We could possibly avoid this (the lhs is converted last anyway, we could let the
    // op_strcat node handle its conversion if required).
    if (lhs)
        temporaryRegisters.append(generator.newTemporary());

    // Emit code for the leftmost node ((a) in the example).
    temporaryRegisters.append(generator.newTemporary());
    RegisterID* leftMostAddChildTempRegister = temporaryRegisters.last().get();
    generator.emitNode(leftMostAddChildTempRegister, leftMostAddChild);

    // Note on ordering of conversions:
    //
    // We maintain the same ordering of conversions as we would see if the concatenations
    // was performed as a sequence of adds (otherwise this optimization could change
    // behaviour should an object have been provided a valueOf or toString method).
    //
    // Considering the above example, the sequnce of execution is:
    //     * evaluate operand (a)
    //     * evaluate operand (b)
    //     * convert (a) to primitive   <-  (this would be triggered by the first add)
    //     * convert (b) to primitive   <-  (ditto)
    //     * evaluate operand (c)
    //     * convert (c) to primitive   <-  (this would be triggered by the second add)
    // And optionally, if there is an assignment:
    //     * convert (d) to primitive   <-  (this would be triggered by the assigning addition)
    //
    // As such we do not plant an op to convert the leftmost child now.  Instead, use
    // 'leftMostAddChildTempRegister' as a flag to trigger generation of the conversion
    // once the second node has been generated.  However, if the leftmost child is an
    // immediate we can trivially determine that no conversion will be required.
    // If this is the case
    if (leftMostAddChild->isString())
        leftMostAddChildTempRegister = 0;

    while (reverseExpressionList.size()) {
        ExpressionNode* node = reverseExpressionList.last();
        reverseExpressionList.removeLast();

        // Emit the code for the current node.
        temporaryRegisters.append(generator.newTemporary());
        generator.emitNode(temporaryRegisters.last().get(), node);

        // On the first iteration of this loop, when we first reach this point we have just
        // generated the second node, which means it is time to convert the leftmost operand.
        if (leftMostAddChildTempRegister) {
            generator.emitToPrimitive(leftMostAddChildTempRegister, leftMostAddChildTempRegister);
            leftMostAddChildTempRegister = 0; // Only do this once.
        }
        // Plant a conversion for this node, if necessary.
        if (!node->isString())
            generator.emitToPrimitive(temporaryRegisters.last().get(), temporaryRegisters.last().get());
    }
    ASSERT(temporaryRegisters.size() >= 3);

    // If there is an assignment convert the lhs now.  This will also copy lhs to
    // the temporary register we allocated for it.
    if (lhs)
        generator.emitToPrimitive(temporaryRegisters[0].get(), lhs);

    return generator.emitStrcat(generator.finalDestination(dst, temporaryRegisters[0].get()), temporaryRegisters[0].get(), temporaryRegisters.size());
}

RegisterID* BinaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    OpcodeID opcodeID = this->opcodeID();

    if (opcodeID == op_add && m_expr1->isAdd() && m_expr1->resultDescriptor().definitelyIsString())
        return emitStrcat(generator, dst);

    if (opcodeID == op_neq) {
        if (m_expr1->isNull() || m_expr2->isNull()) {
            RefPtr<RegisterID> src = generator.tempDestination(dst);
            generator.emitNode(src.get(), m_expr1->isNull() ? m_expr2.get() : m_expr1.get());
            return generator.emitUnaryOp(op_neq_null, generator.finalDestination(dst, src.get()), src.get());
        }
    }

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitBinaryOp(opcodeID, generator.finalDestination(dst, src1.get()), src1.get(), src2, OperandTypes(m_expr1->resultDescriptor(), m_expr2->resultDescriptor()));
}

RegisterID* EqualNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr1->isNull() || m_expr2->isNull()) {
        RefPtr<RegisterID> src = generator.tempDestination(dst);
        generator.emitNode(src.get(), m_expr1->isNull() ? m_expr2.get() : m_expr1.get());
        return generator.emitUnaryOp(op_eq_null, generator.finalDestination(dst, src.get()), src.get());
    }

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitEqualityOp(op_eq, generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

RegisterID* StrictEqualNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitEqualityOp(op_stricteq, generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

RegisterID* ReverseBinaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    return generator.emitBinaryOp(opcodeID(), generator.finalDestination(dst, src1.get()), src2, src1.get(), OperandTypes(m_expr2->resultDescriptor(), m_expr1->resultDescriptor()));
}

RegisterID* ThrowableBinaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_expr2.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    return generator.emitBinaryOp(opcodeID(), generator.finalDestination(dst, src1.get()), src1.get(), src2, OperandTypes(m_expr1->resultDescriptor(), m_expr2->resultDescriptor()));
}

RegisterID* InstanceOfNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1.get(), m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> src2 = generator.emitNode(m_expr2.get());

    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitGetByIdExceptionInfo(op_instanceof);
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

RegisterID* LogicalOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    RefPtr<Label> target = generator.newLabel();
    
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

RegisterID* ConditionalNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newDst = generator.finalDestination(dst);
    RefPtr<Label> beforeElse = generator.newLabel();
    RefPtr<Label> afterElse = generator.newLabel();

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

// FIXME: should this be moved to be a method on BytecodeGenerator?
static ALWAYS_INLINE RegisterID* emitReadModifyAssignment(BytecodeGenerator& generator, RegisterID* dst, RegisterID* src1, RegisterID* src2, Operator oper, OperandTypes types)
{
    OpcodeID opcodeID;
    switch (oper) {
        case OpMultEq:
            opcodeID = op_mul;
            break;
        case OpDivEq:
            opcodeID = op_div;
            break;
        case OpPlusEq:
            opcodeID = op_add;
            break;
        case OpMinusEq:
            opcodeID = op_sub;
            break;
        case OpLShift:
            opcodeID = op_lshift;
            break;
        case OpRShift:
            opcodeID = op_rshift;
            break;
        case OpURShift:
            opcodeID = op_urshift;
            break;
        case OpAndEq:
            opcodeID = op_bitand;
            break;
        case OpXOrEq:
            opcodeID = op_bitxor;
            break;
        case OpOrEq:
            opcodeID = op_bitor;
            break;
        case OpModEq:
            opcodeID = op_mod;
            break;
        default:
            ASSERT_NOT_REACHED();
            return dst;
    }
    
    return generator.emitBinaryOp(opcodeID, dst, src1, src2, types);
}

RegisterID* ReadModifyResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerFor(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RegisterID* src2 = generator.emitNode(m_right.get());
            return emitReadModifyAssignment(generator, generator.finalDestination(dst), local, src2, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
        }
        
        if (generator.leftHandSideNeedsCopy(m_rightHasAssignments, m_right->isPure(generator))) {
            RefPtr<RegisterID> result = generator.newTemporary();
            generator.emitMove(result.get(), local);
            RegisterID* src2 = generator.emitNode(m_right.get());
            emitReadModifyAssignment(generator, result.get(), result.get(), src2, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
            generator.emitMove(local, result.get());
            return generator.moveToDestinationIfNeeded(dst, result.get());
        }
        
        RegisterID* src2 = generator.emitNode(m_right.get());
        RegisterID* result = emitReadModifyAssignment(generator, local, local, src2, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    int index = 0;
    size_t depth = 0;
    JSObject* globalObject = 0;
    if (generator.findScopedProperty(m_ident, index, depth, true, globalObject) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> src1 = generator.emitGetScopedVar(generator.tempDestination(dst), depth, index, globalObject);
        RegisterID* src2 = generator.emitNode(m_right.get());
        RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst, src1.get()), src1.get(), src2, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
        generator.emitPutScopedVar(depth, index, result, globalObject);
        return result;
    }

    RefPtr<RegisterID> src1 = generator.tempDestination(dst);
    generator.emitExpressionInfo(divot() - startOffset() + m_ident.size(), m_ident.size(), 0);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), src1.get(), m_ident);
    RegisterID* src2 = generator.emitNode(m_right.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst, src1.get()), src1.get(), src2, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
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

RegisterID* AssignResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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
        if (dst == generator.ignoredResult())
            dst = 0;
        RegisterID* value = generator.emitNode(dst, m_right.get());
        generator.emitPutScopedVar(depth, index, value, globalObject);
        return value;
    }

    RefPtr<RegisterID> base = generator.emitResolveBase(generator.newTemporary(), m_ident);
    if (dst == generator.ignoredResult())
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

RegisterID* AssignDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* ReadModifyDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));

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

RegisterID* AssignErrorNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
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

RegisterID* AssignBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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

RegisterID* ReadModifyBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript.get(), m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(divot() - m_subexpressionDivotOffset, startOffset() - m_subexpressionDivotOffset, m_subexpressionEndOffset);
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));

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

RegisterID* CommaNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(generator.ignoredResult(), m_expr1.get());
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

RegisterID* ConstDeclNode::emitCodeSingle(BytecodeGenerator& generator)
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

RegisterID* ConstDeclNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
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

RegisterID* ConstStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    return generator.emitNode(m_next.get());
}

// ------------------------------ Helper functions for handling Vectors of StatementNode -------------------------------

static inline RegisterID* statementListEmitCode(const StatementVector& statements, BytecodeGenerator& generator, RegisterID* dst)
{
    StatementVector::const_iterator end = statements.end();
    for (StatementVector::const_iterator it = statements.begin(); it != end; ++it) {
        StatementNode* n = it->get();
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

RegisterID* BlockNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    return statementListEmitCode(m_children, generator, dst);
}

// ------------------------------ EmptyStatementNode ---------------------------

RegisterID* EmptyStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    return dst;
}

// ------------------------------ DebuggerStatementNode ---------------------------

RegisterID* DebuggerStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(DidReachBreakpoint, firstLine(), lastLine());
    return dst;
}

// ------------------------------ ExprStatementNode ----------------------------

RegisterID* ExprStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine()); 
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

RegisterID* VarStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    ASSERT(m_expr);
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
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

RegisterID* IfNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    
    RefPtr<Label> afterThen = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, afterThen.get());

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

RegisterID* IfElseNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    
    RefPtr<Label> beforeElse = generator.newLabel();
    RefPtr<Label> afterElse = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, beforeElse.get());

    generator.emitNode(dst, m_ifBlock.get());
    generator.emitJump(afterElse.get());

    generator.emitLabel(beforeElse.get());

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

RegisterID* DoWhileNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    RefPtr<Label> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());

    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
   
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

RegisterID* WhileNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    generator.emitJump(scope->continueTarget());

    RefPtr<Label> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());
    
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

RegisterID* ForNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        dst = 0;

    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

    if (m_expr1)
        generator.emitNode(generator.ignoredResult(), m_expr1.get());

    RefPtr<Label> condition = generator.newLabel();
    generator.emitJump(condition.get());

    RefPtr<Label> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());

    RefPtr<RegisterID> result = generator.emitNode(dst, m_statement.get());

    generator.emitLabel(scope->continueTarget());
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    if (m_expr3)
        generator.emitNode(generator.ignoredResult(), m_expr3.get());

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

RegisterID* ForInNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    if (!m_lexpr->isLocation())
        return emitThrowError(generator, ReferenceError, "Left side of for-in statement is not a reference.");

    RefPtr<Label> continueTarget = generator.newLabel(); 

    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

    if (m_init)
        generator.emitNode(generator.ignoredResult(), m_init.get());
    RegisterID* forInBase = generator.emitNode(m_expr.get());
    RefPtr<RegisterID> iter = generator.emitGetPropertyNames(generator.newTemporary(), forInBase);
    generator.emitJump(scope->continueTarget());

    RefPtr<Label> loopStart = generator.newLabel();
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

    generator.emitNode(dst, m_statement.get());

    generator.emitLabel(scope->continueTarget());
    generator.emitNextPropertyName(propertyName, iter.get(), loopStart.get());
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    generator.emitLabel(scope->breakTarget());
    return dst;
}

// ------------------------------ ContinueNode ---------------------------------

// ECMA 12.7
RegisterID* ContinueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    
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
RegisterID* BreakNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    
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

RegisterID* ReturnNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    if (generator.codeType() != FunctionCode)
        return emitThrowError(generator, SyntaxError, "Invalid return statement.");

    if (dst == generator.ignoredResult())
        dst = 0;
    RegisterID* r0 = m_value ? generator.emitNode(dst, m_value.get()) : generator.emitLoad(dst, jsUndefined());
    RefPtr<RegisterID> returnRegister;
    if (generator.scopeDepth()) {
        RefPtr<Label> l0 = generator.newLabel();
        if (generator.hasFinaliser() && !r0->isTemporary()) {
            returnRegister = generator.emitMove(generator.newTemporary(), r0);
            r0 = returnRegister.get();
        }
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

RegisterID* WithNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    
    RefPtr<RegisterID> scope = generator.newTemporary();
    generator.emitNode(scope.get(), m_expr.get()); // scope must be protected until popped
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
            JSValue jsValue = JSValue::makeInt32Fast(static_cast<int32_t>(value));
            if ((typeForTable & ~SwitchNumber) || !jsValue || (jsValue.getInt32Fast() != value)) {
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

RegisterID* CaseBlockNode::emitBytecodeForBlock(BytecodeGenerator& generator, RegisterID* switchExpression, RegisterID* dst)
{
    RefPtr<Label> defaultLabel;
    Vector<RefPtr<Label>, 8> labelVector;
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

RegisterID* SwitchNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());
    
    RefPtr<LabelScope> scope = generator.newLabelScope(LabelScope::Switch);

    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    RegisterID* r1 = m_block->emitBytecodeForBlock(generator, r0.get(), dst);

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

RegisterID* LabelNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

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

RegisterID* ThrowNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

    if (dst == generator.ignoredResult())
        dst = 0;
    RefPtr<RegisterID> expr = generator.emitNode(m_expr.get());
    generator.emitExpressionInfo(divot(), startOffset(), endOffset());
    generator.emitThrow(expr.get());
    return 0;
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

RegisterID* TryNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), lastLine());

    RefPtr<Label> tryStartLabel = generator.newLabel();
    RefPtr<Label> tryEndLabel = generator.newLabel();
    RefPtr<Label> finallyStart;
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
        RefPtr<Label> handlerEndLabel = generator.newLabel();
        generator.emitJump(handlerEndLabel.get());
        RefPtr<RegisterID> exceptionRegister = generator.emitCatch(generator.newTemporary(), tryStartLabel.get(), tryEndLabel.get());
        if (m_catchHasEval) {
            RefPtr<RegisterID> dynamicScopeObject = generator.emitNewObject(generator.newTemporary());
            generator.emitPutById(dynamicScopeObject.get(), m_exceptionIdent, exceptionRegister.get());
            generator.emitMove(exceptionRegister.get(), dynamicScopeObject.get());
            generator.emitPushScope(exceptionRegister.get());
        } else
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
        RefPtr<Label> finallyEndLabel = generator.newLabel();
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

// -----------------------------ScopeNodeData ---------------------------

ScopeNodeData::ScopeNodeData(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, int numConstants)
    : m_numConstants(numConstants)
{
    if (varStack)
        m_varStack = *varStack;
    if (funcStack)
        m_functionStack = *funcStack;
    if (children)
        children->releaseContentsIntoVector(m_children);
}

void ScopeNodeData::mark()
{
    FunctionStack::iterator end = m_functionStack.end();
    for (FunctionStack::iterator ptr = m_functionStack.begin(); ptr != end; ++ptr) {
        FunctionBodyNode* body = (*ptr)->body();
        if (!body->isGenerated())
            continue;
        body->generatedBytecode().mark();
    }
}

// ------------------------------ ScopeNode -----------------------------

ScopeNode::ScopeNode(JSGlobalData* globalData)
    : StatementNode(globalData)
    , m_features(NoFeatures)
{
#if ENABLE(OPCODE_SAMPLING)
    globalData->interpreter->sampler()->notifyOfScope(this);
#endif
}

ScopeNode::ScopeNode(JSGlobalData* globalData, const SourceCode& source, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, CodeFeatures features, int numConstants)
    : StatementNode(globalData)
    , m_data(new ScopeNodeData(children, varStack, funcStack, numConstants))
    , m_features(features)
    , m_source(source)
{
#if ENABLE(OPCODE_SAMPLING)
    globalData->interpreter->sampler()->notifyOfScope(this);
#endif
}

ScopeNode::~ScopeNode()
{
    NodeReleaser::releaseAllNodes(this);
}

void ScopeNode::releaseNodes(NodeReleaser& releaser)
{
    if (!m_data)
        return;
    size_t size = m_data->m_children.size();
    for (size_t i = 0; i < size; ++i)
        releaser.release(m_data->m_children[i]);
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

RegisterID* ProgramNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteProgram, firstLine(), lastLine());

    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    statementListEmitCode(children(), generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, firstLine(), lastLine());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void ProgramNode::generateBytecode(ScopeChainNode* scopeChainNode)
{
    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    m_code.set(new ProgramCodeBlock(this, GlobalCode, globalObject, source().provider()));
    
    BytecodeGenerator generator(this, globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_code.get());
    generator.generate();

    destroyData();
}

// ------------------------------ EvalNode -----------------------------

EvalNode::EvalNode(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& source, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, source, children, varStack, funcStack, features, numConstants)
{
}

EvalNode* EvalNode::create(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& source, CodeFeatures features, int numConstants)
{
    return new EvalNode(globalData, children, varStack, funcStack, source, features, numConstants);
}

RegisterID* EvalNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteProgram, firstLine(), lastLine());

    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    statementListEmitCode(children(), generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, firstLine(), lastLine());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void EvalNode::generateBytecode(ScopeChainNode* scopeChainNode)
{
    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    m_code.set(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth()));

    BytecodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_code->symbolTable(), m_code.get());
    generator.generate();

    // Eval code needs to hang on to its declaration stacks to keep declaration info alive until Interpreter::execute time,
    // so the entire ScopeNodeData cannot be destoyed.
    children().clear();
}

EvalCodeBlock& EvalNode::bytecodeForExceptionInfoReparse(ScopeChainNode* scopeChainNode, CodeBlock* codeBlockBeingRegeneratedFrom)
{
    ASSERT(!m_code);

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    m_code.set(new EvalCodeBlock(this, globalObject, source().provider(), scopeChain.localDepth()));

    BytecodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_code->symbolTable(), m_code.get());
    generator.setRegeneratingForExceptionInfo(codeBlockBeingRegeneratedFrom);
    generator.generate();

    return *m_code;
}

void EvalNode::mark()
{
    // We don't need to mark our own CodeBlock as the JSGlobalObject takes care of that
    data()->mark();
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionBodyNode::FunctionBodyNode(JSGlobalData* globalData)
    : ScopeNode(globalData)
#if ENABLE(JIT)
    , m_jitCode(0)
#endif
    , m_parameters(0)
    , m_parameterCount(0)
{
}

FunctionBodyNode::FunctionBodyNode(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& sourceCode, CodeFeatures features, int numConstants)
    : ScopeNode(globalData, sourceCode, children, varStack, funcStack, features, numConstants)
#if ENABLE(JIT)
    , m_jitCode(0)
#endif
    , m_parameters(0)
    , m_parameterCount(0)
{
}

FunctionBodyNode::~FunctionBodyNode()
{
    for (size_t i = 0; i < m_parameterCount; ++i)
        m_parameters[i].~Identifier();
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

#if ENABLE(JIT)
PassRefPtr<FunctionBodyNode> FunctionBodyNode::createNativeThunk(JSGlobalData* globalData)
{
    PassRefPtr<FunctionBodyNode> body = new FunctionBodyNode(globalData);
    body->m_jitCode = globalData->jitStubs.ctiNativeCallThunk();
    return body;
}
#endif

FunctionBodyNode* FunctionBodyNode::create(JSGlobalData* globalData)
{
    return new FunctionBodyNode(globalData);
}

FunctionBodyNode* FunctionBodyNode::create(JSGlobalData* globalData, SourceElements* children, VarStack* varStack, FunctionStack* funcStack, const SourceCode& sourceCode, CodeFeatures features, int numConstants)
{
    return new FunctionBodyNode(globalData, children, varStack, funcStack, sourceCode, features, numConstants);
}

void FunctionBodyNode::generateBytecode(ScopeChainNode* scopeChainNode)
{
    // This branch is only necessary since you can still create a non-stub FunctionBodyNode by
    // calling Parser::parse<FunctionBodyNode>().   
    if (!data())
        scopeChainNode->globalData->parser->reparseInPlace(scopeChainNode->globalData, this);
    ASSERT(data());

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    m_code.set(new CodeBlock(this, FunctionCode, source().provider(), source().startOffset()));

    BytecodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_code->symbolTable(), m_code.get());
    generator.generate();

    destroyData();
}

#if ENABLE(JIT)
void FunctionBodyNode::generateJITCode(ScopeChainNode* scopeChainNode)
{
    bytecode(scopeChainNode);
    ASSERT(m_code);
    ASSERT(!m_code->jitCode());
    JIT::compile(scopeChainNode->globalData, m_code.get());
    ASSERT(m_code->jitCode());
    m_jitCode = m_code->jitCode();
}
#endif

CodeBlock& FunctionBodyNode::bytecodeForExceptionInfoReparse(ScopeChainNode* scopeChainNode, CodeBlock* codeBlockBeingRegeneratedFrom)
{
    ASSERT(!m_code);

    ScopeChain scopeChain(scopeChainNode);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    m_code.set(new CodeBlock(this, FunctionCode, source().provider(), source().startOffset()));

    BytecodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_code->symbolTable(), m_code.get());
    generator.setRegeneratingForExceptionInfo(codeBlockBeingRegeneratedFrom);
    generator.generate();

    return *m_code;
}

RegisterID* FunctionBodyNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(DidEnterCallFrame, firstLine(), lastLine());
    statementListEmitCode(children(), generator, generator.ignoredResult());
    if (children().size() && children().last()->isBlock()) {
        BlockNode* blockNode = static_cast<BlockNode*>(children().last().get());
        if (blockNode->children().size() && blockNode->children().last()->isReturnNode())
            return 0;
    }

    RegisterID* r0 = generator.emitLoad(0, jsUndefined());
    generator.emitDebugHook(WillLeaveCallFrame, firstLine(), lastLine());
    generator.emitReturn(r0);
    return 0;
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

RegisterID* FuncDeclNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        dst = 0;
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

RegisterID* FuncExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
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
