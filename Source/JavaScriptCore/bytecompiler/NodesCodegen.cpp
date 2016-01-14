/*
*  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
*  Copyright (C) 2001 Peter Kelly (pmk@post.com)
*  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012, 2013, 2015 Apple Inc. All rights reserved.
*  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
*  Copyright (C) 2007 Maks Orlovich
*  Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2012 Igalia, S.L.
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

#include "BuiltinNames.h"
#include "BytecodeGenerator.h"
#include "CallFrame.h"
#include "Debugger.h"
#include "JIT.h"
#include "JSFunction.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObject.h"
#include "JSONObject.h"
#include "LabelScope.h"
#include "Lexer.h"
#include "JSCInlines.h"
#include "JSTemplateRegistryKey.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "RegExpCache.h"
#include "RegExpObject.h"
#include "SamplingTool.h"
#include "StackAlignment.h"
#include "TemplateRegistryKey.h"
#include <wtf/Assertions.h>
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/Threading.h>

using namespace WTF;

namespace JSC {

/*
    Details of the emitBytecode function.

    Return value: The register holding the production's value.
             dst: An optional parameter specifying the most efficient destination at
                  which to store the production's value. The callee must honor dst.

    The dst argument provides for a crude form of copy propagation. For example,

        x = 1

    becomes
    
        load r[x], 1
    
    instead of 

        load r0, 1
        mov r[x], r0
    
    because the assignment node, "x =", passes r[x] as dst to the number node, "1".
*/

void ExpressionNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label* trueTarget, Label* falseTarget, FallThroughMode fallThroughMode)
{
    RegisterID* result = generator.emitNode(this);
    if (fallThroughMode == FallThroughMeansTrue)
        generator.emitJumpIfFalse(result, falseTarget);
    else
        generator.emitJumpIfTrue(result, trueTarget);
}

// ------------------------------ ThrowableExpressionData --------------------------------

RegisterID* ThrowableExpressionData::emitThrowReferenceError(BytecodeGenerator& generator, const String& message)
{
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitThrowReferenceError(message);
    return generator.newTemporary();
}

// ------------------------------ ConstantNode ----------------------------------

void ConstantNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label* trueTarget, Label* falseTarget, FallThroughMode fallThroughMode)
{
    TriState value = jsValue(generator).pureToBoolean();
    if (value == MixedTriState)
        ExpressionNode::emitBytecodeInConditionContext(generator, trueTarget, falseTarget, fallThroughMode);
    else if (value == TrueTriState && fallThroughMode == FallThroughMeansFalse)
        generator.emitJump(trueTarget);
    else if (value == FalseTriState && fallThroughMode == FallThroughMeansTrue)
        generator.emitJump(falseTarget);

    // All other cases are unconditional fall-throughs, like "if (true)".
}

RegisterID* ConstantNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitLoad(dst, jsValue(generator));
}

JSValue StringNode::jsValue(BytecodeGenerator& generator) const
{
    return generator.addStringConstant(m_value);
}

// ------------------------------ NumberNode ----------------------------------

RegisterID* NumberNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return nullptr;
    return generator.emitLoad(dst, jsValue(generator), isIntegerNode() ? SourceCodeRepresentation::Integer : SourceCodeRepresentation::Double);
}

// ------------------------------ RegExpNode -----------------------------------

RegisterID* RegExpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitNewRegExp(generator.finalDestination(dst), RegExp::create(*generator.vm(), m_pattern.string(), regExpFlags(m_flags.string())));
}

// ------------------------------ ThisNode -------------------------------------

RegisterID* ThisNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (generator.constructorKind() == ConstructorKind::Derived && generator.needsToUpdateArrowFunctionContext())
        generator.emitLoadThisFromArrowFunctionLexicalEnvironment();

    if (m_shouldAlwaysEmitTDZCheck || generator.constructorKind() == ConstructorKind::Derived || generator.isDerivedConstructorContext())
        generator.emitTDZCheck(generator.thisRegister());

    if (dst == generator.ignoredResult())
        return 0;

    RegisterID* result = generator.moveToDestinationIfNeeded(dst, generator.thisRegister());
    static const unsigned thisLength = 4;
    generator.emitProfileType(generator.thisRegister(), position(), JSTextPosition(-1, position().offset + thisLength, -1));
    return result;
}

// ------------------------------ SuperNode -------------------------------------

RegisterID* SuperNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return 0;

    if (generator.isDerivedConstructorContext())
        return generator.emitGetById(generator.finalDestination(dst), generator.emitLoadDerivedConstructorFromArrowFunctionLexicalEnvironment(), generator.propertyNames().underscoreProto);

    RegisterID callee;
    callee.setIndex(JSStack::Callee);
    return generator.emitGetById(generator.finalDestination(dst), &callee, generator.propertyNames().underscoreProto);
}

static RegisterID* emitHomeObjectForCallee(BytecodeGenerator& generator)
{
    if (generator.isDerivedClassContext() || generator.isDerivedConstructorContext()) {
        RegisterID* derivedConstructor = generator.emitLoadDerivedConstructorFromArrowFunctionLexicalEnvironment();
        return generator.emitGetById(generator.newTemporary(), derivedConstructor, generator.propertyNames().homeObjectPrivateName);
    }

    RegisterID callee;
    callee.setIndex(JSStack::Callee);
    return generator.emitGetById(generator.newTemporary(), &callee, generator.propertyNames().homeObjectPrivateName);
}

static RegisterID* emitSuperBaseForCallee(BytecodeGenerator& generator)
{
    RefPtr<RegisterID> homeObject = emitHomeObjectForCallee(generator);
    return generator.emitGetById(generator.newTemporary(), homeObject.get(), generator.propertyNames().underscoreProto);
}

// ------------------------------ NewTargetNode ----------------------------------

RegisterID* NewTargetNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return nullptr;

    return generator.moveToDestinationIfNeeded(dst, generator.newTarget());
}

// ------------------------------ ResolveNode ----------------------------------

bool ResolveNode::isPure(BytecodeGenerator& generator) const
{
    return generator.variable(m_ident).offset().isStack();
}

RegisterID* ResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local, nullptr);
        if (dst == generator.ignoredResult())
            return nullptr;

        generator.emitProfileType(local, var, m_position, JSTextPosition(-1, m_position.offset + m_ident.length(), -1));
        return generator.moveToDestinationIfNeeded(dst, local);
    }
    
    JSTextPosition divot = m_start + m_ident.length();
    generator.emitExpressionInfo(divot, m_start, divot);
    RefPtr<RegisterID> scope = generator.emitResolveScope(dst, var);
    RegisterID* finalDest = generator.finalDestination(dst);
    RegisterID* result = generator.emitGetFromScope(finalDest, scope.get(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, finalDest, nullptr);
    generator.emitProfileType(finalDest, var, m_position, JSTextPosition(-1, m_position.offset + m_ident.length(), -1));
    return result;
}

// ------------------------------ TemplateStringNode -----------------------------------

RegisterID* TemplateStringNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return nullptr;
    return generator.emitLoad(dst, JSValue(generator.addStringConstant(cooked())));
}

// ------------------------------ TemplateLiteralNode -----------------------------------

RegisterID* TemplateLiteralNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_templateExpressions) {
        TemplateStringNode* templateString = m_templateStrings->value();
        ASSERT_WITH_MESSAGE(!m_templateStrings->next(), "Only one template element exists because there's no expression in a given template literal.");
        return generator.emitNode(dst, templateString);
    }

    Vector<RefPtr<RegisterID>, 16> temporaryRegisters;

    TemplateStringListNode* templateString = m_templateStrings;
    TemplateExpressionListNode* templateExpression = m_templateExpressions;
    for (; templateExpression; templateExpression = templateExpression->next(), templateString = templateString->next()) {
        // Evaluate TemplateString.
        if (!templateString->value()->cooked().isEmpty()) {
            temporaryRegisters.append(generator.newTemporary());
            generator.emitNode(temporaryRegisters.last().get(), templateString->value());
        }

        // Evaluate Expression.
        temporaryRegisters.append(generator.newTemporary());
        generator.emitNode(temporaryRegisters.last().get(), templateExpression->value());
        generator.emitToString(temporaryRegisters.last().get(), temporaryRegisters.last().get());
    }

    // Evaluate tail TemplateString.
    if (!templateString->value()->cooked().isEmpty()) {
        temporaryRegisters.append(generator.newTemporary());
        generator.emitNode(temporaryRegisters.last().get(), templateString->value());
    }

    return generator.emitStrcat(generator.finalDestination(dst, temporaryRegisters[0].get()), temporaryRegisters[0].get(), temporaryRegisters.size());
}

// ------------------------------ TaggedTemplateNode -----------------------------------

RegisterID* TaggedTemplateNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ExpectedFunction expectedFunction = NoExpectedFunction;
    RefPtr<RegisterID> tag = nullptr;
    RefPtr<RegisterID> base = nullptr;
    if (!m_tag->isLocation()) {
        tag = generator.newTemporary();
        tag = generator.emitNode(tag.get(), m_tag);
    } else if (m_tag->isResolveNode()) {
        ResolveNode* resolve = static_cast<ResolveNode*>(m_tag);
        const Identifier& identifier = resolve->identifier();
        expectedFunction = generator.expectedFunctionForIdentifier(identifier);

        Variable var = generator.variable(identifier);
        if (RegisterID* local = var.local())
            tag = generator.emitMove(generator.newTemporary(), local);
        else {
            tag = generator.newTemporary();
            base = generator.newTemporary();

            JSTextPosition newDivot = divotStart() + identifier.length();
            generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
            generator.moveToDestinationIfNeeded(base.get(), generator.emitResolveScope(base.get(), var));
            generator.emitGetFromScope(tag.get(), base.get(), var, ThrowIfNotFound);
        }
    } else if (m_tag->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(m_tag);
        base = generator.newTemporary();
        base = generator.emitNode(base.get(), bracket->base());
        RefPtr<RegisterID> property = generator.emitNode(bracket->subscript());
        tag = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    } else {
        ASSERT(m_tag->isDotAccessorNode());
        DotAccessorNode* dot = static_cast<DotAccessorNode*>(m_tag);
        base = generator.newTemporary();
        base = generator.emitNode(base.get(), dot->base());
        tag = generator.emitGetById(generator.newTemporary(), base.get(), dot->identifier());
    }

    RefPtr<RegisterID> templateObject = generator.emitGetTemplateObject(generator.newTemporary(), this);

    unsigned expressionsCount = 0;
    for (TemplateExpressionListNode* templateExpression = m_templateLiteral->templateExpressions(); templateExpression; templateExpression = templateExpression->next())
        ++expressionsCount;

    CallArguments callArguments(generator, nullptr, 1 + expressionsCount);
    if (base)
        generator.emitMove(callArguments.thisRegister(), base.get());
    else
        generator.emitLoad(callArguments.thisRegister(), jsUndefined());

    unsigned argumentIndex = 0;
    generator.emitMove(callArguments.argumentRegister(argumentIndex++), templateObject.get());
    for (TemplateExpressionListNode* templateExpression = m_templateLiteral->templateExpressions(); templateExpression; templateExpression = templateExpression->next())
        generator.emitNode(callArguments.argumentRegister(argumentIndex++), templateExpression->value());

    return generator.emitCall(generator.finalDestination(dst, tag.get()), tag.get(), expectedFunction, callArguments, divot(), divotStart(), divotEnd());
}

// ------------------------------ ArrayNode ------------------------------------

RegisterID* ArrayNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    // FIXME: Should we put all of this code into emitNewArray?

    unsigned length = 0;
    ElementNode* firstPutElement;
    for (firstPutElement = m_element; firstPutElement; firstPutElement = firstPutElement->next()) {
        if (firstPutElement->elision() || firstPutElement->value()->isSpreadExpression())
            break;
        ++length;
    }

    if (!firstPutElement && !m_elision)
        return generator.emitNewArray(generator.finalDestination(dst), m_element, length);

    RefPtr<RegisterID> array = generator.emitNewArray(generator.tempDestination(dst), m_element, length);
    ElementNode* n = firstPutElement;
    for (; n; n = n->next()) {
        if (n->value()->isSpreadExpression())
            goto handleSpread;
        RegisterID* value = generator.emitNode(n->value());
        length += n->elision();
        generator.emitPutByIndex(array.get(), length++, value);
    }

    if (m_elision) {
        RegisterID* value = generator.emitLoad(0, jsNumber(m_elision + length));
        generator.emitPutById(array.get(), generator.propertyNames().length, value);
    }

    return generator.moveToDestinationIfNeeded(dst, array.get());
    
handleSpread:
    RefPtr<RegisterID> index = generator.emitLoad(generator.newTemporary(), jsNumber(length));
    auto spreader = [this, array, index](BytecodeGenerator& generator, RegisterID* value)
    {
        generator.emitDirectPutByVal(array.get(), index.get(), value);
        generator.emitInc(index.get());
    };
    for (; n; n = n->next()) {
        if (n->elision())
            generator.emitBinaryOp(op_add, index.get(), index.get(), generator.emitLoad(0, jsNumber(n->elision())), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
        if (n->value()->isSpreadExpression()) {
            SpreadExpressionNode* spread = static_cast<SpreadExpressionNode*>(n->value());
            generator.emitEnumeration(spread, spread->expression(), spreader);
        } else {
            generator.emitDirectPutByVal(array.get(), index.get(), generator.emitNode(n->value()));
            generator.emitInc(index.get());
        }
    }
    
    if (m_elision) {
        generator.emitBinaryOp(op_add, index.get(), index.get(), generator.emitLoad(0, jsNumber(m_elision)), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
        generator.emitPutById(array.get(), generator.propertyNames().length, index.get());
    }
    return generator.moveToDestinationIfNeeded(dst, array.get());
}

bool ArrayNode::isSimpleArray() const
{
    if (m_elision || m_optional)
        return false;
    for (ElementNode* ptr = m_element; ptr; ptr = ptr->next()) {
        if (ptr->elision())
            return false;
    }
    return true;
}

ArgumentListNode* ArrayNode::toArgumentList(ParserArena& parserArena, int lineNumber, int startPosition) const
{
    ASSERT(!m_elision && !m_optional);
    ElementNode* ptr = m_element;
    if (!ptr)
        return 0;
    JSTokenLocation location;
    location.line = lineNumber;
    location.startOffset = startPosition;
    ArgumentListNode* head = new (parserArena) ArgumentListNode(location, ptr->value());
    ArgumentListNode* tail = head;
    ptr = ptr->next();
    for (; ptr; ptr = ptr->next()) {
        ASSERT(!ptr->elision());
        tail = new (parserArena) ArgumentListNode(location, tail, ptr->value());
    }
    return head;
}

// ------------------------------ ObjectLiteralNode ----------------------------

RegisterID* ObjectLiteralNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_list) {
        if (dst == generator.ignoredResult())
            return 0;
        return generator.emitNewObject(generator.finalDestination(dst));
    }
    RefPtr<RegisterID> newObj = generator.emitNewObject(generator.tempDestination(dst));
    generator.emitNode(newObj.get(), m_list);
    return generator.moveToDestinationIfNeeded(dst, newObj.get());
}

// ------------------------------ PropertyListNode -----------------------------

static inline void emitPutHomeObject(BytecodeGenerator& generator, RegisterID* function, RegisterID* homeObject)
{
    generator.emitPutById(function, generator.propertyNames().homeObjectPrivateName, homeObject);
}

