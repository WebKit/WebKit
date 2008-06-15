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
#include "nodes.h"

#include "CodeGenerator.h"
#include "ExecState.h"
#include "JSGlobalObject.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "array_object.h"
#include "debugger.h"
#include "function_object.h"
#include "lexer.h"
#include "operations.h"
#include "regexp_object.h"
#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/MathExtras.h>

namespace KJS {

static inline bool isConstant(const LocalStorage& localStorage, size_t index)
{
    ASSERT(index < localStorage.size());
    return localStorage[index].attributes & ReadOnly;
}

static inline UString::Rep* rep(const Identifier& ident)
{
    return ident.ustring().rep();
}

// ------------------------------ Node -----------------------------------------

#ifndef NDEBUG
#ifndef LOG_CHANNEL_PREFIX
#define LOG_CHANNEL_PREFIX Log
#endif
static WTFLogChannel LogKJSNodeLeaks = { 0x00000000, "", WTFLogChannelOn };

struct ParserRefCountedCounter {
    static unsigned count;
    ParserRefCountedCounter()
    {
        if (count)
            LOG(KJSNodeLeaks, "LEAK: %u KJS::Node\n", count);
    }
};
unsigned ParserRefCountedCounter::count = 0;
static ParserRefCountedCounter parserRefCountedCounter;
#endif

static HashSet<ParserRefCounted*>* newTrackedObjects;
static HashCountedSet<ParserRefCounted*>* trackedObjectExtraRefCounts;

ParserRefCounted::ParserRefCounted()
{
#ifndef NDEBUG
    ++ParserRefCountedCounter::count;
#endif
    if (!newTrackedObjects)
        newTrackedObjects = new HashSet<ParserRefCounted*>;
    newTrackedObjects->add(this);
    ASSERT(newTrackedObjects->contains(this));
}

ParserRefCounted::~ParserRefCounted()
{
#ifndef NDEBUG
    --ParserRefCountedCounter::count;
#endif
}

void ParserRefCounted::ref()
{
    // bumping from 0 to 1 is just removing from the new nodes set
    if (newTrackedObjects) {
        HashSet<ParserRefCounted*>::iterator it = newTrackedObjects->find(this);
        if (it != newTrackedObjects->end()) {
            newTrackedObjects->remove(it);
            ASSERT(!trackedObjectExtraRefCounts || !trackedObjectExtraRefCounts->contains(this));
            return;
        }
    }

    ASSERT(!newTrackedObjects || !newTrackedObjects->contains(this));

    if (!trackedObjectExtraRefCounts)
        trackedObjectExtraRefCounts = new HashCountedSet<ParserRefCounted*>;
    trackedObjectExtraRefCounts->add(this);
}

void ParserRefCounted::deref()
{
    ASSERT(!newTrackedObjects || !newTrackedObjects->contains(this));

    if (!trackedObjectExtraRefCounts) {
        delete this;
        return;
    }

    HashCountedSet<ParserRefCounted*>::iterator it = trackedObjectExtraRefCounts->find(this);
    if (it == trackedObjectExtraRefCounts->end())
        delete this;
    else
        trackedObjectExtraRefCounts->remove(it);
}

bool ParserRefCounted::hasOneRef()
{
    if (newTrackedObjects && newTrackedObjects->contains(this)) {
        ASSERT(!trackedObjectExtraRefCounts || !trackedObjectExtraRefCounts->contains(this));
        return false;
    }

    ASSERT(!newTrackedObjects || !newTrackedObjects->contains(this));

    if (!trackedObjectExtraRefCounts)
        return true;

    return !trackedObjectExtraRefCounts->contains(this);
}

void ParserRefCounted::deleteNewObjects()
{
    if (!newTrackedObjects)
        return;

#ifndef NDEBUG
    HashSet<ParserRefCounted*>::iterator end = newTrackedObjects->end();
    for (HashSet<ParserRefCounted*>::iterator it = newTrackedObjects->begin(); it != end; ++it)
        ASSERT(!trackedObjectExtraRefCounts || !trackedObjectExtraRefCounts->contains(*it));
#endif
    deleteAllValues(*newTrackedObjects);
    delete newTrackedObjects;
    newTrackedObjects = 0;
}

Node::Node()
    : m_expectedReturnType(ObjectType)
{
    m_line = JSGlobalData::threadInstance().lexer->lineNo();
}

Node::Node(JSType expectedReturn)
    : m_expectedReturnType(expectedReturn)
{
    m_line = JSGlobalData::threadInstance().lexer->lineNo();
}

static void substitute(UString& string, const UString& substring) KJS_FAST_CALL;
static void substitute(UString& string, const UString& substring)
{
    int position = string.find("%s");
    ASSERT(position != -1);
    UString newString = string.substr(0, position);
    newString.append(substring);
    newString.append(string.substr(position + 2));
    string = newString;
}

static inline int currentSourceId(ExecState* exec) KJS_FAST_CALL;
static inline int currentSourceId(ExecState*)
{
    ASSERT_NOT_REACHED();
    return 0;
}

static inline const UString currentSourceURL(ExecState* exec) KJS_FAST_CALL;
static inline const UString currentSourceURL(ExecState*)
{
    ASSERT_NOT_REACHED();
    return UString();
}

RegisterID* Node::emitThrowError(CodeGenerator& generator, ErrorType e, const char* msg)
{
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(msg));
    generator.emitThrow(exception);
    return exception;
}

RegisterID* Node::emitThrowError(CodeGenerator& generator, ErrorType e, const char* msg, const Identifier& label)
{
    UString message = msg;
    substitute(message, label.ustring());
    RegisterID* exception = generator.emitNewError(generator.newTemporary(), e, jsString(message));
    generator.emitThrow(exception);
    return exception;
}
    
// ------------------------------ StatementNode --------------------------------

