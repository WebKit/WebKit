/*
*  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
*  Copyright (C) 2001 Peter Kelly (pmk@post.com)
*  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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
#include "BytecodeGeneratorBaseInlines.h"
#include "JSArrayIterator.h"
#include "JSAsyncGenerator.h"
#include "JSCInlines.h"
#include "JSGenerator.h"
#include "JSImmutableButterfly.h"
#include "JSMapIterator.h"
#include "JSSetIterator.h"
#include "JSStringIterator.h"
#include "LabelScope.h"
#include "LinkTimeConstant.h"
#include "ModuleScopeData.h"
#include "StackAlignment.h"
#include "UnlinkedMetadataTableInlines.h"
#include "YarrFlags.h"
#include <wtf/Assertions.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {

/*
    Details of the emitBytecode function.

    Return value: The register holding the production's value.
             dst: An optional parameter specifying the most efficient destination at
                  which to store the production's value.
                  If dst is null, you may return whatever VirtualRegister you want. Otherwise you have to return dst.

    The dst argument provides for a crude form of copy propagation. For example,

        x = 1

    becomes

        load r[x], 1

    instead of 

        load r0, 1
        mov r[x], r0

    because the assignment node, "x =", passes r[x] as dst to the number node, "1".
*/

void ExpressionNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label& trueTarget, Label& falseTarget, FallThroughMode fallThroughMode)
{
    RegisterID* result = generator.emitNode(this);
    if (fallThroughMode == FallThroughMeansTrue)
        generator.emitJumpIfFalse(result, falseTarget);
    else
        generator.emitJumpIfTrue(result, trueTarget);
}

// ------------------------------ ThrowableExpressionData --------------------------------

RegisterID* ThrowableExpressionData::emitThrowReferenceError(BytecodeGenerator& generator, const String& message, RegisterID* dst)
{
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitThrowReferenceError(message);
    if (dst)
        return dst;
    return generator.newTemporary();
}

// ------------------------------ ConstantNode ----------------------------------

void ConstantNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label& trueTarget, Label& falseTarget, FallThroughMode fallThroughMode)
{
    TriState value = TriState::Indeterminate;
    JSValue constant = jsValue(generator);
    if (LIKELY(constant))
        value = constant.pureToBoolean();

    if (UNLIKELY(needsDebugHook())) {
        if (value != TriState::Indeterminate)
            generator.emitDebugHook(this);
    }

    if (value == TriState::Indeterminate)
        ExpressionNode::emitBytecodeInConditionContext(generator, trueTarget, falseTarget, fallThroughMode);
    else if (value == TriState::True && fallThroughMode == FallThroughMeansFalse)
        generator.emitJump(trueTarget);
    else if (value == TriState::False && fallThroughMode == FallThroughMeansTrue)
        generator.emitJump(falseTarget);

    // All other cases are unconditional fall-throughs, like "if (true)".
}

RegisterID* ConstantNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return nullptr;
    JSValue constant = jsValue(generator);
    if (UNLIKELY(!constant)) {
        // This can happen if we try to parse a string or BigInt so enormous that we OOM.
        return generator.emitThrowExpressionTooDeepException();
    }
    return generator.emitLoad(dst, constant);
}

JSValue StringNode::jsValue(BytecodeGenerator& generator) const
{
    return generator.addStringConstant(m_value);
}

JSValue BigIntNode::jsValue(BytecodeGenerator& generator) const
{
    return generator.addBigIntConstant(m_value, m_radix, m_sign);
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
        return nullptr;

    auto flags = Yarr::parseFlags(m_flags.string());
    ASSERT(flags.hasValue());
    RegExp* regExp = RegExp::create(generator.vm(), m_pattern.string(), flags.value());
    if (regExp->isValid())
        return generator.emitNewRegExp(generator.finalDestination(dst), regExp);

    const char* messageCharacters = regExp->errorMessage();
    const Identifier& message = generator.parserArena().identifierArena().makeIdentifier(generator.vm(), bitwise_cast<const LChar*>(messageCharacters), strlen(messageCharacters));
    generator.emitThrowStaticError(ErrorTypeWithExtension::SyntaxError, message);
    return generator.emitLoad(generator.finalDestination(dst), jsUndefined());
}

// ------------------------------ ThisNode -------------------------------------

RegisterID* ThisNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    generator.ensureThis();
    if (dst == generator.ignoredResult())
        return nullptr;

    RegisterID* result = generator.move(dst, generator.thisRegister());
    static const unsigned thisLength = strlen("this");
    generator.emitProfileType(generator.thisRegister(), position(), position() + thisLength);
    return result;
}

// ------------------------------ SuperNode -------------------------------------

static RegisterID* emitHomeObjectForCallee(BytecodeGenerator& generator)
{
    if ((generator.isDerivedClassContext() || generator.isDerivedConstructorContext()) && generator.parseMode() != SourceParseMode::InstanceFieldInitializerMode) {
        RegisterID* derivedConstructor = generator.emitLoadDerivedConstructorFromArrowFunctionLexicalEnvironment();
        return generator.emitGetById(generator.newTemporary(), derivedConstructor, generator.propertyNames().builtinNames().homeObjectPrivateName());
    }

    RegisterID callee;
    callee.setIndex(CallFrameSlot::callee);
    return generator.emitGetById(generator.newTemporary(), &callee, generator.propertyNames().builtinNames().homeObjectPrivateName());
}

static RegisterID* emitSuperBaseForCallee(BytecodeGenerator& generator)
{
    RefPtr<RegisterID> homeObject = emitHomeObjectForCallee(generator);
    return generator.emitGetPrototypeOf(generator.newTemporary(), homeObject.get());
}

static RegisterID* emitGetSuperFunctionForConstruct(BytecodeGenerator& generator)
{
    if (generator.isDerivedConstructorContext())
        return generator.emitGetPrototypeOf(generator.newTemporary(), generator.emitLoadDerivedConstructorFromArrowFunctionLexicalEnvironment());

    RegisterID callee;
    callee.setIndex(CallFrameSlot::callee);
    return generator.emitGetPrototypeOf(generator.newTemporary(), &callee);
}

RegisterID* SuperNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RegisterID* result = emitSuperBaseForCallee(generator);
    return generator.move(generator.finalDestination(dst), result);
}

// ------------------------------ ImportNode -------------------------------------

RegisterID* ImportNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> importModule = generator.moveLinkTimeConstant(nullptr, LinkTimeConstant::importModule);
    CallArguments arguments(generator, nullptr, 1);
    generator.emitLoad(arguments.thisRegister(), jsUndefined());
    generator.emitNode(arguments.argumentRegister(0), m_expr);
    return generator.emitCall(generator.finalDestination(dst, importModule.get()), importModule.get(), NoExpectedFunction, arguments, divot(), divotStart(), divotEnd(), DebuggableCall::No);
}

// ------------------------------ NewTargetNode ----------------------------------

RegisterID* NewTargetNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return nullptr;

    return generator.move(dst, generator.newTarget());
}

// ------------------------------ ImportMetaNode ---------------------------------

RegisterID* ImportMetaNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    return generator.emitNode(dst, m_expr);
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

        generator.emitProfileType(local, var, m_position, m_position + m_ident.length());
        return generator.move(dst, local);
    }
    
    JSTextPosition divot = m_start + m_ident.length();
    generator.emitExpressionInfo(divot, m_start, divot);
    RefPtr<RegisterID> scope = generator.emitResolveScope(dst, var);
    RegisterID* finalDest = generator.finalDestination(dst);
    RefPtr<RegisterID> uncheckedResult = generator.newTemporary();
    generator.emitGetFromScope(uncheckedResult.get(), scope.get(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, uncheckedResult.get(), nullptr);
    generator.move(finalDest, uncheckedResult.get());
    generator.emitProfileType(finalDest, var, m_position, m_position + m_ident.length());
    return finalDest;
}

// ------------------------------ TemplateStringNode -----------------------------------

RegisterID* TemplateStringNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return nullptr;
    ASSERT(cooked());
    return generator.emitLoad(dst, JSValue(generator.addStringConstant(*cooked())));
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
        ASSERT(templateString->value()->cooked());
        if (!templateString->value()->cooked()->isEmpty()) {
            temporaryRegisters.append(generator.newTemporary());
            generator.emitNode(temporaryRegisters.last().get(), templateString->value());
        }

        // Evaluate Expression.
        temporaryRegisters.append(generator.newTemporary());
        generator.emitNode(temporaryRegisters.last().get(), templateExpression->value());
        generator.emitToString(temporaryRegisters.last().get(), temporaryRegisters.last().get());
    }

    // Evaluate tail TemplateString.
    ASSERT(templateString->value()->cooked());
    if (!templateString->value()->cooked()->isEmpty()) {
        temporaryRegisters.append(generator.newTemporary());
        generator.emitNode(temporaryRegisters.last().get(), templateString->value());
    }

    if (temporaryRegisters.size() == 1)
        return generator.emitToString(generator.finalDestination(dst, temporaryRegisters[0].get()), temporaryRegisters[0].get());

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
        if (RegisterID* local = var.local()) {
            generator.emitTDZCheckIfNecessary(var, local, nullptr);
            tag = generator.move(generator.newTemporary(), local);
        } else {
            tag = generator.newTemporary();
            base = generator.newTemporary();

            JSTextPosition newDivot = divotStart() + identifier.length();
            generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
            generator.move(base.get(), generator.emitResolveScope(base.get(), var));
            generator.emitGetFromScope(tag.get(), base.get(), var, ThrowIfNotFound);
            generator.emitTDZCheckIfNecessary(var, tag.get(), nullptr);
        }
    } else if (m_tag->isBracketAccessorNode()) {
        BracketAccessorNode* bracket = static_cast<BracketAccessorNode*>(m_tag);
        base = generator.newTemporary();
        base = generator.emitNode(base.get(), bracket->base());
        RefPtr<RegisterID> property = generator.emitNodeForProperty(bracket->subscript());
        if (bracket->base()->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            tag = generator.emitGetByVal(generator.newTemporary(), base.get(), thisValue.get(), property.get());
        } else
            tag = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    } else {
        ASSERT(m_tag->isDotAccessorNode());
        DotAccessorNode* dot = static_cast<DotAccessorNode*>(m_tag);
        base = generator.newTemporary();
        base = generator.emitNode(base.get(), dot->base());
        if (dot->base()->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            tag = generator.emitGetById(generator.newTemporary(), base.get(), thisValue.get(), dot->identifier());
        } else
            tag = generator.emitGetById(generator.newTemporary(), base.get(), dot->identifier());
    }

    RefPtr<RegisterID> templateObject = generator.emitGetTemplateObject(nullptr, this);

    unsigned expressionsCount = 0;
    for (TemplateExpressionListNode* templateExpression = m_templateLiteral->templateExpressions(); templateExpression; templateExpression = templateExpression->next())
        ++expressionsCount;

    CallArguments callArguments(generator, nullptr, 1 + expressionsCount);
    if (base)
        generator.move(callArguments.thisRegister(), base.get());
    else
        generator.emitLoad(callArguments.thisRegister(), jsUndefined());

    unsigned argumentIndex = 0;
    generator.move(callArguments.argumentRegister(argumentIndex++), templateObject.get());
    for (TemplateExpressionListNode* templateExpression = m_templateLiteral->templateExpressions(); templateExpression; templateExpression = templateExpression->next())
        generator.emitNode(callArguments.argumentRegister(argumentIndex++), templateExpression->value());

    return generator.emitCallInTailPosition(generator.finalDestination(dst, tag.get()), tag.get(), expectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
}

// ------------------------------ ArrayNode ------------------------------------

RegisterID* ArrayNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    bool hadVariableExpression = false;
    unsigned length = 0;

    IndexingType recommendedIndexingType = ArrayWithUndecided;
    ElementNode* firstPutElement;
    for (firstPutElement = m_element; firstPutElement; firstPutElement = firstPutElement->next()) {
        if (firstPutElement->elision() || firstPutElement->value()->isSpreadExpression())
            break;
        if (!firstPutElement->value()->isConstant())
            hadVariableExpression = true;
        else {
            JSValue constant = static_cast<ConstantNode*>(firstPutElement->value())->jsValue(generator);
            if (UNLIKELY(!constant))
                hadVariableExpression = true;
            else
                recommendedIndexingType = leastUpperBoundOfIndexingTypeAndValue(recommendedIndexingType, constant);
        }

        ++length;
    }

    auto newArray = [&] (RegisterID* dst, ElementNode* elements, unsigned length, bool hadVariableExpression) {
        if (length && !hadVariableExpression) {
            recommendedIndexingType |= CopyOnWrite;
            ASSERT(generator.vm().heap.isDeferred()); // We run bytecode generator under a DeferGC. If we stopped doing that, we'd need to put a DeferGC here as we filled in these slots.
            auto* array = JSImmutableButterfly::create(generator.vm(), recommendedIndexingType, length);
            unsigned index = 0;
            for (ElementNode* element = elements; index < length; element = element->next()) {
                ASSERT(element->value()->isConstant());
                JSValue constant = static_cast<ConstantNode*>(element->value())->jsValue(generator);
                ASSERT(constant);
                array->setIndex(generator.vm(), index++, constant);
            }
            return generator.emitNewArrayBuffer(dst, array, recommendedIndexingType);
        }
        return generator.emitNewArray(dst, elements, length, recommendedIndexingType);
    };

    if (!firstPutElement && !m_elision)
        return newArray(generator.finalDestination(dst), m_element, length, hadVariableExpression);

    if (firstPutElement && firstPutElement->value()->isSpreadExpression()) {
        bool hasElision = m_elision;
        if (!hasElision) {
            for (ElementNode* node = firstPutElement; node; node = node->next()) {
                if (node->elision()) {
                    hasElision = true;
                    break;
                }
            }
        }

        if (!hasElision)
            return generator.emitNewArrayWithSpread(generator.finalDestination(dst), m_element);
    }

    RefPtr<RegisterID> array = newArray(generator.tempDestination(dst), m_element, length, hadVariableExpression);
    ElementNode* n = firstPutElement;
    for (; n; n = n->next()) {
        if (n->value()->isSpreadExpression())
            goto handleSpread;
        RefPtr<RegisterID> value = generator.emitNode(n->value());
        length += n->elision();

        RefPtr<RegisterID> index = generator.emitLoad(nullptr, jsNumber(length++));
        generator.emitDirectPutByVal(array.get(), index.get(), value.get());
    }

    if (m_elision) {
        RegisterID* value = generator.emitLoad(nullptr, jsNumber(m_elision + length));
        generator.emitPutById(array.get(), generator.propertyNames().length, value);
    }

    return generator.move(dst, array.get());
    
handleSpread:
    RefPtr<RegisterID> index = generator.emitLoad(generator.newTemporary(), jsNumber(length));
    auto spreader = scopedLambda<void(BytecodeGenerator&, RegisterID*)>([array, index](BytecodeGenerator& generator, RegisterID* value)
    {
        generator.emitDirectPutByVal(array.get(), index.get(), value);
        generator.emitInc(index.get());
    });
    for (; n; n = n->next()) {
        if (n->elision())
            generator.emitBinaryOp<OpAdd>(index.get(), index.get(), generator.emitLoad(nullptr, jsNumber(n->elision())), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
        if (n->value()->isSpreadExpression()) {
            SpreadExpressionNode* spread = static_cast<SpreadExpressionNode*>(n->value());
            generator.emitEnumeration(spread, spread->expression(), spreader);
        } else {
            generator.emitDirectPutByVal(array.get(), index.get(), generator.emitNode(n->value()));
            generator.emitInc(index.get());
        }
    }
    
    if (m_elision) {
        generator.emitBinaryOp<OpAdd>(index.get(), index.get(), generator.emitLoad(nullptr, jsNumber(m_elision)), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
        generator.emitPutById(array.get(), generator.propertyNames().length, index.get());
    }
    return generator.move(dst, array.get());
}

bool ArrayNode::isSimpleArray() const
{
    if (m_elision)
        return false;
    for (ElementNode* ptr = m_element; ptr; ptr = ptr->next()) {
        if (ptr->elision())
            return false;
        if (ptr->value()->isSpreadExpression())
            return false;
    }
    return true;
}

ArgumentListNode* ArrayNode::toArgumentList(ParserArena& parserArena, int lineNumber, int startPosition) const
{
    ASSERT(!m_elision);
    ElementNode* ptr = m_element;
    if (!ptr)
        return nullptr;
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
            return nullptr;
        return generator.emitNewObject(generator.finalDestination(dst));
    }
    RefPtr<RegisterID> newObj = generator.emitNewObject(generator.tempDestination(dst));
    generator.emitNode(newObj.get(), m_list);
    return generator.move(dst, newObj.get());
}

// ------------------------------ PropertyListNode -----------------------------

static inline void emitPutHomeObject(BytecodeGenerator& generator, RegisterID* function, RegisterID* homeObject)
{
    generator.emitPutById(function, generator.propertyNames().builtinNames().homeObjectPrivateName(), homeObject);
}

void PropertyListNode::emitDeclarePrivateFieldNames(BytecodeGenerator& generator, RegisterID* scope)
{
    // Walk the list and declare any Private property names (e.g. `#foo`) in the provided scope.
    RefPtr<RegisterID> createPrivateSymbol;
    for (PropertyListNode* p = this; p; p = p->m_next) {
        const PropertyNode& node = *p->m_node;
        if (node.type() & PropertyNode::Private) {
            if (!createPrivateSymbol)
                createPrivateSymbol = generator.moveLinkTimeConstant(nullptr, LinkTimeConstant::createPrivateSymbol);

            CallArguments arguments(generator, nullptr, 0);
            generator.emitLoad(arguments.thisRegister(), jsUndefined());
            RefPtr<RegisterID> symbol = generator.emitCall(generator.finalDestination(nullptr, createPrivateSymbol.get()), createPrivateSymbol.get(), NoExpectedFunction, arguments, position(), position(), position(), DebuggableCall::No);

            Variable var = generator.variable(*node.name());
            generator.emitPutToScope(scope, var, symbol.get(), DoNotThrowIfNotFound, InitializationMode::ConstInitialization);
        }
    }
}