RegisterID* PropertyListNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    // Fast case: this loop just handles regular value properties.
    PropertyListNode* p = this;
    for (; p && (p->m_node->m_type & PropertyNode::Constant); p = p->m_next)
        emitPutConstantProperty(generator, dst, *p->m_node);

    // Were there any get/set properties?
    if (p) {
        // Build a list of getter/setter pairs to try to put them at the same time. If we encounter
        // a computed property, just emit everything as that may override previous values.
        bool hasComputedProperty = false;

        typedef std::pair<PropertyNode*, PropertyNode*> GetterSetterPair;
        typedef HashMap<UniquedStringImpl*, GetterSetterPair, IdentifierRepHash> GetterSetterMap;
        GetterSetterMap map;

        // Build a map, pairing get/set values together.
        for (PropertyListNode* q = p; q; q = q->m_next) {
            PropertyNode* node = q->m_node;
            if (node->m_type & PropertyNode::Computed) {
                hasComputedProperty = true;
                break;
            }
            if (node->m_type & PropertyNode::Constant)
                continue;

            // Duplicates are possible.
            GetterSetterPair pair(node, static_cast<PropertyNode*>(nullptr));
            GetterSetterMap::AddResult result = map.add(node->name()->impl(), pair);
            if (!result.isNewEntry) {
                if (result.iterator->value.first->m_type == node->m_type)
                    result.iterator->value.first = node;
                else
                    result.iterator->value.second = node;
            }
        }

        // Iterate over the remaining properties in the list.
        for (; p; p = p->m_next) {
            PropertyNode* node = p->m_node;

            // Handle regular values.
            if (node->m_type & PropertyNode::Constant) {
                emitPutConstantProperty(generator, dst, *node);
                continue;
            }

            RefPtr<RegisterID> value = generator.emitNode(node->m_assign);
            bool isClassProperty = node->needsSuperBinding();
            if (isClassProperty)
                emitPutHomeObject(generator, value.get(), dst);
            unsigned attribute = isClassProperty ? (Accessor | DontEnum) : Accessor;

            ASSERT(node->m_type & (PropertyNode::Getter | PropertyNode::Setter));

            // This is a get/set property which may be overridden by a computed property later.
            if (hasComputedProperty) {
                // Computed accessors.
                if (node->m_type & PropertyNode::Computed) {
                    RefPtr<RegisterID> propertyName = generator.emitNode(node->m_expression);
                    if (node->m_type & PropertyNode::Getter)
                        generator.emitPutGetterByVal(dst, propertyName.get(), attribute, value.get());
                    else
                        generator.emitPutSetterByVal(dst, propertyName.get(), attribute, value.get());
                    continue;
                }

                if (node->m_type & PropertyNode::Getter)
                    generator.emitPutGetterById(dst, *node->name(), attribute, value.get());
                else
                    generator.emitPutSetterById(dst, *node->name(), attribute, value.get());
                continue;
            }

            // This is a get/set property pair.
            GetterSetterMap::iterator it = map.find(node->name()->impl());
            ASSERT(it != map.end());
            GetterSetterPair& pair = it->value;

            // Was this already generated as a part of its partner?
            if (pair.second == node)
                continue;

            // Generate the paired node now.
            RefPtr<RegisterID> getterReg;
            RefPtr<RegisterID> setterReg;
            RegisterID* secondReg = nullptr;

            if (node->m_type & PropertyNode::Getter) {
                getterReg = value;
                if (pair.second) {
                    ASSERT(pair.second->m_type & PropertyNode::Setter);
                    setterReg = generator.emitNode(pair.second->m_assign);
                    secondReg = setterReg.get();
                } else {
                    setterReg = generator.newTemporary();
                    generator.emitLoad(setterReg.get(), jsUndefined());
                }
            } else {
                ASSERT(node->m_type & PropertyNode::Setter);
                setterReg = value;
                if (pair.second) {
                    ASSERT(pair.second->m_type & PropertyNode::Getter);
                    getterReg = generator.emitNode(pair.second->m_assign);
                    secondReg = getterReg.get();
                } else {
                    getterReg = generator.newTemporary();
                    generator.emitLoad(getterReg.get(), jsUndefined());
                }
            }

            ASSERT(!pair.second || isClassProperty == pair.second->needsSuperBinding());
            if (isClassProperty && pair.second)
                emitPutHomeObject(generator, secondReg, dst);

            generator.emitPutGetterSetter(dst, *node->name(), attribute, getterReg.get(), setterReg.get());
        }
    }

    return dst;
}

void PropertyListNode::emitPutConstantProperty(BytecodeGenerator& generator, RegisterID* newObj, PropertyNode& node)
{
    RefPtr<RegisterID> value = generator.emitNode(node.m_assign);
    if (node.needsSuperBinding()) {
        emitPutHomeObject(generator, value.get(), newObj);

        RefPtr<RegisterID> propertyNameRegister;
        if (node.name())
            propertyNameRegister = generator.emitLoad(generator.newTemporary(), *node.name());
        else
            propertyNameRegister = generator.emitNode(node.m_expression);

        generator.emitCallDefineProperty(newObj, propertyNameRegister.get(),
            value.get(), nullptr, nullptr, BytecodeGenerator::PropertyConfigurable | BytecodeGenerator::PropertyWritable, m_position);
        return;
    }
    if (const auto* identifier = node.name()) {
        Optional<uint32_t> optionalIndex = parseIndex(*identifier);
        if (!optionalIndex) {
            generator.emitDirectPutById(newObj, *identifier, value.get(), node.putType());
            return;
        }

        RefPtr<RegisterID> index = generator.emitLoad(generator.newTemporary(), jsNumber(optionalIndex.value()));
        generator.emitDirectPutByVal(newObj, index.get(), value.get());
        return;
    }
    RefPtr<RegisterID> propertyName = generator.emitNode(node.m_expression);
    generator.emitDirectPutByVal(newObj, propertyName.get(), value.get());
}

// ------------------------------ BracketAccessorNode --------------------------------

static bool isNonIndexStringElement(ExpressionNode& element)
{
    return element.isString() && !parseIndex(static_cast<StringNode&>(element).value());
}

RegisterID* BracketAccessorNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_base->isSuperNode()) {
        // FIXME: Should we generate the profiler info?
        if (isNonIndexStringElement(*m_subscript)) {
            const Identifier& id = static_cast<StringNode*>(m_subscript)->value();
            return generator.emitGetById(generator.finalDestination(dst), emitSuperBaseForCallee(generator), id);
        }
        return generator.emitGetByVal(generator.finalDestination(dst), emitSuperBaseForCallee(generator), generator.emitNode(m_subscript));
    }

    RegisterID* ret;
    RegisterID* finalDest = generator.finalDestination(dst);

    if (isNonIndexStringElement(*m_subscript)) {
        RefPtr<RegisterID> base = generator.emitNode(m_base);
        ret = generator.emitGetById(finalDest, base.get(), static_cast<StringNode*>(m_subscript)->value());
    } else {
        RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments, m_subscript->isPure(generator));
        RegisterID* property = generator.emitNode(m_subscript);
        ret = generator.emitGetByVal(finalDest, base.get(), property);
    }

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());

    generator.emitProfileType(finalDest, divotStart(), divotEnd());
    return ret;
}

// ------------------------------ DotAccessorNode --------------------------------

RegisterID* DotAccessorNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = m_base->isSuperNode() ? emitSuperBaseForCallee(generator) : generator.emitNode(m_base);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* finalDest = generator.finalDestination(dst);
    RegisterID* ret = generator.emitGetById(finalDest, base.get(), m_ident);
    generator.emitProfileType(finalDest, divotStart(), divotEnd());
    return ret;
}

// ------------------------------ ArgumentListNode -----------------------------

RegisterID* ArgumentListNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    return generator.emitNode(dst, m_expr);
}

// ------------------------------ NewExprNode ----------------------------------

RegisterID* NewExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ExpectedFunction expectedFunction;
    if (m_expr->isResolveNode())
        expectedFunction = generator.expectedFunctionForIdentifier(static_cast<ResolveNode*>(m_expr)->identifier());
    else
        expectedFunction = NoExpectedFunction;
    RefPtr<RegisterID> func = generator.emitNode(m_expr);
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
    CallArguments callArguments(generator, m_args);
    generator.emitMove(callArguments.thisRegister(), func.get());
    return generator.emitConstruct(returnValue.get(), func.get(), expectedFunction, callArguments, divot(), divotStart(), divotEnd());
}

CallArguments::CallArguments(BytecodeGenerator& generator, ArgumentsNode* argumentsNode, unsigned additionalArguments)
    : m_argumentsNode(argumentsNode)
    , m_padding(0)
{
    if (generator.shouldEmitProfileHooks())
        m_profileHookRegister = generator.newTemporary();

    size_t argumentCountIncludingThis = 1 + additionalArguments; // 'this' register.
    if (argumentsNode) {
        for (ArgumentListNode* node = argumentsNode->m_listNode; node; node = node->m_next)
            ++argumentCountIncludingThis;
    }

    m_argv.grow(argumentCountIncludingThis);
    for (int i = argumentCountIncludingThis - 1; i >= 0; --i) {
        m_argv[i] = generator.newTemporary();
        ASSERT(static_cast<size_t>(i) == m_argv.size() - 1 || m_argv[i]->index() == m_argv[i + 1]->index() - 1);
    }

    // We need to ensure that the frame size is stack-aligned
    while ((JSStack::CallFrameHeaderSize + m_argv.size()) % stackAlignmentRegisters()) {
        m_argv.insert(0, generator.newTemporary());
        m_padding++;
    }
    
    while (stackOffset() % stackAlignmentRegisters()) {
        m_argv.insert(0, generator.newTemporary());
        m_padding++;
    }
}

// ------------------------------ EvalFunctionCallNode ----------------------------------

RegisterID* EvalFunctionCallNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    // We need try to load 'this' before call eval in constructor, because 'this' can created by 'super' in some of the arrow function
    // var A = class A {
    //   constructor () { this.id = 'A'; }
    // }
    //
    // var B = class B extend A {
    //    constructor () {
    //       var arrow = () => super();
    //       arrow();
    //       eval("this.id = 'B'");
    //    }
    // }
    if (generator.constructorKind() == ConstructorKind::Derived && generator.needsToUpdateArrowFunctionContext())
        generator.emitLoadThisFromArrowFunctionLexicalEnvironment();

    Variable var = generator.variable(generator.propertyNames().eval);
    if (RegisterID* local = var.local()) {
        RefPtr<RegisterID> func = generator.emitMove(generator.tempDestination(dst), local);
        CallArguments callArguments(generator, m_args);
        generator.emitLoad(callArguments.thisRegister(), jsUndefined());
        return generator.emitCallEval(generator.finalDestination(dst, func.get()), func.get(), callArguments, divot(), divotStart(), divotEnd());
    }

    RefPtr<RegisterID> func = generator.newTemporary();
    CallArguments callArguments(generator, m_args);
    JSTextPosition newDivot = divotStart() + 4;
    generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
    generator.moveToDestinationIfNeeded(
        callArguments.thisRegister(),
        generator.emitResolveScope(callArguments.thisRegister(), var));
    generator.emitGetFromScope(func.get(), callArguments.thisRegister(), var, ThrowIfNotFound);
    return generator.emitCallEval(generator.finalDestination(dst, func.get()), func.get(), callArguments, divot(), divotStart(), divotEnd());
}

// ------------------------------ FunctionCallValueNode ----------------------------------

RegisterID* FunctionCallValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> func = generator.emitNode(m_expr);
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
    CallArguments callArguments(generator, m_args);
    if (m_expr->isSuperNode()) {
        ASSERT(generator.isConstructor() || generator.derivedContextType() == DerivedContextType::DerivedConstructorContext);
        ASSERT(generator.constructorKind() == ConstructorKind::Derived || generator.derivedContextType() == DerivedContextType::DerivedConstructorContext);
        generator.emitMove(callArguments.thisRegister(), generator.newTarget());
        RegisterID* ret = generator.emitConstruct(returnValue.get(), func.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
        generator.emitMove(generator.thisRegister(), ret);
        
        bool isConstructorKindDerived = generator.constructorKind() == ConstructorKind::Derived;
        if (generator.isDerivedConstructorContext() || (isConstructorKindDerived && generator.needsToUpdateArrowFunctionContext()))
            generator.emitPutThisToArrowFunctionContextScope();
        
        return ret;
    }
    generator.emitLoad(callArguments.thisRegister(), jsUndefined());
    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), func.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return ret;
}

// ------------------------------ FunctionCallResolveNode ----------------------------------

RegisterID* FunctionCallResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ExpectedFunction expectedFunction = generator.expectedFunctionForIdentifier(m_ident);

    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local, nullptr);
        RefPtr<RegisterID> func = generator.emitMove(generator.tempDestination(dst), local);
        RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
        CallArguments callArguments(generator, m_args);
        generator.emitLoad(callArguments.thisRegister(), jsUndefined());
        // This passes NoExpectedFunction because we expect that if the function is in a
        // local variable, then it's not one of our built-in constructors.
        RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), func.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
        generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
        return ret;
    }

    RefPtr<RegisterID> func = generator.newTemporary();
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
    CallArguments callArguments(generator, m_args);

    JSTextPosition newDivot = divotStart() + m_ident.length();
    generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
    generator.moveToDestinationIfNeeded(
        callArguments.thisRegister(),
        generator.emitResolveScope(callArguments.thisRegister(), var));
    generator.emitGetFromScope(func.get(), callArguments.thisRegister(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, func.get(), nullptr);
    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), func.get(), expectedFunction, callArguments, divot(), divotStart(), divotEnd());
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return ret;
}

// ------------------------------ BytecodeIntrinsicNode ----------------------------------

RegisterID* BytecodeIntrinsicNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    return (this->*m_emitter)(generator, dst);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_assert(BytecodeGenerator& generator, RegisterID* dst)
{
#ifndef NDEBUG
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> condition = generator.emitNode(node);
    generator.emitAssert(condition.get(), node->firstLine());
    return dst;
#else
    UNUSED_PARAM(generator);
    return dst;
#endif
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putByValDirect(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RefPtr<RegisterID> index = generator.emitNode(node);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.moveToDestinationIfNeeded(dst, generator.emitDirectPutByVal(base.get(), index.get(), value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_toString(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> src = generator.emitNode(node);
    ASSERT(!node->m_next);

    return generator.moveToDestinationIfNeeded(dst, generator.emitToString(generator.tempDestination(dst), src.get()));
}

// ------------------------------ FunctionCallBracketNode ----------------------------------

RegisterID* FunctionCallBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    bool baseIsSuper = m_base->isSuperNode();
    bool subscriptIsNonIndexString = isNonIndexStringElement(*m_subscript);

    RefPtr<RegisterID> base;
    if (baseIsSuper)
        base = emitSuperBaseForCallee(generator);
    else {
        if (subscriptIsNonIndexString)
            base = generator.emitNode(m_base);
        else
            base = generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments, m_subscript->isPure(generator));
    }

    RefPtr<RegisterID> function;
    if (subscriptIsNonIndexString) {
        generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
        function = generator.emitGetById(generator.tempDestination(dst), base.get(), static_cast<StringNode*>(m_subscript)->value());
    } else {
        RefPtr<RegisterID> property = generator.emitNode(m_subscript);
        generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
        function = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());
    }

    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, function.get());
    CallArguments callArguments(generator, m_args);
    if (baseIsSuper)
        generator.emitMove(callArguments.thisRegister(), generator.thisRegister());
    else
        generator.emitMove(callArguments.thisRegister(), base.get());
    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return ret;
}

// ------------------------------ FunctionCallDotNode ----------------------------------

RegisterID* FunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> function = generator.tempDestination(dst);
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, function.get());
    CallArguments callArguments(generator, m_args);
    bool baseIsSuper = m_base->isSuperNode();
    if (baseIsSuper)
        generator.emitMove(callArguments.thisRegister(), generator.thisRegister());
    else
        generator.emitNode(callArguments.thisRegister(), m_base);
    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    generator.emitGetById(function.get(), baseIsSuper ? emitSuperBaseForCallee(generator) : callArguments.thisRegister(), m_ident);
    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return ret;
}

RegisterID* CallFunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<Label> realCall = generator.newLabel();
    RefPtr<Label> end = generator.newLabel();
    RefPtr<RegisterID> base = generator.emitNode(m_base);
    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    RefPtr<RegisterID> function;
    bool emitCallCheck = !generator.isBuiltinFunction();
    if (emitCallCheck) {
        function = generator.emitGetById(generator.tempDestination(dst), base.get(), generator.propertyNames().builtinNames().callPublicName());
        generator.emitJumpIfNotFunctionCall(function.get(), realCall.get());
    }
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst);
    {
        if (m_args->m_listNode && m_args->m_listNode->m_expr && m_args->m_listNode->m_expr->isSpreadExpression()) {
            RefPtr<RegisterID> profileHookRegister;
            if (generator.shouldEmitProfileHooks())
                profileHookRegister = generator.newTemporary();
            SpreadExpressionNode* spread = static_cast<SpreadExpressionNode*>(m_args->m_listNode->m_expr);
            ExpressionNode* subject = spread->expression();
            RefPtr<RegisterID> argumentsRegister;
            argumentsRegister = generator.emitNode(subject);
            generator.emitExpressionInfo(spread->divot(), spread->divotStart(), spread->divotEnd());
            RefPtr<RegisterID> thisRegister = generator.emitGetByVal(generator.newTemporary(), argumentsRegister.get(), generator.emitLoad(0, jsNumber(0)));
            generator.emitCallVarargsInTailPosition(returnValue.get(), base.get(), thisRegister.get(), argumentsRegister.get(), generator.newTemporary(), 1, profileHookRegister.get(), divot(), divotStart(), divotEnd());
        } else if (m_args->m_listNode && m_args->m_listNode->m_expr) {
            ArgumentListNode* oldList = m_args->m_listNode;
            m_args->m_listNode = m_args->m_listNode->m_next;

            RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
            CallArguments callArguments(generator, m_args);
            generator.emitNode(callArguments.thisRegister(), oldList->m_expr);
            generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
            m_args->m_listNode = oldList;
        } else {
            RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
            CallArguments callArguments(generator, m_args);
            generator.emitLoad(callArguments.thisRegister(), jsUndefined());
            generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
        }
    }
    if (emitCallCheck) {
        generator.emitJump(end.get());
        generator.emitLabel(realCall.get());
        {
            CallArguments callArguments(generator, m_args);
            generator.emitMove(callArguments.thisRegister(), base.get());
            generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
        }
        generator.emitLabel(end.get());
    }
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return returnValue.get();
}