StatementNode::StatementNode()
    : m_lastLine(-1)
{
    m_line = -1;
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

// ------------------------------ BreakpointCheckStatement --------------------------------

BreakpointCheckStatement::BreakpointCheckStatement(PassRefPtr<StatementNode> statement)
    : m_statement(statement)
{
    ASSERT(m_statement);
}

void BreakpointCheckStatement::streamTo(SourceStream& stream) const
{
    m_statement->streamTo(stream);
}

// ------------------------------ NullNode -------------------------------------

RegisterID* NullNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitLoad(generator.finalDestination(dst), jsNull());
}

// ------------------------------ BooleanNode ----------------------------------

RegisterID* BooleanNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitLoad(generator.finalDestination(dst), m_value);
}

// ------------------------------ NumberNode -----------------------------------

RegisterID* NumberNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitLoad(generator.finalDestination(dst), m_double);
}

// ------------------------------ StringNode -----------------------------------

RegisterID* StringNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    // FIXME: should we try to atomize constant strings?
    return generator.emitLoad(generator.finalDestination(dst), jsOwnedString(m_value));
}

// ------------------------------ RegExpNode -----------------------------------

RegisterID* RegExpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (!m_regExp->isValid())
        return emitThrowError(generator, SyntaxError, "Invalid regular expression: %s", m_regExp->errorMessage());
    return generator.emitNewRegExp(generator.finalDestination(dst), m_regExp.get());
}

// ------------------------------ ThisNode -------------------------------------

RegisterID* ThisNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.moveToDestinationIfNeeded(dst, generator.thisRegister());
}

// ------------------------------ ResolveNode ----------------------------------

bool ResolveNode::isPure(CodeGenerator& generator) const
{
    return generator.isLocal(m_ident);
}

RegisterID* ResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident))
        return generator.moveToDestinationIfNeeded(dst, local);

    return generator.emitResolve(generator.finalDestination(dst), m_ident);
}

// ------------------------------ ArrayNode ------------------------------------


RegisterID* ArrayNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newArray = generator.emitNewArray(generator.tempDestination(dst));
    unsigned length = 0;

    RegisterID* value;
    for (ElementNode* n = m_element.get(); n; n = n->m_next.get()) {
        value = generator.emitNode(n->m_node.get());
        length += n->m_elision;
        generator.emitPutByIndex(newArray.get(), length++, value);
    }

    value = generator.emitLoad(generator.newTemporary(), jsNumber(m_elision + length));
    generator.emitPutById(newArray.get(), generator.propertyNames().length, value);

    return generator.moveToDestinationIfNeeded(dst, newArray.get());
}

// ------------------------------ ObjectLiteralNode ----------------------------

RegisterID* ObjectLiteralNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (m_list)
        return generator.emitNode(dst, m_list.get());
    else
        return generator.emitNewObject(generator.finalDestination(dst));
}

// ------------------------------ PropertyListNode -----------------------------

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

RegisterID* BracketAccessorNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments, m_subscript->isPure(generator));
    RegisterID* property = generator.emitNode(m_subscript.get());

    return generator.emitGetByVal(generator.finalDestination(dst), base.get(), property);
}

// ------------------------------ DotAccessorNode --------------------------------

RegisterID* DotAccessorNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* base = generator.emitNode(m_base.get());
    return generator.emitGetById(generator.finalDestination(dst), base, m_ident);
}

// ------------------------------ ArgumentListNode -----------------------------

RegisterID* ArgumentListNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    return generator.emitNode(dst, m_expr.get());
}

// ------------------------------ NewExprNode ----------------------------------

RegisterID* NewExprNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    return generator.emitConstruct(generator.finalDestination(dst), r0.get(), m_args.get());
}

RegisterID* EvalFunctionCallNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.tempDestination(dst);
    RegisterID* func = generator.newTemporary();
    generator.emitResolveWithBase(base.get(), func, generator.propertyNames().eval);
    return generator.emitCallEval(generator.finalDestination(dst, base.get()), func, base.get(), m_args.get());
}

RegisterID* FunctionCallValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> func = generator.emitNode(m_expr.get());
    return generator.emitCall(generator.finalDestination(dst), func.get(), 0, m_args.get());
}

RegisterID* FunctionCallResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident))
        return generator.emitCall(generator.finalDestination(dst), local, 0, m_args.get());

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RegisterID* func = generator.emitGetScopedVar(generator.newTemporary(), depth, index);
        return generator.emitCall(generator.finalDestination(dst), func, 0, m_args.get());
    }

    RefPtr<RegisterID> base = generator.tempDestination(dst);
    RegisterID* func = generator.newTemporary();
    generator.emitResolveFunction(base.get(), func, m_ident);
    return generator.emitCall(generator.finalDestination(dst, base.get()), func, base.get(), m_args.get());
}

RegisterID* FunctionCallBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RegisterID* property = generator.emitNode(m_subscript.get());
    RegisterID* function = generator.emitGetByVal(generator.newTemporary(), base.get(), property);
    return generator.emitCall(generator.finalDestination(dst, base.get()), function, base.get(), m_args.get());
}

RegisterID* FunctionCallDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RegisterID* function = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    return generator.emitCall(generator.finalDestination(dst, base.get()), function, base.get(), m_args.get());
}

// ------------------------------ PostfixResolveNode ----------------------------------