RegisterID* PropertyListNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dstOrConstructor, RegisterID* prototype, Vector<JSTextPosition>* instanceFieldLocations)
{
    PropertyListNode* p = this;
    RegisterID* dst = nullptr;

    // Fast case: this loop just handles regular value properties.
    for (; p && (p->m_node->m_type & PropertyNode::Constant); p = p->m_next) {
        dst = p->m_node->isInstanceClassProperty() ? prototype : dstOrConstructor;

        if (p->isComputedClassField())
            emitSaveComputedFieldName(generator, *p->m_node);

        if (p->isInstanceClassField()) {
            ASSERT(instanceFieldLocations);
            instanceFieldLocations->append(p->position());
            continue;
        }

        emitPutConstantProperty(generator, dst, *p->m_node);
    }

    // Were there any get/set properties?
    if (p) {
        // Build a list of getter/setter pairs to try to put them at the same time. If we encounter
        // a computed property or a spread, just emit everything as that may override previous values.
        bool canOverrideProperties = false;

        typedef std::pair<PropertyNode*, PropertyNode*> GetterSetterPair;
        typedef HashMap<UniquedStringImpl*, GetterSetterPair, IdentifierRepHash> GetterSetterMap;
        GetterSetterMap instanceMap;
        GetterSetterMap staticMap;

        // Build a map, pairing get/set values together.
        for (PropertyListNode* q = p; q; q = q->m_next) {
            PropertyNode* node = q->m_node;
            if (node->m_type & PropertyNode::Computed || node->m_type & PropertyNode::Spread) {
                canOverrideProperties = true;
                break;
            }

            if (node->m_type & PropertyNode::Constant)
                continue;

            // Duplicates are possible.
            GetterSetterPair pair(node, static_cast<PropertyNode*>(nullptr));
            GetterSetterMap& map = node->isStaticClassProperty() ? staticMap : instanceMap;
            GetterSetterMap::AddResult result = map.add(node->name()->impl(), pair);
            auto& resultPair = result.iterator->value;
            if (!result.isNewEntry) {
                if (resultPair.first->m_type == node->m_type) {
                    resultPair.first->setIsOverriddenByDuplicate();
                    resultPair.first = node;
                } else {
                    if (resultPair.second)
                        resultPair.second->setIsOverriddenByDuplicate();
                    resultPair.second = node;
                }
            }
        }

        // Iterate over the remaining properties in the list.
        for (; p; p = p->m_next) {
            PropertyNode* node = p->m_node;
            dst = node->isInstanceClassProperty() ? prototype : dstOrConstructor;

            if (p->isComputedClassField())
                emitSaveComputedFieldName(generator, *p->m_node);

            if (p->isInstanceClassField()) {
                ASSERT(instanceFieldLocations);
                ASSERT(node->m_type & PropertyNode::Constant);
                instanceFieldLocations->append(p->position());
                continue;
            }

            // Handle regular values.
            if (node->m_type & PropertyNode::Constant) {
                emitPutConstantProperty(generator, dst, *node);
                continue;
            } else if (node->m_type & PropertyNode::Spread) {
                generator.emitNode(dst, node->m_assign);
                continue;
            }

            RefPtr<RegisterID> value = generator.emitNode(node->m_assign);
            bool needsSuperBinding = node->needsSuperBinding();
            if (needsSuperBinding)
                emitPutHomeObject(generator, value.get(), dst);

            unsigned attributes = node->isClassProperty() ? (PropertyAttribute::Accessor | PropertyAttribute::DontEnum) : static_cast<unsigned>(PropertyAttribute::Accessor);

            ASSERT(node->m_type & (PropertyNode::Getter | PropertyNode::Setter));

            // This is a get/set property which may be overridden by a computed property or spread later.
            if (canOverrideProperties) {
                // Computed accessors.
                if (node->m_type & PropertyNode::Computed) {
                    RefPtr<RegisterID> propertyName = generator.emitNode(node->m_expression);
                    if (generator.shouldSetFunctionName(node->m_assign)) {
                        propertyName = generator.emitToPropertyKey(generator.newTemporary(), propertyName.get());
                        generator.emitSetFunctionName(value.get(), propertyName.get());
                    }
                    if (node->m_type & PropertyNode::Getter)
                        generator.emitPutGetterByVal(dst, propertyName.get(), attributes, value.get());
                    else
                        generator.emitPutSetterByVal(dst, propertyName.get(), attributes, value.get());
                    continue;
                }

                if (node->m_type & PropertyNode::Getter)
                    generator.emitPutGetterById(dst, *node->name(), attributes, value.get());
                else
                    generator.emitPutSetterById(dst, *node->name(), attributes, value.get());
                continue;
            }

            // This is a get/set property pair.
            GetterSetterMap& map = node->isStaticClassProperty() ? staticMap : instanceMap;
            GetterSetterMap::iterator it = map.find(node->name()->impl());
            ASSERT(it != map.end());
            GetterSetterPair& pair = it->value;

            // Was this already generated as a part of its partner?
            if (pair.second == node || node->isOverriddenByDuplicate())
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

            ASSERT(!pair.second || needsSuperBinding == pair.second->needsSuperBinding());
            if (needsSuperBinding && pair.second)
                emitPutHomeObject(generator, secondReg, dst);

            generator.emitPutGetterSetter(dst, *node->name(), attributes, getterReg.get(), setterReg.get());
        }
    }

    return dstOrConstructor;
}

void PropertyListNode::emitPutConstantProperty(BytecodeGenerator& generator, RegisterID* newObj, PropertyNode& node)
{
    // Private fields are handled in the synthetic instanceFieldInitializer function, not here.
    ASSERT(!(node.type() & PropertyNode::Private));

    bool shouldSetFunctionName = generator.shouldSetFunctionName(node.m_assign);

    RefPtr<RegisterID> propertyName;
    if (!node.name()) {
        propertyName = generator.newTemporary();
        if (shouldSetFunctionName)
            generator.emitToPropertyKey(propertyName.get(), generator.emitNode(node.m_expression));
        else
            generator.emitNode(propertyName.get(), node.m_expression);
    }

    RefPtr<RegisterID> value = generator.emitNode(node.m_assign);
    if (node.needsSuperBinding())
        emitPutHomeObject(generator, value.get(), newObj);

    if (node.isClassProperty()) {
        ASSERT(node.needsSuperBinding());
        if (node.name())
            propertyName = generator.emitLoad(nullptr, *node.name());

        if (shouldSetFunctionName)
            generator.emitSetFunctionName(value.get(), propertyName.get());
        generator.emitCallDefineProperty(newObj, propertyName.get(), value.get(), nullptr, nullptr, BytecodeGenerator::PropertyConfigurable | BytecodeGenerator::PropertyWritable, m_position);
        return;
    }

    if (const auto* identifier = node.name()) {
        ASSERT(!propertyName);
        Optional<uint32_t> optionalIndex = parseIndex(*identifier);
        if (!optionalIndex) {
            generator.emitDirectPutById(newObj, *identifier, value.get(), node.putType());
            return;
        }

        propertyName = generator.emitLoad(nullptr, jsNumber(optionalIndex.value()));
        generator.emitDirectPutByVal(newObj, propertyName.get(), value.get());
        return;
    }

    if (shouldSetFunctionName)
        generator.emitSetFunctionName(value.get(), propertyName.get());
    generator.emitDirectPutByVal(newObj, propertyName.get(), value.get());
}

void PropertyListNode::emitSaveComputedFieldName(BytecodeGenerator& generator, PropertyNode& node)
{
    ASSERT(node.isComputedClassField());
    RefPtr<RegisterID> propertyExpr;

    // The 'name' refers to a synthetic numeric variable name in the private name scope, where the property key is saved for later use.
    const Identifier& description = *node.name();
    Variable var = generator.variable(description);
    ASSERT(!var.local());

    propertyExpr = generator.emitNode(node.m_expression);
    RegisterID* propertyName = generator.emitToPropertyKey(generator.newTemporary(), propertyExpr.get());

    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    generator.emitPutToScope(scope.get(), var, propertyName, ThrowIfNotFound, InitializationMode::ConstInitialization);
}

// ------------------------------ BracketAccessorNode --------------------------------

static bool isNonIndexStringElement(ExpressionNode& element)
{
    return element.isString() && !parseIndex(static_cast<StringNode&>(element).value());
}

RegisterID* BracketAccessorNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_base->isSuperNode()) {
        RefPtr<RegisterID> finalDest = generator.finalDestination(dst);
        RefPtr<RegisterID> thisValue = generator.ensureThis();
        RefPtr<RegisterID> superBase = emitSuperBaseForCallee(generator);

        if (isNonIndexStringElement(*m_subscript)) {
            const Identifier& id = static_cast<StringNode*>(m_subscript)->value();
            generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
            generator.emitGetById(finalDest.get(), superBase.get(), thisValue.get(), id);
        } else  {
            RefPtr<RegisterID> subscript = generator.emitNodeForProperty(m_subscript);
            generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
            generator.emitGetByVal(finalDest.get(), superBase.get(), thisValue.get(), subscript.get());
        }

        generator.emitProfileType(finalDest.get(), divotStart(), divotEnd());
        return finalDest.get();
    }

    RegisterID* ret;
    RefPtr<RegisterID> finalDest = generator.finalDestination(dst);

    bool subscriptIsNonIndexString = isNonIndexStringElement(*m_subscript);
    RefPtr<RegisterID> base = subscriptIsNonIndexString
        ? generator.emitNode(m_base)
        : generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments, m_subscript->isPure(generator));

    if (m_base->isOptionalChainBase())
        generator.emitOptionalCheck(base.get());

    if (subscriptIsNonIndexString) {
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
        ret = generator.emitGetById(finalDest.get(), base.get(), static_cast<StringNode*>(m_subscript)->value());
    } else {
        RegisterID* property = generator.emitNodeForProperty(m_subscript);
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
        ret = generator.emitGetByVal(finalDest.get(), base.get(), property);
    }

    generator.emitProfileType(finalDest.get(), divotStart(), divotEnd());
    return ret;
}

// ------------------------------ DotAccessorNode --------------------------------

RegisterID* DotAccessorNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> finalDest = generator.finalDestination(dst);
    bool baseIsSuper = m_base->isSuperNode();

    RefPtr<RegisterID> base;
    if (baseIsSuper)
        base = emitSuperBaseForCallee(generator);
    else {
        base = generator.emitNode(m_base);
        if (m_base->isOptionalChainBase())
            generator.emitOptionalCheck(base.get());
    }

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* ret = emitGetPropertyValue(generator, finalDest.get(), base.get());

    generator.emitProfileType(finalDest.get(), divotStart(), divotEnd());
    return ret;
}

RegisterID* BaseDotNode::emitGetPropertyValue(BytecodeGenerator& generator, RegisterID* dst, RegisterID* base, RefPtr<RegisterID>& thisValue)
{
    if (isPrivateField()) {
        Variable var = generator.variable(m_ident);
        ASSERT_WITH_MESSAGE(!var.local(), "Private Field names must be stored in captured variables");

        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> privateName = generator.newTemporary();
        generator.emitGetFromScope(privateName.get(), scope.get(), var, DoNotThrowIfNotFound);
        return generator.emitDirectGetByVal(dst, base, privateName.get());
    }

    if (m_base->isSuperNode()) {
        if (!thisValue)
            thisValue = generator.ensureThis();
        return generator.emitGetById(dst, base, thisValue.get(), m_ident);
    }

    return generator.emitGetById(dst, base, m_ident);
}

RegisterID* BaseDotNode::emitGetPropertyValue(BytecodeGenerator& generator, RegisterID* dst, RegisterID* base)
{
    RefPtr<RegisterID> thisValue;
    return emitGetPropertyValue(generator, dst, base, thisValue);
}

RegisterID* BaseDotNode::emitPutProperty(BytecodeGenerator& generator, RegisterID* base, RegisterID* value, RefPtr<RegisterID>& thisValue)
{
    if (isPrivateField()) {
        Variable var = generator.variable(m_ident);
        ASSERT_WITH_MESSAGE(!var.local(), "Private Field names must be stored in captured variables");

        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> privateName = generator.newTemporary();
        generator.emitGetFromScope(privateName.get(), scope.get(), var, DoNotThrowIfNotFound);
        return generator.emitPrivateFieldPut(base, privateName.get(), value);
    }

    if (m_base->isSuperNode()) {
        if (!thisValue)
            thisValue = generator.ensureThis();
        return generator.emitPutById(base, thisValue.get(), m_ident, value);
    }

    return generator.emitPutById(base, m_ident, value);
}

RegisterID* BaseDotNode::emitPutProperty(BytecodeGenerator& generator, RegisterID* base, RegisterID* value)
{
    RefPtr<RegisterID> thisValue;
    return emitPutProperty(generator, base, value, thisValue);
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
    return generator.emitConstruct(returnValue.get(), func.get(), func.get(), expectedFunction, callArguments, divot(), divotStart(), divotEnd());
}

CallArguments::CallArguments(BytecodeGenerator& generator, ArgumentsNode* argumentsNode, unsigned additionalArguments)
    : m_argumentsNode(argumentsNode)
    , m_padding(0)
{
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
    while ((CallFrame::headerSizeInRegisters + m_argv.size()) % stackAlignmentRegisters()) {
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
    if (generator.constructorKind() == ConstructorKind::Extends && generator.needsToUpdateArrowFunctionContext() && generator.isThisUsedInInnerArrowFunction())
        generator.emitLoadThisFromArrowFunctionLexicalEnvironment();

    Variable var = generator.variable(generator.propertyNames().eval);
    RefPtr<RegisterID> local = var.local();
    RefPtr<RegisterID> func;
    if (local) {
        generator.emitTDZCheckIfNecessary(var, local.get(), nullptr);
        func = generator.move(generator.tempDestination(dst), local.get());
    } else
        func = generator.newTemporary();
    CallArguments callArguments(generator, m_args);

    if (local)
        generator.emitLoad(callArguments.thisRegister(), jsUndefined());
    else {
        JSTextPosition newDivot = divotStart() + 4;
        generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
        generator.move(
            callArguments.thisRegister(),
            generator.emitResolveScope(callArguments.thisRegister(), var));
        generator.emitGetFromScope(func.get(), callArguments.thisRegister(), var, ThrowIfNotFound);
        generator.emitTDZCheckIfNecessary(var, func.get(), nullptr);
    }

    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
    if (isOptionalChainBase())
        generator.emitOptionalCheck(func.get());

    return generator.emitCallEval(returnValue.get(), func.get(), callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::No);
}

// ------------------------------ FunctionCallValueNode ----------------------------------

RegisterID* FunctionCallValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr->isSuperNode()) {
        RefPtr<RegisterID> func = emitGetSuperFunctionForConstruct(generator);
        RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
        CallArguments callArguments(generator, m_args);

        ASSERT(generator.isConstructor() || generator.derivedContextType() == DerivedContextType::DerivedConstructorContext);
        ASSERT(generator.constructorKind() == ConstructorKind::Extends || generator.derivedContextType() == DerivedContextType::DerivedConstructorContext);
        RegisterID* ret = generator.emitConstruct(returnValue.get(), func.get(), generator.newTarget(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd());

        bool isConstructorKindDerived = generator.constructorKind() == ConstructorKind::Extends;
        bool doWeUseArrowFunctionInConstructor = isConstructorKindDerived && generator.needsToUpdateArrowFunctionContext();

        if (generator.isDerivedConstructorContext() || (doWeUseArrowFunctionInConstructor && generator.isSuperCallUsedInInnerArrowFunction()))
            generator.emitLoadThisFromArrowFunctionLexicalEnvironment();
        
        Ref<Label> thisIsEmptyLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitIsEmpty(generator.newTemporary(), generator.thisRegister()), thisIsEmptyLabel.get());
        generator.emitThrowReferenceError("'super()' can't be called more than once in a constructor."_s);
        generator.emitLabel(thisIsEmptyLabel.get());

        generator.move(generator.thisRegister(), ret);
        
        if (generator.isDerivedConstructorContext() || doWeUseArrowFunctionInConstructor)
            generator.emitPutThisToArrowFunctionContextScope();

        // Initialize instance fields after super-call.
        if (Options::usePublicClassFields() && generator.needsClassFieldInitializer() == NeedsClassFieldInitializer::Yes) {
            ASSERT(generator.isConstructor() || generator.isDerivedConstructorContext());
            func = generator.emitLoadDerivedConstructor();
            generator.emitInstanceFieldInitializationIfNeeded(generator.thisRegister(), func.get(), divot(), divotStart(), divotEnd());
        }

        return ret;
    }

    RefPtr<RegisterID> func = generator.emitNode(m_expr);
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
    if (isOptionalChainBase())
        generator.emitOptionalCheck(func.get());

    CallArguments callArguments(generator, m_args);
    generator.emitLoad(callArguments.thisRegister(), jsUndefined());
    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), func.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return ret;
}

// ------------------------------ FunctionCallResolveNode ----------------------------------

RegisterID* FunctionCallResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!ASSERT_ENABLED) {
        if (UNLIKELY(m_ident == generator.vm().propertyNames->builtinNames().assertPrivateName()))
            return generator.move(dst, generator.emitLoad(nullptr, jsUndefined()));
    }

    ExpectedFunction expectedFunction = generator.expectedFunctionForIdentifier(m_ident);

    Variable var = generator.variable(m_ident);
    RefPtr<RegisterID> local = var.local();
    RefPtr<RegisterID> func;
    if (local) {
        generator.emitTDZCheckIfNecessary(var, local.get(), nullptr);
        func = generator.move(generator.tempDestination(dst), local.get());
    } else
        func = generator.newTemporary();
    CallArguments callArguments(generator, m_args);

    if (local) {
        generator.emitLoad(callArguments.thisRegister(), jsUndefined());
        // This passes NoExpectedFunction because we expect that if the function is in a
        // local variable, then it's not one of our built-in constructors.
        expectedFunction = NoExpectedFunction;
    } else {
        JSTextPosition newDivot = divotStart() + m_ident.length();
        generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
        generator.move(
            callArguments.thisRegister(),
            generator.emitResolveScope(callArguments.thisRegister(), var));
        generator.emitGetFromScope(func.get(), callArguments.thisRegister(), var, ThrowIfNotFound);
        generator.emitTDZCheckIfNecessary(var, func.get(), nullptr);
    }

    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, func.get());
    if (isOptionalChainBase())
        generator.emitOptionalCheck(func.get());

    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), func.get(), expectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return ret;
}

// ------------------------------ BytecodeIntrinsicNode ----------------------------------

RegisterID* BytecodeIntrinsicNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_entry.type() == BytecodeIntrinsicRegistry::Type::Emitter)
        return (this->*m_entry.emitter())(generator, dst);
    if (dst == generator.ignoredResult())
        return nullptr;
    return generator.moveLinkTimeConstant(dst, m_entry.linkTimeConstant());
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getByIdDirect(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    ASSERT(node->m_expr->isString());
    const Identifier& ident = static_cast<StringNode*>(node->m_expr)->value();
    ASSERT(!node->m_next);
    return generator.emitDirectGetById(generator.finalDestination(dst), base.get(), ident);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getByIdDirectPrivate(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    ASSERT(node->m_expr->isString());
    SymbolImpl* symbol = generator.vm().propertyNames->builtinNames().lookUpPrivateName(static_cast<StringNode*>(node->m_expr)->value());
    ASSERT(symbol);
    ASSERT(!node->m_next);
    return generator.emitDirectGetById(generator.finalDestination(dst), base.get(), generator.parserArena().identifierArena().makeIdentifier(generator.vm(), symbol));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getPrototypeOf(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> value = generator.emitNode(node);
    ASSERT(!node->m_next);
    return generator.emitGetPrototypeOf(generator.finalDestination(dst), value.get());
}

static JSPromise::Field promiseInternalFieldIndex(BytecodeIntrinsicNode* node)
{
    ASSERT(node->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter);
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_promiseFieldFlags)
        return JSPromise::Field::Flags;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_promiseFieldReactionsOrResult)
        return JSPromise::Field::ReactionsOrResult;
    RELEASE_ASSERT_NOT_REACHED();
    return JSPromise::Field::Flags;
}

static JSGenerator::Field generatorInternalFieldIndex(BytecodeIntrinsicNode* node)
{
    ASSERT(node->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter);
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldState)
        return JSGenerator::Field::State;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldNext)
        return JSGenerator::Field::Next;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldThis)
        return JSGenerator::Field::This;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldFrame)
        return JSGenerator::Field::Frame;
    RELEASE_ASSERT_NOT_REACHED();
    return JSGenerator::Field::State;
}

static JSAsyncGenerator::Field asyncGeneratorInternalFieldIndex(BytecodeIntrinsicNode* node)
{
    ASSERT(node->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter);
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldState)
        return JSAsyncGenerator::Field::State;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldNext)
        return JSAsyncGenerator::Field::Next;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldThis)
        return JSAsyncGenerator::Field::This;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_generatorFieldFrame)
        return JSAsyncGenerator::Field::Frame;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_asyncGeneratorFieldSuspendReason)
        return JSAsyncGenerator::Field::SuspendReason;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_asyncGeneratorFieldQueueFirst)
        return JSAsyncGenerator::Field::QueueFirst;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_asyncGeneratorFieldQueueLast)
        return JSAsyncGenerator::Field::QueueLast;
    RELEASE_ASSERT_NOT_REACHED();
    return JSAsyncGenerator::Field::State;
}

static JSArrayIterator::Field arrayIteratorInternalFieldIndex(BytecodeIntrinsicNode* node)
{
    ASSERT(node->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter);
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_arrayIteratorFieldIndex)
        return JSArrayIterator::Field::Index;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_arrayIteratorFieldIteratedObject)
        return JSArrayIterator::Field::IteratedObject;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_arrayIteratorFieldKind)
        return JSArrayIterator::Field::Kind;
    RELEASE_ASSERT_NOT_REACHED();
    return JSArrayIterator::Field::Index;
}

static JSStringIterator::Field stringIteratorInternalFieldIndex(BytecodeIntrinsicNode* node)
{
    ASSERT(node->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter);
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_stringIteratorFieldIndex)
        return JSStringIterator::Field::Index;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_stringIteratorFieldIteratedString)
        return JSStringIterator::Field::IteratedString;
    RELEASE_ASSERT_NOT_REACHED();
    return JSStringIterator::Field::Index;
}

static JSMapIterator::Field mapIteratorInternalFieldIndex(BytecodeIntrinsicNode* node)
{
    ASSERT(node->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter);
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_mapIteratorFieldMapBucket)
        return JSMapIterator::Field::MapBucket;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_mapIteratorFieldKind)
        return JSMapIterator::Field::Kind;
    RELEASE_ASSERT_NOT_REACHED();
    return JSMapIterator::Field::MapBucket;
}