static bool areTrivialApplyArguments(ArgumentsNode* args)
{
    return !args->m_listNode || !args->m_listNode->m_expr || !args->m_listNode->m_next
        || (!args->m_listNode->m_next->m_next && args->m_listNode->m_next->m_expr->isSimpleArray());
}

RegisterID* ApplyFunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    // A few simple cases can be trivially handled as ordinary function calls.
    // function.apply(), function.apply(arg) -> identical to function.call
    // function.apply(thisArg, [arg0, arg1, ...]) -> can be trivially coerced into function.call(thisArg, arg0, arg1, ...) and saves object allocation
    bool mayBeCall = areTrivialApplyArguments(m_args);

    RefPtr<Label> realCall = generator.newLabel();
    RefPtr<Label> end = generator.newLabel();
    RefPtr<RegisterID> base = generator.emitNode(m_base);
    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    RefPtr<RegisterID> function;
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, function.get());
    bool emitCallCheck = !generator.isBuiltinFunction();
    if (emitCallCheck) {
        function = generator.emitGetById(generator.tempDestination(dst), base.get(), generator.propertyNames().builtinNames().applyPublicName());
        generator.emitJumpIfNotFunctionApply(function.get(), realCall.get());
    }
    if (mayBeCall) {
        if (m_args->m_listNode && m_args->m_listNode->m_expr) {
            ArgumentListNode* oldList = m_args->m_listNode;
            if (m_args->m_listNode->m_expr->isSpreadExpression()) {
                SpreadExpressionNode* spread = static_cast<SpreadExpressionNode*>(m_args->m_listNode->m_expr);
                RefPtr<RegisterID> profileHookRegister;
                if (generator.shouldEmitProfileHooks())
                    profileHookRegister = generator.newTemporary();
                RefPtr<RegisterID> realFunction = generator.emitMove(generator.newTemporary(), base.get());
                RefPtr<RegisterID> index = generator.emitLoad(generator.newTemporary(), jsNumber(0));
                RefPtr<RegisterID> thisRegister = generator.emitLoad(generator.newTemporary(), jsUndefined());
                RefPtr<RegisterID> argumentsRegister = generator.emitLoad(generator.newTemporary(), jsUndefined());
                
                auto extractor = [&thisRegister, &argumentsRegister, &index](BytecodeGenerator& generator, RegisterID* value)
                {
                    RefPtr<Label> haveThis = generator.newLabel();
                    RefPtr<Label> end = generator.newLabel();
                    RefPtr<RegisterID> compareResult = generator.newTemporary();
                    RefPtr<RegisterID> indexZeroCompareResult = generator.emitBinaryOp(op_eq, compareResult.get(), index.get(), generator.emitLoad(0, jsNumber(0)), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
                    generator.emitJumpIfFalse(indexZeroCompareResult.get(), haveThis.get());
                    generator.emitMove(thisRegister.get(), value);
                    generator.emitLoad(index.get(), jsNumber(1));
                    generator.emitJump(end.get());
                    generator.emitLabel(haveThis.get());
                    RefPtr<RegisterID> indexOneCompareResult = generator.emitBinaryOp(op_eq, compareResult.get(), index.get(), generator.emitLoad(0, jsNumber(1)), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
                    generator.emitJumpIfFalse(indexOneCompareResult.get(), end.get());
                    generator.emitMove(argumentsRegister.get(), value);
                    generator.emitLoad(index.get(), jsNumber(2));
                    generator.emitLabel(end.get());
                };
                generator.emitEnumeration(this, spread->expression(), extractor);
                generator.emitCallVarargsInTailPosition(returnValue.get(), realFunction.get(), thisRegister.get(), argumentsRegister.get(), generator.newTemporary(), 0, profileHookRegister.get(), divot(), divotStart(), divotEnd());
            } else if (m_args->m_listNode->m_next) {
                ASSERT(m_args->m_listNode->m_next->m_expr->isSimpleArray());
                ASSERT(!m_args->m_listNode->m_next->m_next);
                m_args->m_listNode = static_cast<ArrayNode*>(m_args->m_listNode->m_next->m_expr)->toArgumentList(generator.parserArena(), 0, 0);
                RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
                CallArguments callArguments(generator, m_args);
                generator.emitNode(callArguments.thisRegister(), oldList->m_expr);
                generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
            } else {
                m_args->m_listNode = m_args->m_listNode->m_next;
                RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
                CallArguments callArguments(generator, m_args);
                generator.emitNode(callArguments.thisRegister(), oldList->m_expr);
                generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
            }
            m_args->m_listNode = oldList;
        } else {
            RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
            CallArguments callArguments(generator, m_args);
            generator.emitLoad(callArguments.thisRegister(), jsUndefined());
            generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
        }
    } else {
        ASSERT(m_args->m_listNode && m_args->m_listNode->m_next);
        RefPtr<RegisterID> profileHookRegister;
        if (generator.shouldEmitProfileHooks())
            profileHookRegister = generator.newTemporary();
        RefPtr<RegisterID> realFunction = generator.emitMove(generator.tempDestination(dst), base.get());
        RefPtr<RegisterID> thisRegister = generator.emitNode(m_args->m_listNode->m_expr);
        RefPtr<RegisterID> argsRegister;
        ArgumentListNode* args = m_args->m_listNode->m_next;
        argsRegister = generator.emitNode(args->m_expr);

        // Function.prototype.apply ignores extra arguments, but we still
        // need to evaluate them for side effects.
        while ((args = args->m_next))
            generator.emitNode(args->m_expr);

        generator.emitCallVarargsInTailPosition(returnValue.get(), realFunction.get(), thisRegister.get(), argsRegister.get(), generator.newTemporary(), 0, profileHookRegister.get(), divot(), divotStart(), divotEnd());
    }
    if (emitCallCheck) {
        generator.emitJump(end.get());
        generator.emitLabel(realCall.get());
        CallArguments callArguments(generator, m_args);
        generator.emitMove(callArguments.thisRegister(), base.get());
        generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());
        generator.emitLabel(end.get());
    }
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return returnValue.get();
}

// ------------------------------ PostfixNode ----------------------------------

static RegisterID* emitIncOrDec(BytecodeGenerator& generator, RegisterID* srcDst, Operator oper)
{
    return (oper == OpPlusPlus) ? generator.emitInc(srcDst) : generator.emitDec(srcDst);
}

static RegisterID* emitPostIncOrDec(BytecodeGenerator& generator, RegisterID* dst, RegisterID* srcDst, Operator oper)
{
    if (dst == srcDst)
        return generator.emitToNumber(generator.finalDestination(dst), srcDst);
    RefPtr<RegisterID> tmp = generator.emitToNumber(generator.tempDestination(dst), srcDst);
    emitIncOrDec(generator, srcDst, oper);
    return generator.moveToDestinationIfNeeded(dst, tmp.get());
}

RegisterID* PostfixNode::emitResolve(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return PrefixNode::emitResolve(generator, dst);

    ASSERT(m_expr->isResolveNode());
    ResolveNode* resolve = static_cast<ResolveNode*>(m_expr);
    const Identifier& ident = resolve->identifier();

    Variable var = generator.variable(ident);
    if (RegisterID* local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local, nullptr);
        RefPtr<RegisterID> localReg = local;
        if (var.isReadOnly()) {
            generator.emitReadOnlyExceptionIfNeeded(var);
            localReg = generator.emitMove(generator.tempDestination(dst), local);
        }
        RefPtr<RegisterID> oldValue = emitPostIncOrDec(generator, generator.finalDestination(dst), localReg.get(), m_operator);
        generator.emitProfileType(localReg.get(), var, divotStart(), divotEnd());
        return oldValue.get();
    }

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, value.get(), nullptr);
    if (var.isReadOnly()) {
        bool threwException = generator.emitReadOnlyExceptionIfNeeded(var);
        if (threwException)
            return value.get();
    }
    RefPtr<RegisterID> oldValue = emitPostIncOrDec(generator, generator.finalDestination(dst), value.get(), m_operator);
    if (!var.isReadOnly()) {
        generator.emitPutToScope(scope.get(), var, value.get(), ThrowIfNotFound, NotInitialization);
        generator.emitProfileType(value.get(), var, divotStart(), divotEnd());
    }

    return oldValue.get();
}

RegisterID* PostfixNode::emitBracket(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return PrefixNode::emitBracket(generator, dst);

    ASSERT(m_expr->isBracketAccessorNode());
    BracketAccessorNode* bracketAccessor = static_cast<BracketAccessorNode*>(m_expr);
    ExpressionNode* baseNode = bracketAccessor->base();
    ExpressionNode* subscript = bracketAccessor->subscript();

    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(baseNode, bracketAccessor->subscriptHasAssignments(), subscript->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNode(subscript);

    generator.emitExpressionInfo(bracketAccessor->divot(), bracketAccessor->divotStart(), bracketAccessor->divotEnd());
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue = emitPostIncOrDec(generator, generator.tempDestination(dst), value.get(), m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitPutByVal(base.get(), property.get(), value.get());
    generator.emitProfileType(value.get(), divotStart(), divotEnd());
    return generator.moveToDestinationIfNeeded(dst, oldValue);
}

RegisterID* PostfixNode::emitDot(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return PrefixNode::emitDot(generator, dst);

    ASSERT(m_expr->isDotAccessorNode());
    DotAccessorNode* dotAccessor = static_cast<DotAccessorNode*>(m_expr);
    ExpressionNode* baseNode = dotAccessor->base();
    const Identifier& ident = dotAccessor->identifier();

    RefPtr<RegisterID> base = generator.emitNode(baseNode);

    generator.emitExpressionInfo(dotAccessor->divot(), dotAccessor->divotStart(), dotAccessor->divotEnd());
    RefPtr<RegisterID> value = generator.emitGetById(generator.newTemporary(), base.get(), ident);
    RegisterID* oldValue = emitPostIncOrDec(generator, generator.tempDestination(dst), value.get(), m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitPutById(base.get(), ident, value.get());
    generator.emitProfileType(value.get(), divotStart(), divotEnd());
    return generator.moveToDestinationIfNeeded(dst, oldValue);
}

RegisterID* PostfixNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr->isResolveNode())
        return emitResolve(generator, dst);

    if (m_expr->isBracketAccessorNode())
        return emitBracket(generator, dst);

    if (m_expr->isDotAccessorNode())
        return emitDot(generator, dst);

    return emitThrowReferenceError(generator, m_operator == OpPlusPlus
        ? ASCIILiteral("Postfix ++ operator applied to value that is not a reference.")
        : ASCIILiteral("Postfix -- operator applied to value that is not a reference."));
}

// ------------------------------ DeleteResolveNode -----------------------------------

RegisterID* DeleteResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    Variable var = generator.variable(m_ident);
    if (var.local()) {
        generator.emitTDZCheckIfNecessary(var, var.local(), nullptr);
        return generator.emitLoad(generator.finalDestination(dst), false);
    }

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RefPtr<RegisterID> base = generator.emitResolveScope(dst, var);
    generator.emitTDZCheckIfNecessary(var, nullptr, base.get());
    return generator.emitDeleteById(generator.finalDestination(dst, base.get()), base.get(), m_ident);
}

// ------------------------------ DeleteBracketNode -----------------------------------

RegisterID* DeleteBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_base);
    RefPtr<RegisterID> r1 = generator.emitNode(m_subscript);

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (m_base->isSuperNode())
        return emitThrowReferenceError(generator, "Cannot delete a super property");
    return generator.emitDeleteByVal(generator.finalDestination(dst), r0.get(), r1.get());
}

// ------------------------------ DeleteDotNode -----------------------------------

RegisterID* DeleteDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> r0 = generator.emitNode(m_base);

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (m_base->isSuperNode())
        return emitThrowReferenceError(generator, "Cannot delete a super property");
    return generator.emitDeleteById(generator.finalDestination(dst), r0.get(), m_ident);
}

// ------------------------------ DeleteValueNode -----------------------------------

RegisterID* DeleteValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(generator.ignoredResult(), m_expr);

    // delete on a non-location expression ignores the value and returns true
    return generator.emitLoad(generator.finalDestination(dst), true);
}

// ------------------------------ VoidNode -------------------------------------

RegisterID* VoidNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult()) {
        generator.emitNode(generator.ignoredResult(), m_expr);
        return 0;
    }
    RefPtr<RegisterID> r0 = generator.emitNode(m_expr);
    return generator.emitLoad(dst, jsUndefined());
}

// ------------------------------ TypeOfResolveNode -----------------------------------

RegisterID* TypeOfResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local, nullptr);
        if (dst == generator.ignoredResult())
            return 0;
        return generator.emitTypeOf(generator.finalDestination(dst), local);
    }

    RefPtr<RegisterID> scope = generator.emitResolveScope(dst, var);
    RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, DoNotThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, value.get(), nullptr);
    if (dst == generator.ignoredResult())
        return 0;
    return generator.emitTypeOf(generator.finalDestination(dst, scope.get()), value.get());
}

// ------------------------------ TypeOfValueNode -----------------------------------

RegisterID* TypeOfValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult()) {
        generator.emitNode(generator.ignoredResult(), m_expr);
        return 0;
    }
    RefPtr<RegisterID> src = generator.emitNode(m_expr);
    return generator.emitTypeOf(generator.finalDestination(dst), src.get());
}

// ------------------------------ PrefixNode ----------------------------------

RegisterID* PrefixNode::emitResolve(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr->isResolveNode());
    ResolveNode* resolve = static_cast<ResolveNode*>(m_expr);
    const Identifier& ident = resolve->identifier();

    Variable var = generator.variable(ident);
    if (RegisterID* local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local, nullptr);
        RefPtr<RegisterID> localReg = local;
        if (var.isReadOnly()) {
            generator.emitReadOnlyExceptionIfNeeded(var);
            localReg = generator.emitMove(generator.tempDestination(dst), localReg.get());
        } else if (generator.vm()->typeProfiler()) {
            RefPtr<RegisterID> tempDst = generator.tempDestination(dst);
            generator.emitMove(tempDst.get(), localReg.get());
            emitIncOrDec(generator, tempDst.get(), m_operator);
            generator.emitMove(localReg.get(), tempDst.get());
            generator.emitProfileType(localReg.get(), var, divotStart(), divotEnd());
            return generator.moveToDestinationIfNeeded(dst, tempDst.get());
        }
        emitIncOrDec(generator, localReg.get(), m_operator);
        return generator.moveToDestinationIfNeeded(dst, localReg.get());
    }

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RefPtr<RegisterID> scope = generator.emitResolveScope(dst, var);
    RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, value.get(), nullptr);
    if (var.isReadOnly()) {
        bool threwException = generator.emitReadOnlyExceptionIfNeeded(var);
        if (threwException)
            return value.get();
    }

    emitIncOrDec(generator, value.get(), m_operator);
    if (!var.isReadOnly()) {
        generator.emitPutToScope(scope.get(), var, value.get(), ThrowIfNotFound, NotInitialization);
        generator.emitProfileType(value.get(), var, divotStart(), divotEnd());
    }
    return generator.moveToDestinationIfNeeded(dst, value.get());
}