RegisterID* PostIncResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    // FIXME: I think we can detect the absense of dependent expressions here, 
    // and emit a PreInc instead of a PostInc. A post-pass to eliminate dead
    // code would work, too.
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident))
            return generator.emitToJSNumber(generator.finalDestination(dst), local);
        
        return generator.emitPostInc(generator.finalDestination(dst), local);
    }

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> value = generator.emitGetScopedVar(generator.newTemporary(), depth, index);
        RegisterID* oldValue = generator.emitPostInc(generator.finalDestination(dst), value.get());
        generator.emitPutScopedVar(depth, index, value.get());
        return oldValue;
    }

    RefPtr<RegisterID> value = generator.newTemporary();
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), value.get(), m_ident);
    RegisterID* oldValue = generator.emitPostInc(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

RegisterID* PostDecResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    // FIXME: I think we can detect the absense of dependent expressions here, 
    // and emit a PreDec instead of a PostDec. A post-pass to eliminate dead
    // code would work, too.
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident))
            return generator.emitToJSNumber(generator.finalDestination(dst), local);
        
        return generator.emitPostDec(generator.finalDestination(dst), local);
    }

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> value = generator.emitGetScopedVar(generator.newTemporary(), depth, index);
        RegisterID* oldValue = generator.emitPostDec(generator.finalDestination(dst), value.get());
        generator.emitPutScopedVar(depth, index, value.get());
        return oldValue;
    }

    RefPtr<RegisterID> value = generator.newTemporary();
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), value.get(), m_ident);
    RegisterID* oldValue = generator.emitPostDec(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

// ------------------------------ PostfixBracketNode ----------------------------------

RegisterID* PostIncBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue = generator.emitPostInc(generator.finalDestination(dst), value.get());
    generator.emitPutByVal(base.get(), property.get(), value.get());
    return oldValue;
}

RegisterID* PostDecBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue = generator.emitPostDec(generator.finalDestination(dst), value.get());
    generator.emitPutByVal(base.get(), property.get(), value.get());
    return oldValue;
}

// ------------------------------ PostfixDotNode ----------------------------------

RegisterID* PostIncDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> value = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    RegisterID* oldValue = generator.emitPostInc(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

RegisterID* PostDecDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> value = generator.emitGetById(generator.newTemporary(), base.get(), m_ident);
    RegisterID* oldValue = generator.emitPostDec(generator.finalDestination(dst), value.get());
    generator.emitPutById(base.get(), m_ident, value.get());
    return oldValue;
}

// ------------------------------ PostfixErrorNode -----------------------------------

RegisterID* PostfixErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, m_operator == OpPlusPlus ? "Postfix ++ operator applied to value that is not a reference." : "Postfix -- operator applied to value that is not a reference.");
}

// ------------------------------ DeleteResolveNode -----------------------------------

RegisterID* DeleteResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.registerForLocal(m_ident))
        return generator.emitLoad(generator.finalDestination(dst), false);

    RegisterID* base = generator.emitResolveBase(generator.tempDestination(dst), m_ident);
    return generator.emitDeleteById(generator.finalDestination(dst, base), base, m_ident);
}

// ------------------------------ DeleteBracketNode -----------------------------------

RegisterID* DeleteBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_base.get());
    RefPtr<RegisterID> r1 = generator.emitNode(m_subscript.get());
    return generator.emitDeleteByVal(generator.finalDestination(dst), r0.get(), r1.get());
}

// ------------------------------ DeleteDotNode -----------------------------------

RegisterID* DeleteDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* r0 = generator.emitNode(m_base.get());
    return generator.emitDeleteById(generator.finalDestination(dst), r0, m_ident);
}

// ------------------------------ DeleteValueNode -----------------------------------

RegisterID* DeleteValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(m_expr.get());

    // delete on a non-location expression ignores the value and returns true
    return generator.emitLoad(generator.finalDestination(dst), true);
}

// ------------------------------ VoidNode -------------------------------------

RegisterID* VoidNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    return generator.emitLoad(generator.finalDestination(dst, r0.get()), jsUndefined());
}

// ------------------------------ TypeOfValueNode -----------------------------------

RegisterID* TypeOfResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident))
        return generator.emitTypeOf(generator.finalDestination(dst), local);

    RefPtr<RegisterID> scratch = generator.emitResolveBase(generator.tempDestination(dst), m_ident);
    generator.emitGetById(scratch.get(), scratch.get(), m_ident);
    return generator.emitTypeOf(generator.finalDestination(dst, scratch.get()), scratch.get());
}

// ------------------------------ TypeOfValueNode -----------------------------------

RegisterID* TypeOfValueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src = generator.emitNode(m_expr.get());
    return generator.emitTypeOf(generator.finalDestination(dst), src.get());
}

// ------------------------------ PrefixResolveNode ----------------------------------

RegisterID* PreIncResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RefPtr<RegisterID> r0 = generator.emitLoad(generator.finalDestination(dst), 1.0);
            return generator.emitBinaryOp(op_add, r0.get(), local, r0.get());
        }
        
        generator.emitPreInc(local);
        return generator.moveToDestinationIfNeeded(dst, local);
    }

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> propDst = generator.emitGetScopedVar(generator.tempDestination(dst), depth, index);
        generator.emitPreInc(propDst.get());
        generator.emitPutScopedVar(depth, index, propDst.get());
        return generator.moveToDestinationIfNeeded(dst, propDst.get());;
    }

    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), propDst.get(), m_ident);
    generator.emitPreInc(propDst.get());
    generator.emitPutById(base.get(), m_ident, propDst.get());
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

RegisterID* PreDecResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RefPtr<RegisterID> r0 = generator.emitLoad(generator.finalDestination(dst), -1.0);
            return generator.emitBinaryOp(op_add, r0.get(), local, r0.get());
        }
        
        generator.emitPreDec(local);
        return generator.moveToDestinationIfNeeded(dst, local);
    }

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> propDst = generator.emitGetScopedVar(generator.tempDestination(dst), depth, index);
        generator.emitPreDec(propDst.get());
        generator.emitPutScopedVar(depth, index, propDst.get());
        return generator.moveToDestinationIfNeeded(dst, propDst.get());;
    }

    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), propDst.get(), m_ident);
    generator.emitPreDec(propDst.get());
    generator.emitPutById(base.get(), m_ident, propDst.get());
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

// ------------------------------ PrefixBracketNode ----------------------------------