static JSSetIterator::Field setIteratorInternalFieldIndex(BytecodeIntrinsicNode* node)
{
    ASSERT(node->entry().type() == BytecodeIntrinsicRegistry::Type::Emitter);
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_setIteratorFieldSetBucket)
        return JSSetIterator::Field::SetBucket;
    if (node->entry().emitter() == &BytecodeIntrinsicNode::emit_intrinsic_setIteratorFieldKind)
        return JSSetIterator::Field::Kind;
    RELEASE_ASSERT_NOT_REACHED();
    return JSSetIterator::Field::SetBucket;
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getPromiseInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(promiseInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSPromise::numberOfInternalFields);
    ASSERT(!node->m_next);

    return generator.emitGetInternalField(generator.finalDestination(dst), base.get(), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getGeneratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(generatorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSGenerator::numberOfInternalFields);
    ASSERT(!node->m_next);

    return generator.emitGetInternalField(generator.finalDestination(dst), base.get(), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getAsyncGeneratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(asyncGeneratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSAsyncGenerator::numberOfInternalFields);
    ASSERT(!node->m_next);

    return generator.emitGetInternalField(generator.finalDestination(dst), base.get(), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getArrayIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(arrayIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSArrayIterator::numberOfInternalFields);
    ASSERT(!node->m_next);

    return generator.emitGetInternalField(generator.finalDestination(dst), base.get(), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getStringIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(stringIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSStringIterator::numberOfInternalFields);
    ASSERT(!node->m_next);

    return generator.emitGetInternalField(generator.finalDestination(dst), base.get(), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getMapIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(mapIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSMapIterator::numberOfInternalFields);
    ASSERT(!node->m_next);

    return generator.emitGetInternalField(generator.finalDestination(dst), base.get(), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_getSetIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(setIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSSetIterator::numberOfInternalFields);
    ASSERT(!node->m_next);

    return generator.emitGetInternalField(generator.finalDestination(dst), base.get(), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_argument(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    ASSERT(node->m_expr->isNumber());
    double value = static_cast<NumberNode*>(node->m_expr)->value();
    int32_t index = static_cast<int32_t>(value);
    ASSERT(value == index);
    ASSERT(index >= 0);
    ASSERT(!node->m_next);

    // The body functions of generator and async have different mechanism for arguments.
    ASSERT(generator.parseMode() != SourceParseMode::GeneratorBodyMode);
    ASSERT(!isAsyncFunctionBodyParseMode(generator.parseMode()));

    return generator.emitGetArgument(generator.finalDestination(dst), index);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_argumentCount(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(!m_args->m_listNode);

    return generator.emitArgumentCount(generator.finalDestination(dst));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putByIdDirect(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    ASSERT(node->m_expr->isString());
    const Identifier& ident = static_cast<StringNode*>(node->m_expr)->value();
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitDirectPutById(base.get(), ident, value.get(), PropertyNode::KnownDirect));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putByIdDirectPrivate(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    ASSERT(node->m_expr->isString());
    SymbolImpl* symbol = generator.vm().propertyNames->builtinNames().lookUpPrivateName(static_cast<StringNode*>(node->m_expr)->value());
    ASSERT(symbol);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitDirectPutById(base.get(), generator.parserArena().identifierArena().makeIdentifier(generator.vm(), symbol), value.get(), PropertyNode::KnownDirect));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putByValDirect(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RefPtr<RegisterID> index = generator.emitNodeForProperty(node);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitDirectPutByVal(base.get(), index.get(), value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putPromiseInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(promiseInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSPromise::numberOfInternalFields);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitPutInternalField(base.get(), index, value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putGeneratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(generatorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSGenerator::numberOfInternalFields);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitPutInternalField(base.get(), index, value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putAsyncGeneratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(asyncGeneratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSAsyncGenerator::numberOfInternalFields);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitPutInternalField(base.get(), index, value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putArrayIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(arrayIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSArrayIterator::numberOfInternalFields);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitPutInternalField(base.get(), index, value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putStringIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(stringIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSStringIterator::numberOfInternalFields);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitPutInternalField(base.get(), index, value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putMapIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(mapIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSMapIterator::numberOfInternalFields);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitPutInternalField(base.get(), index, value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_putSetIteratorInternalField(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;
    RELEASE_ASSERT(node->m_expr->isBytecodeIntrinsicNode());
    unsigned index = static_cast<unsigned>(setIteratorInternalFieldIndex(static_cast<BytecodeIntrinsicNode*>(node->m_expr)));
    ASSERT(index < JSSetIterator::numberOfInternalFields);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);

    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitPutInternalField(base.get(), index, value.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_tailCallForwardArguments(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> function = generator.emitNode(node);
    node = node->m_next;
    RefPtr<RegisterID> thisRegister = generator.emitNode(node);
    ASSERT(!node->m_next);

    RefPtr<RegisterID> finalDst = generator.finalDestination(dst);
    return generator.emitCallForwardArgumentsInTailPosition(finalDst.get(), function.get(), thisRegister.get(), generator.newTemporary(), 0, divot(), divotStart(), divotEnd(), DebuggableCall::No);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_throwTypeError(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    ASSERT(!node->m_next);
    if (node->m_expr->isString()) {
        const Identifier& ident = static_cast<StringNode*>(node->m_expr)->value();
        generator.emitThrowTypeError(ident);
    } else {
        RefPtr<RegisterID> message = generator.emitNode(node);
        generator.emitThrowStaticError(ErrorTypeWithExtension::TypeError, message.get());
    }
    return dst;
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_throwRangeError(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    ASSERT(!node->m_next);
    if (node->m_expr->isString()) {
        const Identifier& ident = static_cast<StringNode*>(node->m_expr)->value();
        generator.emitThrowRangeError(ident);
    } else {
        RefPtr<RegisterID> message = generator.emitNode(node);
        generator.emitThrowStaticError(ErrorTypeWithExtension::RangeError, message.get());
    }

    return dst;
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_throwOutOfMemoryError(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(!m_args->m_listNode);

    generator.emitThrowOutOfMemoryError();
    return dst;
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_tryGetById(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;

    ASSERT(node->m_expr->isString());
    const Identifier& ident = static_cast<StringNode*>(node->m_expr)->value();
    ASSERT(!node->m_next);

    RefPtr<RegisterID> finalDest = generator.finalDestination(dst);
    return generator.emitTryGetById(finalDest.get(), base.get(), ident);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_tryGetByIdWithWellKnownSymbol(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> base = generator.emitNode(node);
    node = node->m_next;

    ASSERT(node->m_expr->isString());
    SymbolImpl* symbol = generator.vm().propertyNames->builtinNames().lookUpWellKnownSymbol(static_cast<StringNode*>(node->m_expr)->value());
    RELEASE_ASSERT(symbol);
    ASSERT(!node->m_next);

    RefPtr<RegisterID> finalDest = generator.finalDestination(dst);
    return generator.emitTryGetById(finalDest.get(), base.get(), generator.parserArena().identifierArena().makeIdentifier(generator.vm(), symbol));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_toNumber(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> src = generator.emitNode(node);
    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitToNumber(generator.tempDestination(dst), src.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_toString(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> src = generator.emitNode(node);
    ASSERT(!node->m_next);

    return generator.move(dst, generator.emitToString(generator.tempDestination(dst), src.get()));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_toObject(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> src = generator.emitNode(node);
    node = node->m_next;

    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    if (node) {
        ASSERT(node->m_expr->isString());
        const Identifier& message = static_cast<StringNode*>(node->m_expr)->value();
        ASSERT(!node->m_next);
        return generator.move(dst, generator.emitToObject(temp.get(), src.get(), message));
    }
    return generator.move(dst, generator.emitToObject(temp.get(), src.get(), generator.vm().propertyNames->emptyIdentifier));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_idWithProfile(BytecodeGenerator& generator, RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> idValue = generator.newTemporary();
    generator.emitNode(idValue.get(), node);
    SpeculatedType speculation = SpecNone;
    while (node->m_next) {
        node = node->m_next;
        ASSERT(node->m_expr->isString());
        const Identifier& ident = static_cast<StringNode*>(node->m_expr)->value();
        speculation |= speculationFromString(ident.utf8().data());
    }

    return generator.move(dst, generator.emitIdWithProfile(idValue.get(), speculation));
}

#define CREATE_INTRINSIC_FOR_BRAND_CHECK(lowerName, upperName) \
    RegisterID* BytecodeIntrinsicNode::emit_intrinsic_##lowerName(JSC::BytecodeGenerator& generator, JSC::RegisterID* dst) \
    {                                                                                                                      \
        ArgumentListNode* node = m_args->m_listNode;                                                                       \
        RefPtr<RegisterID> src = generator.emitNode(node);                                                                 \
        ASSERT(!node->m_next);                                                                                             \
        return generator.move(dst, generator.emit##upperName(generator.tempDestination(dst), src.get()));                  \
    }

CREATE_INTRINSIC_FOR_BRAND_CHECK(isObject, IsObject)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isCallable, IsCallable)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isConstructor, IsConstructor)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isJSArray, IsJSArray)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isProxyObject, IsProxyObject)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isDerivedArray, IsDerivedArray)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isGenerator, IsGenerator)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isAsyncGenerator, IsAsyncGenerator)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isPromise, IsPromise)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isRegExpObject, IsRegExpObject)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isMap, IsMap)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isSet, IsSet)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isStringIterator, IsStringIterator)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isArrayIterator, IsArrayIterator)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isMapIterator, IsMapIterator)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isSetIterator, IsSetIterator)
CREATE_INTRINSIC_FOR_BRAND_CHECK(isUndefinedOrNull, IsUndefinedOrNull)

#undef CREATE_INTRINSIC_FOR_BRAND_CHECK

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_newArrayWithSize(JSC::BytecodeGenerator& generator, JSC::RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> size = generator.emitNode(node);
    ASSERT(!node->m_next);

    RefPtr<RegisterID> finalDestination = generator.finalDestination(dst);
    generator.emitNewArrayWithSize(finalDestination.get(), size.get());
    return finalDestination.get();
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_createPromise(JSC::BytecodeGenerator& generator, JSC::RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> newTarget = generator.emitNode(node);
    node = node->m_next;
    bool isInternalPromise = static_cast<BooleanNode*>(node->m_expr)->value();
    ASSERT(!node->m_next);

    return generator.emitCreatePromise(generator.finalDestination(dst), newTarget.get(), isInternalPromise);
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_newPromise(JSC::BytecodeGenerator& generator, JSC::RegisterID* dst)
{
    ASSERT(!m_args->m_listNode);
    RefPtr<RegisterID> finalDestination = generator.finalDestination(dst);
    bool isInternalPromise = false;
    generator.emitNewPromise(finalDestination.get(), isInternalPromise);
    return finalDestination.get();
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_createArgumentsButterfly(JSC::BytecodeGenerator& generator, JSC::RegisterID* dst)
{
    ASSERT(!m_args->m_listNode);
    return generator.emitCreateArgumentsButterfly(generator.finalDestination(dst));
}

RegisterID* BytecodeIntrinsicNode::emit_intrinsic_defineEnumerableWritableConfigurableDataProperty(JSC::BytecodeGenerator& generator, JSC::RegisterID* dst)
{
    ArgumentListNode* node = m_args->m_listNode;
    RefPtr<RegisterID> newObj = generator.emitNode(node);
    node = node->m_next;
    RefPtr<RegisterID> propertyNameRegister = generator.emitNode(node);
    node = node->m_next;
    RefPtr<RegisterID> value = generator.emitNode(node);
    ASSERT(!node->m_next);

    generator.emitCallDefineProperty(newObj.get(), propertyNameRegister.get(), value.get(), nullptr, nullptr, BytecodeGenerator::PropertyConfigurable | BytecodeGenerator::PropertyWritable | BytecodeGenerator::PropertyEnumerable, m_position);
    return dst;
}

#define JSC_DECLARE_BYTECODE_INTRINSIC_CONSTANT_GENERATORS(name) \
    RegisterID* BytecodeIntrinsicNode::emit_intrinsic_##name(BytecodeGenerator& generator, RegisterID* dst) \
    { \
        ASSERT(!m_args); \
        ASSERT(type() == Type::Constant); \
        if (dst == generator.ignoredResult()) \
            return nullptr; \
        return generator.emitLoad(dst, generator.vm().bytecodeIntrinsicRegistry().name##Value(generator)); \
    }
    JSC_COMMON_BYTECODE_INTRINSIC_CONSTANTS_EACH_NAME(JSC_DECLARE_BYTECODE_INTRINSIC_CONSTANT_GENERATORS)
#undef JSC_DECLARE_BYTECODE_INTRINSIC_CONSTANT_GENERATORS

// ------------------------------ FunctionCallBracketNode ----------------------------------

RegisterID* FunctionCallBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> function = generator.tempDestination(dst);
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst, function.get());
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

        if (m_base->isOptionalChainBase())
            generator.emitOptionalCheck(base.get());
    }

    RefPtr<RegisterID> thisRegister;
    if (baseIsSuper) {
        // Note that we only need to do this once because we either have a non-TDZ this or we throw. Once we have a non-TDZ this, we can't change its value back to TDZ.
        thisRegister = generator.ensureThis();
    }
    if (subscriptIsNonIndexString) {
        generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
        if (baseIsSuper)
            generator.emitGetById(function.get(), base.get(), thisRegister.get(), static_cast<StringNode*>(m_subscript)->value());
        else
            generator.emitGetById(function.get(), base.get(), static_cast<StringNode*>(m_subscript)->value());
    } else {
        RefPtr<RegisterID> property = generator.emitNodeForProperty(m_subscript);
        generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
        if (baseIsSuper)
            generator.emitGetByVal(function.get(), base.get(), thisRegister.get(), property.get());
        else
            generator.emitGetByVal(function.get(), base.get(), property.get());
    }
    if (isOptionalChainBase())
        generator.emitOptionalCheck(function.get());

    CallArguments callArguments(generator, m_args);
    if (baseIsSuper) {
        generator.emitTDZCheck(generator.thisRegister());
        generator.move(callArguments.thisRegister(), thisRegister.get());
    } else
        generator.move(callArguments.thisRegister(), base.get());
    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
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
        generator.move(callArguments.thisRegister(), generator.ensureThis());
    else {
        generator.emitNode(callArguments.thisRegister(), m_base);
        if (m_base->isOptionalChainBase())
            generator.emitOptionalCheck(callArguments.thisRegister());
    }
    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());

    RefPtr<RegisterID> base = baseIsSuper ? emitSuperBaseForCallee(generator) : callArguments.thisRegister();
    emitGetPropertyValue(generator, function.get(), base.get());

    if (isOptionalChainBase())
        generator.emitOptionalCheck(function.get());

    RegisterID* ret = generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return ret;
}

static constexpr size_t maxDistanceToInnermostCallOrApply = 2;

RegisterID* CallFunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst);
    RefPtr<RegisterID> base = generator.emitNode(m_base);

    if (m_base->isOptionalChainBase())
        generator.emitOptionalCheck(base.get());

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());

    RefPtr<RegisterID> function;
    auto makeFunction = [&] {
        if (m_base->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            function = generator.emitGetById(generator.tempDestination(dst), base.get(), thisValue.get(), generator.propertyNames().builtinNames().callPublicName());
        } else
            function = generator.emitGetById(generator.tempDestination(dst), base.get(), generator.propertyNames().builtinNames().callPublicName());

        if (isOptionalChainBase())
            generator.emitOptionalCheck(function.get());
    };

    bool emitCallCheck = !generator.isBuiltinFunction();
    if (m_distanceToInnermostCallOrApply > maxDistanceToInnermostCallOrApply && emitCallCheck) {
        makeFunction();
        CallArguments callArguments(generator, m_args);
        generator.move(callArguments.thisRegister(), base.get());
        generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        generator.move(dst, returnValue.get());
        return returnValue.get();
    }

    Ref<Label> realCall = generator.newLabel();
    Ref<Label> end = generator.newLabel();

    if (emitCallCheck) {
        makeFunction();
        generator.emitJumpIfNotFunctionCall(function.get(), realCall.get());
    }
    {
        if (m_args->m_listNode && m_args->m_listNode->m_expr && m_args->m_listNode->m_expr->isSpreadExpression()) {
            SpreadExpressionNode* spread = static_cast<SpreadExpressionNode*>(m_args->m_listNode->m_expr);
            ExpressionNode* subject = spread->expression();
            RefPtr<RegisterID> argumentsRegister;
            argumentsRegister = generator.emitNode(subject);
            generator.emitExpressionInfo(spread->divot(), spread->divotStart(), spread->divotEnd());
            RefPtr<RegisterID> thisRegister = generator.emitGetByVal(generator.newTemporary(), argumentsRegister.get(), generator.emitLoad(nullptr, jsNumber(0)));
            generator.emitCallVarargsInTailPosition(returnValue.get(), base.get(), thisRegister.get(), argumentsRegister.get(), generator.newTemporary(), 1, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        } else if (m_args->m_listNode && m_args->m_listNode->m_expr) {
            ArgumentListNode* oldList = m_args->m_listNode;
            m_args->m_listNode = m_args->m_listNode->m_next;

            RefPtr<RegisterID> realFunction = generator.move(generator.tempDestination(dst), base.get());
            CallArguments callArguments(generator, m_args);
            generator.emitNode(callArguments.thisRegister(), oldList->m_expr);
            generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
            m_args->m_listNode = oldList;
        } else {
            RefPtr<RegisterID> realFunction = generator.move(generator.tempDestination(dst), base.get());
            CallArguments callArguments(generator, m_args);
            generator.emitLoad(callArguments.thisRegister(), jsUndefined());
            generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        }
    }
    if (emitCallCheck) {
        generator.emitJump(end.get());
        generator.emitLabel(realCall.get());
        {
            CallArguments callArguments(generator, m_args);
            generator.move(callArguments.thisRegister(), base.get());
            generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        }
        generator.emitLabel(end.get());
    }
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return returnValue.get();
}

RegisterID* HasOwnPropertyFunctionCallDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> returnValue = generator.finalDestination(dst);
    RefPtr<RegisterID> base = generator.emitNode(m_base);

    if (m_base->isOptionalChainBase())
        generator.emitOptionalCheck(base.get());

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());

    RefPtr<RegisterID> function = generator.emitGetById(generator.newTemporary(), base.get(), generator.propertyNames().hasOwnProperty);
    if (isOptionalChainBase())
        generator.emitOptionalCheck(function.get());

    RELEASE_ASSERT(m_args->m_listNode && m_args->m_listNode->m_expr && !m_args->m_listNode->m_next);  
    ExpressionNode* argument = m_args->m_listNode->m_expr;
    RELEASE_ASSERT(argument->isResolveNode());
    StructureForInContext* structureContext = nullptr;
    Variable argumentVariable = generator.variable(static_cast<ResolveNode*>(argument)->identifier());
    if (argumentVariable.isLocal()) {
        RegisterID* property = argumentVariable.local();
        structureContext = generator.findStructureForInContext(property);
    }

    auto canUseFastHasOwnProperty = [&] {
        if (!structureContext)
            return false;
        if (!structureContext->baseVariable())
            return false;
        if (m_base->isResolveNode())
            return generator.variable(static_cast<ResolveNode*>(m_base)->identifier()) == structureContext->baseVariable().value();
        if (m_base->isThisNode()) {
            // After generator.ensureThis (which must be invoked in |base|'s materialization), we can ensure that |this| is in local this-register.
            ASSERT(base);
            return generator.variable(generator.propertyNames().builtinNames().thisPrivateName(), ThisResolutionType::Local) == structureContext->baseVariable().value();
        }
        return false;
    };

    if (canUseFastHasOwnProperty()) {
        // It is possible that base register is variable and each for-in body replaces JS object in the base register with a different one.
        // Even though, this is OK since HasOwnStructureProperty will reject the replaced JS object.
        Ref<Label> realCall = generator.newLabel();
        Ref<Label> end = generator.newLabel();

        unsigned branchInsnOffset = generator.emitWideJumpIfNotFunctionHasOwnProperty(function.get(), realCall.get());
        generator.emitHasOwnStructureProperty(returnValue.get(), base.get(), generator.emitNode(argument), structureContext->enumerator());
        generator.emitJump(end.get());

        generator.emitLabel(realCall.get());
        {
            CallArguments callArguments(generator, m_args);
            generator.move(callArguments.thisRegister(), base.get());
            generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        }

        generator.emitLabel(end.get());

        generator.recordHasOwnStructurePropertyInForInLoop(*structureContext, branchInsnOffset, realCall);
    } else {
        CallArguments callArguments(generator, m_args);
        generator.move(callArguments.thisRegister(), base.get());
        generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
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

    RefPtr<RegisterID> returnValue = generator.finalDestination(dst);
    RefPtr<RegisterID> base = generator.emitNode(m_base);

    if (m_base->isOptionalChainBase())
        generator.emitOptionalCheck(base.get());

    RefPtr<RegisterID> function;
    auto makeFunction = [&] {
        if (m_base->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            function = generator.emitGetById(generator.tempDestination(dst), base.get(), thisValue.get(), generator.propertyNames().builtinNames().applyPublicName());
        } else
            function = generator.emitGetById(generator.tempDestination(dst), base.get(), generator.propertyNames().builtinNames().applyPublicName());

        if (isOptionalChainBase())
            generator.emitOptionalCheck(function.get());
    };

    bool emitCallCheck = !generator.isBuiltinFunction();
    if (m_distanceToInnermostCallOrApply > maxDistanceToInnermostCallOrApply && emitCallCheck) {
        makeFunction();
        CallArguments callArguments(generator, m_args);
        generator.move(callArguments.thisRegister(), base.get());
        generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        generator.move(dst, returnValue.get());
        return returnValue.get();
    }

    Ref<Label> realCall = generator.newLabel();
    Ref<Label> end = generator.newLabel();
    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    if (emitCallCheck) {
        makeFunction();
        ASSERT(!m_base->isResolveNode() || static_cast<ResolveNode*>(m_base)->identifier() != "Reflect");
        generator.emitJumpIfNotFunctionApply(function.get(), realCall.get());
    }
    if (mayBeCall) {
        if (m_args->m_listNode && m_args->m_listNode->m_expr) {
            ArgumentListNode* oldList = m_args->m_listNode;
            if (m_args->m_listNode->m_expr->isSpreadExpression()) {
                SpreadExpressionNode* spread = static_cast<SpreadExpressionNode*>(m_args->m_listNode->m_expr);
                RefPtr<RegisterID> realFunction = generator.move(generator.newTemporary(), base.get());
                RefPtr<RegisterID> index = generator.emitLoad(generator.newTemporary(), jsNumber(0));
                RefPtr<RegisterID> thisRegister = generator.emitLoad(generator.newTemporary(), jsUndefined());
                RefPtr<RegisterID> argumentsRegister = generator.emitLoad(generator.newTemporary(), jsUndefined());
                
                auto extractor = scopedLambda<void(BytecodeGenerator&, RegisterID*)>([&thisRegister, &argumentsRegister, &index](BytecodeGenerator& generator, RegisterID* value)
                {
                    Ref<Label> haveThis = generator.newLabel();
                    Ref<Label> end = generator.newLabel();
                    RefPtr<RegisterID> compareResult = generator.newTemporary();
                    RefPtr<RegisterID> indexZeroCompareResult = generator.emitBinaryOp<OpEq>(compareResult.get(), index.get(), generator.emitLoad(nullptr, jsNumber(0)), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
                    generator.emitJumpIfFalse(indexZeroCompareResult.get(), haveThis.get());
                    generator.move(thisRegister.get(), value);
                    generator.emitLoad(index.get(), jsNumber(1));
                    generator.emitJump(end.get());
                    generator.emitLabel(haveThis.get());
                    RefPtr<RegisterID> indexOneCompareResult = generator.emitBinaryOp<OpEq>(compareResult.get(), index.get(), generator.emitLoad(nullptr, jsNumber(1)), OperandTypes(ResultType::numberTypeIsInt32(), ResultType::numberTypeIsInt32()));
                    generator.emitJumpIfFalse(indexOneCompareResult.get(), end.get());
                    generator.move(argumentsRegister.get(), value);
                    generator.emitLoad(index.get(), jsNumber(2));
                    generator.emitLabel(end.get());
                });
                generator.emitEnumeration(this, spread->expression(), extractor);
                generator.emitCallVarargsInTailPosition(returnValue.get(), realFunction.get(), thisRegister.get(), argumentsRegister.get(), generator.newTemporary(), 0, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
            } else if (m_args->m_listNode->m_next) {
                ASSERT(m_args->m_listNode->m_next->m_expr->isSimpleArray());
                ASSERT(!m_args->m_listNode->m_next->m_next);
                m_args->m_listNode = static_cast<ArrayNode*>(m_args->m_listNode->m_next->m_expr)->toArgumentList(generator.parserArena(), 0, 0);
                RefPtr<RegisterID> realFunction = generator.move(generator.tempDestination(dst), base.get());
                CallArguments callArguments(generator, m_args);
                generator.emitNode(callArguments.thisRegister(), oldList->m_expr);
                generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
            } else {
                m_args->m_listNode = m_args->m_listNode->m_next;
                RefPtr<RegisterID> realFunction = generator.move(generator.tempDestination(dst), base.get());
                CallArguments callArguments(generator, m_args);
                generator.emitNode(callArguments.thisRegister(), oldList->m_expr);
                generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
            }
            m_args->m_listNode = oldList;
        } else {
            RefPtr<RegisterID> realFunction = generator.move(generator.tempDestination(dst), base.get());
            CallArguments callArguments(generator, m_args);
            generator.emitLoad(callArguments.thisRegister(), jsUndefined());
            generator.emitCallInTailPosition(returnValue.get(), realFunction.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        }
    } else {
        ASSERT(m_args->m_listNode && m_args->m_listNode->m_next);
        RefPtr<RegisterID> realFunction = generator.move(generator.tempDestination(dst), base.get());
        RefPtr<RegisterID> thisRegister = generator.emitNode(m_args->m_listNode->m_expr);
        RefPtr<RegisterID> argsRegister;
        ArgumentListNode* args = m_args->m_listNode->m_next;
        argsRegister = generator.emitNode(args->m_expr);

        // Function.prototype.apply ignores extra arguments, but we still
        // need to evaluate them for side effects.
        while ((args = args->m_next))
            generator.emitNode(args->m_expr);

        generator.emitCallVarargsInTailPosition(returnValue.get(), realFunction.get(), thisRegister.get(), argsRegister.get(), generator.newTemporary(), 0, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
    }
    if (emitCallCheck) {
        generator.emitJump(end.get());
        generator.emitLabel(realCall.get());
        CallArguments callArguments(generator, m_args);
        generator.move(callArguments.thisRegister(), base.get());
        generator.emitCallInTailPosition(returnValue.get(), function.get(), NoExpectedFunction, callArguments, divot(), divotStart(), divotEnd(), DebuggableCall::Yes);
        generator.emitLabel(end.get());
    }
    generator.emitProfileType(returnValue.get(), divotStart(), divotEnd());
    return returnValue.get();
}

// ------------------------------ PostfixNode ----------------------------------

static RegisterID* emitIncOrDec(BytecodeGenerator& generator, RegisterID* srcDst, Operator oper)
{
    return (oper == Operator::PlusPlus) ? generator.emitInc(srcDst) : generator.emitDec(srcDst);
}

static RegisterID* emitPostIncOrDec(BytecodeGenerator& generator, RegisterID* dst, RegisterID* srcDst, Operator oper)
{
    if (dst == srcDst)
        return generator.emitToNumeric(generator.finalDestination(dst), srcDst);
    RefPtr<RegisterID> tmp = generator.emitToNumeric(generator.newTemporary(), srcDst);
    RefPtr<RegisterID> result = generator.tempDestination(srcDst);
    generator.move(result.get(), tmp.get());
    emitIncOrDec(generator, result.get(), oper);
    generator.move(srcDst, result.get());
    return generator.move(dst, tmp.get());
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
            localReg = generator.move(generator.tempDestination(dst), local);
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
        generator.emitPutToScope(scope.get(), var, value.get(), ThrowIfNotFound, InitializationMode::NotInitialization);
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
    RefPtr<RegisterID> property = generator.emitNodeForProperty(subscript);

    generator.emitExpressionInfo(bracketAccessor->divot(), bracketAccessor->divotStart(), bracketAccessor->divotEnd());
    RefPtr<RegisterID> value;
    RefPtr<RegisterID> thisValue;
    if (baseNode->isSuperNode()) {
        thisValue = generator.ensureThis();
        value = generator.emitGetByVal(generator.newTemporary(), base.get(), thisValue.get(), property.get());
    } else
        value = generator.emitGetByVal(generator.newTemporary(), base.get(), property.get());
    RegisterID* oldValue = emitPostIncOrDec(generator, generator.tempDestination(dst), value.get(), m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (baseNode->isSuperNode())
        generator.emitPutByVal(base.get(), thisValue.get(), property.get(), value.get());
    else
        generator.emitPutByVal(base.get(), property.get(), value.get());
    generator.emitProfileType(value.get(), divotStart(), divotEnd());
    return generator.move(dst, oldValue);
}

RegisterID* PostfixNode::emitDot(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        return PrefixNode::emitDot(generator, dst);

    ASSERT(m_expr->isDotAccessorNode());
    DotAccessorNode* dotAccessor = static_cast<DotAccessorNode*>(m_expr);
    ExpressionNode* baseNode = dotAccessor->base();
    bool baseIsSuper = baseNode->isSuperNode();
    const Identifier& ident = dotAccessor->identifier();

    RefPtr<RegisterID> base = generator.emitNode(baseNode);

    generator.emitExpressionInfo(dotAccessor->divot(), dotAccessor->divotStart(), dotAccessor->divotEnd());

    if (dotAccessor->isPrivateField()) {
        ASSERT(!baseIsSuper);
        Variable var = generator.variable(ident);
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> privateName = generator.newTemporary();
        generator.emitGetFromScope(privateName.get(), scope.get(), var, DoNotThrowIfNotFound);

        RefPtr<RegisterID> value = generator.emitDirectGetByVal(generator.newTemporary(), base.get(), privateName.get());
        RefPtr<RegisterID> oldValue = emitPostIncOrDec(generator, generator.tempDestination(dst), value.get(), m_operator);
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
        generator.emitPrivateFieldPut(base.get(), privateName.get(), value.get());
        generator.emitProfileType(value.get(), divotStart(), divotEnd());
        return generator.move(dst, oldValue.get());
    }

    RefPtr<RegisterID> value;
    RefPtr<RegisterID> thisValue;
    if (baseIsSuper) {
        thisValue = generator.ensureThis();
        value = generator.emitGetById(generator.newTemporary(), base.get(), thisValue.get(), ident);
    } else
        value = generator.emitGetById(generator.newTemporary(), base.get(), ident);
    RegisterID* oldValue = emitPostIncOrDec(generator, generator.tempDestination(dst), value.get(), m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (baseIsSuper)
        generator.emitPutById(base.get(), thisValue.get(), ident, value.get());
    else
        generator.emitPutById(base.get(), ident, value.get());
    generator.emitProfileType(value.get(), divotStart(), divotEnd());
    return generator.move(dst, oldValue);
}

RegisterID* PostfixNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr->isResolveNode())
        return emitResolve(generator, dst);

    if (m_expr->isBracketAccessorNode())
        return emitBracket(generator, dst);

    if (m_expr->isDotAccessorNode())
        return emitDot(generator, dst);

    ASSERT(m_expr->isFunctionCall());
    return emitThrowReferenceError(generator, m_operator == Operator::PlusPlus
        ? "Postfix ++ operator applied to value that is not a reference."_s
        : "Postfix -- operator applied to value that is not a reference."_s,
        dst);
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
    RefPtr<RegisterID> finalDest = generator.finalDestination(dst);
    RefPtr<RegisterID> r0 = generator.emitNode(m_base);

    if (m_base->isOptionalChainBase())
        generator.emitOptionalCheck(r0.get());

    RefPtr<RegisterID> r1 = generator.emitNode(m_subscript);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (m_base->isSuperNode())
        return emitThrowReferenceError(generator, "Cannot delete a super property", dst);
    return generator.emitDeleteByVal(finalDest.get(), r0.get(), r1.get());
}

// ------------------------------ DeleteDotNode -----------------------------------

RegisterID* DeleteDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> finalDest = generator.finalDestination(dst);
    RefPtr<RegisterID> r0 = generator.emitNode(m_base);

    if (m_base->isOptionalChainBase())
        generator.emitOptionalCheck(r0.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (m_base->isSuperNode())
        return emitThrowReferenceError(generator, "Cannot delete a super property", dst);
    return generator.emitDeleteById(finalDest.get(), r0.get(), m_ident);
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
        return nullptr;
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
            return nullptr;
        return generator.emitTypeOf(generator.finalDestination(dst), local);
    }

    RefPtr<RegisterID> scope = generator.emitResolveScope(dst, var);
    RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, DoNotThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, value.get(), nullptr);
    if (dst == generator.ignoredResult())
        return nullptr;
    return generator.emitTypeOf(generator.finalDestination(dst, scope.get()), value.get());
}

// ------------------------------ TypeOfValueNode -----------------------------------

RegisterID* TypeOfValueNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult()) {
        generator.emitNode(generator.ignoredResult(), m_expr);
        return nullptr;
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
            localReg = generator.move(generator.tempDestination(dst), localReg.get());
        } else if (generator.shouldEmitTypeProfilerHooks()) {
            RefPtr<RegisterID> tempDst = generator.tempDestination(dst);
            generator.move(tempDst.get(), localReg.get());
            emitIncOrDec(generator, tempDst.get(), m_operator);
            generator.move(localReg.get(), tempDst.get());
            generator.emitProfileType(localReg.get(), var, divotStart(), divotEnd());
            return generator.move(dst, tempDst.get());
        }
        emitIncOrDec(generator, localReg.get(), m_operator);
        return generator.move(dst, localReg.get());
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
        generator.emitPutToScope(scope.get(), var, value.get(), ThrowIfNotFound, InitializationMode::NotInitialization);
        generator.emitProfileType(value.get(), var, divotStart(), divotEnd());
    }
    return generator.move(dst, value.get());
}

RegisterID* PrefixNode::emitBracket(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr->isBracketAccessorNode());
    BracketAccessorNode* bracketAccessor = static_cast<BracketAccessorNode*>(m_expr);
    ExpressionNode* baseNode = bracketAccessor->base();
    ExpressionNode* subscript = bracketAccessor->subscript();

    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(baseNode, bracketAccessor->subscriptHasAssignments(), subscript->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForProperty(subscript);
    RefPtr<RegisterID> propDst = generator.tempDestination(dst);

    generator.emitExpressionInfo(bracketAccessor->divot(), bracketAccessor->divotStart(), bracketAccessor->divotEnd());
    RegisterID* value;
    RefPtr<RegisterID> thisValue;
    if (baseNode->isSuperNode()) {
        thisValue = generator.ensureThis();
        value = generator.emitGetByVal(propDst.get(), base.get(), thisValue.get(), property.get());
    } else
        value = generator.emitGetByVal(propDst.get(), base.get(), property.get());
    emitIncOrDec(generator, value, m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (baseNode->isSuperNode())
        generator.emitPutByVal(base.get(), thisValue.get(), property.get(), value);
    else
        generator.emitPutByVal(base.get(), property.get(), value);
    generator.emitProfileType(value, divotStart(), divotEnd());
    return generator.move(dst, propDst.get());
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
    RegisterID* value;
    if (dotAccessor->isPrivateField()) {
        ASSERT(!baseNode->isSuperNode());
        Variable var = generator.variable(ident);
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> privateName = generator.newTemporary();
        generator.emitGetFromScope(privateName.get(), scope.get(), var, DoNotThrowIfNotFound);

        value = generator.emitDirectGetByVal(propDst.get(), base.get(), privateName.get());
        emitIncOrDec(generator, value, m_operator);
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
        generator.emitPrivateFieldPut(base.get(), privateName.get(), value);
        generator.emitProfileType(value, divotStart(), divotEnd());
        return generator.move(dst, propDst.get());
    }

    RefPtr<RegisterID> thisValue;
    if (baseNode->isSuperNode()) {
        thisValue = generator.ensureThis();
        value = generator.emitGetById(propDst.get(), base.get(), thisValue.get(), ident);
    } else
        value = generator.emitGetById(propDst.get(), base.get(), ident);
    emitIncOrDec(generator, value, m_operator);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (baseNode->isSuperNode())
        generator.emitPutById(base.get(), thisValue.get(), ident, value);
    else
        generator.emitPutById(base.get(), ident, value);
    generator.emitProfileType(value, divotStart(), divotEnd());
    return generator.move(dst, propDst.get());
}

RegisterID* PrefixNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr->isResolveNode())
        return emitResolve(generator, dst);

    if (m_expr->isBracketAccessorNode())
        return emitBracket(generator, dst);

    if (m_expr->isDotAccessorNode())
        return emitDot(generator, dst);

    ASSERT(m_expr->isFunctionCall());
    return emitThrowReferenceError(generator, m_operator == Operator::PlusPlus
        ? "Prefix ++ operator applied to value that is not a reference."_s
        : "Prefix -- operator applied to value that is not a reference."_s,
        dst);
}

// ------------------------------ Unary Operation Nodes -----------------------------------

RegisterID* UnaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src = generator.emitNode(m_expr);
    generator.emitExpressionInfo(position(), position(), position());
    return generator.emitUnaryOp(opcodeID(), generator.finalDestination(dst), src.get(), m_expr->resultDescriptor());
}

// ------------------------------ UnaryPlusNode -----------------------------------

RegisterID* UnaryPlusNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(opcodeID() == op_to_number);
    RefPtr<RegisterID> src = generator.emitNode(expr());
    generator.emitExpressionInfo(position(), position(), position());
    return generator.emitToNumber(generator.finalDestination(dst), src.get());
}
 
// ------------------------------ LogicalNotNode -----------------------------------

void LogicalNotNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label& trueTarget, Label& falseTarget, FallThroughMode fallThroughMode)
{
    if (UNLIKELY(needsDebugHook()))
        generator.emitDebugHook(this);

    // Reverse the true and false targets.
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
        leftMostAddChildTempRegister = nullptr;

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
            leftMostAddChildTempRegister = nullptr; // Only do this once.
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

void BinaryOpNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label& trueTarget, Label& falseTarget, FallThroughMode fallThroughMode)
{
    TriState branchCondition;
    ExpressionNode* branchExpression;
    tryFoldToBranch(generator, branchCondition, branchExpression);

    if (UNLIKELY(needsDebugHook())) {
        if (branchCondition != TriState::Indeterminate)
            generator.emitDebugHook(this);
    }

    if (branchCondition == TriState::Indeterminate)
        ExpressionNode::emitBytecodeInConditionContext(generator, trueTarget, falseTarget, fallThroughMode);
    else if (branchCondition == TriState::True)
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
    branchCondition = TriState::Indeterminate;
    branchExpression = nullptr;

    ConstantNode* constant = nullptr;
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
    if (UNLIKELY(!value))
        return;
    bool canFoldToBranch = JSC::canFoldToBranch(opcodeID, branchExpression, value);
    if (!canFoldToBranch)
        return;

    if (opcodeID == op_eq || opcodeID == op_stricteq)
        branchCondition = triState(value.pureToBoolean() != TriState::False);
    else if (opcodeID == op_neq || opcodeID == op_nstricteq)
        branchCondition = triState(value.pureToBoolean() == TriState::False);
}

RegisterID* BinaryOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    OpcodeID opcodeID = this->opcodeID();

    if (opcodeID == op_less || opcodeID == op_lesseq || opcodeID == op_greater || opcodeID == op_greatereq) {
        auto isUInt32 = [&] (ExpressionNode* node) -> Optional<UInt32Result> {
            if (node->isBinaryOpNode() && static_cast<BinaryOpNode*>(node)->opcodeID() == op_urshift)
                return UInt32Result::UInt32;
            if (node->isNumber() && static_cast<NumberNode*>(node)->isIntegerNode()) {
                auto value = jsNumber(static_cast<NumberNode*>(node)->value());
                if (value.isInt32() && value.asInt32() >= 0)
                    return UInt32Result::Constant;
            }
            return WTF::nullopt;
        };
        auto leftResult = isUInt32(m_expr1);
        auto rightResult = isUInt32(m_expr2);
        if ((leftResult && rightResult) && (leftResult.value() == UInt32Result::UInt32 || rightResult.value() == UInt32Result::UInt32)) {
            auto* left = m_expr1;
            auto* right = m_expr2;
            if (left->isBinaryOpNode()) {
                ASSERT(static_cast<BinaryOpNode*>(left)->opcodeID() == op_urshift);
                static_cast<BinaryOpNode*>(left)->m_shouldToUnsignedResult = false;
            }
            if (right->isBinaryOpNode()) {
                ASSERT(static_cast<BinaryOpNode*>(right)->opcodeID() == op_urshift);
                static_cast<BinaryOpNode*>(right)->m_shouldToUnsignedResult = false;
            }
            RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(left, m_rightHasAssignments, right->isPure(generator));
            RefPtr<RegisterID> src2 = generator.emitNode(right);
            generator.emitExpressionInfo(position(), position(), position());

            // Since the both sides only accept Int32, replacing operands is not observable to users.
            bool replaceOperands = false;
            OpcodeID resultOp = opcodeID;
            switch (opcodeID) {
            case op_less:
                resultOp = op_below;
                break;
            case op_lesseq:
                resultOp = op_beloweq;
                break;
            case op_greater:
                resultOp = op_below;
                replaceOperands = true;
                break;
            case op_greatereq:
                resultOp = op_beloweq;
                replaceOperands = true;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            OperandTypes operandTypes(left->resultDescriptor(), right->resultDescriptor());
            if (replaceOperands) {
                std::swap(src1, src2);
                operandTypes = OperandTypes(right->resultDescriptor(), left->resultDescriptor());
            }
            return generator.emitBinaryOp(resultOp, generator.finalDestination(dst, src1.get()), src1.get(), src2.get(), operandTypes);
        }
    }

    if (opcodeID == op_add && m_expr1->isAdd() && m_expr1->resultDescriptor().definitelyIsString()) {
        generator.emitExpressionInfo(position(), position(), position());
        return emitStrcat(generator, dst);
    }

    if (opcodeID == op_neq) {
        if (m_expr1->isNull() || m_expr2->isNull()) {
            RefPtr<RegisterID> src = generator.tempDestination(dst);
            generator.emitNode(src.get(), m_expr1->isNull() ? m_expr2 : m_expr1);
            return generator.emitUnaryOp<OpNeqNull>(generator.finalDestination(dst, src.get()), src.get());
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
            generator.emitEqualityOp<OpEq>(generator.finalDestination(tmp.get(), src1.get()), src1.get(), src2.get());
        else if (opcodeID == op_nstricteq)
            generator.emitEqualityOp<OpStricteq>(generator.finalDestination(tmp.get(), src1.get()), src1.get(), src2.get());
        else
            RELEASE_ASSERT_NOT_REACHED();
        return generator.emitUnaryOp<OpNot>(generator.finalDestination(dst, tmp.get()), tmp.get());
    }
    RegisterID* result = generator.emitBinaryOp(opcodeID, generator.finalDestination(dst, src1.get()), src1.get(), src2.get(), OperandTypes(left->resultDescriptor(), right->resultDescriptor()));
    if (m_shouldToUnsignedResult) {
        if (opcodeID == op_urshift && dst != generator.ignoredResult())
            return generator.emitUnaryOp<OpUnsigned>(result, result);
    }
    return result;
}

RegisterID* EqualNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (m_expr1->isNull() || m_expr2->isNull()) {
        RefPtr<RegisterID> src = generator.tempDestination(dst);
        generator.emitNode(src.get(), m_expr1->isNull() ? m_expr2 : m_expr1);
        return generator.emitUnaryOp<OpEqNull>(generator.finalDestination(dst, src.get()), src.get());
    }

    ExpressionNode* left = m_expr1;
    ExpressionNode* right = m_expr2;
    if (left->isString())
        std::swap(left, right);

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(left, m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> src2 = generator.emitNode(right);
    return generator.emitEqualityOp<OpEq>(generator.finalDestination(dst, src1.get()), src1.get(), src2.get());
}

RegisterID* StrictEqualNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ExpressionNode* left = m_expr1;
    ExpressionNode* right = m_expr2;
    if (left->isString())
        std::swap(left, right);

    RefPtr<RegisterID> src1 = generator.emitNodeForLeftHandSide(left, m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> src2 = generator.emitNode(right);
    return generator.emitEqualityOp<OpStricteq>(generator.finalDestination(dst, src1.get()), src1.get(), src2.get());
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
    Ref<Label> custom = generator.newLabel();
    Ref<Label> done = generator.newLabel();
    Ref<Label> typeError = generator.newLabel();

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitIsObject(isObject.get(), constructor.get());
    generator.emitJumpIfFalse(isObject.get(), typeError.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitGetById(hasInstanceValue.get(), constructor.get(), generator.vm().propertyNames->hasInstanceSymbol);

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitOverridesHasInstance(isCustom.get(), constructor.get(), hasInstanceValue.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitJumpIfTrue(isCustom.get(), custom.get());

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    generator.emitGetById(prototype.get(), constructor.get(), generator.vm().propertyNames->prototype);

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

// ------------------------------ InNode ----------------------------

RegisterID* InNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (isNonIndexStringElement(*m_expr1)) {
        RefPtr<RegisterID> base = generator.emitNode(m_expr2);
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
        return generator.emitInById(generator.finalDestination(dst, base.get()), base.get(), static_cast<StringNode*>(m_expr1)->value());
    }

    RefPtr<RegisterID> key = generator.emitNodeForLeftHandSide(m_expr1, m_rightHasAssignments, m_expr2->isPure(generator));
    RefPtr<RegisterID> base = generator.emitNode(m_expr2);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    return generator.emitInByVal(generator.finalDestination(dst, key.get()), key.get(), base.get());
}


// ------------------------------ LogicalOpNode ----------------------------

RegisterID* LogicalOpNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    Ref<Label> target = generator.newLabel();
    
    generator.emitNode(temp.get(), m_expr1);
    if (m_operator == LogicalOperator::And)
        generator.emitJumpIfFalse(temp.get(), target.get());
    else
        generator.emitJumpIfTrue(temp.get(), target.get());
    generator.emitNodeInTailPosition(temp.get(), m_expr2);
    generator.emitLabel(target.get());

    return generator.move(dst, temp.get());
}

void LogicalOpNode::emitBytecodeInConditionContext(BytecodeGenerator& generator, Label& trueTarget, Label& falseTarget, FallThroughMode fallThroughMode)
{
    if (UNLIKELY(needsDebugHook()))
        generator.emitDebugHook(this);

    Ref<Label> afterExpr1 = generator.newLabel();
    if (m_operator == LogicalOperator::And)
        generator.emitNodeInConditionContext(m_expr1, afterExpr1.get(), falseTarget, FallThroughMeansTrue);
    else 
        generator.emitNodeInConditionContext(m_expr1, trueTarget, afterExpr1.get(), FallThroughMeansFalse);
    generator.emitLabel(afterExpr1.get());

    generator.emitNodeInConditionContext(m_expr2, trueTarget, falseTarget, fallThroughMode);
}

// ------------------------------ CoalesceNode ----------------------------

RegisterID* CoalesceNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> temp = generator.tempDestination(dst);
    Ref<Label> endLabel = generator.newLabel();

    if (m_hasAbsorbedOptionalChain)
        generator.pushOptionalChainTarget();
    generator.emitNode(temp.get(), m_expr1);
    generator.emitJumpIfFalse(generator.emitIsUndefinedOrNull(generator.newTemporary(), temp.get()), endLabel.get());

    if (m_hasAbsorbedOptionalChain)
        generator.popOptionalChainTarget();
    generator.emitNodeInTailPosition(temp.get(), m_expr2);

    generator.emitLabel(endLabel.get());
    return generator.move(dst, temp.get());
}

// ------------------------------ OptionalChainNode ----------------------------

RegisterID* OptionalChainNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> finalDest = generator.finalDestination(dst);

    if (m_isOutermost)
        generator.pushOptionalChainTarget();
    generator.emitNodeInTailPosition(finalDest.get(), m_expr);
    if (m_isOutermost)
        generator.popOptionalChainTarget(finalDest.get(), m_expr->isDeleteNode());

    return finalDest.get();
}

// ------------------------------ ConditionalNode ------------------------------

RegisterID* ConditionalNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> newDst = generator.finalDestination(dst);
    Ref<Label> beforeElse = generator.newLabel();
    Ref<Label> afterElse = generator.newLabel();

    Ref<Label> beforeThen = generator.newLabel();
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
static ALWAYS_INLINE RegisterID* emitReadModifyAssignment(BytecodeGenerator& generator, RegisterID* dst, RegisterID* src1, ExpressionNode* m_right, Operator oper, OperandTypes types, ReadModifyResolveNode* emitExpressionInfoForMe = nullptr, Variable* emitReadOnlyExceptionIfNeededForMe = nullptr)
{
    OpcodeID opcodeID;
    switch (oper) {
    case Operator::MultEq:
        opcodeID = op_mul;
        break;
    case Operator::DivEq:
        opcodeID = op_div;
        break;
    case Operator::PlusEq:
        if (m_right->isAdd() && m_right->resultDescriptor().definitelyIsString()) {
            RegisterID* result = static_cast<AddNode*>(m_right)->emitStrcat(generator, dst, src1, emitExpressionInfoForMe);
            if (emitReadOnlyExceptionIfNeededForMe)
                generator.emitReadOnlyExceptionIfNeeded(*emitReadOnlyExceptionIfNeededForMe);
            return result;
        }

        opcodeID = op_add;
        break;
    case Operator::MinusEq:
        opcodeID = op_sub;
        break;
    case Operator::LShift:
        opcodeID = op_lshift;
        break;
    case Operator::RShift:
        opcodeID = op_rshift;
        break;
    case Operator::URShift:
        opcodeID = op_urshift;
        break;
    case Operator::BitAndEq:
        opcodeID = op_bitand;
        break;
    case Operator::BitXOrEq:
        opcodeID = op_bitxor;
        break;
    case Operator::BitOrEq:
        opcodeID = op_bitor;
        break;
    case Operator::ModEq:
        opcodeID = op_mod;
        break;
    case Operator::PowEq:
        opcodeID = op_pow;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return dst;
    }

    RegisterID* src2 = generator.emitNode(m_right);

    if (emitReadOnlyExceptionIfNeededForMe) {
        bool threwException = generator.emitReadOnlyExceptionIfNeeded(*emitReadOnlyExceptionIfNeededForMe);
        if (threwException)
            return src2;
    }

    // Certain read-modify nodes require expression info to be emitted *after* m_right has been generated.
    // If this is required the node is passed as 'emitExpressionInfoForMe'; do so now.
    if (emitExpressionInfoForMe)
        generator.emitExpressionInfo(emitExpressionInfoForMe->divot(), emitExpressionInfoForMe->divotStart(), emitExpressionInfoForMe->divotEnd());

    RegisterID* result = generator.emitBinaryOp(opcodeID, dst, src1, src2, types);
    if (oper == Operator::URShift)
        return generator.emitUnaryOp<OpUnsigned>(result, result);
    return result;
}

RegisterID* ReadModifyResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    JSTextPosition newDivot = divotStart() + m_ident.length();
    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local, nullptr);
        if (var.isReadOnly()) {
            RegisterID* result = emitReadModifyAssignment(generator, generator.finalDestination(dst), local, m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()), nullptr, &var);
            generator.emitProfileType(result, divotStart(), divotEnd());
            return result;
        }
        
        if (generator.leftHandSideNeedsCopy(m_rightHasAssignments, m_right->isPure(generator))) {
            RefPtr<RegisterID> result = generator.newTemporary();
            generator.move(result.get(), local);
            emitReadModifyAssignment(generator, result.get(), result.get(), m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
            generator.move(local, result.get());
            generator.emitProfileType(local, divotStart(), divotEnd());
            return generator.move(dst, result.get());
        }
        
        RegisterID* result = emitReadModifyAssignment(generator, local, local, m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));
        generator.emitProfileType(result, divotStart(), divotEnd());
        return generator.move(dst, result);
    }

    generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, value.get(), nullptr);
    RefPtr<RegisterID> result = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()), this, var.isReadOnly() ? &var : nullptr);
    RegisterID* returnResult = result.get();
    if (!var.isReadOnly()) {
        returnResult = generator.emitPutToScope(scope.get(), var, result.get(), ThrowIfNotFound, InitializationMode::NotInitialization);
        generator.emitProfileType(result.get(), var, divotStart(), divotEnd());
    }
    return returnResult;
}