RegisterID* PrefixNode::emitBracket(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr->isBracketAccessorNode());
    BracketAccessorNode* bracketAccessor = static_cast<BracketAccessorNode*>(m_expr);
    ExpressionNode* baseNode = bracketAccessor->base();
    ExpressionNode* subscript = bracketAccessor->subscript();

    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(baseNode, bracketAccessor->subscriptHasAssignments(), subscript->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNode(subscript);
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);

    generator.emitExpressionInfo(bracketAccessor->divot(), bracketAccessor->divotStart(), bracketAccessor->divotEnd());
    RegisterID* value = generator.emitGetByVal(propDst.get(), base.get(), property.get());
    emitIncOrDec(generator, value, m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitPutByVal(base.get(), property.get(), value);
    generator.emitProfileType(value, divotStart(), divotEnd());
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

RegisterID* PrefixNode::emitDot(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr->isDotAccessorNode());
    DotAccessorNode* dotAccessor = static_cast<DotAccessorNode*>(m_expr);
    ExpressionNode* baseNode = dotAccessor->base();
    const Identifier& ident = dotAccessor->identifier();

    RefPtr<RegisterID> base = generator.emitNode(baseNode);
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);

    generator.emitExpressionInfo(dotAccessor->divot(), dotAccessor->divotStart(), dotAccessor->divotEnd());
    RegisterID* value = generator.emitGetById(propDst.get(), base.get(), ident);
    emitIncOrDec(generator, value, m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitPutById(base.get(), ident, value);
    generator.emitProfileType(value, divotStart(), divotEnd());
    return generator.moveToDestinationIfNeeded(dst, propDst.get());
}

RegisterID* PrefixNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr->isResolveNode())
        return emitResolve(generator, dst);

    if (m_expr->isBracketAccessorNode())
        return emitBracket(generator, dst);

    if (m_expr->isDotAccessorNode())
        return emitDot(generator, dst);

    return emitThrowReferenceError(generator, m_operator == OpPlusPlus
        ? ASCIILiteral("Prefix ++ operator applied to value that is not a reference.")
        : ASCIILiteral("Prefix -- operator applied to value that is not a reference."));
}

// ------------------------------ Unary Operation Nodes -----------------------------------

RegisterID* UnaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src = generator.emitNode(m_expr);
    generator.emitExpressionInfo(position(), position(), position());
    return generator.emitUnaryOp(opcodeID(), generator.finalDestination(dst), src.get());
}

// ------------------------------ BitwiseNotNode -----------------------------------
 
RegisterID* BitwiseNotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src2 = generator.emitLoad(generator.newTemporary(), jsNumber(-1));
    RefPtr<RegisterID> src1 = generator.emitNode(m_expr);
    return generator.emitBinaryOp(op_bitxor, generator.finalDestination(dst, src1.get()), src1.get(), src2.get(), OperandTypes(m_expr->resultDescriptor(), ResultType::numberTypeIsInt32()));
}
 
// ------------------------------ LogicalNotNode -----------------------------------

void LogicalNotNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label* trueTarget, Label* falseTarget, FallThroughMode fallThroughMode)
{
    // reverse the true and false targets
    generator.emitNodeInConditionContext(expr(), falseTarget, trueTarget, invert(fallThroughMode));
}


// ------------------------------ Binary Operation Nodes -----------------------------------

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
RegisterID* BinaryOpNode::emitStrcat(BytecodeGenerator& generator, RegisterID* dst, RegisterID* lhs, ReadModifyResolveNode* emitExpressionInfoForMe)
{
    ASSERT(isAdd());
    ASSERT(resultDescriptor().definitelyIsString());

    // Create a list of expressions for all the adds in the tree of nodes we can convert into
    // a string concatenation.  The rightmost node (c) is added first.  The rightmost node is
    // added first, and the leftmost child is never added, so the vector produced for the
    // example above will be [ c, b ].
    Vector<ExpressionNode*, 16> reverseExpressionList;
    reverseExpressionList.append(m_expr2);

    // Examine the left child of the add.  So long as this is a string add, add its right-child
    // to the list, and keep processing along the left fork.
    ExpressionNode* leftMostAddChild = m_expr1;
    while (leftMostAddChild->isAdd() && leftMostAddChild->resultDescriptor().definitelyIsString()) {
        reverseExpressionList.append(static_cast<AddNode*>(leftMostAddChild)->m_expr2);
        leftMostAddChild = static_cast<AddNode*>(leftMostAddChild)->m_expr1;
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

    // Certain read-modify nodes require expression info to be emitted *after* m_right has been generated.
    // If this is required the node is passed as 'emitExpressionInfoForMe'; do so now.
    if (emitExpressionInfoForMe)
        generator.emitExpressionInfo(emitExpressionInfoForMe->divot(), emitExpressionInfoForMe->divotStart(), emitExpressionInfoForMe->divotEnd());
    // If there is an assignment convert the lhs now.  This will also copy lhs to
    // the temporary register we allocated for it.
    if (lhs)
        generator.emitToPrimitive(temporaryRegisters[0].get(), lhs);

    return generator.emitStrcat(generator.finalDestination(dst, temporaryRegisters[0].get()), temporaryRegisters[0].get(), temporaryRegisters.size());
}

void BinaryOpNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label* trueTarget, Label* falseTarget, FallThroughMode fallThroughMode)
{
    TriState branchCondition;
    ExpressionNode* branchExpression;
    tryFoldToBranch(generator, branchCondition, branchExpression);

    if (branchCondition == MixedTriState)
        ExpressionNode::emitBytecodeInConditionContext(generator, trueTarget, falseTarget, fallThroughMode);
    else if (branchCondition == TrueTriState)
        generator.emitNodeInConditionContext(branchExpression, trueTarget, falseTarget, fallThroughMode);
    else
        generator.emitNodeInConditionContext(branchExpression, falseTarget, trueTarget, invert(fallThroughMode));
}

static inline bool canFoldToBranch(OpcodeID opcodeID, ExpressionNode* branchExpression, JSValue constant)
{
    ResultType expressionType = branchExpression->resultDescriptor();

    if (expressionType.definitelyIsBoolean() && constant.isBoolean())
        return true;
    else if (expressionType.definitelyIsBoolean() && constant.isInt32() && (constant.asInt32() == 0 || constant.asInt32() == 1))
        return opcodeID == op_eq || opcodeID == op_neq; // Strict equality is false in the case of type mismatch.
    else if (expressionType.isInt32() && constant.isInt32() && constant.asInt32() == 0)
        return true;

    return false;
}

void BinaryOpNode::tryFoldToBranch(BytecodeGenerator& generator, TriState& branchCondition, ExpressionNode*& branchExpression)
{
    branchCondition = MixedTriState;
    branchExpression = 0;

    ConstantNode* constant = 0;
    if (m_expr1->isConstant()) {
        constant = static_cast<ConstantNode*>(m_expr1);
        branchExpression = m_expr2;
    } else if (m_expr2->isConstant()) {
        constant = static_cast<ConstantNode*>(m_expr2);
        branchExpression = m_expr1;
    }

    if (!constant)
        return;
    ASSERT(branchExpression);

    OpcodeID opcodeID = this->opcodeID();
    JSValue value = constant->jsValue(generator);
    bool canFoldToBranch = JSC::canFoldToBranch(opcodeID, branchExpression, value);
    if (!canFoldToBranch)
        return;

    if (opcodeID == op_eq || opcodeID == op_stricteq)
        branchCondition = triState(value.pureToBoolean());
    else if (opcodeID == op_neq || opcodeID == op_nstricteq)
        branchCondition = triState(!value.pureToBoolean());
}

RegisterID* BinaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    OpcodeID opcodeID = this->opcodeID();

    if (opcodeID == op_add && m_expr1->isAdd() && m_expr1->resultDescriptor().definitelyIsString()) {
        generator.emitExpressionInfo(position(), position(), position());
        return emitStrcat(generator, dst);
    }

    if (opcodeID == op_neq) {
        if (m_expr1->isNull() || m_expr2->isNull()) {
            RefPtr<RegisterID> src = generator.tempDestination(dst);
            generator.emitNode(src.get(), m_expr1->isNull() ? m_expr2 : m_expr1);
            return generator.emitUnaryOp(op_neq_null, generator.finalDestination(dst, src.get()), src.get());
        }
    }

    ExpressionNode* left = m_expr1;
    ExpressionNode* right = m_expr2;
    if (opcodeID == op_neq || opcodeID == op_nstricteq) {
        if (left->isString())
            std::swap(left, right);
    }

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(left, m_rightHasAssignments, right->isPure(generator));
    bool wasTypeof = generator.lastOpcodeID() == op_typeof;
    RefPtr<RegisterID> src2 = generator.emitNode(right);
    generator.emitExpressionInfo(position(), position(), position());
    if (wasTypeof && (opcodeID == op_neq || opcodeID == op_nstricteq)) {
        RefPtr<RegisterID> tmp = generator.tempDestination(dst);
        if (opcodeID == op_neq)
            generator.emitEqualityOp(op_eq, generator.finalDestination(tmp.get(), src1.get()), src1.get(), src2.get());
        else if (opcodeID == op_nstricteq)
            generator.emitEqualityOp(op_stricteq, generator.finalDestination(tmp.get(), src1.get()), src1.get(), src2.get());
        else
            RELEASE_ASSERT_NOT_REACHED();
        return generator.emitUnaryOp(op_not, generator.finalDestination(dst, tmp.get()), tmp.get());
    }
    RegisterID* result = generator.emitBinaryOp(opcodeID, generator.finalDestination(dst, src1.get()), src1.get(), src2.get(), OperandTypes(left->resultDescriptor(), right->resultDescriptor()));
    if (opcodeID == op_urshift && dst != generator.ignoredResult())
        return generator.emitUnaryOp(op_unsigned, result, result);
    return result;
}

RegisterID* EqualNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr1->isNull() || m_expr2->isNull()) {
        RefPtr<RegisterID> src = generator.tempDestination(dst);
        generator.emitNode(src.get(), m_expr1->isNull() ? m_expr2 : m_expr1);
        return generator.emitUnaryOp(op_eq_null, generator.finalDestination(dst, src.get()), src.get());
    }

    ExpressionNode* left = m_expr1;
    ExpressionNode* right = m_expr2;
    if (left->isString())
        std::swap(left, right);

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(left, m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> src2 = generator.emitNode(right);
    return generator.emitEqualityOp(op_eq, generator.finalDestination(dst, src1.get()), src1.get(), src2.get());
}

RegisterID* StrictEqualNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ExpressionNode* left = m_expr1;
    ExpressionNode* right = m_expr2;
    if (left->isString())
        std::swap(left, right);

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(left, m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> src2 = generator.emitNode(right);
    return generator.emitEqualityOp(op_stricteq, generator.finalDestination(dst, src1.get()), src1.get(), src2.get());
}

RegisterID* ThrowableBinaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(m_expr1, m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> src2 = generator.emitNode(m_expr2);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    return generator.emitBinaryOp(opcodeID(), generator.finalDestination(dst, src1.get()), src1.get(), src2.get(), OperandTypes(m_expr1->resultDescriptor(), m_expr2->resultDescriptor()));
}

RegisterID* InstanceOfNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> hasInstanceValue = generator.newTemporary();
    RefPtr<RegisterID> isObject = generator.newTemporary();
    RefPtr<RegisterID> isCustom = generator.newTemporary();
    RefPtr<RegisterID> prototype = generator.newTemporary();
    RefPtr<RegisterID> value = generator.emitNodeForLeftHandSide(m_expr1, m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> constructor = generator.emitNode(m_expr2);
    RefPtr<RegisterID> dstReg = generator.finalDestination(dst, value.get());
    RefPtr<Label> custom = generator.newLabel();
    RefPtr<Label> done = generator.newLabel();
    RefPtr<Label> typeError = generator.newLabel();

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitIsObject(isObject.get(), constructor.get());
    generator.emitJumpIfFalse(isObject.get(), typeError.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitGetById(hasInstanceValue.get(), constructor.get(), generator.vm()->propertyNames->hasInstanceSymbol);

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitOverridesHasInstance(isCustom.get(), constructor.get(), hasInstanceValue.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitJumpIfTrue(isCustom.get(), custom.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitGetById(prototype.get(), constructor.get(), generator.vm()->propertyNames->prototype);

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitInstanceOf(dstReg.get(), value.get(), prototype.get());

    generator.emitJump(done.get());

    generator.emitLabel(typeError.get());
    generator.emitThrowTypeError("Right hand side of instanceof is not an object");

    generator.emitLabel(custom.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitInstanceOfCustom(dstReg.get(), value.get(), constructor.get(), hasInstanceValue.get());

    generator.emitLabel(done.get());

    return dstReg.get();
}

// ------------------------------ LogicalOpNode ----------------------------

RegisterID* LogicalOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    RefPtr<Label> target = generator.newLabel();
    
    generator.emitNode(temp.get(), m_expr1);
    if (m_operator == OpLogicalAnd)
        generator.emitJumpIfFalse(temp.get(), target.get());
    else
        generator.emitJumpIfTrue(temp.get(), target.get());
    generator.emitNodeInTailPosition(temp.get(), m_expr2);
    generator.emitLabel(target.get());

    return generator.moveToDestinationIfNeeded(dst, temp.get());
}

void LogicalOpNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label* trueTarget, Label* falseTarget, FallThroughMode fallThroughMode)
{
    RefPtr<Label> afterExpr1 = generator.newLabel();
    if (m_operator == OpLogicalAnd)
        generator.emitNodeInConditionContext(m_expr1, afterExpr1.get(), falseTarget, FallThroughMeansTrue);
    else 
        generator.emitNodeInConditionContext(m_expr1, trueTarget, afterExpr1.get(), FallThroughMeansFalse);
    generator.emitLabel(afterExpr1.get());

    generator.emitNodeInConditionContext(m_expr2, trueTarget, falseTarget, fallThroughMode);
}

// ------------------------------ ConditionalNode ------------------------------

RegisterID* ConditionalNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newDst = generator.finalDestination(dst);
    RefPtr<Label> beforeElse = generator.newLabel();
    RefPtr<Label> afterElse = generator.newLabel();

    RefPtr<Label> beforeThen = generator.newLabel();
    generator.emitNodeInConditionContext(m_logical, beforeThen.get(), beforeElse.get(), FallThroughMeansTrue);
    generator.emitLabel(beforeThen.get());

    generator.emitProfileControlFlow(m_expr1->startOffset());
    generator.emitNodeInTailPosition(newDst.get(), m_expr1);
    generator.emitJump(afterElse.get());

    generator.emitLabel(beforeElse.get());
    generator.emitProfileControlFlow(m_expr1->endOffset() + 1);
    generator.emitNodeInTailPosition(newDst.get(), m_expr2);

    generator.emitLabel(afterElse.get());

    generator.emitProfileControlFlow(m_expr2->endOffset() + 1);

    return newDst.get();
}

// ------------------------------ ReadModifyResolveNode -----------------------------------

// FIXME: should this be moved to be a method on BytecodeGenerator?
static ALWAYS_INLINE RegisterID* emitReadModifyAssignment(BytecodeGenerator& generator, RegisterID* dst, RegisterID* src1, ExpressionNode* m_right, Operator oper, OperandTypes types, ReadModifyResolveNode* emitExpressionInfoForMe = 0)
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
            if (m_right->isAdd() && m_right->resultDescriptor().definitelyIsString())
                return static_cast<AddNode*>(m_right)->emitStrcat(generator, dst, src1, emitExpressionInfoForMe);
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
            RELEASE_ASSERT_NOT_REACHED();
            return dst;
    }

    RegisterID* src2 = generator.emitNode(m_right);

    // Certain read-modify nodes require expression info to be emitted *after* m_right has been generated.
    // If this is required the node is passed as 'emitExpressionInfoForMe'; do so now.
    if (emitExpressionInfoForMe)
        generator.emitExpressionInfo(emitExpressionInfoForMe->divot(), emitExpressionInfoForMe->divotStart(), emitExpressionInfoForMe->divotEnd());
    RegisterID* result = generator.emitBinaryOp(opcodeID, dst, src1, src2, types);
    if (oper == OpURShift)
        return generator.emitUnaryOp(op_unsigned, result, result);
    return result;
}

RegisterID* ReadModifyResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    JSTextPosition newDivot = divotStart() + m_ident.length();
    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local, nullptr);
        if (var.isReadOnly()) {
            generator.emitReadOnlyExceptionIfNeeded(var);
            RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst), local, m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
            generator.emitProfileType(result, divotStart(), divotEnd());
            return result;
        }
        
        if (generator.leftHandSideNeedsCopy(m_rightHasAssignments, m_right->isPure(generator))) {
            RefPtr<RegisterID> result = generator.newTemporary();
            generator.emitMove(result.get(), local);
            emitReadModifyAssignment(generator, result.get(), result.get(), m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
            generator.emitMove(local, result.get());
            generator.invalidateForInContextForLocal(local);
            generator.emitProfileType(local, divotStart(), divotEnd());
            return generator.moveToDestinationIfNeeded(dst, result.get());
        }
        
        RegisterID* result = emitReadModifyAssignment(generator, local, local, m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
        generator.invalidateForInContextForLocal(local);
        generator.emitProfileType(result, divotStart(), divotEnd());
        return generator.moveToDestinationIfNeeded(dst, result);
    }

    generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, value.get(), nullptr);
    if (var.isReadOnly()) {
        bool threwException = generator.emitReadOnlyExceptionIfNeeded(var);
        if (threwException)
            return value.get();
    }
    RefPtr<RegisterID> result = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()), this);
    RegisterID* returnResult = result.get();
    if (!var.isReadOnly()) {
        returnResult = generator.emitPutToScope(scope.get(), var, result.get(), ThrowIfNotFound, NotInitialization);
        generator.emitProfileType(result.get(), var, divotStart(), divotEnd());
    }
    return returnResult;
}