RegisterID* PreIncBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetByVal(propDst.get(), base.get(), property.get());
    generator.emitPreInc(value);
    generator.emitPutByVal(base.get(), property.get(), value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

RegisterID* PreDecBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> property = generator.emitNode(m_subscript.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetByVal(propDst.get(), base.get(), property.get());
    generator.emitPreDec(value);
    generator.emitPutByVal(base.get(), property.get(), value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

// ------------------------------ PrefixDotNode ----------------------------------

RegisterID* PreIncDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetById(propDst.get(), base.get(), m_ident);
    generator.emitPreInc(value);
    generator.emitPutById(base.get(), m_ident, value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

RegisterID* PreDecDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNode(m_base.get());
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);
    RegisterID* value = generator.emitGetById(propDst.get(), base.get(), m_ident);
    generator.emitPreDec(value);
    generator.emitPutById(base.get(), m_ident, value);
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

// ------------------------------ PrefixErrorNode -----------------------------------

RegisterID* PrefixErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, m_operator == OpPlusPlus ? "Prefix ++ operator applied to value that is not a reference." : "Prefix -- operator applied to value that is not a reference.");
}

// ------------------------------ UnaryPlusNode --------------------------------

RegisterID* UnaryPlusNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitToJSNumber(generator.finalDestination(dst), src);
}

// ------------------------------ NegateNode -----------------------------------

RegisterID* NegateNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitNegate(generator.finalDestination(dst), src);
}

// ------------------------------ BitwiseNotNode -------------------------------

RegisterID* BitwiseNotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitBitNot(generator.finalDestination(dst), src);
}

// ------------------------------ LogicalNotNode -------------------------------

RegisterID* LogicalNotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RegisterID* src = generator.emitNode(m_expr.get());
    return generator.emitNot(generator.finalDestination(dst), src);
}

// ------------------------------ Binary Operation Nodes -----------------------------------

RegisterID* BinaryOpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_term1.get(), m_rightHasAssignments, m_term2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_term2.get());
    return generator.emitBinaryOp(opcode(), generator.finalDestination(dst, src1.get()), src1.get(), src2);
}

RegisterID* ReverseBinaryOpNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_term1.get(), m_rightHasAssignments, m_term2->isPure(generator));
    RegisterID* src2 = generator.emitNode(m_term2.get());
    return generator.emitBinaryOp(opcode(), generator.finalDestination(dst, src1.get()), src2, src1.get());
}

// ------------------------------ Binary Logical Nodes ----------------------------

RegisterID* LogicalAndNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    RefPtr<LabelID> target = generator.newLabel();
    
    generator.emitNode(temp.get(), m_expr1.get());
    generator.emitJumpIfFalse(temp.get(), target.get());
    generator.emitNode(temp.get(), m_expr2.get());
    generator.emitLabel(target.get());

    return generator.moveToDestinationIfNeeded(dst, temp.get());
}

RegisterID* LogicalOrNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    RefPtr<LabelID> target = generator.newLabel();
    
    generator.emitNode(temp.get(), m_expr1.get());
    generator.emitJumpIfTrue(temp.get(), target.get());
    generator.emitNode(temp.get(), m_expr2.get());
    generator.emitLabel(target.get());

    return generator.moveToDestinationIfNeeded(dst, temp.get());
}

// ------------------------------ ConditionalNode ------------------------------

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

// FIXME: should this be moved to be a method on CodeGenerator?
static ALWAYS_INLINE RegisterID* emitReadModifyAssignment(CodeGenerator& generator, RegisterID* dst, RegisterID* src1, RegisterID* src2, Operator oper)
{
    switch (oper) {
        case OpMultEq:
            return generator.emitBinaryOp(op_mul, dst, src1, src2);
        case OpDivEq:
            return generator.emitBinaryOp(op_div, dst, src1, src2);
        case OpPlusEq:
            return generator.emitBinaryOp(op_add, dst, src1, src2);
        case OpMinusEq:
            return generator.emitBinaryOp(op_sub, dst, src1, src2);
        case OpLShift:
            return generator.emitBinaryOp(op_lshift, dst, src1, src2);
        case OpRShift:
            return generator.emitBinaryOp(op_rshift, dst, src1, src2);
        case OpURShift:
            return generator.emitBinaryOp(op_urshift, dst, src1, src2);
        case OpAndEq:
            return generator.emitBinaryOp(op_bitand, dst, src1, src2);
        case OpXOrEq:
            return generator.emitBinaryOp(op_bitxor, dst, src1, src2);
        case OpOrEq:
            return generator.emitBinaryOp(op_bitor, dst, src1, src2);
        case OpModEq:
            return generator.emitBinaryOp(op_mod, dst, src1, src2);
        default:
            ASSERT_NOT_REACHED();
    }

    return dst;
}

RegisterID* ReadModifyResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident)) {
            RegisterID* src2 = generator.emitNode(m_right.get());
            return emitReadModifyAssignment(generator, generator.finalDestination(dst), local, src2, m_operator);
        }
        
        if (generator.leftHandSideNeedsCopy(m_rightHasAssignments, m_right->isPure(generator))) {
            RefPtr<RegisterID> result = generator.newTemporary();
            generator.emitMove(result.get(), local);
            RegisterID* src2 = generator.emitNode(m_right.get());
            emitReadModifyAssignment(generator, result.get(), result.get(), src2, m_operator);
            generator.emitMove(local, result.get());
            return generator.moveToDestinationIfNeeded(dst, result.get());
        }
        
        RegisterID* src2 = generator.emitNode(m_right.get());
        RegisterID* result = emitReadModifyAssignment(generator, local, local, src2, m_operator);
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RefPtr<RegisterID> src1 = generator.emitGetScopedVar(generator.tempDestination(dst), depth, index);
        RegisterID* src2 = generator.emitNode(m_right.get());
        RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst, src1.get()), src1.get(), src2, m_operator);
        generator.emitPutScopedVar(depth, index, result);
        return result;
    }

    RefPtr<RegisterID> src1 = generator.tempDestination(dst);
    RefPtr<RegisterID> base = generator.emitResolveWithBase(generator.newTemporary(), src1.get(), m_ident);
    RegisterID* src2 = generator.emitNode(m_right.get());
    RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst, src1.get()), src1.get(), src2, m_operator);
    return generator.emitPutById(base.get(), m_ident, result);
}