// ------------------------------ ShortCircuitReadModifyResolveNode -----------------------------------

static ALWAYS_INLINE void emitShortCircuitAssignment(BytecodeGenerator& generator, RegisterID* value, Operator oper, Label& afterAssignment)
{
    switch (oper) {
    case Operator::CoalesceEq:
        generator.emitJumpIfFalse(generator.emitIsUndefinedOrNull(generator.newTemporary(), value), afterAssignment);
        break;

    case Operator::OrEq:
        generator.emitJumpIfTrue(value, afterAssignment);
        break;

    case Operator::AndEq:
        generator.emitJumpIfFalse(value, afterAssignment);
        break;

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

RegisterID* ShortCircuitReadModifyResolveNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    JSTextPosition newDivot = divotStart() + m_ident.length();

    Variable var = generator.variable(m_ident);
    bool isReadOnly = var.isReadOnly();

    if (RefPtr<RegisterID> local = var.local()) {
        generator.emitTDZCheckIfNecessary(var, local.get(), nullptr);

        if (isReadOnly) {
            RefPtr<RegisterID> result = local;

            Ref<Label> afterAssignment = generator.newLabel();
            emitShortCircuitAssignment(generator, result.get(), m_operator, afterAssignment.get());

            result = generator.emitNode(result.get(), m_right); // Execute side effects first.
            bool threwException = generator.emitReadOnlyExceptionIfNeeded(var);

            if (!threwException)
                generator.emitProfileType(result.get(), divotStart(), divotEnd());

            generator.emitLabel(afterAssignment.get());
            return generator.move(dst, result.get());
        }

        if (generator.leftHandSideNeedsCopy(m_rightHasAssignments, m_right->isPure(generator))) {
            RefPtr<RegisterID> result = generator.tempDestination(dst);
            generator.move(result.get(), local.get());

            Ref<Label> afterAssignment = generator.newLabel();
            emitShortCircuitAssignment(generator, result.get(), m_operator, afterAssignment.get());

            result = generator.emitNode(result.get(), m_right);
            generator.move(local.get(), result.get());
            generator.emitProfileType(result.get(), var, divotStart(), divotEnd());

            generator.emitLabel(afterAssignment.get());
            return generator.move(dst, result.get());
        }

        RefPtr<RegisterID> result = local;

        Ref<Label> afterAssignment = generator.newLabel();
        emitShortCircuitAssignment(generator, result.get(), m_operator, afterAssignment.get());

        result = generator.emitNode(result.get(), m_right);
        generator.emitProfileType(result.get(), var, divotStart(), divotEnd());

        generator.emitLabel(afterAssignment.get());
        return generator.move(dst, result.get());
    }

    generator.emitExpressionInfo(newDivot, divotStart(), newDivot);
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);

    RefPtr<RegisterID> result = generator.emitGetFromScope(generator.tempDestination(dst), scope.get(), var, ThrowIfNotFound);
    generator.emitTDZCheckIfNecessary(var, result.get(), nullptr);

    Ref<Label> afterAssignment = generator.newLabel();
    emitShortCircuitAssignment(generator, result.get(), m_operator, afterAssignment.get());

    generator.emitNode(result.get(), m_right); // Execute side effects first.

    bool threwException = isReadOnly ? generator.emitReadOnlyExceptionIfNeeded(var) : false;

    if (!threwException)
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());

    if (!isReadOnly) {
        result = generator.emitPutToScope(scope.get(), var, result.get(), ThrowIfNotFound, InitializationMode::NotInitialization);
        generator.emitProfileType(result.get(), var, divotStart(), divotEnd());
    }

    generator.emitLabel(afterAssignment.get());
    return generator.move(dst, result.get());
}