// ------------------------------ AssignResolveNode -----------------------------------

RegisterID* AssignResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    Variable var = generator.variable(m_ident);
    bool isReadOnly = var.isReadOnly() && m_assignmentContext != AssignmentContext::ConstDeclarationStatement;
    if (RegisterID* local = var.local()) {
        RegisterID* result = nullptr;
        if (m_assignmentContext == AssignmentContext::AssignmentExpression)
            generator.emitTDZCheckIfNecessary(var, local, nullptr);

        if (isReadOnly) {
            result = generator.emitNode(dst, m_right); // Execute side effects first.
            generator.emitReadOnlyExceptionIfNeeded(var);
            generator.emitProfileType(result, var, divotStart(), divotEnd());
        } else if (var.isSpecial()) {
            RefPtr<RegisterID> tempDst = generator.tempDestination(dst);
            generator.emitNode(tempDst.get(), m_right);
            generator.emitMove(local, tempDst.get());
            generator.emitProfileType(local, var, divotStart(), divotEnd());
            generator.invalidateForInContextForLocal(local);
            result = generator.moveToDestinationIfNeeded(dst, tempDst.get());
        } else {
            RegisterID* right = generator.emitNode(local, m_right);
            generator.emitProfileType(right, var, divotStart(), divotEnd());
            generator.invalidateForInContextForLocal(local);
            result = generator.moveToDestinationIfNeeded(dst, right);
        }

        if (m_assignmentContext == AssignmentContext::DeclarationStatement || m_assignmentContext == AssignmentContext::ConstDeclarationStatement)
            generator.liftTDZCheckIfPossible(var);
        return result;
    }

    if (generator.isStrictMode())
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    if (m_assignmentContext == AssignmentContext::AssignmentExpression)
        generator.emitTDZCheckIfNecessary(var, nullptr, scope.get());
    if (dst == generator.ignoredResult())
        dst = 0;
    RefPtr<RegisterID> result = generator.emitNode(dst, m_right);
    if (isReadOnly) {
        RegisterID* result = generator.emitNode(dst, m_right); // Execute side effects first.
        bool threwException = generator.emitReadOnlyExceptionIfNeeded(var);
        if (threwException)
            return result;
    }
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* returnResult = result.get();
    if (!isReadOnly) {
        returnResult = generator.emitPutToScope(scope.get(), var, result.get(), generator.isStrictMode() ? ThrowIfNotFound : DoNotThrowIfNotFound, 
            m_assignmentContext == AssignmentContext::ConstDeclarationStatement || m_assignmentContext == AssignmentContext::DeclarationStatement  ? Initialization : NotInitialization);
        generator.emitProfileType(result.get(), var, divotStart(), divotEnd());
    }

    if (m_assignmentContext == AssignmentContext::DeclarationStatement || m_assignmentContext == AssignmentContext::ConstDeclarationStatement)
        generator.liftTDZCheckIfPossible(var);
    return returnResult;
}

// ------------------------------ AssignDotNode -----------------------------------

RegisterID* AssignDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RefPtr<RegisterID> result = generator.emitNode(value.get(), m_right);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* forwardResult = (dst == generator.ignoredResult()) ? result.get() : generator.moveToDestinationIfNeeded(generator.tempDestination(result.get()), result.get());
    generator.emitPutById(base.get(), m_ident, forwardResult);
    generator.emitProfileType(forwardResult, divotStart(), divotEnd());
    return generator.moveToDestinationIfNeeded(dst, forwardResult);
}

// ------------------------------ ReadModifyDotNode -----------------------------------

RegisterID* ReadModifyDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    RefPtr<RegisterID> value = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), m_right, static_cast<JSC::Operator>(m_operator), OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* ret = generator.emitPutById(base.get(), m_ident, updatedValue);
    generator.emitProfileType(updatedValue, divotStart(), divotEnd());
    return ret;
}

// ------------------------------ AssignErrorNode -----------------------------------

RegisterID* AssignErrorNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    return emitThrowReferenceError(generator, ASCIILiteral("Left side of assignment is not a reference."));
}

// ------------------------------ AssignBracketNode -----------------------------------

RegisterID* AssignBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript, m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RefPtr<RegisterID> result = generator.emitNode(value.get(), m_right);

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* forwardResult = (dst == generator.ignoredResult()) ? result.get() : generator.moveToDestinationIfNeeded(generator.tempDestination(result.get()), result.get());

    if (isNonIndexStringElement(*m_subscript))
        generator.emitPutById(base.get(), static_cast<StringNode*>(m_subscript)->value(), forwardResult);
    else
        generator.emitPutByVal(base.get(), property.get(), forwardResult);

    generator.emitProfileType(forwardResult, divotStart(), divotEnd());
    return generator.moveToDestinationIfNeeded(dst, forwardResult);
}

// ------------------------------ ReadModifyBracketNode -----------------------------------

RegisterID* ReadModifyBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(m_subscript, m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    RefPtr<RegisterID> value = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), m_right, static_cast<JSC::Operator>(m_operator), OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitPutByVal(base.get(), property.get(), updatedValue);
    generator.emitProfileType(updatedValue, divotStart(), divotEnd());

    return updatedValue;
}

// ------------------------------ CommaNode ------------------------------------

RegisterID* CommaNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    CommaNode* node = this;
    for (; node && node->next(); node = node->next())
        generator.emitNode(generator.ignoredResult(), node->m_expr);
    return generator.emitNodeInTailPosition(dst, node->m_expr);
}

// ------------------------------ SourceElements -------------------------------


inline StatementNode* SourceElements::lastStatement() const
{
    return m_tail;
}

inline void SourceElements::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    for (StatementNode* statement = m_head; statement; statement = statement->next())
        generator.emitNodeInTailPosition(dst, statement);
}

// ------------------------------ BlockNode ------------------------------------

inline StatementNode* BlockNode::lastStatement() const
{
    return m_statements ? m_statements->lastStatement() : 0;
}

StatementNode* BlockNode::singleStatement() const
{
    return m_statements ? m_statements->singleStatement() : 0;
}

void BlockNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_statements)
        return;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested);
    m_statements->emitBytecode(generator, dst);
    generator.popLexicalScope(this);
}

// ------------------------------ EmptyStatementNode ---------------------------

void EmptyStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
}

// ------------------------------ DebuggerStatementNode ---------------------------

void DebuggerStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(DidReachBreakpoint, lastLine(), startOffset(), lineStartOffset());
}

// ------------------------------ ExprStatementNode ----------------------------

void ExprStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    generator.emitNode(dst, m_expr);
}

// ------------------------------ DeclarationStatement ----------------------------

void DeclarationStatement::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    ASSERT(m_expr);
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    generator.emitNode(m_expr);
}

// ------------------------------ EmptyVarExpression ----------------------------

RegisterID* EmptyVarExpression::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    // It's safe to return null here because this node will always be a child node of DeclarationStatement which ignores our return value.
    if (!generator.vm()->typeProfiler())
        return nullptr;

    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local())
        generator.emitProfileType(local, var, position(), JSTextPosition(-1, position().offset + m_ident.length(), -1));
    else {
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, DoNotThrowIfNotFound);
        generator.emitProfileType(value.get(), var, position(), JSTextPosition(-1, position().offset + m_ident.length(), -1));
    }

    return nullptr;
}

// ------------------------------ EmptyLetExpression ----------------------------

RegisterID* EmptyLetExpression::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    // Lexical declarations like 'let' must move undefined into their variables so we don't
    // get TDZ errors for situations like this: `let x; x;`
    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local()) {
        generator.emitLoad(local, jsUndefined());
        generator.emitProfileType(local, var, position(), JSTextPosition(-1, position().offset + m_ident.length(), -1));
    } else {
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> value = generator.emitLoad(nullptr, jsUndefined());
        generator.emitPutToScope(scope.get(), var, value.get(), generator.isStrictMode() ? ThrowIfNotFound : DoNotThrowIfNotFound, Initialization);
        generator.emitProfileType(value.get(), var, position(), JSTextPosition(-1, position().offset + m_ident.length(), -1)); 
    }

    // It's safe to return null here because this node will always be a child node of DeclarationStatement which ignores our return value.
    return nullptr;
}

// ------------------------------ IfElseNode ---------------------------------------

static inline StatementNode* singleStatement(StatementNode* statementNode)
{
    if (statementNode->isBlock())
        return static_cast<BlockNode*>(statementNode)->singleStatement();
    return statementNode;
}

bool IfElseNode::tryFoldBreakAndContinue(BytecodeGenerator& generator, StatementNode* ifBlock,
    Label*& trueTarget, FallThroughMode& fallThroughMode)
{
    StatementNode* singleStatement = JSC::singleStatement(ifBlock);
    if (!singleStatement)
        return false;

    if (singleStatement->isBreak()) {
        BreakNode* breakNode = static_cast<BreakNode*>(singleStatement);
        Label* target = breakNode->trivialTarget(generator);
        if (!target)
            return false;
        trueTarget = target;
        fallThroughMode = FallThroughMeansFalse;
        return true;
    }

    if (singleStatement->isContinue()) {
        ContinueNode* continueNode = static_cast<ContinueNode*>(singleStatement);
        Label* target = continueNode->trivialTarget(generator);
        if (!target)
            return false;
        trueTarget = target;
        fallThroughMode = FallThroughMeansFalse;
        return true;
    }

    return false;
}

void IfElseNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    
    RefPtr<Label> beforeThen = generator.newLabel();
    RefPtr<Label> beforeElse = generator.newLabel();
    RefPtr<Label> afterElse = generator.newLabel();

    Label* trueTarget = beforeThen.get();
    Label* falseTarget = beforeElse.get();
    FallThroughMode fallThroughMode = FallThroughMeansTrue;
    bool didFoldIfBlock = tryFoldBreakAndContinue(generator, m_ifBlock, trueTarget, fallThroughMode);

    generator.emitNodeInConditionContext(m_condition, trueTarget, falseTarget, fallThroughMode);
    generator.emitLabel(beforeThen.get());
    generator.emitProfileControlFlow(m_ifBlock->startOffset());

    if (!didFoldIfBlock) {
        generator.emitNodeInTailPosition(dst, m_ifBlock);
        if (m_elseBlock)
            generator.emitJump(afterElse.get());
    }

    generator.emitLabel(beforeElse.get());

    if (m_elseBlock) {
        generator.emitProfileControlFlow(m_ifBlock->endOffset() + (m_ifBlock->isBlock() ? 1 : 0));
        generator.emitNodeInTailPosition(dst, m_elseBlock);
    }

    generator.emitLabel(afterElse.get());
    StatementNode* endingBlock = m_elseBlock ? m_elseBlock : m_ifBlock;
    generator.emitProfileControlFlow(endingBlock->endOffset() + (endingBlock->isBlock() ? 1 : 0));
}

// ------------------------------ DoWhileNode ----------------------------------

void DoWhileNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    LabelScopePtr scope = generator.newLabelScope(LabelScope::Loop);

    RefPtr<Label> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());
    generator.emitLoopHint();
    generator.emitDebugHook(WillExecuteStatement, lastLine(), startOffset(), lineStartOffset());

    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(scope->continueTarget());
    generator.emitDebugHook(WillExecuteStatement, lastLine(), startOffset(), lineStartOffset());
    generator.emitNodeInConditionContext(m_expr, topOfLoop.get(), scope->breakTarget(), FallThroughMeansFalse);

    generator.emitLabel(scope->breakTarget());
}

// ------------------------------ WhileNode ------------------------------------

void WhileNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    LabelScopePtr scope = generator.newLabelScope(LabelScope::Loop);
    RefPtr<Label> topOfLoop = generator.newLabel();

    generator.emitDebugHook(WillExecuteStatement, m_expr->firstLine(), m_expr->startOffset(), m_expr->lineStartOffset());
    generator.emitNodeInConditionContext(m_expr, topOfLoop.get(), scope->breakTarget(), FallThroughMeansTrue);

    generator.emitLabel(topOfLoop.get());
    generator.emitLoopHint();
    
    generator.emitProfileControlFlow(m_statement->startOffset());
    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(scope->continueTarget());
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());

    generator.emitNodeInConditionContext(m_expr, topOfLoop.get(), scope->breakTarget(), FallThroughMeansFalse);

    generator.emitLabel(scope->breakTarget());

    generator.emitProfileControlFlow(m_statement->endOffset() + (m_statement->isBlock() ? 1 : 0));
}

// ------------------------------ ForNode --------------------------------------

void ForNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    LabelScopePtr scope = generator.newLabelScope(LabelScope::Loop);

    RegisterID* forLoopSymbolTable = nullptr;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested, &forLoopSymbolTable);

    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());

    if (m_expr1)
        generator.emitNode(generator.ignoredResult(), m_expr1);
    
    RefPtr<Label> topOfLoop = generator.newLabel();
    if (m_expr2)
        generator.emitNodeInConditionContext(m_expr2, topOfLoop.get(), scope->breakTarget(), FallThroughMeansTrue);

    generator.emitLabel(topOfLoop.get());
    generator.emitLoopHint();
    generator.emitProfileControlFlow(m_statement->startOffset());

    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(scope->continueTarget());
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    generator.prepareLexicalScopeForNextForLoopIteration(this, forLoopSymbolTable);
    if (m_expr3)
        generator.emitNode(generator.ignoredResult(), m_expr3);

    if (m_expr2)
        generator.emitNodeInConditionContext(m_expr2, topOfLoop.get(), scope->breakTarget(), FallThroughMeansFalse);
    else
        generator.emitJump(topOfLoop.get());

    generator.emitLabel(scope->breakTarget());
    generator.popLexicalScope(this);
    generator.emitProfileControlFlow(m_statement->endOffset() + (m_statement->isBlock() ? 1 : 0));
}

// ------------------------------ ForInNode ------------------------------------

RegisterID* ForInNode::tryGetBoundLocal(BytecodeGenerator& generator)
{
    if (m_lexpr->isResolveNode()) {
        const Identifier& ident = static_cast<ResolveNode*>(m_lexpr)->identifier();
        return generator.variable(ident).local();
    }

    if (m_lexpr->isDestructuringNode()) {
        DestructuringAssignmentNode* assignNode = static_cast<DestructuringAssignmentNode*>(m_lexpr);
        auto binding = assignNode->bindings();
        if (!binding->isBindingNode())
            return nullptr;

        auto simpleBinding = static_cast<BindingNode*>(binding);
        const Identifier& ident = simpleBinding->boundProperty();
        Variable var = generator.variable(ident);
        if (var.isSpecial())
            return nullptr;
        return var.local();
    }

    return nullptr;
}