// ------------------------------ AssignResolveNode -----------------------------------

RegisterID* AssignResolveNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* local = generator.registerForLocal(m_ident)) {
        if (generator.isLocalConstant(m_ident))
            return generator.emitNode(dst, m_right.get());
        
        RegisterID* result = generator.emitNode(local, m_right.get());
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    int index = 0;
    size_t depth = 0;
    if (generator.findScopedProperty(m_ident, index, depth) && index != missingSymbolMarker()) {
        RegisterID* value = generator.emitNode(dst, m_right.get());
        generator.emitPutScopedVar(depth, index, value);
        return value;
    }

    RefPtr<RegisterID> base = generator.emitResolveBase(generator.newTemporary(), m_ident);
    RegisterID* value = generator.emitNode(dst, m_right.get());
    return generator.emitPutById(base.get(), m_ident, value);
}

// ------------------------------ AssignDotNode -----------------------------------

RegisterID* AssignDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RegisterID* result = generator.emitNode(value.get(), m_right.get());
    generator.emitPutById(base.get(), m_ident, result);
    return generator.moveToDestinationIfNeeded(dst, result);
}

// ------------------------------ ReadModifyDotNode -----------------------------------

RegisterID* ReadModifyDotNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator);
    return generator.emitPutById(base.get(), m_ident, updatedValue);
}

// ------------------------------ AssignErrorNode -----------------------------------

RegisterID* AssignErrorNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return emitThrowError(generator, ReferenceError, "Left side of assignment is not a reference.");
}

// ------------------------------ AssignBracketNode -----------------------------------

RegisterID* AssignBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript.get(), m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RegisterID* result = generator.emitNode(value.get(), m_right.get());
    generator.emitPutByVal(base.get(), property.get(), result);
    return generator.moveToDestinationIfNeeded(dst, result);
}

RegisterID* ReadModifyBracketNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base.get(), m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript.get(), m_rightHasAssignments, m_right->isPure(generator));

    RefPtr<RegisterID> value = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());
    RegisterID* change = generator.emitNode(m_right.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), change, m_operator);

    generator.emitPutByVal(base.get(), property.get(), updatedValue);

    return updatedValue;
}

// ------------------------------ CommaNode ------------------------------------

RegisterID* CommaNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(m_expr1.get());
    return generator.emitNode(dst, m_expr2.get());
}

// ------------------------------ ConstDeclNode ----------------------------------

ConstDeclNode::ConstDeclNode(const Identifier& ident, ExpressionNode* init)
    : m_ident(ident)
    , m_init(init)
{
}

RegisterID* ConstDeclNode::emitCodeSingle(CodeGenerator& generator)
{
    if (RegisterID* local = generator.registerForLocalConstInit(m_ident)) {
        if (!m_init)
            return local;

        return generator.emitNode(local, m_init.get());
    }
    
    // FIXME: While this code should only be hit in eval code, it will potentially
    // assign to the wrong base if m_ident exists in an intervening dynamic scope.
    RefPtr<RegisterID> base = generator.emitResolveBase(generator.newTemporary(), m_ident);
    RegisterID* value = generator.emitNode(m_init.get());
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

RegisterID* ConstStatementNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    return generator.emitNode(m_next.get());
}

// ------------------------------ Helper functions for handling Vectors of StatementNode -------------------------------

static inline RegisterID* statementListEmitCode(StatementVector& statements, CodeGenerator& generator, RegisterID* dst = 0)
{
    RefPtr<RegisterID> r0 = dst;

    StatementVector::iterator end = statements.end();
    for (StatementVector::iterator it = statements.begin(); it != end; ++it) {
        StatementNode* n = it->get();
        generator.emitDebugHook(WillExecuteStatement, n->firstLine(), n->lastLine());
        if (RegisterID* r1 = generator.emitNode(dst, n))
            r0 = r1;
    }
    
    return r0.get();
}

static inline void statementListPushFIFO(StatementVector& statements, DeclarationStacks::NodeStack& stack)
{
    StatementVector::iterator it = statements.end();
    StatementVector::iterator begin = statements.begin();
    while (it != begin) {
        --it;
        stack.append((*it).get());
    }
}

static inline Node* statementListInitializeVariableAccessStack(StatementVector& statements, DeclarationStacks::NodeStack& stack)
{
    if (statements.isEmpty())
        return 0;

    StatementVector::iterator it = statements.end();
    StatementVector::iterator begin = statements.begin();
    StatementVector::iterator beginPlusOne = begin + 1;

    while (it != beginPlusOne) {
        --it;
        stack.append((*it).get());
    }

    return (*begin).get();
}

// ------------------------------ BlockNode ------------------------------------

BlockNode::BlockNode(SourceElements* children)
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

RegisterID* VarStatementNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    ASSERT(m_expr);
    return generator.emitNode(m_expr.get());
}

// ------------------------------ IfNode ---------------------------------------

RegisterID* IfNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> afterThen = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, afterThen.get());

    generator.emitNode(dst, m_ifBlock.get());
    generator.emitLabel(afterThen.get());

    // FIXME: This should return the last statement exectuted so that it can be returned as a Completion
    return 0;
}

RegisterID* IfElseNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> beforeElse = generator.newLabel();
    RefPtr<LabelID> afterElse = generator.newLabel();

    RegisterID* cond = generator.emitNode(m_condition.get());
    generator.emitJumpIfFalse(cond, beforeElse.get());

    generator.emitNode(dst, m_ifBlock.get());
    generator.emitJump(afterElse.get());

    generator.emitLabel(beforeElse.get());
    generator.emitNode(dst, m_elseBlock.get());

    generator.emitLabel(afterElse.get());

    // FIXME: This should return the last statement exectuted so that it can be returned as a Completion
    return 0;
}