// ------------------------------ AssignResolveNode -----------------------------------

static InitializationMode initializationModeForAssignmentContext(AssignmentContext assignmentContext)
{
    switch (assignmentContext) {
    case AssignmentContext::DeclarationStatement:
        return InitializationMode::Initialization;
    case AssignmentContext::ConstDeclarationStatement:
        return InitializationMode::ConstInitialization;
    case AssignmentContext::AssignmentExpression:
        return InitializationMode::NotInitialization;
    }

    ASSERT_NOT_REACHED();
    return InitializationMode::NotInitialization;
}

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
            generator.move(local, tempDst.get());
            generator.emitProfileType(local, var, divotStart(), divotEnd());
            result = generator.move(dst, tempDst.get());
        } else {
            RegisterID* right = generator.emitNode(local, m_right);
            generator.emitProfileType(right, var, divotStart(), divotEnd());
            result = generator.move(dst, right);
        }

        if (m_assignmentContext == AssignmentContext::DeclarationStatement || m_assignmentContext == AssignmentContext::ConstDeclarationStatement)
            generator.liftTDZCheckIfPossible(var);
        return result;
    }

    if (generator.ecmaMode().isStrict())
        generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    if (m_assignmentContext == AssignmentContext::AssignmentExpression)
        generator.emitTDZCheckIfNecessary(var, nullptr, scope.get());
    if (dst == generator.ignoredResult())
        dst = nullptr;
    RefPtr<RegisterID> result = generator.emitNode(dst, m_right); // Execute side effects first.
    if (isReadOnly) {
        bool threwException = generator.emitReadOnlyExceptionIfNeeded(var);
        if (threwException)
            return result.get();
    }
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* returnResult = result.get();
    if (!isReadOnly) {
        returnResult = generator.emitPutToScope(scope.get(), var, result.get(), generator.ecmaMode().isStrict() ? ThrowIfNotFound : DoNotThrowIfNotFound, initializationModeForAssignmentContext(m_assignmentContext));
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
    RefPtr<RegisterID> forwardResult = (dst == generator.ignoredResult()) ? result.get() : generator.move(generator.tempDestination(result.get()), result.get());
    emitPutProperty(generator, base.get(), forwardResult.get());
    generator.emitProfileType(forwardResult.get(), divotStart(), divotEnd());
    return generator.move(dst, forwardResult.get());
}

// ------------------------------ ReadModifyDotNode -----------------------------------

RegisterID* ReadModifyDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    RefPtr<RegisterID> thisValue;
    RefPtr<RegisterID> value = emitGetPropertyValue(generator, generator.tempDestination(dst), base.get(), thisValue);

    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RefPtr<RegisterID> ret = emitPutProperty(generator, base.get(), updatedValue, thisValue);
    generator.emitProfileType(updatedValue, divotStart(), divotEnd());
    return ret.get();
}

// ------------------------------ ShortCircuitReadModifyDotNode -----------------------------------

RegisterID* ShortCircuitReadModifyDotNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> thisValue;

    RefPtr<RegisterID> result;

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    if (m_base->isSuperNode()) {
        thisValue = generator.ensureThis();
        result = generator.emitGetById(generator.tempDestination(dst), base.get(), thisValue.get(), m_ident);
    } else
        result = generator.emitGetById(generator.tempDestination(dst), base.get(), m_ident);

    Ref<Label> afterAssignment = generator.newLabel();
    emitShortCircuitAssignment(generator, result.get(), m_operator, afterAssignment.get());

    result = generator.emitNode(result.get(), m_right);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (m_base->isSuperNode())
        result = generator.emitPutById(base.get(), thisValue.get(), m_ident, result.get());
    else
        result = generator.emitPutById(base.get(), m_ident, result.get());
    generator.emitProfileType(result.get(), divotStart(), divotEnd());

    generator.emitLabel(afterAssignment.get());
    return generator.move(dst, result.get());
}

// ------------------------------ AssignErrorNode -----------------------------------

RegisterID* AssignErrorNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    return emitThrowReferenceError(generator, "Left side of assignment is not a reference."_s, dst);
}

// ------------------------------ AssignBracketNode -----------------------------------

RegisterID* AssignBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSideForProperty(m_subscript, m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> value = generator.destinationForAssignResult(dst);
    RefPtr<RegisterID> result = generator.emitNode(value.get(), m_right);

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    RegisterID* forwardResult = (dst == generator.ignoredResult()) ? result.get() : generator.move(generator.tempDestination(result.get()), result.get());

    if (isNonIndexStringElement(*m_subscript)) {
        if (m_base->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            generator.emitPutById(base.get(), thisValue.get(), static_cast<StringNode*>(m_subscript)->value(), forwardResult);
        } else
            generator.emitPutById(base.get(), static_cast<StringNode*>(m_subscript)->value(), forwardResult);
    } else {
        if (m_base->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            generator.emitPutByVal(base.get(), thisValue.get(), property.get(), forwardResult);
        } else
            generator.emitPutByVal(base.get(), property.get(), forwardResult);
    }

    generator.emitProfileType(forwardResult, divotStart(), divotEnd());
    return generator.move(dst, forwardResult);
}