void ForInNode::emitLoopHeader(BytecodeGenerator& generator, RegisterID* propertyName)
{
    if (m_lexpr->isResolveNode()) {
        const Identifier& ident = static_cast<ResolveNode*>(m_lexpr)->identifier();
        Variable var = generator.variable(ident);
        if (RegisterID* local = var.local())
            generator.emitMove(local, propertyName);
        else {
            if (generator.isStrictMode())
                generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
            RegisterID* scope = generator.emitResolveScope(nullptr, var);
            generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
            generator.emitPutToScope(scope, var, propertyName, generator.isStrictMode() ? ThrowIfNotFound : DoNotThrowIfNotFound, NotInitialization);
        }
        generator.emitProfileType(propertyName, var, m_lexpr->position(), JSTextPosition(-1, m_lexpr->position().offset + ident.length(), -1));
        return;
    }
    if (m_lexpr->isDotAccessorNode()) {
        DotAccessorNode* assignNode = static_cast<DotAccessorNode*>(m_lexpr);
        const Identifier& ident = assignNode->identifier();
        RegisterID* base = generator.emitNode(assignNode->base());
        generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
        generator.emitPutById(base, ident, propertyName);
        generator.emitProfileType(propertyName, assignNode->divotStart(), assignNode->divotEnd());
        return;
    }
    if (m_lexpr->isBracketAccessorNode()) {
        BracketAccessorNode* assignNode = static_cast<BracketAccessorNode*>(m_lexpr);
        RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
        RegisterID* subscript = generator.emitNode(assignNode->subscript());
        generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
        generator.emitPutByVal(base.get(), subscript, propertyName);
        generator.emitProfileType(propertyName, assignNode->divotStart(), assignNode->divotEnd());
        return;
    }

    if (m_lexpr->isDestructuringNode()) {
        DestructuringAssignmentNode* assignNode = static_cast<DestructuringAssignmentNode*>(m_lexpr);
        auto binding = assignNode->bindings();
        if (!binding->isBindingNode()) {
            assignNode->bindings()->bindValue(generator, propertyName);
            return;
        }

        auto simpleBinding = static_cast<BindingNode*>(binding);
        const Identifier& ident = simpleBinding->boundProperty();
        Variable var = generator.variable(ident);
        if (!var.local() || var.isSpecial()) {
            assignNode->bindings()->bindValue(generator, propertyName);
            return;
        }
        generator.emitMove(var.local(), propertyName);
        generator.emitProfileType(propertyName, var, simpleBinding->divotStart(), simpleBinding->divotEnd());
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void ForInNode::emitMultiLoopBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_lexpr->isAssignmentLocation()) {
        emitThrowReferenceError(generator, ASCIILiteral("Left side of for-in statement is not a reference."));
        return;
    }

    RefPtr<Label> end = generator.newLabel();

    RegisterID* forLoopSymbolTable = nullptr;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested, &forLoopSymbolTable);

    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());

    RefPtr<RegisterID> base = generator.newTemporary();
    RefPtr<RegisterID> length;
    RefPtr<RegisterID> enumerator;
    generator.emitNode(base.get(), m_expr);
    RefPtr<RegisterID> local = this->tryGetBoundLocal(generator);
    RefPtr<RegisterID> enumeratorIndex;

    int profilerStartOffset = m_statement->startOffset();
    int profilerEndOffset = m_statement->endOffset() + (m_statement->isBlock() ? 1 : 0);

    enumerator = generator.emitGetPropertyEnumerator(generator.newTemporary(), base.get());

    // Indexed property loop.
    {
        LabelScopePtr scope = generator.newLabelScope(LabelScope::Loop);
        RefPtr<Label> loopStart = generator.newLabel();
        RefPtr<Label> loopEnd = generator.newLabel();

        length = generator.emitGetEnumerableLength(generator.newTemporary(), enumerator.get());
        RefPtr<RegisterID> i = generator.emitLoad(generator.newTemporary(), jsNumber(0));
        RefPtr<RegisterID> propertyName = generator.newTemporary();

        generator.emitLabel(loopStart.get());
        generator.emitLoopHint();

        RefPtr<RegisterID> result = generator.emitEqualityOp(op_less, generator.newTemporary(), i.get(), length.get());
        generator.emitJumpIfFalse(result.get(), loopEnd.get());
        generator.emitHasIndexedProperty(result.get(), base.get(), i.get());
        generator.emitJumpIfFalse(result.get(), scope->continueTarget());

        generator.emitToIndexString(propertyName.get(), i.get());
        this->emitLoopHeader(generator, propertyName.get());

        generator.emitProfileControlFlow(profilerStartOffset);

        generator.pushIndexedForInScope(local.get(), i.get());
        generator.emitNode(dst, m_statement);
        generator.popIndexedForInScope(local.get());

        generator.emitProfileControlFlow(profilerEndOffset);

        generator.emitLabel(scope->continueTarget());
        generator.prepareLexicalScopeForNextForLoopIteration(this, forLoopSymbolTable);
        generator.emitInc(i.get());
        generator.emitJump(loopStart.get());

        generator.emitLabel(scope->breakTarget());
        generator.emitJump(end.get());
        generator.emitLabel(loopEnd.get());
    }

    // Structure property loop.
    {
        LabelScopePtr scope = generator.newLabelScope(LabelScope::Loop);
        RefPtr<Label> loopStart = generator.newLabel();
        RefPtr<Label> loopEnd = generator.newLabel();

        enumeratorIndex = generator.emitLoad(generator.newTemporary(), jsNumber(0));
        RefPtr<RegisterID> propertyName = generator.newTemporary();
        generator.emitEnumeratorStructurePropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());

        generator.emitLabel(loopStart.get());
        generator.emitLoopHint();

        RefPtr<RegisterID> result = generator.emitUnaryOp(op_eq_null, generator.newTemporary(), propertyName.get());
        generator.emitJumpIfTrue(result.get(), loopEnd.get());
        generator.emitHasStructureProperty(result.get(), base.get(), propertyName.get(), enumerator.get());
        generator.emitJumpIfFalse(result.get(), scope->continueTarget());

        this->emitLoopHeader(generator, propertyName.get());

        generator.emitProfileControlFlow(profilerStartOffset);

        generator.pushStructureForInScope(local.get(), enumeratorIndex.get(), propertyName.get(), enumerator.get());
        generator.emitNode(dst, m_statement);
        generator.popStructureForInScope(local.get());

        generator.emitProfileControlFlow(profilerEndOffset);

        generator.emitLabel(scope->continueTarget());
        generator.prepareLexicalScopeForNextForLoopIteration(this, forLoopSymbolTable);
        generator.emitInc(enumeratorIndex.get());
        generator.emitEnumeratorStructurePropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());
        generator.emitJump(loopStart.get());
        
        generator.emitLabel(scope->breakTarget());
        generator.emitJump(end.get());
        generator.emitLabel(loopEnd.get());
    }

    // Generic property loop.
    {
        LabelScopePtr scope = generator.newLabelScope(LabelScope::Loop);
        RefPtr<Label> loopStart = generator.newLabel();
        RefPtr<Label> loopEnd = generator.newLabel();

        RefPtr<RegisterID> propertyName = generator.newTemporary();

        generator.emitEnumeratorGenericPropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());

        generator.emitLabel(loopStart.get());
        generator.emitLoopHint();

        RefPtr<RegisterID> result = generator.emitUnaryOp(op_eq_null, generator.newTemporary(), propertyName.get());
        generator.emitJumpIfTrue(result.get(), loopEnd.get());

        generator.emitHasGenericProperty(result.get(), base.get(), propertyName.get());
        generator.emitJumpIfFalse(result.get(), scope->continueTarget());

        this->emitLoopHeader(generator, propertyName.get());

        generator.emitProfileControlFlow(profilerStartOffset);

        generator.emitNode(dst, m_statement);

        generator.emitLabel(scope->continueTarget());
        generator.prepareLexicalScopeForNextForLoopIteration(this, forLoopSymbolTable);
        generator.emitInc(enumeratorIndex.get());
        generator.emitEnumeratorGenericPropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());
        generator.emitJump(loopStart.get());

        generator.emitLabel(scope->breakTarget());
        generator.emitJump(end.get());
        generator.emitLabel(loopEnd.get());
    }

    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    generator.emitLabel(end.get());
    generator.popLexicalScope(this);
    generator.emitProfileControlFlow(profilerEndOffset);
}

void ForInNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    this->emitMultiLoopBytecode(generator, dst);
}

// ------------------------------ ForOfNode ------------------------------------
void ForOfNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_lexpr->isAssignmentLocation()) {
        emitThrowReferenceError(generator, ASCIILiteral("Left side of for-of statement is not a reference."));
        return;
    }

    RegisterID* forLoopSymbolTable = nullptr;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested, &forLoopSymbolTable);
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    auto extractor = [this, dst](BytecodeGenerator& generator, RegisterID* value)
    {
        if (m_lexpr->isResolveNode()) {
            const Identifier& ident = static_cast<ResolveNode*>(m_lexpr)->identifier();
            Variable var = generator.variable(ident);
            if (RegisterID* local = var.local())
                generator.emitMove(local, value);
            else {
                if (generator.isStrictMode())
                    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
                RegisterID* scope = generator.emitResolveScope(nullptr, var);
                generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
                generator.emitPutToScope(scope, var, value, generator.isStrictMode() ? ThrowIfNotFound : DoNotThrowIfNotFound, NotInitialization);
            }
            generator.emitProfileType(value, var, m_lexpr->position(), JSTextPosition(-1, m_lexpr->position().offset + ident.length(), -1));
        } else if (m_lexpr->isDotAccessorNode()) {
            DotAccessorNode* assignNode = static_cast<DotAccessorNode*>(m_lexpr);
            const Identifier& ident = assignNode->identifier();
            RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
            
            generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
            generator.emitPutById(base.get(), ident, value);
            generator.emitProfileType(value, assignNode->divotStart(), assignNode->divotEnd());
        } else if (m_lexpr->isBracketAccessorNode()) {
            BracketAccessorNode* assignNode = static_cast<BracketAccessorNode*>(m_lexpr);
            RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
            RegisterID* subscript = generator.emitNode(assignNode->subscript());
            
            generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
            generator.emitPutByVal(base.get(), subscript, value);
            generator.emitProfileType(value, assignNode->divotStart(), assignNode->divotEnd());
        } else {
            ASSERT(m_lexpr->isDestructuringNode());
            DestructuringAssignmentNode* assignNode = static_cast<DestructuringAssignmentNode*>(m_lexpr);
            assignNode->bindings()->bindValue(generator, value);
        }
        generator.emitProfileControlFlow(m_statement->startOffset());
        generator.emitNode(dst, m_statement);
    };
    generator.emitEnumeration(this, m_expr, extractor, this, forLoopSymbolTable);
    generator.popLexicalScope(this);
    generator.emitProfileControlFlow(m_statement->endOffset() + (m_statement->isBlock() ? 1 : 0));
}

// ------------------------------ ContinueNode ---------------------------------

Label* ContinueNode::trivialTarget(BytecodeGenerator& generator)
{
    if (generator.shouldEmitDebugHooks())
        return 0;

    LabelScopePtr scope = generator.continueTarget(m_ident);
    ASSERT(scope);

    if (generator.labelScopeDepth() != scope->scopeDepth())
        return 0;

    return scope->continueTarget();
}

void ContinueNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    
    LabelScopePtr scope = generator.continueTarget(m_ident);
    ASSERT(scope);

    generator.emitPopScopes(generator.scopeRegister(), scope->scopeDepth());
    generator.emitJump(scope->continueTarget());

    generator.emitProfileControlFlow(endOffset());
}

// ------------------------------ BreakNode ------------------------------------

Label* BreakNode::trivialTarget(BytecodeGenerator& generator)
{
    if (generator.shouldEmitDebugHooks())
        return 0;

    LabelScopePtr scope = generator.breakTarget(m_ident);
    ASSERT(scope);

    if (generator.labelScopeDepth() != scope->scopeDepth())
        return 0;

    return scope->breakTarget();
}

void BreakNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    
    LabelScopePtr scope = generator.breakTarget(m_ident);
    ASSERT(scope);

    generator.emitPopScopes(generator.scopeRegister(), scope->scopeDepth());
    generator.emitJump(scope->breakTarget());

    generator.emitProfileControlFlow(endOffset());
}

// ------------------------------ ReturnNode -----------------------------------

void ReturnNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    ASSERT(generator.codeType() == FunctionCode);

    if (dst == generator.ignoredResult())
        dst = 0;

    RefPtr<RegisterID> returnRegister = m_value ? generator.emitNodeInTailPosition(dst, m_value) : generator.emitLoad(dst, jsUndefined());

    generator.emitProfileType(returnRegister.get(), ProfileTypeBytecodeFunctionReturnStatement, divotStart(), divotEnd());
    if (generator.isInFinallyBlock()) {
        returnRegister = generator.emitMove(generator.newTemporary(), returnRegister.get());
        generator.emitPopScopes(generator.scopeRegister(), 0);
    }

    generator.emitDebugHook(WillLeaveCallFrame, lastLine(), startOffset(), lineStartOffset());
    generator.emitReturn(returnRegister.get());
    generator.emitProfileControlFlow(endOffset()); 
    // Emitting an unreachable return here is needed in case this op_profile_control_flow is the 
    // last opcode in a CodeBlock because a CodeBlock's instructions must end with a terminal opcode.
    if (generator.vm()->controlFlowProfiler())
        generator.emitReturn(generator.emitLoad(nullptr, jsUndefined()));
}

// ------------------------------ WithNode -------------------------------------

void WithNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());

    RefPtr<RegisterID> scope = generator.emitNode(m_expr);
    generator.emitExpressionInfo(m_divot, m_divot - m_expressionLength, m_divot);
    generator.emitPushWithScope(scope.get());
    generator.emitNodeInTailPosition(dst, m_statement);
    generator.emitPopWithScope();
}

// ------------------------------ CaseClauseNode --------------------------------

inline void CaseClauseNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitProfileControlFlow(m_startOffset);
    if (!m_statements)
        return;
    m_statements->emitBytecode(generator, dst);
}

// ------------------------------ CaseBlockNode --------------------------------

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
            int32_t intVal = static_cast<int32_t>(value);
            if ((typeForTable & ~SwitchNumber) || (intVal != value)) {
                typeForTable = SwitchNeither;
                break;
            }
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
            const String& value = static_cast<StringNode*>(clauseExpression)->value().string();
            if (singleCharacterSwitch &= value.length() == 1) {
                int32_t intVal = value[0];
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

static inline size_t length(ClauseListNode* list1, ClauseListNode* list2)
{
    size_t length = 0;
    for (ClauseListNode* node = list1; node; node = node->getNext())
        ++length;
    for (ClauseListNode* node = list2; node; node = node->getNext())
        ++length;
    return length;
}

SwitchInfo::SwitchType CaseBlockNode::tryTableSwitch(Vector<ExpressionNode*, 8>& literalVector, int32_t& min_num, int32_t& max_num)
{
    if (length(m_list1, m_list2) < s_tableSwitchMinimum)
        return SwitchInfo::SwitchNone;

    SwitchKind typeForTable = SwitchUnset;
    bool singleCharacterSwitch = true;
    
    processClauseList(m_list1, literalVector, typeForTable, singleCharacterSwitch, min_num, max_num);
    processClauseList(m_list2, literalVector, typeForTable, singleCharacterSwitch, min_num, max_num);
    
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

void CaseBlockNode::emitBytecodeForBlock(BytecodeGenerator& generator, RegisterID* switchExpression, RegisterID* dst)
{
    RefPtr<Label> defaultLabel;
    Vector<RefPtr<Label>, 8> labelVector;
    Vector<ExpressionNode*, 8> literalVector;
    int32_t min_num = std::numeric_limits<int32_t>::max();
    int32_t max_num = std::numeric_limits<int32_t>::min();
    SwitchInfo::SwitchType switchType = tryTableSwitch(literalVector, min_num, max_num);

    if (switchType != SwitchInfo::SwitchNone) {
        // Prepare the various labels
        for (uint32_t i = 0; i < literalVector.size(); i++)
            labelVector.append(generator.newLabel());
        defaultLabel = generator.newLabel();
        generator.beginSwitch(switchExpression, switchType);
    } else {
        // Setup jumps
        for (ClauseListNode* list = m_list1; list; list = list->getNext()) {
            RefPtr<RegisterID> clauseVal = generator.newTemporary();
            generator.emitNode(clauseVal.get(), list->getClause()->expr());
            generator.emitBinaryOp(op_stricteq, clauseVal.get(), clauseVal.get(), switchExpression, OperandTypes());
            labelVector.append(generator.newLabel());
            generator.emitJumpIfTrue(clauseVal.get(), labelVector[labelVector.size() - 1].get());
        }
        
        for (ClauseListNode* list = m_list2; list; list = list->getNext()) {
            RefPtr<RegisterID> clauseVal = generator.newTemporary();
            generator.emitNode(clauseVal.get(), list->getClause()->expr());
            generator.emitBinaryOp(op_stricteq, clauseVal.get(), clauseVal.get(), switchExpression, OperandTypes());
            labelVector.append(generator.newLabel());
            generator.emitJumpIfTrue(clauseVal.get(), labelVector[labelVector.size() - 1].get());
        }
        defaultLabel = generator.newLabel();
        generator.emitJump(defaultLabel.get());
    }

    size_t i = 0;
    for (ClauseListNode* list = m_list1; list; list = list->getNext()) {
        generator.emitLabel(labelVector[i++].get());
        list->getClause()->emitBytecode(generator, dst);
    }

    if (m_defaultClause) {
        generator.emitLabel(defaultLabel.get());
        m_defaultClause->emitBytecode(generator, dst);
    }

    for (ClauseListNode* list = m_list2; list; list = list->getNext()) {
        generator.emitLabel(labelVector[i++].get());
        list->getClause()->emitBytecode(generator, dst);
    }
    if (!m_defaultClause)
        generator.emitLabel(defaultLabel.get());

    ASSERT(i == labelVector.size());
    if (switchType != SwitchInfo::SwitchNone) {
        ASSERT(labelVector.size() == literalVector.size());
        generator.endSwitch(labelVector.size(), labelVector.data(), literalVector.data(), defaultLabel.get(), min_num, max_num);
    }
}

// ------------------------------ SwitchNode -----------------------------------

void SwitchNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());
    
    LabelScopePtr scope = generator.newLabelScope(LabelScope::Switch);

    RefPtr<RegisterID> r0 = generator.emitNode(m_expr);

    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::DoNotOptimize, BytecodeGenerator::NestedScopeType::IsNested);
    m_block->emitBytecodeForBlock(generator, r0.get(), dst);
    generator.popLexicalScope(this);

    generator.emitLabel(scope->breakTarget());
    generator.emitProfileControlFlow(endOffset());
}