// ------------------------------ DoWhileNode ----------------------------------

RegisterID* DoWhileNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());

    RefPtr<LabelID> continueTarget = generator.newLabel();
    RefPtr<LabelID> breakTarget = generator.newLabel();
    
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    RefPtr<RegisterID> result = generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();
    
    generator.emitLabel(continueTarget.get());
    RegisterID* cond = generator.emitNode(m_expr.get());
    generator.emitJumpIfTrueMayCombine(cond, topOfLoop.get());
    generator.emitLabel(breakTarget.get());
    return result.get();
}

// ------------------------------ WhileNode ------------------------------------

RegisterID* WhileNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> topOfLoop = generator.newLabel();
    RefPtr<LabelID> continueTarget = generator.newLabel();
    RefPtr<LabelID> breakTarget = generator.newLabel();

    generator.emitJump(continueTarget.get());
    generator.emitLabel(topOfLoop.get());
    
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();

    generator.emitLabel(continueTarget.get());
    RegisterID* cond = generator.emitNode(m_expr.get());
    generator.emitJumpIfTrueMayCombine(cond, topOfLoop.get());

    generator.emitLabel(breakTarget.get());
    
    // FIXME: This should return the last statement executed so that it can be returned as a Completion
    return 0;
}

// ------------------------------ ForNode --------------------------------------

RegisterID* ForNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (m_expr1)
        generator.emitNode(m_expr1.get());
    
    RefPtr<LabelID> topOfLoop = generator.newLabel();
    RefPtr<LabelID> beforeCondition = generator.newLabel();
    RefPtr<LabelID> continueTarget = generator.newLabel(); 
    RefPtr<LabelID> breakTarget = generator.newLabel(); 
    generator.emitJump(beforeCondition.get());

    generator.emitLabel(topOfLoop.get());
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    RefPtr<RegisterID> result = generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();
    generator.emitLabel(continueTarget.get());
    if (m_expr3)
        generator.emitNode(m_expr3.get());

    generator.emitLabel(beforeCondition.get());
    if (m_expr2) {
        RegisterID* cond = generator.emitNode(m_expr2.get());
        generator.emitJumpIfTrueMayCombine(cond, topOfLoop.get());
    } else {
        generator.emitJump(topOfLoop.get());
    }
    generator.emitLabel(breakTarget.get());
    return result.get();
}

// ------------------------------ ForInNode ------------------------------------

ForInNode::ForInNode(ExpressionNode* l, ExpressionNode* expr, StatementNode* statement)
    : m_init(0L)
    , m_lexpr(l)
    , m_expr(expr)
    , m_statement(statement)
    , m_identIsVarDecl(false)
{
}

ForInNode::ForInNode(const Identifier& ident, ExpressionNode* in, ExpressionNode* expr, StatementNode* statement)
    : m_ident(ident)
    , m_lexpr(new ResolveNode(ident))
    , m_expr(expr)
    , m_statement(statement)
    , m_identIsVarDecl(true)
{
    if (in)
        m_init = new AssignResolveNode(ident, in, true);
    // for( var foo = bar in baz )
}

RegisterID* ForInNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> loopStart = generator.newLabel();
    RefPtr<LabelID> continueTarget = generator.newLabel(); 
    RefPtr<LabelID> breakTarget = generator.newLabel(); 

    if (m_init)
        generator.emitNode(m_init.get());
    RegisterID* forInBase = generator.emitNode(m_expr.get());
    RefPtr<RegisterID> iter = generator.emitGetPropertyNames(generator.newTemporary(), forInBase);
    generator.emitJump(continueTarget.get());
    generator.emitLabel(loopStart.get());
    RegisterID* propertyName;
    if (m_lexpr->isResolveNode()) {
        const Identifier& ident = static_cast<ResolveNode*>(m_lexpr.get())->identifier();
        propertyName = generator.registerForLocal(ident);
        if (!propertyName) {
            propertyName = generator.newTemporary();
            RefPtr<RegisterID> protect = propertyName;
            RegisterID* base = generator.emitResolveBase(generator.newTemporary(), ident);
            generator.emitPutById(base, ident, propertyName);
        }
    } else if (m_lexpr->isDotAccessorNode()) {
        DotAccessorNode* assignNode = static_cast<DotAccessorNode*>(m_lexpr.get());
        const Identifier& ident = assignNode->identifier();
        propertyName = generator.newTemporary();
        RefPtr<RegisterID> protect = propertyName;
        RegisterID* base = generator.emitNode(assignNode->base());
        generator.emitPutById(base, ident, propertyName);
    } else {
        ASSERT(m_lexpr->isBracketAccessorNode());
        BracketAccessorNode* assignNode = static_cast<BracketAccessorNode*>(m_lexpr.get());
        propertyName = generator.newTemporary();
        RefPtr<RegisterID> protect = propertyName;
        RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
        RegisterID* subscript = generator.emitNode(assignNode->subscript());
        generator.emitPutByVal(base.get(), subscript, propertyName);
    }   
    
    generator.pushJumpContext(&m_labelStack, continueTarget.get(), breakTarget.get(), true);
    generator.emitNode(dst, m_statement.get());
    generator.popJumpContext();

    generator.emitLabel(continueTarget.get());
    generator.emitNextPropertyName(propertyName, iter.get(), loopStart.get());
    generator.emitLabel(breakTarget.get());
    return dst;
}

// ------------------------------ ContinueNode ---------------------------------

// ECMA 12.7
RegisterID* ContinueNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (!generator.inContinueContext())
        return emitThrowError(generator, SyntaxError, "Invalid continue statement.");

    JumpContext* targetContext = generator.jumpContextForContinue(m_ident);

    if (!targetContext) {
        if (m_ident.isEmpty())
            return emitThrowError(generator, SyntaxError, "Invalid continue statement.");
        else
            return emitThrowError(generator, SyntaxError, "Label %s not found.", m_ident);
    }

    if (!targetContext->continueTarget)
        return emitThrowError(generator, SyntaxError, "Invalid continue statement.");        

    generator.emitJumpScopes(targetContext->continueTarget, targetContext->scopeDepth);
    
    return dst;
}