// ------------------------------ ReadModifyBracketNode -----------------------------------

RegisterID* ReadModifyBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSideForProperty(m_subscript, m_rightHasAssignments, m_right->isPure(generator));

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    RefPtr<RegisterID> value;
    RefPtr<RegisterID> thisValue;
    if (m_base->isSuperNode()) {
        thisValue = generator.ensureThis();
        value = generator.emitGetByVal(generator.tempDestination(dst), base.get(), thisValue.get(), property.get());
    } else
        value = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());
    RegisterID* updatedValue = emitReadModifyAssignment(generator, generator.finalDestination(dst, value.get()), value.get(), m_right, m_operator, OperandTypes(ResultType::unknownType(), m_right->resultDescriptor()));

    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (m_base->isSuperNode())
        generator.emitPutByVal(base.get(), thisValue.get(), property.get(), updatedValue);
    else
        generator.emitPutByVal(base.get(), property.get(), updatedValue);
    generator.emitProfileType(updatedValue, divotStart(), divotEnd());

    return updatedValue;
}

// ------------------------------ ShortCircuitReadModifyBracketNode -----------------------------------

RegisterID* ShortCircuitReadModifyBracketNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(m_base, m_subscriptHasAssignments || m_rightHasAssignments, m_subscript->isPure(generator) && m_right->isPure(generator));
    RefPtr<RegisterID> property = generator.emitNodeForLeftHandSideForProperty(m_subscript, m_rightHasAssignments, m_right->isPure(generator));
    RefPtr<RegisterID> thisValue;

    RefPtr<RegisterID> result;

    generator.emitExpressionInfo(subexpressionDivot(), subexpressionStart(), subexpressionEnd());
    if (m_base->isSuperNode()) {
        thisValue = generator.ensureThis();
        result = generator.emitGetByVal(generator.tempDestination(dst), base.get(), thisValue.get(), property.get());
    } else
        result = generator.emitGetByVal(generator.tempDestination(dst), base.get(), property.get());

    Ref<Label> afterAssignment = generator.newLabel();
    emitShortCircuitAssignment(generator, result.get(), m_operator, afterAssignment.get());

    result = generator.emitNode(result.get(), m_right);
    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
    if (m_base->isSuperNode())
        result = generator.emitPutByVal(base.get(), thisValue.get(), property.get(), result.get());
    else
        result = generator.emitPutByVal(base.get(), property.get(), result.get());
    generator.emitProfileType(result.get(), divotStart(), divotEnd());

    generator.emitLabel(afterAssignment.get());
    return generator.move(dst, result.get());
}

// ------------------------------ CommaNode ------------------------------------

RegisterID* CommaNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    DebugHookType debugHookType = isOnlyChildOfStatement() ? WillExecuteStatement : WillExecuteExpression;

    CommaNode* node = this;
    for (; node->next(); node = node->next()) {
        generator.emitDebugHook(debugHookType, node->m_expr->position());
        generator.emitNode(generator.ignoredResult(), node->m_expr);
    }
    generator.emitDebugHook(debugHookType, node->m_expr->position());
    return generator.emitNodeInTailPosition(dst, node->m_expr);
}

// ------------------------------ SourceElements -------------------------------

inline void SourceElements::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    StatementNode* lastStatementWithCompletionValue = nullptr;
    if (generator.shouldBeConcernedWithCompletionValue()) {
        for (StatementNode* statement = m_head; statement; statement = statement->next()) {
            if (statement->hasCompletionValue())
                lastStatementWithCompletionValue = statement;
        }
    }

    for (StatementNode* statement = m_head; statement; statement = statement->next()) {
        if (statement == lastStatementWithCompletionValue)
            generator.emitLoad(dst, jsUndefined());
        generator.emitNodeInTailPosition(dst, statement);
    }
}

// ------------------------------ BlockNode ------------------------------------

void BlockNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_statements)
        return;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested);
    m_statements->emitBytecode(generator, dst);
    generator.popLexicalScope(this);
}

// ------------------------------ EmptyStatementNode ---------------------------

void EmptyStatementNode::emitBytecode(BytecodeGenerator&, RegisterID*)
{
    RELEASE_ASSERT(needsDebugHook());
}

// ------------------------------ DebuggerStatementNode ---------------------------

void DebuggerStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitDebugHook(DidReachDebuggerStatement, position());
}

// ------------------------------ ExprStatementNode ----------------------------

void ExprStatementNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(m_expr);
    generator.emitNode(dst, m_expr);
}

// ------------------------------ DeclarationStatement ----------------------------

void DeclarationStatement::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    ASSERT(m_expr);
    generator.emitNode(m_expr);
}

// ------------------------------ EmptyVarExpression ----------------------------

RegisterID* EmptyVarExpression::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    // It's safe to return null here because this node will always be a child node of DeclarationStatement which ignores our return value.
    if (!generator.shouldEmitTypeProfilerHooks())
        return nullptr;

    Variable var = generator.variable(m_ident);
    if (RegisterID* local = var.local())
        generator.emitProfileType(local, var, position(), position() + m_ident.length());
    else {
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> value = generator.emitGetFromScope(generator.newTemporary(), scope.get(), var, DoNotThrowIfNotFound);
        generator.emitProfileType(value.get(), var, position(), position() + m_ident.length());
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
        generator.emitProfileType(local, var, position(), position() + m_ident.length());
    } else {
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> value = generator.emitLoad(nullptr, jsUndefined());
        generator.emitPutToScope(scope.get(), var, value.get(), generator.ecmaMode().isStrict() ? ThrowIfNotFound : DoNotThrowIfNotFound, InitializationMode::Initialization);
        generator.emitProfileType(value.get(), var, position(), position() + m_ident.length()); 
    }

    generator.liftTDZCheckIfPossible(var);

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
    if (generator.shouldBeConcernedWithCompletionValue()) {
        if (m_ifBlock->hasEarlyBreakOrContinue() || (m_elseBlock && m_elseBlock->hasEarlyBreakOrContinue()))
            generator.emitLoad(dst, jsUndefined());
    }

    Ref<Label> beforeThen = generator.newLabel();
    Ref<Label> beforeElse = generator.newLabel();
    Ref<Label> afterElse = generator.newLabel();

    Label* trueTarget = beforeThen.ptr();
    Label& falseTarget = beforeElse.get();
    FallThroughMode fallThroughMode = FallThroughMeansTrue;
    bool didFoldIfBlock = tryFoldBreakAndContinue(generator, m_ifBlock, trueTarget, fallThroughMode);

    generator.emitNodeInConditionContext(m_condition, *trueTarget, falseTarget, fallThroughMode);
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
    if (generator.shouldBeConcernedWithCompletionValue() && m_statement->hasEarlyBreakOrContinue())
        generator.emitLoad(dst, jsUndefined());

    Ref<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    Ref<Label> topOfLoop = generator.newLabel();
    generator.emitLabel(topOfLoop.get());
    generator.emitLoopHint();

    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(*scope->continueTarget());
    generator.emitNodeInConditionContext(m_expr, topOfLoop.get(), scope->breakTarget(), FallThroughMeansFalse);

    generator.emitLabel(scope->breakTarget());
}

// ------------------------------ WhileNode ------------------------------------

void WhileNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (generator.shouldBeConcernedWithCompletionValue() && m_statement->hasEarlyBreakOrContinue())
        generator.emitLoad(dst, jsUndefined());

    Ref<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);
    Ref<Label> topOfLoop = generator.newLabel();

    generator.emitNodeInConditionContext(m_expr, topOfLoop.get(), scope->breakTarget(), FallThroughMeansTrue);

    generator.emitLabel(topOfLoop.get());
    generator.emitLoopHint();
    
    generator.emitProfileControlFlow(m_statement->startOffset());
    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(*scope->continueTarget());

    generator.emitNodeInConditionContext(m_expr, topOfLoop.get(), scope->breakTarget(), FallThroughMeansFalse);

    generator.emitLabel(scope->breakTarget());

    generator.emitProfileControlFlow(m_statement->endOffset() + (m_statement->isBlock() ? 1 : 0));
}

// ------------------------------ ForNode --------------------------------------

void ForNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (generator.shouldBeConcernedWithCompletionValue() && m_statement->hasEarlyBreakOrContinue())
        generator.emitLoad(dst, jsUndefined());

    Ref<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);

    RegisterID* forLoopSymbolTable = nullptr;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested, &forLoopSymbolTable);

    if (m_expr1)
        generator.emitNode(generator.ignoredResult(), m_expr1);

    Ref<Label> topOfLoop = generator.newLabel();
    if (m_expr2)
        generator.emitNodeInConditionContext(m_expr2, topOfLoop.get(), scope->breakTarget(), FallThroughMeansTrue);

    generator.emitLabel(topOfLoop.get());
    generator.emitLoopHint();
    generator.emitProfileControlFlow(m_statement->startOffset());

    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(*scope->continueTarget());
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
    auto lambdaEmitResolveVariable = [&] (const Identifier& ident) {
        Variable var = generator.variable(ident);
        if (RegisterID* local = var.local()) {
            if (var.isReadOnly())
                generator.emitReadOnlyExceptionIfNeeded(var);
            generator.move(local, propertyName);
        } else {
            if (generator.ecmaMode().isStrict())
                generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
            if (var.isReadOnly())
                generator.emitReadOnlyExceptionIfNeeded(var);
            RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
            generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
            generator.emitPutToScope(scope.get(), var, propertyName, generator.ecmaMode().isStrict() ? ThrowIfNotFound : DoNotThrowIfNotFound, InitializationMode::NotInitialization);
        }
        generator.emitProfileType(propertyName, var, m_lexpr->position(), m_lexpr->position() + ident.length());
    };

    if (m_lexpr->isResolveNode()) {
        const Identifier& ident = static_cast<ResolveNode*>(m_lexpr)->identifier();
        lambdaEmitResolveVariable(ident);
        return;
    }

    if (m_lexpr->isAssignResolveNode()) {
        const Identifier& ident = static_cast<AssignResolveNode*>(m_lexpr)->identifier();
        lambdaEmitResolveVariable(ident);
        return;
    }

    if (m_lexpr->isDotAccessorNode()) {
        DotAccessorNode* assignNode = static_cast<DotAccessorNode*>(m_lexpr);
        const Identifier& ident = assignNode->identifier();
        RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
        generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
        if (assignNode->base()->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            generator.emitPutById(base.get(), thisValue.get(), ident, propertyName);
        } else
            generator.emitPutById(base.get(), ident, propertyName);
        generator.emitProfileType(propertyName, assignNode->divotStart(), assignNode->divotEnd());
        return;
    }
    if (m_lexpr->isBracketAccessorNode()) {
        BracketAccessorNode* assignNode = static_cast<BracketAccessorNode*>(m_lexpr);
        RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
        RefPtr<RegisterID> subscript = generator.emitNodeForProperty(assignNode->subscript());
        generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
        if (assignNode->base()->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            generator.emitPutByVal(base.get(), thisValue.get(), subscript.get(), propertyName);
        } else
            generator.emitPutByVal(base.get(), subscript.get(), propertyName);
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
        generator.move(var.local(), propertyName);
        generator.emitProfileType(propertyName, var, simpleBinding->divotStart(), simpleBinding->divotEnd());
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void ForInNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_lexpr->isAssignResolveNode() && !m_lexpr->isAssignmentLocation()) {
        emitThrowReferenceError(generator, "Left side of for-in statement is not a reference."_s);
        return;
    }

    if (generator.shouldBeConcernedWithCompletionValue() && m_statement->hasEarlyBreakOrContinue())
        generator.emitLoad(dst, jsUndefined());

    Ref<Label> end = generator.newLabel();

    RegisterID* forLoopSymbolTable = nullptr;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested, &forLoopSymbolTable);

    if (m_lexpr->isAssignResolveNode())
        generator.emitNode(generator.ignoredResult(), m_lexpr);

    RefPtr<RegisterID> base = generator.newTemporary();
    RefPtr<RegisterID> length;
    RefPtr<RegisterID> enumerator;

    generator.emitNode(base.get(), m_expr);
    RefPtr<RegisterID> local = this->tryGetBoundLocal(generator);
    RefPtr<RegisterID> enumeratorIndex;

    Optional<Variable> baseVariable;
    if (m_expr->isResolveNode())
        baseVariable = generator.variable(static_cast<ResolveNode*>(m_expr)->identifier());
    else if (m_expr->isThisNode()) {
        // After generator.ensureThis (which must be invoked in |base|'s materialization), we can ensure that |this| is in local this-register.
        ASSERT(base);
        baseVariable = generator.variable(generator.propertyNames().builtinNames().thisPrivateName(), ThisResolutionType::Local);
    }

    // Pause at the assignment expression for each for..in iteration.
    generator.emitDebugHook(m_lexpr);

    int profilerStartOffset = m_statement->startOffset();
    int profilerEndOffset = m_statement->endOffset() + (m_statement->isBlock() ? 1 : 0);

    enumerator = generator.emitGetPropertyEnumerator(generator.newTemporary(), base.get());

    BytecodeGenerator::PreservedTDZStack preservedTDZStack;
    generator.preserveTDZStack(preservedTDZStack);

    // Indexed property loop.
    {
        Ref<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);
        Ref<Label> loopStart = generator.newLabel();
        Ref<Label> loopEnd = generator.newLabel();

        length = generator.emitGetEnumerableLength(generator.newTemporary(), enumerator.get());
        RefPtr<RegisterID> i = generator.emitLoad(generator.newTemporary(), jsNumber(0));
        RefPtr<RegisterID> propertyName = generator.newTemporary();

        generator.emitLabel(loopStart.get());
        generator.emitLoopHint();

        RefPtr<RegisterID> result = generator.emitEqualityOp<OpLess>(generator.newTemporary(), i.get(), length.get());
        generator.emitJumpIfFalse(result.get(), loopEnd.get());
        generator.emitHasIndexedProperty(result.get(), base.get(), i.get());
        generator.emitJumpIfFalse(result.get(), *scope->continueTarget());

        generator.emitToIndexString(propertyName.get(), i.get());
        this->emitLoopHeader(generator, propertyName.get());

        generator.emitProfileControlFlow(profilerStartOffset);

        generator.pushIndexedForInScope(local.get(), i.get());
        generator.emitNode(dst, m_statement);
        generator.popIndexedForInScope(local.get());

        generator.emitProfileControlFlow(profilerEndOffset);

        generator.emitLabel(*scope->continueTarget());
        generator.prepareLexicalScopeForNextForLoopIteration(this, forLoopSymbolTable);
        generator.emitInc(i.get());
        generator.emitDebugHook(m_lexpr); // Pause at the assignment expression for each for..in iteration.
        generator.emitJump(loopStart.get());

        generator.emitLabel(scope->breakTarget());
        generator.emitJump(end.get());
        generator.emitLabel(loopEnd.get());
    }
    generator.restoreTDZStack(preservedTDZStack);

    // Structure property loop.
    {
        Ref<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);
        Ref<Label> loopStart = generator.newLabel();
        Ref<Label> loopEnd = generator.newLabel();

        enumeratorIndex = generator.emitLoad(generator.newTemporary(), jsNumber(0));
        RefPtr<RegisterID> propertyName = generator.newTemporary();
        generator.emitEnumeratorStructurePropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());

        generator.emitLabel(loopStart.get());
        generator.emitLoopHint();

        RefPtr<RegisterID> result = generator.emitIsNull(generator.newTemporary(), propertyName.get());
        generator.emitJumpIfTrue(result.get(), loopEnd.get());
        generator.emitHasStructureProperty(result.get(), base.get(), propertyName.get(), enumerator.get());
        generator.emitJumpIfFalse(result.get(), *scope->continueTarget());

        this->emitLoopHeader(generator, propertyName.get());

        generator.emitProfileControlFlow(profilerStartOffset);

        generator.pushStructureForInScope(local.get(), enumeratorIndex.get(), propertyName.get(), enumerator.get(), baseVariable);
        generator.emitNode(dst, m_statement);
        generator.popStructureForInScope(local.get());

        generator.emitProfileControlFlow(profilerEndOffset);

        generator.emitLabel(*scope->continueTarget());
        generator.prepareLexicalScopeForNextForLoopIteration(this, forLoopSymbolTable);
        generator.emitInc(enumeratorIndex.get());
        generator.emitEnumeratorStructurePropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());
        generator.emitDebugHook(m_lexpr); // Pause at the assignment expression for each for..in iteration.
        generator.emitJump(loopStart.get());
        
        generator.emitLabel(scope->breakTarget());
        generator.emitJump(end.get());
        generator.emitLabel(loopEnd.get());
    }
    generator.restoreTDZStack(preservedTDZStack);

    // Generic property loop.
    {
        Ref<LabelScope> scope = generator.newLabelScope(LabelScope::Loop);
        Ref<Label> loopStart = generator.newLabel();
        Ref<Label> loopEnd = generator.newLabel();

        RefPtr<RegisterID> propertyName = generator.newTemporary();

        generator.emitEnumeratorGenericPropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());

        generator.emitLabel(loopStart.get());
        generator.emitLoopHint();

        RefPtr<RegisterID> result = generator.emitIsNull(generator.newTemporary(), propertyName.get());
        generator.emitJumpIfTrue(result.get(), loopEnd.get());

        generator.emitHasGenericProperty(result.get(), base.get(), propertyName.get());
        generator.emitJumpIfFalse(result.get(), *scope->continueTarget());

        this->emitLoopHeader(generator, propertyName.get());

        generator.emitProfileControlFlow(profilerStartOffset);

        generator.emitNode(dst, m_statement);

        generator.emitLabel(*scope->continueTarget());
        generator.prepareLexicalScopeForNextForLoopIteration(this, forLoopSymbolTable);
        generator.emitInc(enumeratorIndex.get());
        generator.emitEnumeratorGenericPropertyName(propertyName.get(), enumerator.get(), enumeratorIndex.get());
        generator.emitDebugHook(m_lexpr); // Pause at the assignment expression for each for..in iteration.
        generator.emitJump(loopStart.get());

        generator.emitLabel(scope->breakTarget());
        generator.emitJump(end.get());
        generator.emitLabel(loopEnd.get());
    }

    generator.emitLabel(end.get());
    generator.popLexicalScope(this);
    generator.emitProfileControlFlow(profilerEndOffset);
}

// ------------------------------ ForOfNode ------------------------------------
void ForOfNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (!m_lexpr->isAssignmentLocation()) {
        emitThrowReferenceError(generator, "Left side of for-of statement is not a reference."_s);
        return;
    }

    if (generator.shouldBeConcernedWithCompletionValue() && m_statement->hasEarlyBreakOrContinue())
        generator.emitLoad(dst, jsUndefined());

    RegisterID* forLoopSymbolTable = nullptr;
    generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested, &forLoopSymbolTable);
    auto extractor = scopedLambda<void(BytecodeGenerator&, RegisterID*)>([this, dst](BytecodeGenerator& generator, RegisterID* value)
    {
        if (m_lexpr->isResolveNode()) {
            const Identifier& ident = static_cast<ResolveNode*>(m_lexpr)->identifier();
            Variable var = generator.variable(ident);
            if (RegisterID* local = var.local()) {
                if (var.isReadOnly())
                    generator.emitReadOnlyExceptionIfNeeded(var);
                generator.move(local, value);
            } else {
                if (generator.ecmaMode().isStrict())
                    generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
                if (var.isReadOnly())
                    generator.emitReadOnlyExceptionIfNeeded(var);
                RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
                generator.emitExpressionInfo(divot(), divotStart(), divotEnd());
                generator.emitPutToScope(scope.get(), var, value, generator.ecmaMode().isStrict() ? ThrowIfNotFound : DoNotThrowIfNotFound, InitializationMode::NotInitialization);
            }
            generator.emitProfileType(value, var, m_lexpr->position(), m_lexpr->position() + ident.length());
        } else if (m_lexpr->isDotAccessorNode()) {
            DotAccessorNode* assignNode = static_cast<DotAccessorNode*>(m_lexpr);
            const Identifier& ident = assignNode->identifier();
            RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
            
            generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
            if (assignNode->base()->isSuperNode()) {
                RefPtr<RegisterID> thisValue = generator.ensureThis();
                generator.emitPutById(base.get(), thisValue.get(), ident, value);
            } else
                generator.emitPutById(base.get(), ident, value);
            generator.emitProfileType(value, assignNode->divotStart(), assignNode->divotEnd());
        } else if (m_lexpr->isBracketAccessorNode()) {
            BracketAccessorNode* assignNode = static_cast<BracketAccessorNode*>(m_lexpr);
            RefPtr<RegisterID> base = generator.emitNode(assignNode->base());
            RegisterID* subscript = generator.emitNodeForProperty(assignNode->subscript());
            
            generator.emitExpressionInfo(assignNode->divot(), assignNode->divotStart(), assignNode->divotEnd());
            if (assignNode->base()->isSuperNode()) {
                RefPtr<RegisterID> thisValue = generator.ensureThis();
                generator.emitPutByVal(base.get(), thisValue.get(), subscript, value);
            } else
                generator.emitPutByVal(base.get(), subscript, value);
            generator.emitProfileType(value, assignNode->divotStart(), assignNode->divotEnd());
        } else {
            ASSERT(m_lexpr->isDestructuringNode());
            DestructuringAssignmentNode* assignNode = static_cast<DestructuringAssignmentNode*>(m_lexpr);
            assignNode->bindings()->bindValue(generator, value);
        }
        generator.emitProfileControlFlow(m_statement->startOffset());
        generator.emitNode(dst, m_statement);
    });
    generator.emitEnumeration(this, m_expr, extractor, this, forLoopSymbolTable);
    generator.popLexicalScope(this);
    generator.emitProfileControlFlow(m_statement->endOffset() + (m_statement->isBlock() ? 1 : 0));
}