// ------------------------------ LabelNode ------------------------------------

void LabelNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());

    ASSERT(!generator.breakTarget(m_name));

    LabelScopePtr scope = generator.newLabelScope(LabelScope::NamedLabel, &m_name);
    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(scope->breakTarget());
}

// ------------------------------ ThrowNode ------------------------------------

void ThrowNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());

    if (dst == generator.ignoredResult())
        dst = 0;
    RefPtr<RegisterID> expr = generator.emitNode(m_expr);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitThrow(expr.get());

    generator.emitProfileControlFlow(endOffset()); 
}

// ------------------------------ TryNode --------------------------------------

void TryNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    // NOTE: The catch and finally blocks must be labeled explicitly, so the
    // optimizer knows they may be jumped to from anywhere.

    generator.emitDebugHook(WillExecuteStatement, firstLine(), startOffset(), lineStartOffset());

    ASSERT(m_catchBlock || m_finallyBlock);

    RefPtr<Label> tryStartLabel = generator.newLabel();
    generator.emitLabel(tryStartLabel.get());
    
    if (m_finallyBlock)
        generator.pushFinallyContext(m_finallyBlock);
    TryData* tryData = generator.pushTry(tryStartLabel.get());

    generator.emitNode(dst, m_tryBlock);

    if (m_catchBlock) {
        RefPtr<Label> catchEndLabel = generator.newLabel();
        
        // Normal path: jump over the catch block.
        generator.emitJump(catchEndLabel.get());

        // Uncaught exception path: the catch block.
        RefPtr<Label> here = generator.emitLabel(generator.newLabel().get());
        RefPtr<RegisterID> exceptionRegister = generator.newTemporary();
        RefPtr<RegisterID> thrownValueRegister = generator.newTemporary();
        generator.popTryAndEmitCatch(tryData, exceptionRegister.get(), thrownValueRegister.get(), here.get(), HandlerType::Catch);
        
        if (m_finallyBlock) {
            // If the catch block throws an exception and we have a finally block, then the finally
            // block should "catch" that exception.
            tryData = generator.pushTry(here.get());
        }

        generator.emitPushCatchScope(m_thrownValueIdent, thrownValueRegister.get(), m_lexicalVariables);
        generator.emitProfileControlFlow(m_tryBlock->endOffset() + 1);
        if (m_finallyBlock)
            generator.emitNode(dst, m_catchBlock);
        else
            generator.emitNodeInTailPosition(dst, m_catchBlock);
        generator.emitPopCatchScope(m_lexicalVariables);
        generator.emitLabel(catchEndLabel.get());
    }

    if (m_finallyBlock) {
        RefPtr<Label> preFinallyLabel = generator.emitLabel(generator.newLabel().get());
        
        generator.popFinallyContext();

        RefPtr<Label> finallyEndLabel = generator.newLabel();

        int finallyStartOffset = m_catchBlock ? m_catchBlock->endOffset() + 1 : m_tryBlock->endOffset() + 1;

        // Normal path: run the finally code, and jump to the end.
        generator.emitProfileControlFlow(finallyStartOffset);
        generator.emitNodeInTailPosition(dst, m_finallyBlock);
        generator.emitProfileControlFlow(m_finallyBlock->endOffset() + 1);
        generator.emitJump(finallyEndLabel.get());

        // Uncaught exception path: invoke the finally block, then re-throw the exception.
        RefPtr<RegisterID> exceptionRegister = generator.newTemporary();
        RefPtr<RegisterID> thrownValueRegister = generator.newTemporary();
        generator.popTryAndEmitCatch(tryData, exceptionRegister.get(), thrownValueRegister.get(), preFinallyLabel.get(), HandlerType::Finally);
        generator.emitProfileControlFlow(finallyStartOffset);
        generator.emitNodeInTailPosition(dst, m_finallyBlock);
        generator.emitThrow(exceptionRegister.get());

        generator.emitLabel(finallyEndLabel.get());
        generator.emitProfileControlFlow(m_finallyBlock->endOffset() + 1);
    } else
        generator.emitProfileControlFlow(m_catchBlock->endOffset() + 1);

}

// ------------------------------ ScopeNode -----------------------------

inline void ScopeNode::emitStatementsBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_statements)
        return;
    m_statements->emitBytecode(generator, dst);
}

static void emitProgramNodeBytecode(BytecodeGenerator& generator, ScopeNode& scopeNode)
{
    generator.emitDebugHook(WillExecuteProgram, scopeNode.startLine(), scopeNode.startStartOffset(), scopeNode.startLineStartOffset());

    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    generator.emitProfileControlFlow(scopeNode.startStartOffset());
    scopeNode.emitStatementsBytecode(generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, scopeNode.lastLine(), scopeNode.startOffset(), scopeNode.lineStartOffset());
    generator.emitEnd(dstRegister.get());
}

// ------------------------------ ProgramNode -----------------------------

void ProgramNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    emitProgramNodeBytecode(generator, *this);
}

// ------------------------------ ModuleProgramNode --------------------

void ModuleProgramNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    emitProgramNodeBytecode(generator, *this);
}

// ------------------------------ EvalNode -----------------------------

void EvalNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(WillExecuteProgram, startLine(), startStartOffset(), startLineStartOffset());

    RefPtr<RegisterID> dstRegister = generator.newTemporary();
    generator.emitLoad(dstRegister.get(), jsUndefined());
    emitStatementsBytecode(generator, dstRegister.get());

    generator.emitDebugHook(DidExecuteProgram, lastLine(), startOffset(), lineStartOffset());
    generator.emitEnd(dstRegister.get());
}

// ------------------------------ FunctionNode -----------------------------

void FunctionNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    if (generator.vm()->typeProfiler()) {
        for (size_t i = 0; i < m_parameters->size(); i++) {
            // Destructuring parameters are handled in destructuring nodes.
            if (!m_parameters->at(i).first->isBindingNode())
                continue;
            BindingNode* parameter = static_cast<BindingNode*>(m_parameters->at(i).first);
            RegisterID reg(CallFrame::argumentOffset(i));
            generator.emitProfileType(&reg, ProfileTypeBytecodeFunctionArgument, parameter->divotStart(), parameter->divotEnd());
        }
    }

    generator.emitProfileControlFlow(startStartOffset());
    generator.emitDebugHook(DidEnterCallFrame, startLine(), startStartOffset(), startLineStartOffset());

    switch (generator.parseMode()) {
    case SourceParseMode::GeneratorWrapperFunctionMode: {
        StatementNode* singleStatement = this->singleStatement();
        ASSERT(singleStatement->isExprStatement());
        ExprStatementNode* exprStatement = static_cast<ExprStatementNode*>(singleStatement);
        ExpressionNode* expr = exprStatement->expr();
        ASSERT(expr->isFuncExprNode());
        FuncExprNode* funcExpr = static_cast<FuncExprNode*>(expr);

        RefPtr<RegisterID> next = generator.newTemporary();
        generator.emitNode(next.get(), funcExpr);

        if (generator.superBinding() == SuperBinding::Needed) {
            RefPtr<RegisterID> homeObject = emitHomeObjectForCallee(generator);
            emitPutHomeObject(generator, next.get(), homeObject.get());
        }

        // FIXME: Currently, we just create an object and store generator related fields as its properties for ease.
        // But to make it efficient, we will introduce JSGenerator class, add opcode new_generator and use its C++ fields instead of these private properties.
        // https://bugs.webkit.org/show_bug.cgi?id=151545

        generator.emitDirectPutById(generator.generatorRegister(), generator.propertyNames().generatorNextPrivateName, next.get(), PropertyNode::KnownDirect);

        generator.emitDirectPutById(generator.generatorRegister(), generator.propertyNames().generatorThisPrivateName, generator.thisRegister(), PropertyNode::KnownDirect);

        RegisterID* initialState = generator.emitLoad(nullptr, jsNumber(0));
        generator.emitDirectPutById(generator.generatorRegister(), generator.propertyNames().generatorStatePrivateName, initialState, PropertyNode::KnownDirect);

        generator.emitDirectPutById(generator.generatorRegister(), generator.propertyNames().generatorFramePrivateName, generator.emitLoad(nullptr, jsNull()), PropertyNode::KnownDirect);

        ASSERT(startOffset() >= lineStartOffset());
        generator.emitDebugHook(WillLeaveCallFrame, lastLine(), startOffset(), lineStartOffset());
        generator.emitReturn(generator.generatorRegister());
        break;
    }

    case SourceParseMode::GeneratorBodyMode: {
        RefPtr<Label> generatorBodyLabel = generator.newLabel();
        {
            RefPtr<RegisterID> condition = generator.newTemporary();
            generator.emitEqualityOp(op_stricteq, condition.get(), generator.generatorResumeModeRegister(), generator.emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGeneratorFunction::GeneratorResumeMode::NormalMode))));
            generator.emitJumpIfTrue(condition.get(), generatorBodyLabel.get());

            RefPtr<Label> throwLabel = generator.newLabel();
            generator.emitEqualityOp(op_stricteq, condition.get(), generator.generatorResumeModeRegister(), generator.emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGeneratorFunction::GeneratorResumeMode::ThrowMode))));
            generator.emitJumpIfTrue(condition.get(), throwLabel.get());

            generator.emitReturn(generator.generatorValueRegister());

            generator.emitLabel(throwLabel.get());
            generator.emitThrow(generator.generatorValueRegister());
        }

        generator.emitLabel(generatorBodyLabel.get());

        emitStatementsBytecode(generator, generator.ignoredResult());

        RefPtr<Label> done = generator.newLabel();
        generator.emitLabel(done.get());
        generator.emitReturn(generator.emitLoad(nullptr, jsUndefined()));
        generator.endGenerator(done.get());
        break;
    }

    default: {
        emitStatementsBytecode(generator, generator.ignoredResult());

        StatementNode* singleStatement = this->singleStatement();
        ReturnNode* returnNode = 0;

        // Check for a return statement at the end of a function composed of a single block.
        if (singleStatement && singleStatement->isBlock()) {
            StatementNode* lastStatementInBlock = static_cast<BlockNode*>(singleStatement)->lastStatement();
            if (lastStatementInBlock && lastStatementInBlock->isReturnNode())
                returnNode = static_cast<ReturnNode*>(lastStatementInBlock);
        }

        // If there is no return we must automatically insert one.
        if (!returnNode) {
            if (generator.constructorKind() == ConstructorKind::Derived && generator.needsToUpdateArrowFunctionContext())
                generator.emitLoadThisFromArrowFunctionLexicalEnvironment(); // Arrow function can invoke 'super' in constructor and before leave constructor we need load 'this' from lexical arrow function environment
            
            RegisterID* r0 = generator.isConstructor() ? generator.thisRegister() : generator.emitLoad(0, jsUndefined());
            generator.emitProfileType(r0, ProfileTypeBytecodeFunctionReturnStatement); // Do not emit expression info for this profile because it's not in the user's source code.
            ASSERT(startOffset() >= lineStartOffset());
            generator.emitDebugHook(WillLeaveCallFrame, lastLine(), startOffset(), lineStartOffset());
            generator.emitReturn(r0);
            return;
        }
        break;
    }
    }
}

// ------------------------------ FuncDeclNode ---------------------------------

void FuncDeclNode::emitBytecode(BytecodeGenerator&, RegisterID*)
{
}

// ------------------------------ FuncExprNode ---------------------------------

RegisterID* FuncExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    return generator.emitNewFunctionExpression(generator.finalDestination(dst), this);
}

// ------------------------------ ArrowFuncExprNode ---------------------------------

RegisterID* ArrowFuncExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    return generator.emitNewArrowFunctionExpression(generator.finalDestination(dst), this);
}

// ------------------------------ YieldExprNode --------------------------------

RegisterID* YieldExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!delegate()) {
        RefPtr<RegisterID> arg = nullptr;
        if (argument()) {
            arg = generator.newTemporary();
            generator.emitNode(arg.get(), argument());
        } else
            arg = generator.emitLoad(nullptr, jsUndefined());
        RefPtr<RegisterID> value = generator.emitYield(arg.get());
        if (dst == generator.ignoredResult())
            return nullptr;
        return generator.emitMove(generator.finalDestination(dst), value.get());
    }
    RefPtr<RegisterID> arg = generator.newTemporary();
    generator.emitNode(arg.get(), argument());
    RefPtr<RegisterID> value = generator.emitDelegateYield(arg.get(), this);
    if (dst == generator.ignoredResult())
        return nullptr;
    return generator.emitMove(generator.finalDestination(dst), value.get());
}

// ------------------------------ ClassDeclNode ---------------------------------

void ClassDeclNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.emitNode(dst, m_classDeclaration);
}

// ------------------------------ ClassExprNode ---------------------------------

RegisterID* ClassExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_name.isNull())
        generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested);

    RefPtr<RegisterID> superclass;
    if (m_classHeritage) {
        superclass = generator.newTemporary();
        generator.emitNode(superclass.get(), m_classHeritage);
    }

    RefPtr<RegisterID> constructor;

    // FIXME: Make the prototype non-configurable & non-writable.
    if (m_constructorExpression)
        constructor = generator.emitNode(dst, m_constructorExpression);
    else {
        constructor = generator.emitNewDefaultConstructor(generator.finalDestination(dst),
            m_classHeritage ? ConstructorKind::Derived : ConstructorKind::Base, m_name);
    }

    const auto& propertyNames = generator.propertyNames();
    RefPtr<RegisterID> prototype = generator.emitNewObject(generator.newTemporary());

    if (superclass) {
        RefPtr<RegisterID> protoParent = generator.newTemporary();
        generator.emitLoad(protoParent.get(), jsNull());

        RefPtr<RegisterID> tempRegister = generator.newTemporary();

        // FIXME: Throw TypeError if it's a generator function.
        RefPtr<Label> superclassIsUndefinedLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitIsUndefined(tempRegister.get(), superclass.get()), superclassIsUndefinedLabel.get());

        RefPtr<Label> superclassIsNullLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitUnaryOp(op_eq_null, tempRegister.get(), superclass.get()), superclassIsNullLabel.get());

        RefPtr<Label> superclassIsObjectLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitIsObject(tempRegister.get(), superclass.get()), superclassIsObjectLabel.get());
        generator.emitLabel(superclassIsUndefinedLabel.get());
        generator.emitThrowTypeError(ASCIILiteral("The superclass is not an object."));
        generator.emitLabel(superclassIsObjectLabel.get());
        generator.emitGetById(protoParent.get(), superclass.get(), generator.propertyNames().prototype);

        RefPtr<Label> protoParentIsObjectOrNullLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitUnaryOp(op_is_object_or_null, tempRegister.get(), protoParent.get()), protoParentIsObjectOrNullLabel.get());
        generator.emitJumpIfTrue(generator.emitUnaryOp(op_is_function, tempRegister.get(), protoParent.get()), protoParentIsObjectOrNullLabel.get());
        generator.emitThrowTypeError(ASCIILiteral("The value of the superclass's prototype property is not an object."));
        generator.emitLabel(protoParentIsObjectOrNullLabel.get());

        generator.emitDirectPutById(constructor.get(), generator.propertyNames().underscoreProto, superclass.get(), PropertyNode::Unknown);
        generator.emitLabel(superclassIsNullLabel.get());
        generator.emitDirectPutById(prototype.get(), generator.propertyNames().underscoreProto, protoParent.get(), PropertyNode::Unknown);

        emitPutHomeObject(generator, constructor.get(), prototype.get());
    }

    RefPtr<RegisterID> constructorNameRegister = generator.emitLoad(generator.newTemporary(), propertyNames.constructor);
    generator.emitCallDefineProperty(prototype.get(), constructorNameRegister.get(), constructor.get(), nullptr, nullptr,
        BytecodeGenerator::PropertyConfigurable | BytecodeGenerator::PropertyWritable, m_position);

    RefPtr<RegisterID> prototypeNameRegister = generator.emitLoad(generator.newTemporary(), propertyNames.prototype);
    generator.emitCallDefineProperty(constructor.get(), prototypeNameRegister.get(), prototype.get(), nullptr, nullptr, 0, m_position);

    if (m_staticMethods)
        generator.emitNode(constructor.get(), m_staticMethods);

    if (m_instanceMethods)
        generator.emitNode(prototype.get(), m_instanceMethods);

    if (!m_name.isNull()) {
        Variable classNameVar = generator.variable(m_name);
        RELEASE_ASSERT(classNameVar.isResolved());
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, classNameVar);
        generator.emitPutToScope(scope.get(), classNameVar, constructor.get(), ThrowIfNotFound, Initialization);
        generator.popLexicalScope(this);
    }

    return generator.moveToDestinationIfNeeded(dst, constructor.get());
}