// ------------------------------ BreakNode ------------------------------------

// ECMA 12.8
RegisterID* BreakNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (!generator.inJumpContext())
        return emitThrowError(generator, SyntaxError, "Invalid break statement.");
    
    JumpContext* targetContext = generator.jumpContextForBreak(m_ident);
    
    if (!targetContext) {
        if (m_ident.isEmpty())
            return emitThrowError(generator, SyntaxError, "Invalid break statement.");
        else
            return emitThrowError(generator, SyntaxError, "Label %s not found.", m_ident);
    }

    ASSERT(targetContext->breakTarget);

    generator.emitJumpScopes(targetContext->breakTarget, targetContext->scopeDepth);

    return dst;
}

// ------------------------------ ReturnNode -----------------------------------

RegisterID* ReturnNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.codeType() != FunctionCode)
        return emitThrowError(generator, SyntaxError, "Invalid return statement.");
        
    RegisterID* r0 = m_value ? generator.emitNode(dst, m_value.get()) : generator.emitLoad(generator.finalDestination(dst), jsUndefined());
    if (generator.scopeDepth()) {
        RefPtr<LabelID> l0 = generator.newLabel();
        generator.emitJumpScopes(l0.get(), 0);
        generator.emitLabel(l0.get());
    }
    generator.emitDebugHook(WillLeaveCallFrame, firstLine(), lastLine());
    return generator.emitReturn(r0);
}

// ------------------------------ WithNode -------------------------------------

RegisterID* WithNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> scope = generator.emitNode(m_expr.get()); // scope must be protected until popped
    generator.emitPushScope(scope.get());
    RegisterID* result = generator.emitNode(dst, m_statement.get());
    generator.emitPopScope();
    return result;
}

// ------------------------------ CaseBlockNode --------------------------------

RegisterID* CaseBlockNode::emitCodeForBlock(CodeGenerator& generator, RegisterID* switchExpression, RegisterID* dst)
{
    Vector<RefPtr<LabelID>, 8> labelVector;

    // Setup jumps
    for (ClauseListNode* list = m_list1.get(); list; list = list->getNext()) {
        RegisterID* clauseVal = generator.emitNode(list->getClause()->expr());
        generator.emitBinaryOp(op_stricteq, clauseVal, clauseVal, switchExpression);
        labelVector.append(generator.newLabel());
        generator.emitJumpIfTrueMayCombine(clauseVal, labelVector[labelVector.size() - 1].get());
    }

    for (ClauseListNode* list = m_list2.get(); list; list = list->getNext()) {
        RegisterID* clauseVal = generator.emitNode(list->getClause()->expr());
        generator.emitBinaryOp(op_stricteq, clauseVal, clauseVal, switchExpression);
        labelVector.append(generator.newLabel());
        generator.emitJumpIfTrueMayCombine(clauseVal, labelVector[labelVector.size() - 1].get());
    }

    RefPtr<LabelID> defaultLabel;
    defaultLabel = generator.newLabel();
    generator.emitJump(defaultLabel.get());

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

    return result;
}

// ------------------------------ SwitchNode -----------------------------------

RegisterID* SwitchNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    RefPtr<LabelID> breakTarget = generator.newLabel();

    RefPtr<RegisterID> r0 = generator.emitNode(m_expr.get());
    generator.pushJumpContext(&m_labelStack, 0, breakTarget.get(), true);
    RegisterID* r1 = m_block->emitCodeForBlock(generator, r0.get(), dst);
    generator.popJumpContext();

    generator.emitLabel(breakTarget.get());

    return r1;
}

// ------------------------------ LabelNode ------------------------------------

RegisterID* LabelNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    if (generator.jumpContextForBreak(m_label))
        return emitThrowError(generator, SyntaxError, "Duplicated label %s found.", m_label);
    
    RefPtr<LabelID> l0 = generator.newLabel();
    m_labelStack.push(m_label);
    generator.pushJumpContext(&m_labelStack, 0, l0.get(), false);
    
    RegisterID* r0 = generator.emitNode(dst, m_statement.get());
    
    generator.popJumpContext();
    m_labelStack.pop();
    
    generator.emitLabel(l0.get());
    return r0;
}

// ------------------------------ ThrowNode ------------------------------------

RegisterID* ThrowNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    generator.emitThrow(generator.emitNode(dst, m_expr.get()));
    return dst;
}

// ------------------------------ TryNode --------------------------------------

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
        RefPtr<RegisterID> newScope = generator.emitNewObject(generator.newTemporary()); // scope must be protected until popped
        generator.emitPutById(newScope.get(), m_exceptionIdent, exceptionRegister.get());
        exceptionRegister = 0; // Release register used for temporaries
        generator.emitPushScope(newScope.get());
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
        generator.emitJump(finallyEndLabel.get());

        // Finally block for exception path
        RefPtr<RegisterID> tempExceptionRegister = generator.emitCatch(generator.newTemporary(), tryStartLabel.get(), generator.emitLabel(generator.newLabel().get()).get());
        generator.emitJumpSubroutine(finallyReturnAddr.get(), finallyStart.get());
        generator.emitThrow(tempExceptionRegister.get());

        // emit the finally block itself
        generator.emitLabel(finallyStart.get());
        generator.emitNode(dst, m_finallyBlock.get());
        generator.emitSubroutineReturn(finallyReturnAddr.get());

        generator.emitLabel(finallyEndLabel.get());
    }

    return dst;
}


// ------------------------------ FunctionBodyNode -----------------------------

ScopeNode::ScopeNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : BlockNode(children)
    , m_sourceURL(JSGlobalData::threadInstance().parser->sourceURL())
    , m_sourceId(JSGlobalData::threadInstance().parser->sourceId())
    , m_usesEval(usesEval)
    , m_needsClosure(needsClosure)
{
    if (varStack)
        m_varStack = *varStack;
    if (funcStack)
        m_functionStack = *funcStack;
}