// ------------------------------ ContinueNode ---------------------------------

Label* ContinueNode::trivialTarget(BytecodeGenerator& generator)
{
    if (generator.shouldEmitDebugHooks())
        return nullptr;

    LabelScope* scope = generator.continueTarget(m_ident);
    ASSERT(scope);

    if (generator.labelScopeDepth() != scope->scopeDepth())
        return nullptr;

    return scope->continueTarget();
}

void ContinueNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    LabelScope* scope = generator.continueTarget(m_ident);
    ASSERT(scope);

    bool hasFinally = generator.emitJumpViaFinallyIfNeeded(scope->scopeDepth(), *scope->continueTarget());
    if (!hasFinally) {
        int lexicalScopeIndex = generator.labelScopeDepthToLexicalScopeIndex(scope->scopeDepth());
        generator.restoreScopeRegister(lexicalScopeIndex);
        generator.emitJump(*scope->continueTarget());
    }

    generator.emitProfileControlFlow(endOffset());
}

// ------------------------------ BreakNode ------------------------------------

Label* BreakNode::trivialTarget(BytecodeGenerator& generator)
{
    if (generator.shouldEmitDebugHooks())
        return nullptr;

    LabelScope* scope = generator.breakTarget(m_ident);
    ASSERT(scope);

    if (generator.labelScopeDepth() != scope->scopeDepth())
        return nullptr;

    return &scope->breakTarget();
}

void BreakNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    LabelScope* scope = generator.breakTarget(m_ident);
    ASSERT(scope);

    bool hasFinally = generator.emitJumpViaFinallyIfNeeded(scope->scopeDepth(), scope->breakTarget());
    if (!hasFinally) {
        int lexicalScopeIndex = generator.labelScopeDepthToLexicalScopeIndex(scope->scopeDepth());
        generator.restoreScopeRegister(lexicalScopeIndex);
        generator.emitJump(scope->breakTarget());
    }

    generator.emitProfileControlFlow(endOffset());
}

// ------------------------------ ReturnNode -----------------------------------

void ReturnNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    ASSERT(generator.codeType() == FunctionCode);

    if (dst == generator.ignoredResult())
        dst = nullptr;

    RefPtr<RegisterID> returnRegister = m_value ? generator.emitNodeInTailPosition(dst, m_value) : generator.emitLoad(dst, jsUndefined());

    generator.emitProfileType(returnRegister.get(), ProfileTypeBytecodeFunctionReturnStatement, divotStart(), divotEnd());

    bool hasFinally = generator.emitReturnViaFinallyIfNeeded(returnRegister.get());
    if (!hasFinally) {
        if (generator.parseMode() == SourceParseMode::AsyncGeneratorBodyMode) {
            returnRegister = generator.move(generator.newTemporary(), returnRegister.get());
            generator.emitAwait(returnRegister.get());
        }

        generator.emitWillLeaveCallFrameDebugHook();
        generator.emitReturn(returnRegister.get());
    }

    generator.emitProfileControlFlow(endOffset());
    // Emitting an unreachable return here is needed in case this op_profile_control_flow is the 
    // last opcode in a CodeBlock because a CodeBlock's instructions must end with a terminal opcode.
    if (generator.shouldEmitControlFlowProfilerHooks())
        generator.emitReturn(generator.emitLoad(nullptr, jsUndefined()));
}

// ------------------------------ WithNode -------------------------------------

void WithNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> scope = generator.emitNode(m_expr);
    generator.emitExpressionInfo(m_divot, m_divot - m_expressionLength, m_divot);
    generator.emitPushWithScope(scope.get());
    if (generator.shouldBeConcernedWithCompletionValue() && m_statement->hasEarlyBreakOrContinue())
        generator.emitLoad(dst, jsUndefined());
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
    Vector<Ref<Label>, 8> labelVector;
    Vector<ExpressionNode*, 8> literalVector;
    int32_t min_num = std::numeric_limits<int32_t>::max();
    int32_t max_num = std::numeric_limits<int32_t>::min();
    SwitchInfo::SwitchType switchType = tryTableSwitch(literalVector, min_num, max_num);

    Ref<Label> defaultLabel = generator.newLabel();
    if (switchType != SwitchInfo::SwitchNone) {
        // Prepare the various labels
        for (uint32_t i = 0; i < literalVector.size(); i++)
            labelVector.append(generator.newLabel());
        generator.beginSwitch(switchExpression, switchType);
    } else {
        // Setup jumps
        for (ClauseListNode* list = m_list1; list; list = list->getNext()) {
            RefPtr<RegisterID> clauseVal = generator.newTemporary();
            generator.emitNode(clauseVal.get(), list->getClause()->expr());
            generator.emitBinaryOp<OpStricteq>(clauseVal.get(), clauseVal.get(), switchExpression, OperandTypes());
            labelVector.append(generator.newLabel());
            generator.emitJumpIfTrue(clauseVal.get(), labelVector[labelVector.size() - 1].get());
        }
        
        for (ClauseListNode* list = m_list2; list; list = list->getNext()) {
            RefPtr<RegisterID> clauseVal = generator.newTemporary();
            generator.emitNode(clauseVal.get(), list->getClause()->expr());
            generator.emitBinaryOp<OpStricteq>(clauseVal.get(), clauseVal.get(), switchExpression, OperandTypes());
            labelVector.append(generator.newLabel());
            generator.emitJumpIfTrue(clauseVal.get(), labelVector[labelVector.size() - 1].get());
        }
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
        generator.endSwitch(labelVector.size(), labelVector, literalVector.data(), defaultLabel.get(), min_num, max_num);
    }
}

// ------------------------------ SwitchNode -----------------------------------

void SwitchNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (generator.shouldBeConcernedWithCompletionValue())
        generator.emitLoad(dst, jsUndefined());

    Ref<LabelScope> scope = generator.newLabelScope(LabelScope::Switch);

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
    ASSERT(!generator.breakTarget(m_name));

    Ref<LabelScope> scope = generator.newLabelScope(LabelScope::NamedLabel, &m_name);
    generator.emitNodeInTailPosition(dst, m_statement);

    generator.emitLabel(scope->breakTarget());
}

// ------------------------------ ThrowNode ------------------------------------

void ThrowNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    if (dst == generator.ignoredResult())
        dst = nullptr;
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

    if (generator.shouldBeConcernedWithCompletionValue() && m_tryBlock->hasEarlyBreakOrContinue())
        generator.emitLoad(dst, jsUndefined());

    ASSERT(m_catchBlock || m_finallyBlock);

    RefPtr<Label> catchLabel;
    RefPtr<Label> catchEndLabel;
    RefPtr<Label> finallyLabel;
    RefPtr<Label> finallyEndLabel;
    Optional<FinallyContext> finallyContext;

    if (m_finallyBlock) {
        finallyLabel = generator.newLabel();
        finallyEndLabel = generator.newLabel();

        finallyContext.emplace(generator, *finallyLabel);
        generator.pushFinallyControlFlowScope(finallyContext.value());
    }
    if (m_catchBlock) {
        catchLabel = generator.newLabel();
        catchEndLabel = generator.newLabel();
    }

    Ref<Label> tryLabel = generator.newEmittedLabel();
    Label& tryHandlerLabel = m_catchBlock ? *catchLabel : *finallyLabel;
    HandlerType tryHandlerType = m_catchBlock ? HandlerType::Catch : HandlerType::Finally;
    TryData* tryData = generator.pushTry(tryLabel.get(), tryHandlerLabel, tryHandlerType);
    TryData* finallyTryData = nullptr;
    if (!m_catchBlock && m_finallyBlock)
        finallyTryData = tryData;

    generator.emitNode(dst, m_tryBlock);

    if (m_finallyBlock)
        generator.emitJump(*finallyLabel);
    else
        generator.emitJump(*catchEndLabel);

    Ref<Label> tryEndLabel = generator.newEmittedLabel();
    generator.popTry(tryData, tryEndLabel.get());

    if (m_catchBlock) {
        // Uncaught exception path: the catch block.
        generator.emitLabel(*catchLabel);
        RefPtr<RegisterID> thrownValueRegister = generator.newTemporary();
        RegisterID* completionTypeRegister = m_finallyBlock ? finallyContext->completionTypeRegister() : nullptr;
        generator.emitOutOfLineCatchHandler(thrownValueRegister.get(), completionTypeRegister, tryData);
        generator.restoreScopeRegister();

        if (m_finallyBlock) {
            // If the catch block throws an exception and we have a finally block, then the finally
            // block should "catch" that exception.
            finallyTryData = generator.pushTry(*catchLabel, *finallyLabel, HandlerType::Finally);
        }

        if (m_catchPattern) {
            generator.emitPushCatchScope(m_lexicalVariables);
            m_catchPattern->bindValue(generator, thrownValueRegister.get());
        }

        generator.emitProfileControlFlow(m_tryBlock->endOffset() + 1);
        if (m_finallyBlock)
            generator.emitNode(dst, m_catchBlock);
        else
            generator.emitNodeInTailPosition(dst, m_catchBlock);
        generator.emitLoad(thrownValueRegister.get(), jsUndefined());

        if (m_catchPattern)
            generator.emitPopCatchScope(m_lexicalVariables);

        if (m_finallyBlock) {
            generator.emitLoad(finallyContext->completionTypeRegister(), CompletionType::Normal);
            generator.emitJump(*finallyLabel);
            generator.popTry(finallyTryData, *finallyLabel);
        }

        generator.emitLabel(*catchEndLabel);
        generator.emitProfileControlFlow(m_catchBlock->endOffset() + 1);
    }

    if (m_finallyBlock) {
        generator.popFinallyControlFlowScope();

        // Entry to the finally block for CompletionType::Throw to be generated later.
        generator.emitOutOfLineFinallyHandler(finallyContext->completionValueRegister(), finallyContext->completionTypeRegister(), finallyTryData);

        // Entry to the finally block for CompletionTypes other than Throw.
        generator.emitLabel(*finallyLabel);
        generator.restoreScopeRegister();

        int finallyStartOffset = m_catchBlock ? m_catchBlock->endOffset() + 1 : m_tryBlock->endOffset() + 1;
        generator.emitProfileControlFlow(finallyStartOffset);
        generator.emitNodeInTailPosition(m_finallyBlock);

        generator.emitFinallyCompletion(finallyContext.value(), *finallyEndLabel);
        generator.emitLabel(*finallyEndLabel);
        generator.emitProfileControlFlow(m_finallyBlock->endOffset() + 1);
    }
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
    if (generator.shouldEmitTypeProfilerHooks()) {
        // If the parameter list is non simple one, it is handled in bindValue's code.
        if (m_parameters->isSimpleParameterList()) {
            for (size_t i = 0; i < m_parameters->size(); i++) {
                BindingNode* bindingNode = static_cast<BindingNode*>(m_parameters->at(i).first);
                RegisterID reg(CallFrame::argumentOffset(i));
                generator.emitProfileType(&reg, ProfileTypeBytecodeFunctionArgument, bindingNode->divotStart(), bindingNode->divotEnd());
            }
        }
    }

    generator.emitProfileControlFlow(startStartOffset());
    generator.emitDebugHook(DidEnterCallFrame, startLine(), startStartOffset(), startLineStartOffset());

    switch (generator.parseMode()) {
    case SourceParseMode::GeneratorWrapperFunctionMode:
    case SourceParseMode::GeneratorWrapperMethodMode:
    case SourceParseMode::AsyncGeneratorWrapperMethodMode:
    case SourceParseMode::AsyncGeneratorWrapperFunctionMode: {
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

        if (isGeneratorWrapperParseMode(generator.parseMode()))
            generator.emitPutGeneratorFields(next.get());
        else {
            ASSERT(isAsyncGeneratorWrapperParseMode(generator.parseMode()));
            generator.emitPutAsyncGeneratorFields(next.get());
        }
        
        ASSERT(startOffset() >= lineStartOffset());
        generator.emitDebugHook(WillLeaveCallFrame, lastLine(), startOffset(), lineStartOffset());
        generator.emitReturn(generator.generatorRegister());
        break;
    }

    case SourceParseMode::AsyncFunctionMode:
    case SourceParseMode::AsyncMethodMode:
    case SourceParseMode::AsyncArrowFunctionMode: {
        StatementNode* singleStatement = this->singleStatement();
        ASSERT(singleStatement->isExprStatement());
        ExprStatementNode* exprStatement = static_cast<ExprStatementNode*>(singleStatement);
        ExpressionNode* expr = exprStatement->expr();
        ASSERT(expr->isFuncExprNode());
        FuncExprNode* funcExpr = static_cast<FuncExprNode*>(expr);

        RefPtr<RegisterID> next = generator.newTemporary();
        generator.emitNode(next.get(), funcExpr);

        if (generator.superBinding() == SuperBinding::Needed || (generator.parseMode() == SourceParseMode::AsyncArrowFunctionMode && generator.isSuperUsedInInnerArrowFunction())) {
            RefPtr<RegisterID> homeObject = emitHomeObjectForCallee(generator);
            emitPutHomeObject(generator, next.get(), homeObject.get());
        }
        
        if (generator.parseMode() == SourceParseMode::AsyncArrowFunctionMode && generator.isThisUsedInInnerArrowFunction())
            generator.emitLoadThisFromArrowFunctionLexicalEnvironment();

        generator.emitPutGeneratorFields(next.get());

        ASSERT(startOffset() >= lineStartOffset());
        generator.emitDebugHook(WillLeaveCallFrame, lastLine(), startOffset(), lineStartOffset());

        // load and call @asyncFunctionResume
        RefPtr<RegisterID> asyncFunctionResume = generator.moveLinkTimeConstant(nullptr, LinkTimeConstant::asyncFunctionResume);

        CallArguments args(generator, nullptr, 4);
        unsigned argumentCount = 0;
        generator.emitLoad(args.thisRegister(), jsUndefined());
        generator.move(args.argumentRegister(argumentCount++), generator.generatorRegister());
        generator.move(args.argumentRegister(argumentCount++), generator.promiseRegister());
        generator.emitLoad(args.argumentRegister(argumentCount++), jsUndefined());
        generator.emitLoad(args.argumentRegister(argumentCount++), jsNumber(static_cast<int32_t>(JSGenerator::GeneratorResumeMode::NormalMode)));
        // JSTextPosition(int _line, int _offset, int _lineStartOffset)
        JSTextPosition divot(firstLine(), startOffset(), lineStartOffset());

        RefPtr<RegisterID> result = generator.newTemporary();
        generator.emitCallInTailPosition(result.get(), asyncFunctionResume.get(), NoExpectedFunction, args, divot, divot, divot, DebuggableCall::No);
        generator.emitReturn(result.get());
        break;
    }

    case SourceParseMode::AsyncGeneratorBodyMode:
    case SourceParseMode::AsyncArrowFunctionBodyMode:
    case SourceParseMode::AsyncFunctionBodyMode:
    case SourceParseMode::GeneratorBodyMode: {
        Ref<Label> generatorBodyLabel = generator.newLabel();
        {
            RefPtr<RegisterID> condition = generator.newTemporary();
            generator.emitEqualityOp<OpStricteq>(condition.get(), generator.generatorResumeModeRegister(), generator.emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGenerator::GeneratorResumeMode::NormalMode))));
            generator.emitJumpIfTrue(condition.get(), generatorBodyLabel.get());

            Ref<Label> throwLabel = generator.newLabel();
            generator.emitEqualityOp<OpStricteq>(condition.get(), generator.generatorResumeModeRegister(), generator.emitLoad(nullptr, jsNumber(static_cast<int32_t>(JSGenerator::GeneratorResumeMode::ThrowMode))));
            generator.emitJumpIfTrue(condition.get(), throwLabel.get());

            generator.emitReturn(generator.generatorValueRegister());

            generator.emitLabel(throwLabel.get());
            generator.emitThrow(generator.generatorValueRegister());
        }

        generator.emitLabel(generatorBodyLabel.get());

        emitStatementsBytecode(generator, generator.ignoredResult());

        Ref<Label> done = generator.newLabel();
        generator.emitLabel(done.get());
        generator.emitReturn(generator.emitLoad(nullptr, jsUndefined()));
        break;
    }

    default: {
        emitStatementsBytecode(generator, generator.ignoredResult());

        StatementNode* singleStatement = this->singleStatement();
        ReturnNode* returnNode = nullptr;

        // Check for a return statement at the end of a function composed of a single block.
        if (singleStatement && singleStatement->isBlock()) {
            StatementNode* lastStatementInBlock = static_cast<BlockNode*>(singleStatement)->lastStatement();
            if (lastStatementInBlock && lastStatementInBlock->isReturnNode())
                returnNode = static_cast<ReturnNode*>(lastStatementInBlock);
        }

        // If there is no return we must automatically insert one.
        if (!returnNode) {
            if (generator.constructorKind() == ConstructorKind::Extends && generator.needsToUpdateArrowFunctionContext() && generator.isSuperCallUsedInInnerArrowFunction())
                generator.emitLoadThisFromArrowFunctionLexicalEnvironment(); // Arrow function can invoke 'super' in constructor and before leave constructor we need load 'this' from lexical arrow function environment
            
            RegisterID* r0 = nullptr;
            if (generator.isConstructor() && generator.constructorKind() != ConstructorKind::Naked)
                r0 = generator.thisRegister();
            else
                r0 = generator.emitLoad(nullptr, jsUndefined());
            generator.emitProfileType(r0, ProfileTypeBytecodeFunctionReturnStatement); // Do not emit expression info for this profile because it's not in the user's source code.
            ASSERT(startOffset() >= lineStartOffset());
            generator.emitWillLeaveCallFrameDebugHook();
            generator.emitReturn(r0);
            return;
        }
        break;
    }
    }
}

// ------------------------------ FuncDeclNode ---------------------------------

void FuncDeclNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.hoistSloppyModeFunctionIfNecessary(metadata()->ident());
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

// ------------------------------ MethodDefinitionNode ---------------------------------

RegisterID* MethodDefinitionNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    return generator.emitNewMethodDefinition(generator.finalDestination(dst), this);
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
        RefPtr<RegisterID> value = generator.emitYield(arg.get(), JSAsyncGenerator::AsyncGeneratorSuspendReason::Yield);
        if (dst == generator.ignoredResult())
            return nullptr;
        return generator.move(generator.finalDestination(dst), value.get());
    }
    RefPtr<RegisterID> arg = generator.newTemporary();
    generator.emitNode(arg.get(), argument());
    RefPtr<RegisterID> value = generator.emitDelegateYield(arg.get(), this);
    if (dst == generator.ignoredResult())
        return nullptr;
    return generator.move(generator.finalDestination(dst), value.get());
}

// ------------------------------ AwaitExprNode --------------------------------

RegisterID* AwaitExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> arg = generator.newTemporary();
    generator.emitNode(arg.get(), argument());
    RefPtr<RegisterID> value = generator.emitYield(arg.get(), JSAsyncGenerator::AsyncGeneratorSuspendReason::Await);
    if (dst == generator.ignoredResult())
        return nullptr;
    return generator.move(generator.finalDestination(dst), value.get());
}

// ------------------------------ DefineFieldNode ---------------------------------

void DefineFieldNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    RefPtr<RegisterID> value = generator.newTemporary();

    if (!m_assign)
        generator.emitLoad(value.get(), jsUndefined());
    else {
        generator.emitNode(value.get(), m_assign);
        if (m_ident && generator.shouldSetFunctionName(m_assign))
            generator.emitSetFunctionName(value.get(), *m_ident);
    }

    switch (m_type) {
    case DefineFieldNode::Type::Name: {
        // FIXME: Improve performance of public class fields
        // https://bugs.webkit.org/show_bug.cgi?id=198330
        RefPtr<RegisterID> propertyName = generator.emitLoad(nullptr, *m_ident);
        generator.emitCallDefineProperty(generator.thisRegister(), propertyName.get(), value.get(), nullptr, nullptr, BytecodeGenerator::PropertyConfigurable | BytecodeGenerator::PropertyWritable | BytecodeGenerator::PropertyEnumerable, m_position);
        break;
    }
    case DefineFieldNode::Type::PrivateName: {
        Variable var = generator.variable(*m_ident);
        ASSERT_WITH_MESSAGE(!var.local(), "Private Field names must be stored in captured variables");

        generator.emitExpressionInfo(position(), position(), position() + m_ident->length());
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> privateName = generator.newTemporary();
        generator.emitGetFromScope(privateName.get(), scope.get(), var, DoNotThrowIfNotFound);
        generator.emitDefinePrivateField(generator.thisRegister(), privateName.get(), value.get());
        break;
    }
    case DefineFieldNode::Type::ComputedName: {
        // FIXME: Improve performance of public class fields
        // https://bugs.webkit.org/show_bug.cgi?id=198330

        // For ComputedNames, the expression has already been evaluated earlier during evaluation of a ClassExprNode.
        // Here, `m_ident` refers to an integer ID in a class lexical scope, containing the value already converted to an Expression.
        Variable var = generator.variable(*m_ident);
        ASSERT_WITH_MESSAGE(!var.local(), "Computed names must be stored in captured variables");

        generator.emitExpressionInfo(position(), position(), position() + 1);
        RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
        RefPtr<RegisterID> privateName = generator.newTemporary();
        generator.emitGetFromScope(privateName.get(), scope.get(), var, ThrowIfNotFound);
        generator.emitProfileType(privateName.get(), var, m_position, m_position + m_ident->length());
        generator.emitCallDefineProperty(generator.thisRegister(), privateName.get(), value.get(), nullptr, nullptr, BytecodeGenerator::PropertyConfigurable | BytecodeGenerator::PropertyWritable | BytecodeGenerator::PropertyEnumerable, m_position);
        break;
    }
    }
}

// ------------------------------ ClassDeclNode ---------------------------------

void ClassDeclNode::emitBytecode(BytecodeGenerator& generator, RegisterID*)
{
    generator.emitNode(m_classDeclaration);
}

// ------------------------------ ClassExprNode ---------------------------------

RegisterID* ClassExprNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    StrictModeScope strictModeScope(generator);

    if (m_needsLexicalScope)
        generator.pushLexicalScope(this, BytecodeGenerator::TDZCheckOptimization::Optimize, BytecodeGenerator::NestedScopeType::IsNested);

    RefPtr<RegisterID> superclass;
    if (m_classHeritage) {
        superclass = generator.newTemporary();
        generator.emitNode(superclass.get(), m_classHeritage);
    }

    RefPtr<RegisterID> constructor = generator.tempDestination(dst);
    bool needsHomeObject = false;

    auto needsClassFieldInitializer = this->hasInstanceFields() ? NeedsClassFieldInitializer::Yes : NeedsClassFieldInitializer::No;

    if (m_constructorExpression) {
        ASSERT(m_constructorExpression->isFuncExprNode());
        FunctionMetadataNode* metadata = static_cast<FuncExprNode*>(m_constructorExpression)->metadata();
        metadata->setEcmaName(ecmaName());
        metadata->setClassSource(m_classSource);
        metadata->setNeedsClassFieldInitializer(needsClassFieldInitializer == NeedsClassFieldInitializer::Yes);
        constructor = generator.emitNode(constructor.get(), m_constructorExpression);
        needsHomeObject = m_classHeritage || metadata->superBinding() == SuperBinding::Needed;
    } else
        constructor = generator.emitNewDefaultConstructor(constructor.get(), m_classHeritage ? ConstructorKind::Extends : ConstructorKind::Base, m_name, ecmaName(), m_classSource, needsClassFieldInitializer);

    const auto& propertyNames = generator.propertyNames();
    RefPtr<RegisterID> prototype = generator.emitNewObject(generator.newTemporary());

    if (superclass) {
        RefPtr<RegisterID> protoParent = generator.newTemporary();
        generator.emitLoad(protoParent.get(), jsNull());

        RefPtr<RegisterID> tempRegister = generator.newTemporary();

        Ref<Label> superclassIsNullLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitIsNull(tempRegister.get(), superclass.get()), superclassIsNullLabel.get());

        Ref<Label> superclassIsConstructorLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitIsConstructor(tempRegister.get(), superclass.get()), superclassIsConstructorLabel.get());
        generator.emitThrowTypeError("The superclass is not a constructor."_s);
        generator.emitLabel(superclassIsConstructorLabel.get());
        generator.emitGetById(protoParent.get(), superclass.get(), generator.propertyNames().prototype);

        Ref<Label> protoParentIsObjectOrNullLabel = generator.newLabel();
        generator.emitJumpIfTrue(generator.emitIsObject(tempRegister.get(), protoParent.get()), protoParentIsObjectOrNullLabel.get());
        generator.emitJumpIfTrue(generator.emitIsNull(tempRegister.get(), protoParent.get()), protoParentIsObjectOrNullLabel.get());
        generator.emitThrowTypeError("The value of the superclass's prototype property is not an object or null."_s);
        generator.emitLabel(protoParentIsObjectOrNullLabel.get());

        generator.emitDirectSetPrototypeOf(tempRegister.get(), constructor.get(), superclass.get(), divot(), divotStart(), divotEnd());
        generator.emitLabel(superclassIsNullLabel.get());
        generator.emitDirectSetPrototypeOf(tempRegister.get(), prototype.get(), protoParent.get(), divot(), divotStart(), divotEnd());
    }

    if (needsHomeObject)
        emitPutHomeObject(generator, constructor.get(), prototype.get());

    RefPtr<RegisterID> constructorNameRegister = generator.emitLoad(nullptr, propertyNames.constructor);
    generator.emitCallDefineProperty(prototype.get(), constructorNameRegister.get(), constructor.get(), nullptr, nullptr,
        BytecodeGenerator::PropertyConfigurable | BytecodeGenerator::PropertyWritable, m_position);

    RefPtr<RegisterID> prototypeNameRegister = generator.emitLoad(nullptr, propertyNames.prototype);
    generator.emitCallDefineProperty(constructor.get(), prototypeNameRegister.get(), prototype.get(), nullptr, nullptr, 0, m_position);

    if (m_classElements) {
        m_classElements->emitDeclarePrivateFieldNames(generator, generator.scopeRegister());

        Vector<JSTextPosition> instanceFieldLocations;
        generator.emitDefineClassElements(m_classElements, constructor.get(), prototype.get(), instanceFieldLocations);
        if (!instanceFieldLocations.isEmpty()) {
            RefPtr<RegisterID> instanceFieldInitializer = generator.emitNewInstanceFieldInitializerFunction(generator.newTemporary(), WTFMove(instanceFieldLocations), m_classHeritage);

            // FIXME: Skip this if the initializer function isn't going to need a home object (no eval or super properties)
            // https://bugs.webkit.org/show_bug.cgi?id=196867
            emitPutHomeObject(generator, instanceFieldInitializer.get(), prototype.get());

            generator.emitDirectPutById(constructor.get(), generator.propertyNames().builtinNames().instanceFieldInitializerPrivateName(), instanceFieldInitializer.get(), PropertyNode::Unknown);
        }
    }

    if (m_needsLexicalScope) {
        if (!m_name.isNull()) {
            Variable classNameVar = generator.variable(m_name);
            RELEASE_ASSERT(classNameVar.isResolved());
            RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, classNameVar);
            generator.emitPutToScope(scope.get(), classNameVar, constructor.get(), ThrowIfNotFound, InitializationMode::Initialization);
        }
        generator.popLexicalScope(this);
    }

    return generator.move(generator.finalDestination(dst, constructor.get()), constructor.get());
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
    return generator.move(dst, initializer.get());
}

static void assignDefaultValueIfUndefined(BytecodeGenerator& generator, RegisterID* maybeUndefined, ExpressionNode* defaultValue)
{
    ASSERT(defaultValue);
    Ref<Label> isNotUndefined = generator.newLabel();
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
        generator.move(args.thisRegister(), rhs);
        generator.emitCall(iterator.get(), iterator.get(), NoExpectedFunction, args, divot(), divotStart(), divotEnd(), DebuggableCall::No);
    }
    RefPtr<RegisterID> nextMethod = generator.emitGetById(generator.newTemporary(), iterator.get(), generator.propertyNames().next);

    if (m_targetPatterns.isEmpty()) {
        generator.emitIteratorGenericClose(iterator.get(), this);
        return;
    }

    RefPtr<RegisterID> done;
    for (auto& target : m_targetPatterns) {
        switch (target.bindingType) {
        case BindingType::Elision:
        case BindingType::Element: {
            Ref<Label> iterationSkipped = generator.newLabel();
            if (!done)
                done = generator.newTemporary();
            else
                generator.emitJumpIfTrue(done.get(), iterationSkipped.get());

            RefPtr<RegisterID> value = generator.newTemporary();
            generator.emitIteratorGenericNext(value.get(), nextMethod.get(), iterator.get(), this);
            generator.emitGetById(done.get(), value.get(), generator.propertyNames().done);
            generator.emitJumpIfTrue(done.get(), iterationSkipped.get());
            generator.emitGetById(value.get(), value.get(), generator.propertyNames().value);

            {
                Ref<Label> valueIsSet = generator.newLabel();
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
            RefPtr<RegisterID> array = generator.emitNewArray(generator.newTemporary(), nullptr, 0, ArrayWithUndecided);

            Ref<Label> iterationDone = generator.newLabel();
            if (!done)
                done = generator.newTemporary();
            else
                generator.emitJumpIfTrue(done.get(), iterationDone.get());

            RefPtr<RegisterID> index = generator.newTemporary();
            generator.emitLoad(index.get(), jsNumber(0));
            Ref<Label> loopStart = generator.newLabel();
            generator.emitLabel(loopStart.get());

            RefPtr<RegisterID> value = generator.newTemporary();
            generator.emitIteratorGenericNext(value.get(), nextMethod.get(), iterator.get(), this);
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

    Ref<Label> iteratorClosed = generator.newLabel();
    generator.emitJumpIfTrue(done.get(), iteratorClosed.get());
    generator.emitIteratorGenericClose(iterator.get(), this);
    generator.emitLabel(iteratorClosed.get());
}

RegisterID* ArrayPatternNode::emitDirectBinding(BytecodeGenerator& generator, RegisterID* dst, ExpressionNode* rhs)
{
    if (!rhs->isSimpleArray())
        return nullptr;

    ElementNode* elementNodes = static_cast<ArrayNode*>(rhs)->elements();
    Vector<ExpressionNode*> elements;
    for (; elementNodes; elementNodes = elementNodes->next()) {
        ExpressionNode* value = elementNodes->value();
        ASSERT(!value->isSpreadExpression());
        elements.append(value);
    }

    RefPtr<RegisterID> resultRegister;
    if (dst != generator.ignoredResult())
        resultRegister = generator.emitNewArray(generator.newTemporary(), nullptr, 0, ArrayWithUndecided);
    if (m_targetPatterns.size() != elements.size())
        return nullptr;
    Vector<RefPtr<RegisterID>> registers;
    registers.reserveCapacity(m_targetPatterns.size());
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        registers.uncheckedAppend(generator.newTemporary());
        generator.emitNode(registers.last().get(), elements[i]);
        if (m_targetPatterns[i].defaultValue)
            assignDefaultValueIfUndefined(generator, registers.last().get(), m_targetPatterns[i].defaultValue);
        if (resultRegister) {
            RefPtr<RegisterID> index = generator.emitLoad(nullptr, jsNumber(i));
            generator.emitDirectPutByVal(resultRegister.get(), index.get(), registers.last().get());
        }
    }
    
    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        if (m_targetPatterns[i].pattern)
            m_targetPatterns[i].pattern->bindValue(generator, registers[i].get());
    }
    if (resultRegister)
        return generator.move(generator.finalDestination(dst, resultRegister.get()), resultRegister.get());
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
            builder.appendLiteral("...");
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
    generator.emitRequireObjectCoercible(rhs, "Right side of assignment cannot be destructured"_s);

    RefPtr<RegisterID> excludedList;
    IdentifierSet excludedSet;
    RefPtr<RegisterID> addMethod;
    if (m_containsRestElement && m_containsComputedProperty) {
        RefPtr<RegisterID> setConstructor = generator.moveLinkTimeConstant(nullptr, LinkTimeConstant::Set);

        CallArguments args(generator, nullptr, 0);
        excludedList = generator.emitConstruct(generator.newTemporary(), setConstructor.get(), setConstructor.get(), NoExpectedFunction, args, divot(), divotStart(), divotEnd());

        addMethod = generator.emitGetById(generator.newTemporary(), excludedList.get(), generator.propertyNames().builtinNames().addPrivateName());
    }

    BytecodeGenerator::PreservedTDZStack preservedTDZStack;
    generator.preserveTDZStack(preservedTDZStack);

    for (size_t i = 0; i < m_targetPatterns.size(); i++) {
        const auto& target = m_targetPatterns[i];
        if (target.bindingType == BindingType::Element) {
            RefPtr<RegisterID> temp = generator.newTemporary();
            RefPtr<RegisterID> propertyName;
            if (!target.propertyExpression) {
                Optional<uint32_t> optionalIndex = parseIndex(target.propertyName);
                if (!optionalIndex)
                    generator.emitGetById(temp.get(), rhs, target.propertyName);
                else {
                    RefPtr<RegisterID> propertyIndex = generator.emitLoad(nullptr, jsNumber(optionalIndex.value()));
                    generator.emitGetByVal(temp.get(), rhs, propertyIndex.get());
                }
            } else {
                propertyName = generator.emitNodeForProperty(target.propertyExpression);
                generator.emitGetByVal(temp.get(), rhs, propertyName.get());
            }

            if (m_containsRestElement) {
                if (m_containsComputedProperty) {
                    if (!target.propertyExpression)
                        propertyName = generator.emitLoad(nullptr, target.propertyName);

                    CallArguments args(generator, nullptr, 1);
                    generator.move(args.thisRegister(), excludedList.get());
                    generator.move(args.argumentRegister(0), propertyName.get());
                    generator.emitCall(generator.newTemporary(), addMethod.get(), NoExpectedFunction, args, divot(), divotStart(), divotEnd(), DebuggableCall::No);
                } else
                    excludedSet.add(target.propertyName.impl());
            }

            if (target.defaultValue)
                assignDefaultValueIfUndefined(generator, temp.get(), target.defaultValue);
            target.pattern->bindValue(generator, temp.get());
        } else {
            ASSERT(target.bindingType == BindingType::RestElement);
            ASSERT(i == m_targetPatterns.size() - 1);
            RefPtr<RegisterID> newObject = generator.emitNewObject(generator.newTemporary());
            
            // load and call @copyDataProperties
            RefPtr<RegisterID> copyDataProperties = generator.moveLinkTimeConstant(nullptr, LinkTimeConstant::copyDataProperties);
            
            CallArguments args(generator, nullptr, 3);
            generator.emitLoad(args.thisRegister(), jsUndefined());
            generator.move(args.argumentRegister(0), newObject.get());
            generator.move(args.argumentRegister(1), rhs);
            if (m_containsComputedProperty)
                generator.move(args.argumentRegister(2), excludedList.get());
            else {
                RefPtr<RegisterID> excludedSetReg = generator.emitLoad(generator.newTemporary(), excludedSet);
                generator.move(args.argumentRegister(2), excludedSetReg.get());
            }

            RefPtr<RegisterID> result = generator.newTemporary();
            generator.emitCall(result.get(), copyDataProperties.get(), NoExpectedFunction, args, divot(), divotStart(), divotEnd(), DebuggableCall::No);
            target.pattern->bindValue(generator, result.get());
        }
    }

    generator.restoreTDZStack(preservedTDZStack);
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
        generator.move(local, value);
        generator.emitProfileType(local, var, divotStart(), divotEnd());
        if (m_bindingContext == AssignmentContext::DeclarationStatement || m_bindingContext == AssignmentContext::ConstDeclarationStatement)
            generator.liftTDZCheckIfPossible(var);
        return;
    }
    if (generator.ecmaMode().isStrict())
        generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
    RefPtr<RegisterID> scope = generator.emitResolveScope(nullptr, var);
    generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
    if (m_bindingContext == AssignmentContext::AssignmentExpression)
        generator.emitTDZCheckIfNecessary(var, nullptr, scope.get());
    if (isReadOnly) {
        generator.emitReadOnlyExceptionIfNeeded(var);
        return;
    }
    generator.emitPutToScope(scope.get(), var, value, generator.ecmaMode().isStrict() ? ThrowIfNotFound : DoNotThrowIfNotFound, initializationModeForAssignmentContext(m_bindingContext));
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
                generator.move(local, value);
                generator.emitProfileType(local, divotStart(), divotEnd());
            }
            return;
        }
        if (generator.ecmaMode().isStrict())
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
            generator.emitPutToScope(scope.get(), var, value, generator.ecmaMode().isStrict() ? ThrowIfNotFound : DoNotThrowIfNotFound, InitializationMode::NotInitialization);
            generator.emitProfileType(value, var, divotStart(), divotEnd());
        }
    } else if (m_assignmentTarget->isDotAccessorNode()) {
        DotAccessorNode* lhs = static_cast<DotAccessorNode*>(m_assignmentTarget);
        RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(lhs->base(), true, false);
        generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
        if (lhs->base()->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            generator.emitPutById(base.get(), thisValue.get(), lhs->identifier(), value);
        } else
            generator.emitPutById(base.get(), lhs->identifier(), value);
        generator.emitProfileType(value, divotStart(), divotEnd());
    } else if (m_assignmentTarget->isBracketAccessorNode()) {
        BracketAccessorNode* lhs = static_cast<BracketAccessorNode*>(m_assignmentTarget);
        RefPtr<RegisterID> base = generator.emitNodeForLeftHandSide(lhs->base(), true, false);
        RefPtr<RegisterID> property = generator.emitNodeForLeftHandSideForProperty(lhs->subscript(), true, false);
        generator.emitExpressionInfo(divotEnd(), divotStart(), divotEnd());
        if (lhs->base()->isSuperNode()) {
            RefPtr<RegisterID> thisValue = generator.ensureThis();
            generator.emitPutByVal(base.get(), thisValue.get(), property.get(), value);
        } else
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
    m_pattern->collectBoundIdentifiers(identifiers);
}

void RestParameterNode::toString(StringBuilder& builder) const
{
    builder.appendLiteral("...");
    m_pattern->toString(builder);
}

void RestParameterNode::bindValue(BytecodeGenerator&, RegisterID*) const
{
    RELEASE_ASSERT_NOT_REACHED();
}

void RestParameterNode::emit(BytecodeGenerator& generator)
{
    RefPtr<RegisterID> temp = generator.newTemporary();
    generator.emitRestParameter(temp.get(), m_numParametersToSkip);
    m_pattern->bindValue(generator, temp.get());
}


RegisterID* SpreadExpressionNode::emitBytecode(BytecodeGenerator&, RegisterID*)
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

RegisterID* ObjectSpreadExpressionNode::emitBytecode(BytecodeGenerator& generator, RegisterID* dst)
{
    RefPtr<RegisterID> src = generator.newTemporary();
    generator.emitNode(src.get(), m_expression);
    
    // load and call @copyDataPropertiesNoExclusions
    RefPtr<RegisterID> copyDataProperties = generator.moveLinkTimeConstant(nullptr, LinkTimeConstant::copyDataPropertiesNoExclusions);
    
    CallArguments args(generator, nullptr, 2);
    generator.emitLoad(args.thisRegister(), jsUndefined());
    generator.move(args.argumentRegister(0), dst);
    generator.move(args.argumentRegister(1), src.get());
    
    generator.emitCall(generator.newTemporary(), copyDataProperties.get(), NoExpectedFunction, args, divot(), divotStart(), divotEnd(), DebuggableCall::No);
    
    return dst;
}

} // namespace JSC