// ------------------------------ ImportDeclarationNode -----------------------

void ImportDeclarationNode::emitBytecode(BytecodeGenerator&, RegisterID*)
{
    // Do nothing at runtime.
}

// ------------------------------ ExportAllDeclarationNode --------------------

void ExportAllDeclarationNode::emitBytecode(BytecodeGenerator&, RegisterID*)
{
    // Do nothing at runtime.
}

// ------------------------------ ExportDefaultDeclarationNode ----------------

void ExportDefaultDeclarationNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_declaration);
    generator.emitNode(dst, m_declaration);
}

// ------------------------------ ExportLocalDeclarationNode ------------------

void ExportLocalDeclarationNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_declaration);
    generator.emitNode(dst, m_declaration);
}

// ------------------------------ ExportNamedDeclarationNode ------------------

void ExportNamedDeclarationNode::emitBytecode(BytecodeGenerator&, RegisterID*)
{
    // Do nothing at runtime.
}

// ------------------------------ DestructuringAssignmentNode -----------------
RegisterID* DestructuringAssignmentNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (RegisterID* result = m_bindings->emitDirectBinding(generator, dst, m_initializer))
        return result;
    RefPtr<RegisterID> initializer = generator.tempDestination(dst);
    generator.emitNode(initializer.get(), m_initializer);
    m_bindings->bindValue(generator, initializer.get());
    return generator.moveToDestinationIfNeeded(dst, initializer.get());
}

static void assignDefaultValueIfUndefined(BytecodeGenerator& generator, RegisterID* maybeUndefined, ExpressionNode* defaultValue)
{
    ASSERT(defaultValue);
    RefPtr<Label> isNotUndefined = generator.newLabel();
    generator.emitJumpIfFalse(generator.emitIsUndefined(generator.newTemporary(), maybeUndefined), isNotUndefined.get());
    generator.emitNode(maybeUndefined, defaultValue);
    generator.emitLabel(isNotUndefined.get());
}

void ArrayPatternNode::bindValue(BytecodeGenerator& generator, RegisterID* rhs) const
{
    RefPtr<RegisterID> iterator = generator.newTemporary();
    {
        generator.emitGetById(iterator.get(), rhs, generator.propertyNames().iteratorSymbol);
        CallArguments args(generator, nullptr);
        generator.emitMove(args.thisRegister(), rhs);
        generator.emitCall(iterator.get(), iterator.get(), NoExpectedFunction, args, divot(), divotStart(), divotEnd());
    }

    if (m_targetPatterns.isEmpty()) {
        generator.emitIteratorClose(iterator.get(), this);
        return;
    }

    RefPtr<RegisterID> done;
    for (auto& target : m_targetPatterns) {
        switch (target.bindingType) {
        case BindingType::Elision:
        case BindingType::Element: {
            RefPtr<Label> iterationSkipped = generator.newLabel();
            if (!done)
                done = generator.newTemporary();
            else
                generator.emitJumpIfTrue(done.get(), iterationSkipped.get());

            RefPtr<RegisterID> value = generator.newTemporary();
            generator.emitIteratorNext(value.get(), iterator.get(), this);
            generator.emitGetById(done.get(), value.get(), generator.propertyNames().done);
            generator.emitJumpIfTrue(done.get(), iterationSkipped.get());
            generator.emitGetById(value.get(), value.get(), generator.propertyNames().value);

            {
                RefPtr<Label> valueIsSet = generator.newLabel();
                generator.emitJump(valueIsSet.get());
                generator.emitLabel(iterationSkipped.get());
                generator.emitLoad(value.get(), jsUndefined());
                generator.emitLabel(valueIsSet.get());
            }

            if (target.bindingType == BindingType::Element) {
                if (target.defaultValue)
                    assignDefaultValueIfUndefined(generator, value.get(), target.defaultValue);
                target.pattern->bindValue(generator, value.get());
            }
            break;
        }

        case BindingType::RestElement: {
            RefPtr<RegisterID> array = generator.emitNewArray(generator.newTemporary(), 0, 0);

            RefPtr<Label> iterationDone = generator.newLabel();
            if (!done)
                done = generator.newTemporary();
            else
                generator.emitJumpIfTrue(done.get(), iterationDone.get());

            RefPtr<RegisterID> index = generator.newTemporary();
            generator.emitLoad(index.get(), jsNumber(0));
            RefPtr<Label> loopStart = generator.newLabel();
            generator.emitLabel(loopStart.get());

            RefPtr<RegisterID> value = generator.newTemporary();
            generator.emitIteratorNext(value.get(), iterator.get(), this);
            generator.emitGetById(done.get(), value.get(), generator.propertyNames().done);
            generator.emitJumpIfTrue(done.get(), iterationDone.get());
            generator.emitGetById(value.get(), value.get(), generator.propertyNames().value);

            generator.emitDirectPutByVal(array.get(), index.get(), value.get());
            generator.emitInc(index.get());
            generator.emitJump(loopStart.get());

            generator.emitLabel(iterationDone.get());
            target.pattern->bindValue(generator, array.get());
            break;
        }
        }
    }

    RefPtr<Label> iteratorClosed = generator.newLabel();
    generator.emitJumpIfTrue(done.get(), iteratorClosed.get());
    generator.emitIteratorClose(iterator.get(), this);
    generator.emitLabel(iteratorClosed.get());
}

RegisterID* ArrayPatternNode::emitDirectBinding(BytecodeGenerator& generator, RegisterID* dst, ExpressionNode* rhs)
{
    if (!rhs->isSimpleArray())
        return 0;

    RefPtr<RegisterID> resultRegister;
    if (dst && dst != generator.ignoredResult())
        resultRegister = generator.emitNewArray(generator.newTemporary(), 0, 0);
    ElementNode* elementNodes = static_cast<ArrayNode*>(rhs)->elements();
    Vector<ExpressionNode*> elements;
    for (; elementNodes; elementNodes = elementNodes->next())
        elements.append(elementNodes->value());
    if (m_targetPatterns.size() != elements.size())
        return 0;
    Vector<RefPtr<RegisterID>> registers;
    registers.reserveCapacity(m_targetPatterns.size());
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        registers.uncheckedAppend(generator.newTemporary());
        generator.emitNode(registers.last().get(), elements[i]);
        if (m_targetPatterns[i].defaultValue)
            assignDefaultValueIfUndefined(generator, registers.last().get(), m_targetPatterns[i].defaultValue);
        if (resultRegister)
            generator.emitPutByIndex(resultRegister.get(), i, registers.last().get());
    }
    
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        if (m_targetPatterns[i].pattern)
            m_targetPatterns[i].pattern->bindValue(generator, registers[i].get());
    }
    if (resultRegister)
        return generator.moveToDestinationIfNeeded(dst, resultRegister.get());
    return generator.emitLoad(generator.finalDestination(dst), jsUndefined());
}

void ArrayPatternNode::toString(StringBuilder& builder) const
{
    builder.append('[');
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        const auto& target = m_targetPatterns[i];

        switch (target.bindingType) {
        case BindingType::Elision:
            builder.append(',');
            break;

        case BindingType::Element:
            target.pattern->toString(builder);
            if (i < m_targetPatterns.size() - 1)
                builder.append(',');
            break;

        case BindingType::RestElement:
            builder.append("...");
            target.pattern->toString(builder);
            break;
        }
    }
    builder.append(']');
}

void ArrayPatternNode::collectBoundIdentifiers(Vector<Identifier>& identifiers) const
{
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        if (DestructuringPatternNode* node = m_targetPatterns[i].pattern)
            node->collectBoundIdentifiers(identifiers);
    }
}

void ObjectPatternNode::toString(StringBuilder& builder) const
{
    builder.append('{');
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        if (m_targetPatterns[i].wasString)
            builder.appendQuotedJSONString(m_targetPatterns[i].propertyName.string());
        else
            builder.append(m_targetPatterns[i].propertyName.string());
        builder.append(':');
        m_targetPatterns[i].pattern->toString(builder);
        if (i < m_targetPatterns.size() - 1)
            builder.append(',');
    }
    builder.append('}');
}
    
void ObjectPatternNode::bindValue(BytecodeGenerator& generator, RegisterID* rhs) const
{
    generator.emitRequireObjectCoercible(rhs, ASCIILiteral("Right side of assignment cannot be destructured"));
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        auto& target = m_targetPatterns[i];
        RefPtr<RegisterID> temp = generator.newTemporary();
        if (!target.propertyExpression) {
            // Should not emit get_by_id for indexed ones.
            Optional<uint32_t> optionalIndex = parseIndex(target.propertyName);
            if (!optionalIndex)
                generator.emitGetById(temp.get(), rhs, target.propertyName);
            else {
                RefPtr<RegisterID> index = generator.emitLoad(generator.newTemporary(), jsNumber(optionalIndex.value()));
                generator.emitGetByVal(temp.get(), rhs, index.get());
            }
        } else {
            RefPtr<RegisterID> propertyName = generator.emitNode(target.propertyExpression);
            generator.emitGetByVal(temp.get(), rhs, propertyName.get());
        }

        if (target.defaultValue)
            assignDefaultValueIfUndefined(generator, temp.get(), target.defaultValue);
        target.pattern->bindValue(generator, temp.get());
    }
}

void ObjectPatternNode::collectBoundIdentifiers(Vector<Identifier>& identifiers) const
{
    for (size_t i = 0; i < m_targetPatterns.size(); i++)
        m_targetPatterns[i].pattern->collectBoundIdentifiers(identifiers);
}

void BindingNode::bindValue(BytecodeGenerator& generator, RegisterID* value) const
{
    Variable var = generator.variable(m_boundProperty);
    bool isReadOnly = var.isReadOnly() && m_bindingContext != AssignmentContext::ConstDeclarationStatement;
    if (RegisterID* local = var.local()) {
        if (m_bindingContext == AssignmentContext::AssignmentExpression)
            generator.emitTDZCheckIfNecessary(var, local, nullptr);
        if (isReadOnly) {
            generator.emitReadOnlyExceptionIfNeeded(var);
            return;
        }
        generator.emitMove(local, value);
        generator.emitProfileType(local, var, divotStart(), divotEnd());
        if (m_bindingContext == AssignmentContext::DeclarationStatement || m_bindingContext == AssignmentContext::ConstDeclarationStatement)
            generator.liftTDZCheckIfPossible(var);
        return;
    }
    if (generator.isStrictMode())
        generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
    RegisterID* scope = generator.emitResolveScope(nullptr, var);
    generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
    if (m_bindingContext == AssignmentContext::AssignmentExpression)
        generator.emitTDZCheckIfNecessary(var, nullptr, scope);
    if (isReadOnly) {
        generator.emitReadOnlyExceptionIfNeeded(var);
        return;
    }
    generator.emitPutToScope(scope, var, value, generator.isStrictMode() ? ThrowIfNotFound : DoNotThrowIfNotFound, 
        m_bindingContext == AssignmentContext::ConstDeclarationStatement || m_bindingContext == AssignmentContext::DeclarationStatement ? Initialization : NotInitialization);
    generator.emitProfileType(value, var, divotStart(), divotEnd());
    if (m_bindingContext == AssignmentContext::DeclarationStatement || m_bindingContext == AssignmentContext::ConstDeclarationStatement)
        generator.liftTDZCheckIfPossible(var);
    return;
}

void BindingNode::toString(StringBuilder& builder) const
{
    builder.append(m_boundProperty.string());
}

void BindingNode::collectBoundIdentifiers(Vector<Identifier>& identifiers) const
{
    identifiers.append(m_boundProperty);
}

void AssignmentElementNode::collectBoundIdentifiers(Vector<Identifier>&) const
{
}

void AssignmentElementNode::bindValue(BytecodeGenerator& generator, RegisterID* value) const
{
    if (m_assignmentTarget->isResolveNode()) {
        ResolveNode* lhs = static_cast<ResolveNode*>(m_assignmentTarget);
        Variable var = generator.variable(lhs->identifier());
        bool isReadOnly = var.isReadOnly();
        if (RegisterID* local = var.local()) {
            generator.emitTDZCheckIfNecessary(var, local, nullptr);

            if (isReadOnly)
                generator.emitReadOnlyExceptionIfNeeded(var);
            else {
                generator.invalidateForInContextForLocal(local);
                generator.moveToDestinationIfNeeded(local, value);
                generator.emitProfileType(local, divotStart(), divotEnd());
            }
            return;
        }
        if (generator.isStrictMode())
            generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        generator.emitTDZCheckIfNecessary(var, nullptr, scope.get());
        if (isReadOnly) {
            bool threwException = generator.emitReadOnlyExceptionIfNeeded(var);
            if (threwException)
                return;
        }
        generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
        if (!isReadOnly) {
            generator.emitPutToScope(scope.get(), var, value, generator.isStrictMode() ? ThrowIfNotFound : DoNotThrowIfNotFound, NotInitialization);
            generator.emitProfileType(value, var, divotStart(), divotEnd());
        }
    } else if (m_assignmentTarget->isDotAccessorNode()) {
        DotAccessorNode* lhs = static_cast<DotAccessorNode*>(m_assignmentTarget);
        RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(lhs->base(), true, false);
        generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
        generator.emitPutById(base.get(), lhs->identifier(), value);
        generator.emitProfileType(value, divotStart(), divotEnd());
    } else if (m_assignmentTarget->isBracketAccessorNode()) {
        BracketAccessorNode* lhs = static_cast<BracketAccessorNode*>(m_assignmentTarget);
        RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(lhs->base(), true, false);
        RefPtr<RegisterID> property = generator.emitNodeForLeftHandSide(lhs->subscript(), true, false);
        generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
        generator.emitPutByVal(base.get(), property.get(), value);
        generator.emitProfileType(value, divotStart(), divotEnd());
    }
}

void AssignmentElementNode::toString(StringBuilder& builder) const
{
    if (m_assignmentTarget->isResolveNode())
        builder.append(static_cast<ResolveNode*>(m_assignmentTarget)->identifier().string());
}

void RestParameterNode::collectBoundIdentifiers(Vector<Identifier>& identifiers) const
{
    identifiers.append(m_name);
}
void RestParameterNode::toString(StringBuilder& builder) const
{
    builder.append(m_name.string());
}
void RestParameterNode::bindValue(BytecodeGenerator&, RegisterID*) const
{
    RELEASE_ASSERT_NOT_REACHED();
}
void RestParameterNode::emit(BytecodeGenerator& generator)
{
    Variable var = generator.variable(m_name);
    if (RegisterID* local = var.local()) {
        generator.emitRestParameter(local, m_numParametersToSkip);
        generator.emitProfileType(local, var, m_divotStart, m_divotEnd);
        return;
    }

    RefPtr<RegisterID> restParameterArray = generator.newTemporary();
    generator.emitRestParameter(restParameterArray.get(), m_numParametersToSkip);
    generator.emitProfileType(restParameterArray.get(), var, m_divotStart, m_divotEnd);
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    generator.emitExpressionInfo(m_divotEnd, m_divotStart, m_divotEnd);
    generator.emitPutToScope(scope.get(), var, restParameterArray.get(), generator.isStrictMode() ? ThrowIfNotFound : DoNotThrowIfNotFound, Initialization);
}


RegisterID* SpreadExpressionNode::emitBytecode(BytecodeGenerator&, RegisterID*)
{
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

} // namespace JSC