// ------------------------------ ProgramNode -----------------------------

ProgramNode::ProgramNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : ScopeNode(children, varStack, funcStack, usesEval, needsClosure)
{
}

ProgramNode::~ProgramNode()
{
}

ProgramNode* ProgramNode::create(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
{
    return new ProgramNode(children, varStack, funcStack, usesEval, needsClosure);
}

// ------------------------------ EvalNode -----------------------------

EvalNode::EvalNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : ScopeNode(children, varStack, funcStack, usesEval, needsClosure)
{
}

EvalNode::~EvalNode()
{
}

RegisterID* EvalNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteProgram, firstLine(), lastLine());

    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    statementListEmitCode(m_children, generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, firstLine(), lastLine());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void EvalNode::generateCode(ScopeChainNode* sc)
{
    ScopeChain scopeChain(sc);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    SymbolTable symbolTable;

    m_code.set(new EvalCodeBlock(this, globalObject));

    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &symbolTable, m_code.get());
    generator.generate();
}

EvalNode* EvalNode::create(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
{
    return new EvalNode(children, varStack, funcStack, usesEval, needsClosure);
}

// ------------------------------ FunctionBodyNode -----------------------------

FunctionBodyNode::FunctionBodyNode(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
    : ScopeNode(children, varStack, funcStack, usesEval, needsClosure)
{
}

FunctionBodyNode::~FunctionBodyNode()
{
}

void FunctionBodyNode::mark()
{
    if (m_code)
        m_code->mark();
}

FunctionBodyNode* FunctionBodyNode::create(SourceElements* children, VarStack* varStack, FunctionStack* funcStack, bool usesEval, bool needsClosure)
{
    return new FunctionBodyNode(children, varStack, funcStack, usesEval, needsClosure);
}

void FunctionBodyNode::generateCode(ScopeChainNode* sc)
{
    ScopeChain scopeChain(sc);
    JSGlobalObject* globalObject = scopeChain.globalObject();

    m_code.set(new CodeBlock(this, FunctionCode));

    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &m_symbolTable, m_code.get());
    generator.generate();
}

RegisterID* FunctionBodyNode::emitCode(CodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(DidEnterCallFrame, firstLine(), lastLine());
    statementListEmitCode(m_children, generator);
    if (!m_children.size() || !m_children.last()->isReturnNode()) {
        RegisterID* r0 = generator.emitLoad(generator.newTemporary(), jsUndefined());
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
    statementListEmitCode(m_children, generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, firstLine(), lastLine());
    generator.emitEnd(dstRegister.get());
    return 0;
}

void ProgramNode::generateCode(ScopeChainNode* sc, bool canCreateGlobals)
{
    ScopeChain scopeChain(sc);
    JSGlobalObject* globalObject = scopeChain.globalObject();
    
    m_code.set(new ProgramCodeBlock(this, GlobalCode, globalObject));
    
    CodeGenerator generator(this, globalObject->debugger(), scopeChain, &globalObject->symbolTable(), m_code.get(), m_varStack, m_functionStack, canCreateGlobals);
    generator.generate();
}

UString FunctionBodyNode::paramString() const
{
    UString s("");
    size_t count = m_parameters.size();
    for (size_t pos = 0; pos < count; ++pos) {
        if (!s.isEmpty())
            s += ", ";
        s += m_parameters[pos].ustring();
    }

    return s;
}

// ------------------------------ FuncDeclNode ---------------------------------

void FuncDeclNode::addParams()
{
    for (ParameterNode* p = m_parameter.get(); p; p = p->nextParam())
        m_body->parameters().append(p->ident());
}

FunctionImp* FuncDeclNode::makeFunction(ExecState* exec, ScopeChainNode* scopeChain)
{
    FunctionImp* func = new FunctionImp(exec, m_ident, m_body.get(), scopeChain);

    JSObject* proto = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());
    proto->putDirect(exec->propertyNames().constructor, func, DontEnum);
    func->putDirect(exec->propertyNames().prototype, proto, DontDelete);
    func->putDirect(exec->propertyNames().length, jsNumber(m_body->parameters().size()), ReadOnly | DontDelete | DontEnum);
    return func;
}

RegisterID* FuncDeclNode::emitCode(CodeGenerator&, RegisterID* dst)
{
    return dst;
}

// ------------------------------ FuncExprNode ---------------------------------

RegisterID* FuncExprNode::emitCode(CodeGenerator& generator, RegisterID* dst)
{
    return generator.emitNewFunctionExpression(generator.finalDestination(dst), this);
}

FunctionImp* FuncExprNode::makeFunction(ExecState* exec, ScopeChainNode* scopeChain)
{
    FunctionImp* func = new FunctionImp(exec, m_ident, m_body.get(), scopeChain);
    JSObject* proto = exec->lexicalGlobalObject()->objectConstructor()->construct(exec, exec->emptyList());
    proto->putDirect(exec->propertyNames().constructor, func, DontEnum);
    func->putDirect(exec->propertyNames().prototype, proto, DontDelete);

    /* 
        The Identifier in a FunctionExpression can be referenced from inside
        the FunctionExpression's FunctionBody to allow the function to call
        itself recursively. However, unlike in a FunctionDeclaration, the
        Identifier in a FunctionExpression cannot be referenced from and
        does not affect the scope enclosing the FunctionExpression.
     */

    if (!m_ident.isNull()) {
        JSObject* functionScopeObject = new JSObject;
        functionScopeObject->putDirect(m_ident, func, ReadOnly | DontDelete);
        func->scope().push(functionScopeObject);
    }

    return func;
}

// ECMA 13
void FuncExprNode::addParams()
{
    for (ParameterNode* p = m_parameter.get(); p; p = p->nextParam())
        m_body->parameters().append(p->ident());
}

} // namespace KJS
