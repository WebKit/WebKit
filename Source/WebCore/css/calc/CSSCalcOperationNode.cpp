/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCalcOperationNode.h"

#include "CSSCalcCategoryMapping.h"
#include "CSSCalcInvertNode.h"
#include "CSSCalcNegateNode.h"
#include "CSSCalcPrimitiveValueNode.h"
#include "CSSCalcValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSUnits.h"
#include "CalcExpressionOperation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

// This is the result of the "To add two types type1 and type2, perform the following steps:" rules.

static const CalculationCategory addSubtractResult[static_cast<unsigned>(CalculationCategory::Angle)][static_cast<unsigned>(CalculationCategory::Angle)] = {
//    CalculationCategory::Number         CalculationCategory::Length         CalculationCategory::Percent        CalculationCategory::PercentNumber  CalculationCategory::PercentLength
    { CalculationCategory::Number,        CalculationCategory::Other,         CalculationCategory::PercentNumber, CalculationCategory::PercentNumber, CalculationCategory::Other }, //         CalculationCategory::Number
    { CalculationCategory::Other,         CalculationCategory::Length,        CalculationCategory::PercentLength, CalculationCategory::Other,         CalculationCategory::PercentLength }, // CalculationCategory::Length
    { CalculationCategory::PercentNumber, CalculationCategory::PercentLength, CalculationCategory::Percent,       CalculationCategory::PercentNumber, CalculationCategory::PercentLength }, // CalculationCategory::Percent
    { CalculationCategory::PercentNumber, CalculationCategory::Other,         CalculationCategory::PercentNumber, CalculationCategory::PercentNumber, CalculationCategory::Other }, //         CalculationCategory::PercentNumber
    { CalculationCategory::Other,         CalculationCategory::PercentLength, CalculationCategory::PercentLength, CalculationCategory::Other,         CalculationCategory::PercentLength }, // CalculationCategory::PercentLength
};

static CalculationCategory determineCategory(const CSSCalcExpressionNode& leftSide, const CSSCalcExpressionNode& rightSide, CalcOperator op)
{
    CalculationCategory leftCategory = leftSide.category();
    CalculationCategory rightCategory = rightSide.category();
    ASSERT(leftCategory < CalculationCategory::Other);
    ASSERT(rightCategory < CalculationCategory::Other);

    switch (op) {
    case CalcOperator::Add:
    case CalcOperator::Subtract:
        if (leftCategory < CalculationCategory::Angle && rightCategory < CalculationCategory::Angle)
            return addSubtractResult[static_cast<unsigned>(leftCategory)][static_cast<unsigned>(rightCategory)];
        if (leftCategory == rightCategory)
            return leftCategory;
        return CalculationCategory::Other;
    case CalcOperator::Multiply:
        if (leftCategory != CalculationCategory::Number && rightCategory != CalculationCategory::Number)
            return CalculationCategory::Other;
        return leftCategory == CalculationCategory::Number ? rightCategory : leftCategory;
    case CalcOperator::Divide:
        if (rightCategory != CalculationCategory::Number || rightSide.isZero())
            return CalculationCategory::Other;
        return leftCategory;
    case CalcOperator::Min:
    case CalcOperator::Max:
    case CalcOperator::Clamp:
        ASSERT_NOT_REACHED();
        return CalculationCategory::Other;
    }

    ASSERT_NOT_REACHED();
    return CalculationCategory::Other;
}

// FIXME: Need to implement correct category computation per:
// <https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-invert-a-type>
// To invert a type type, perform the following steps:
// Let result be a new type with an initially empty ordered map and an initially null percent hint
// For each unit → exponent of type, set result[unit] to (-1 * exponent).
static CalculationCategory categoryForInvert(CalculationCategory category)
{
    return category;
}

static CalculationCategory determineCategory(const Vector<Ref<CSSCalcExpressionNode>>& nodes, CalcOperator op)
{
    if (nodes.isEmpty())
        return CalculationCategory::Other;

    auto currentCategory = nodes[0]->category();

    for (unsigned i = 1; i < nodes.size(); ++i) {
        const auto& node = nodes[i].get();
        
        auto usedOperator = op;
        if (node.type() == CSSCalcExpressionNode::Type::CssCalcInvert)
            usedOperator = CalcOperator::Divide;

        auto nextCategory = node.category();

        switch (usedOperator) {
        case CalcOperator::Add:
        case CalcOperator::Subtract:
            // <https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-add-two-types>
            // At a + or - sub-expression, attempt to add the types of the left and right arguments.
            // If this returns failure, the entire calculation’s type is failure. Otherwise, the sub-expression’s type is the returned type.
            if (currentCategory < CalculationCategory::Angle && nextCategory < CalculationCategory::Angle)
                currentCategory = addSubtractResult[static_cast<unsigned>(currentCategory)][static_cast<unsigned>(nextCategory)];
            else if (currentCategory != nextCategory)
                return CalculationCategory::Other;
            break;

        case CalcOperator::Multiply:
            // <https://drafts.css-houdini.org/css-typed-om-1/#cssnumericvalue-multiply-two-types>
            // At a * sub-expression, multiply the types of the left and right arguments. The sub-expression’s type is the returned result.
            if (currentCategory != CalculationCategory::Number && nextCategory != CalculationCategory::Number)
                return CalculationCategory::Other;

            currentCategory = currentCategory == CalculationCategory::Number ? nextCategory : currentCategory;
            break;

        case CalcOperator::Divide: {
            auto invertCategory = categoryForInvert(nextCategory);
            
            // At a / sub-expression, let left type be the result of finding the types of its left argument,
            // and right type be the result of finding the types of its right argument and then inverting it.
            // The sub-expression’s type is the result of multiplying the left type and right type.
            if (invertCategory != CalculationCategory::Number || node.isZero())
                return CalculationCategory::Other;
            break;
        }

        case CalcOperator::Min:
        case CalcOperator::Max:
        case CalcOperator::Clamp:
            // The type of a min(), max(), or clamp() expression is the result of adding the types of its comma-separated calculations
            return CalculationCategory::Other;
        }
    }

    return currentCategory;
}

static CalculationCategory resolvedTypeForMinOrMaxOrClamp(CalculationCategory category, CalculationCategory destinationCategory)
{
    switch (category) {
    case CalculationCategory::Number:
    case CalculationCategory::Length:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::PercentLength:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
    case CalculationCategory::Other:
        return category;

    case CalculationCategory::Percent:
        if (destinationCategory == CalculationCategory::Length)
            return CalculationCategory::PercentLength;
        if (destinationCategory == CalculationCategory::Number)
            return CalculationCategory::PercentNumber;
        return category;
    }

    return CalculationCategory::Other;
}

static bool isSamePair(CalculationCategory a, CalculationCategory b, CalculationCategory x, CalculationCategory y)
{
    return (a == x && b == y) || (a == y && b == x);
}

enum class SortingCategory {
    Number,
    Percent,
    Dimension,
    Other
};

static SortingCategory sortingCategoryForType(CSSUnitType unitType)
{
    static constexpr SortingCategory sortOrder[] = {
        SortingCategory::Number,        // CalculationCategory::Number,
        SortingCategory::Dimension,     // CalculationCategory::Length,
        SortingCategory::Percent,       // CalculationCategory::Percent,
        SortingCategory::Number,        // CalculationCategory::PercentNumber,
        SortingCategory::Dimension,     // CalculationCategory::PercentLength,
        SortingCategory::Dimension,     // CalculationCategory::Angle,
        SortingCategory::Dimension,     // CalculationCategory::Time,
        SortingCategory::Dimension,     // CalculationCategory::Frequency,
        SortingCategory::Other,         // UOther
    };

    COMPILE_ASSERT(ARRAY_SIZE(sortOrder) == static_cast<unsigned>(CalculationCategory::Other) + 1, sortOrder_size_should_match_UnitCategory);
    return sortOrder[static_cast<unsigned>(calcUnitCategory(unitType))];
}

static SortingCategory sortingCategory(const CSSCalcExpressionNode& node)
{
    if (is<CSSCalcPrimitiveValueNode>(node))
        return sortingCategoryForType(node.primitiveType());

    return SortingCategory::Other;
}

static CSSUnitType primitiveTypeForCombination(const CSSCalcExpressionNode& node)
{
    if (is<CSSCalcPrimitiveValueNode>(node))
        return node.primitiveType();
    
    return CSSUnitType::CSS_UNKNOWN;
}

static CSSCalcPrimitiveValueNode::UnitConversion conversionToAddValuesWithTypes(CSSUnitType firstType, CSSUnitType secondType)
{
    if (firstType == CSSUnitType::CSS_UNKNOWN || secondType == CSSUnitType::CSS_UNKNOWN)
        return CSSCalcPrimitiveValueNode::UnitConversion::Invalid;

    auto firstCategory = calculationCategoryForCombination(firstType);

    // Compatible types.
    if (firstCategory != CalculationCategory::Other && firstCategory == calculationCategoryForCombination(secondType))
        return CSSCalcPrimitiveValueNode::UnitConversion::Canonicalize;

    // Matching types.
    if (firstType == secondType && hasDoubleValue(firstType))
        return CSSCalcPrimitiveValueNode::UnitConversion::Preserve;

    return CSSCalcPrimitiveValueNode::UnitConversion::Invalid;
}


static CSSValueID functionFromOperator(CalcOperator op)
{
    switch (op) {
    case CalcOperator::Add:
    case CalcOperator::Subtract:
    case CalcOperator::Multiply:
    case CalcOperator::Divide:
        return CSSValueCalc;
    case CalcOperator::Min:
        return CSSValueMin;
    case CalcOperator::Max:
        return CSSValueMax;
    case CalcOperator::Clamp:
        return CSSValueClamp;
    }
    return CSSValueCalc;
}

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::create(CalcOperator op, RefPtr<CSSCalcExpressionNode>&& leftSide, RefPtr<CSSCalcExpressionNode>&& rightSide)
{
    if (!leftSide || !rightSide)
        return nullptr;

    ASSERT(op == CalcOperator::Add || op == CalcOperator::Multiply);

    ASSERT(leftSide->category() < CalculationCategory::Other);
    ASSERT(rightSide->category() < CalculationCategory::Other);

    auto newCategory = determineCategory(*leftSide, *rightSide, op);
    if (newCategory == CalculationCategory::Other) {
        LOG_WITH_STREAM(Calc, stream << "Failed to create CSSCalcOperationNode " << op << " node because unable to determine category from " << prettyPrintNode(*leftSide) << " and " << *rightSide);
        return nullptr;
    }

    return adoptRef(new CSSCalcOperationNode(newCategory, op, leftSide.releaseNonNull(), rightSide.releaseNonNull()));
}

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::createSum(Vector<Ref<CSSCalcExpressionNode>>&& values)
{
    if (values.isEmpty())
        return nullptr;

    auto newCategory = determineCategory(values, CalcOperator::Add);
    if (newCategory == CalculationCategory::Other) {
        LOG_WITH_STREAM(Calc, stream << "Failed to create sum node because unable to determine category from " << prettyPrintNodes(values));
        newCategory = determineCategory(values, CalcOperator::Add);
        return nullptr;
    }

    return adoptRef(new CSSCalcOperationNode(newCategory, CalcOperator::Add, WTFMove(values)));
}

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::createProduct(Vector<Ref<CSSCalcExpressionNode>>&& values)
{
    if (values.isEmpty())
        return nullptr;

    auto newCategory = determineCategory(values, CalcOperator::Multiply);
    if (newCategory == CalculationCategory::Other) {
        LOG_WITH_STREAM(Calc, stream << "Failed to create product node because unable to determine category from " << prettyPrintNodes(values));
        return nullptr;
    }

    return adoptRef(new CSSCalcOperationNode(newCategory, CalcOperator::Multiply, WTFMove(values)));
}

RefPtr<CSSCalcOperationNode> CSSCalcOperationNode::createMinOrMaxOrClamp(CalcOperator op, Vector<Ref<CSSCalcExpressionNode>>&& values, CalculationCategory destinationCategory)
{
    ASSERT(op == CalcOperator::Min || op == CalcOperator::Max || op == CalcOperator::Clamp);
    ASSERT_IMPLIES(op == CalcOperator::Clamp, values.size() == 3);

    std::optional<CalculationCategory> category = std::nullopt;
    for (auto& value : values) {
        auto valueCategory = resolvedTypeForMinOrMaxOrClamp(value->category(), destinationCategory);

        ASSERT(valueCategory < CalculationCategory::Other);
        if (!category) {
            if (valueCategory == CalculationCategory::Other) {
                LOG_WITH_STREAM(Calc, stream << "Failed to create CSSCalcOperationNode " << op << " node because unable to determine category from " << prettyPrintNodes(values));
                return nullptr;
            }
            category = valueCategory;
        }

        if (category != valueCategory) {
            if (isSamePair(category.value(), valueCategory, CalculationCategory::Length, CalculationCategory::PercentLength)) {
                category = CalculationCategory::PercentLength;
                continue;
            }
            if (isSamePair(category.value(), valueCategory, CalculationCategory::Number, CalculationCategory::PercentNumber)) {
                category = CalculationCategory::PercentNumber;
                continue;
            }
            return nullptr;
        }
    }

    return adoptRef(new CSSCalcOperationNode(category.value(), op, WTFMove(values)));
}

void CSSCalcOperationNode::hoistChildrenWithOperator(CalcOperator op)
{
    ASSERT(op == CalcOperator::Add || op == CalcOperator::Multiply);

    auto hasChildWithOperator = [&] (CalcOperator op) {
        for (auto& child : m_children) {
            if (is<CSSCalcOperationNode>(child.get()) && downcast<CSSCalcOperationNode>(child.get()).calcOperator() == op)
                return true;
        }
        return false;
    };

    if (!hasChildWithOperator(op))
        return;

    Vector<Ref<CSSCalcExpressionNode>> newChildren;
    for (auto& child : m_children) {
        if (is<CSSCalcOperationNode>(child.get()) && downcast<CSSCalcOperationNode>(child.get()).calcOperator() == op) {
            auto& children = downcast<CSSCalcOperationNode>(child.get()).children();
            for (auto& childToMove : children)
                newChildren.append(WTFMove(childToMove));
        } else
            newChildren.append(WTFMove(child));
    }
    
    newChildren.shrinkToFit();
    m_children = WTFMove(newChildren);
}

bool CSSCalcOperationNode::canCombineAllChildren() const
{
    if (m_children.size() < 2)
        return false;

    if (!is<CSSCalcPrimitiveValueNode>(m_children[0]))
        return false;

    auto firstUnitType = m_children[0]->primitiveType();
    auto firstCategory = calculationCategoryForCombination(m_children[0]->primitiveType());

    for (unsigned i = 1; i < m_children.size(); ++i) {
        auto& node = m_children[i];

        if (!is<CSSCalcPrimitiveValueNode>(node))
            return false;

        auto nodeUnitType = node->primitiveType();
        auto nodeCategory = calculationCategoryForCombination(nodeUnitType);

        if (nodeCategory != firstCategory)
            return false;

        if (nodeCategory == CalculationCategory::Other && nodeUnitType != firstUnitType)
            return false;
        
        if (!hasDoubleValue(nodeUnitType))
            return false;
    }

    return true;
}

void CSSCalcOperationNode::combineChildren()
{
    if (m_children.size() < 2)
        return;

    if (shouldSortChildren()) {
        // <https://drafts.csswg.org/css-values-4/#sort-a-calculations-children>
        std::stable_sort(m_children.begin(), m_children.end(), [](const auto& first, const auto& second) {
            // Sort order: number, percentage, dimension, other.
            SortingCategory firstCategory = sortingCategory(first.get());
            SortingCategory secondCategory = sortingCategory(second.get());
            
            if (firstCategory == SortingCategory::Dimension && secondCategory == SortingCategory::Dimension) {
                // If nodes contains any dimensions, remove them from nodes, sort them by their units, and append them to ret.
                auto firstUnitString = CSSPrimitiveValue::unitTypeString(first->primitiveType());
                auto secondUnitString = CSSPrimitiveValue::unitTypeString(second->primitiveType());
                return codePointCompareLessThan(firstUnitString, secondUnitString);
            }

            return static_cast<unsigned>(firstCategory) < static_cast<unsigned>(secondCategory);
        });

        LOG_WITH_STREAM(Calc, stream << "post-sort: " << *this);
    }

    if (calcOperator() == CalcOperator::Add) {
        // For each set of root’s children that are numeric values with identical units,
        // remove those children and replace them with a single numeric value containing
        // the sum of the removed nodes, and with the same unit.
        Vector<Ref<CSSCalcExpressionNode>> newChildren;
        newChildren.reserveInitialCapacity(m_children.size());
        newChildren.uncheckedAppend(m_children[0].copyRef());

        CSSUnitType previousType = primitiveTypeForCombination(newChildren[0].get());

        for (unsigned i = 1; i < m_children.size(); ++i) {
            auto& currentNode = m_children[i];
            CSSUnitType currentType = primitiveTypeForCombination(currentNode.get());

            auto conversionType = conversionToAddValuesWithTypes(previousType, currentType);
            if (conversionType != CSSCalcPrimitiveValueNode::UnitConversion::Invalid) {
                downcast<CSSCalcPrimitiveValueNode>(newChildren.last().get()).add(downcast<CSSCalcPrimitiveValueNode>(currentNode.get()), conversionType);
                continue;
            }

            previousType = primitiveTypeForCombination(currentNode);
            newChildren.uncheckedAppend(currentNode.copyRef());
        }
        
        newChildren.shrinkToFit();
        m_children = WTFMove(newChildren);
        return;
    }

    if (calcOperator() == CalcOperator::Multiply) {
        // If root has multiple children that are numbers (not percentages or dimensions),
        // remove them and replace them with a single number containing the product of the removed nodes.
        double multiplier = 1;

        // Sorting will have put the number nodes first.
        unsigned leadingNumberNodeCount = 0;
        for (auto& node : m_children) {
            auto nodeType = primitiveTypeForCombination(node.get());
            if (nodeType != CSSUnitType::CSS_NUMBER)
                break;
            
            multiplier *= node->doubleValue(CSSUnitType::CSS_NUMBER);
            ++leadingNumberNodeCount;
        }
        
        Vector<Ref<CSSCalcExpressionNode>> newChildren;
        newChildren.reserveInitialCapacity(m_children.size());
        
        // If root contains only two children, one of which is a number (not a percentage or dimension) and the other of
        // which is a Sum whose children are all numeric values, multiply all of the Sum’s children by the number, then
        // return the Sum.
        // The Sum's children simplification will have happened already.
        bool didMultiply = false;
        if (leadingNumberNodeCount && m_children.size() - leadingNumberNodeCount == 1) {
            auto multiplicandCategory = calcUnitCategory(primitiveTypeForCombination(m_children.last().get()));
            if (multiplicandCategory != CalculationCategory::Other) {
                newChildren.uncheckedAppend(m_children.last().copyRef());
                downcast<CSSCalcPrimitiveValueNode>(newChildren[0].get()).multiply(multiplier);
                didMultiply = true;
            } else if (is<CSSCalcOperationNode>(m_children.last().get()) && downcast<CSSCalcOperationNode>(m_children.last().get()).calcOperator() == CalcOperator::Add) {
                // If we're multiplying with another operation that is an addition and all the added children
                // are percentages or dimensions, we should multiply each child and make this expression an
                // addition.
                auto allChildrenArePrimitiveValues = [](const Vector<Ref<CSSCalcExpressionNode>>& children) -> bool
                {
                    for (auto& child : children) {
                        if (!is<CSSCalcPrimitiveValueNode>(child.get()))
                            return false;
                    }
                    return true;
                };

                auto& children = downcast<CSSCalcOperationNode>(m_children.last().get()).children();
                if (allChildrenArePrimitiveValues(children)) {
                    for (auto& child : children) {
                        newChildren.append(child.copyRef());
                        downcast<CSSCalcPrimitiveValueNode>(newChildren.last().get()).multiply(multiplier);
                    }
                    m_operator = CalcOperator::Add;
                    didMultiply = true;
                }
            }
        }

        if (!didMultiply) {
            if (leadingNumberNodeCount) {
                auto multiplierNode = CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(multiplier, CSSUnitType::CSS_NUMBER));
                newChildren.uncheckedAppend(WTFMove(multiplierNode));
            }

            for (unsigned i = leadingNumberNodeCount; i < m_children.size(); ++i)
                newChildren.uncheckedAppend(m_children[i].copyRef());
        }

        newChildren.shrinkToFit();
        m_children = WTFMove(newChildren);
    }

    if (isMinOrMaxNode() && canCombineAllChildren()) {
        auto combinedUnitType = m_children[0]->primitiveType();
        auto category = calculationCategoryForCombination(combinedUnitType);
        if (category != CalculationCategory::Other)
            combinedUnitType = canonicalUnitTypeForCalculationCategory(category);

        double resolvedValue = doubleValue(combinedUnitType);
        auto newChild = CSSCalcPrimitiveValueNode::create(CSSPrimitiveValue::create(resolvedValue, combinedUnitType));

        m_children.clear();
        m_children.append(WTFMove(newChild));
    }
}

// https://drafts.csswg.org/css-values-4/#simplify-a-calculation-tree

Ref<CSSCalcExpressionNode> CSSCalcOperationNode::simplify(Ref<CSSCalcExpressionNode>&& rootNode)
{
    return simplifyRecursive(WTFMove(rootNode), 0);
}

Ref<CSSCalcExpressionNode> CSSCalcOperationNode::simplifyRecursive(Ref<CSSCalcExpressionNode>&& rootNode, int depth)
{
    if (is<CSSCalcOperationNode>(rootNode)) {
        auto& operationNode = downcast<CSSCalcOperationNode>(rootNode.get());
        
        auto& children = operationNode.children();
        for (unsigned i = 0; i < children.size(); ++i) {
            auto child = children[i].copyRef();
            auto newNode = simplifyRecursive(WTFMove(child), depth + 1);
            if (newNode.ptr() != children[i].ptr())
                children[i] = WTFMove(newNode);
        }
    } else if (is<CSSCalcNegateNode>(rootNode)) {
        auto& negateNode = downcast<CSSCalcNegateNode>(rootNode.get());
        Ref<CSSCalcExpressionNode> child = negateNode.child();
        auto newNode = simplifyRecursive(WTFMove(child), depth + 1);
        if (newNode.ptr() != &negateNode.child())
            negateNode.setChild(WTFMove(newNode));
    } else if (is<CSSCalcInvertNode>(rootNode)) {
        auto& invertNode = downcast<CSSCalcInvertNode>(rootNode.get());
        Ref<CSSCalcExpressionNode> child = invertNode.child();
        auto newNode = simplifyRecursive(WTFMove(child), depth + 1);
        if (newNode.ptr() != &invertNode.child())
            invertNode.setChild(WTFMove(newNode));
    }

    return simplifyNode(WTFMove(rootNode), depth);
}

Ref<CSSCalcExpressionNode> CSSCalcOperationNode::simplifyNode(Ref<CSSCalcExpressionNode>&& rootNode, int depth)
{
    if (is<CSSCalcPrimitiveValueNode>(rootNode)) {
        // If root is a percentage that will be resolved against another value, and there is enough information
        // available to resolve it, do so, and express the resulting numeric value in the appropriate canonical
        // unit. Return the value.

        // If root is a dimension that is not expressed in its canonical unit, and there is enough information
        // available to convert it to the canonical unit, do so, and return the value.
        auto& primitiveValueNode = downcast<CSSCalcPrimitiveValueNode>(rootNode.get());
        primitiveValueNode.canonicalizeUnit();
        return WTFMove(rootNode);
    }

    // If root is an operator node that’s not one of the calc-operator nodes, and all of its children are numeric values
    // with enough information to computed the operation root represents, return the result of running root’s operation
    // using its children, expressed in the result’s canonical unit.
    if (is<CSSCalcOperationNode>(rootNode)) {
        auto& calcOperationNode = downcast<CSSCalcOperationNode>(rootNode.get());
        // Don't simplify at the root, otherwise we lose track of the operation for serialization.
        if (calcOperationNode.children().size() == 1 && depth)
            return WTFMove(calcOperationNode.children()[0]);
        
        if (calcOperationNode.isCalcSumNode()) {
            calcOperationNode.hoistChildrenWithOperator(CalcOperator::Add);
            calcOperationNode.combineChildren();
        }

        if (calcOperationNode.isCalcProductNode()) {
            calcOperationNode.hoistChildrenWithOperator(CalcOperator::Multiply);
            calcOperationNode.combineChildren();
        }
        
        if (calcOperationNode.isMinOrMaxNode())
            calcOperationNode.combineChildren();

        // If only one child remains, return the child (except at the root).
        auto shouldCombineParentWithOnlyChild = [](const CSSCalcOperationNode& parent, int depth)
        {
            if (parent.children().size() != 1)
                return false;

            // Always simplify below the root.
            if (depth)
                return true;

            // At the root, preserve the root function by only merging nodes with the same function.
            auto& child = parent.children().first();
            if (!is<CSSCalcOperationNode>(child))
                return false;

            auto parentFunction = functionFromOperator(parent.calcOperator());
            auto childFunction = functionFromOperator(downcast<CSSCalcOperationNode>(child.get()).calcOperator());
            return childFunction == parentFunction;
        };

        if (shouldCombineParentWithOnlyChild(calcOperationNode, depth))
            return WTFMove(calcOperationNode.children().first());

        return WTFMove(rootNode);
    }

    if (is<CSSCalcNegateNode>(rootNode)) {
        auto& childNode = downcast<CSSCalcNegateNode>(rootNode.get()).child();
        // If root’s child is a numeric value, return an equivalent numeric value, but with the value negated (0 - value).
        if (is<CSSCalcPrimitiveValueNode>(childNode) && downcast<CSSCalcPrimitiveValueNode>(childNode).isNumericValue()) {
            downcast<CSSCalcPrimitiveValueNode>(childNode).negate();
            return childNode;
        }
        
        // If root’s child is a Negate node, return the child’s child.
        if (is<CSSCalcNegateNode>(childNode))
            return downcast<CSSCalcNegateNode>(childNode).child();
        
        return WTFMove(rootNode);
    }

    if (is<CSSCalcInvertNode>(rootNode)) {
        auto& childNode = downcast<CSSCalcInvertNode>(rootNode.get()).child();
        // If root’s child is a number (not a percentage or dimension) return the reciprocal of the child’s value.
        if (is<CSSCalcPrimitiveValueNode>(childNode) && downcast<CSSCalcPrimitiveValueNode>(childNode).isNumericValue()) {
            downcast<CSSCalcPrimitiveValueNode>(childNode).invert();
            return childNode;
        }

        // If root’s child is an Invert node, return the child’s child.
        if (is<CSSCalcInvertNode>(childNode))
            return downcast<CSSCalcInvertNode>(childNode).child();

        return WTFMove(rootNode);
    }

    return WTFMove(rootNode);
}

CSSUnitType CSSCalcOperationNode::primitiveType() const
{
    auto unitCategory = category();
    switch (unitCategory) {
    case CalculationCategory::Number:
#if ASSERT_ENABLED
        for (auto& child : m_children)
            ASSERT(child->category() == CalculationCategory::Number);
#endif
        return CSSUnitType::CSS_NUMBER;

    case CalculationCategory::Percent: {
        if (m_children.isEmpty())
            return CSSUnitType::CSS_UNKNOWN;

        if (m_children.size() == 2) {
            if (m_children[0]->category() == CalculationCategory::Number)
                return m_children[1]->primitiveType();
            if (m_children[1]->category() == CalculationCategory::Number)
                return m_children[0]->primitiveType();
        }
        CSSUnitType firstType = m_children[0]->primitiveType();
        for (auto& child : m_children) {
            if (firstType != child->primitiveType())
                return CSSUnitType::CSS_UNKNOWN;
        }
        return firstType;
    }

    case CalculationCategory::Length:
    case CalculationCategory::Angle:
    case CalculationCategory::Time:
    case CalculationCategory::Frequency:
        if (m_children.size() == 1)
            return m_children.first()->primitiveType();
        return canonicalUnitTypeForCalculationCategory(unitCategory);

    case CalculationCategory::PercentLength:
    case CalculationCategory::PercentNumber:
    case CalculationCategory::Other:
        return CSSUnitType::CSS_UNKNOWN;
    }
    ASSERT_NOT_REACHED();
    return CSSUnitType::CSS_UNKNOWN;
}

std::unique_ptr<CalcExpressionNode> CSSCalcOperationNode::createCalcExpression(const CSSToLengthConversionData& conversionData) const
{
    Vector<std::unique_ptr<CalcExpressionNode>> nodes;
    nodes.reserveInitialCapacity(m_children.size());

    for (auto& child : m_children) {
        auto node = child->createCalcExpression(conversionData);
        if (!node)
            return nullptr;
        nodes.uncheckedAppend(WTFMove(node));
    }

    // Reverse the operation we did when creating this node, recovering a suitable destination category for otherwise-ambiguous min/max/clamp nodes.
    // Note that this category is really only good enough for that purpose and is not accurate for other node types; we could use a boolean instead.
    auto destinationCategory = CalculationCategory::Other;
    if (category() == CalculationCategory::PercentLength)
        destinationCategory = CalculationCategory::Length;
    else if (category() == CalculationCategory::PercentNumber)
        destinationCategory = CalculationCategory::Number;

    return makeUnique<CalcExpressionOperation>(WTFMove(nodes), m_operator, destinationCategory);
}

double CSSCalcOperationNode::doubleValue(CSSUnitType unitType) const
{
    bool allowNumbers = calcOperator() == CalcOperator::Multiply;

    return evaluate(m_children.map([&] (auto& child) {
        CSSUnitType childType = unitType;
        if (allowNumbers && unitType != CSSUnitType::CSS_NUMBER && child->primitiveType() == CSSUnitType::CSS_NUMBER)
            childType = CSSUnitType::CSS_NUMBER;
        return child->doubleValue(childType);
    }));
}

double CSSCalcOperationNode::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    return evaluate(m_children.map([&] (auto& child) {
        return child->computeLengthPx(conversionData);
    }));
}

void CSSCalcOperationNode::collectDirectComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    for (auto& child : m_children)
        child->collectDirectComputationalDependencies(values);
}

void CSSCalcOperationNode::collectDirectRootComputationalDependencies(HashSet<CSSPropertyID>& values) const
{
    for (auto& child : m_children)
        child->collectDirectRootComputationalDependencies(values);
}

void CSSCalcOperationNode::buildCSSText(const CSSCalcExpressionNode& node, StringBuilder& builder)
{
    auto shouldOutputEnclosingCalc = [](const CSSCalcExpressionNode& rootNode) {
        if (is<CSSCalcOperationNode>(rootNode)) {
            auto& operationNode = downcast<CSSCalcOperationNode>(rootNode);
            return operationNode.isCalcSumNode() || operationNode.isCalcProductNode();
        }
        return !is<CSSCalcPrimitiveValueNode>(rootNode);
    };
    
    bool outputCalc = shouldOutputEnclosingCalc(node);
    if (outputCalc)
        builder.append("calc(");

    buildCSSTextRecursive(node, builder, GroupingParens::Omit);

    if (outputCalc)
        builder.append(')');
}

static const char* functionPrefixForOperator(CalcOperator op)
{
    switch (op) {
    case CalcOperator::Add:
    case CalcOperator::Subtract:
    case CalcOperator::Multiply:
    case CalcOperator::Divide:
        ASSERT_NOT_REACHED();
        return "";
    case CalcOperator::Min: return "min(";
    case CalcOperator::Max: return "max(";
    case CalcOperator::Clamp: return "clamp(";
    }
    
    return "";
}

// <https://drafts.csswg.org/css-values-4/#serialize-a-calculation-tree>
void CSSCalcOperationNode::buildCSSTextRecursive(const CSSCalcExpressionNode& node, StringBuilder& builder, GroupingParens parens)
{
    // If root is a numeric value, or a non-math function, serialize root per the normal rules for it and return the result.
    if (is<CSSCalcPrimitiveValueNode>(node)) {
        auto& valueNode = downcast<CSSCalcPrimitiveValueNode>(node);
        builder.append(valueNode.customCSSText());
        return;
    }

    if (is<CSSCalcOperationNode>(node)) {
        auto& operationNode = downcast<CSSCalcOperationNode>(node);

        if (operationNode.isCalcSumNode()) {
            // If root is a Sum node, let s be a string initially containing "(".
            if (parens == GroupingParens::Include)
                builder.append('(');

            // Simplification already sorted children.
            auto& children = operationNode.children();
            ASSERT(children.size());
            // Serialize root’s first child, and append it to s.
            buildCSSTextRecursive(children.first(), builder);

            // For each child of root beyond the first:
            // If child is a Negate node, append " - " to s, then serialize the Negate’s child and append the result to s.
            // If child is a negative numeric value, append " - " to s, then serialize the negation of child as normal and append the result to s.
            // Otherwise, append " + " to s, then serialize child and append the result to s.
            for (unsigned i = 1; i < children.size(); ++i) {
                auto& child = children[i];
                if (is<CSSCalcNegateNode>(child)) {
                    builder.append(" - ");
                    buildCSSTextRecursive(downcast<CSSCalcNegateNode>(child.get()).child(), builder);
                    continue;
                }
                
                if (is<CSSCalcPrimitiveValueNode>(child)) {
                    auto& primitiveValueNode = downcast<CSSCalcPrimitiveValueNode>(child.get());
                    if (primitiveValueNode.isNegative()) {
                        builder.append(" - ");
                        // Serialize the negation of child.
                        auto unitType = primitiveValueNode.value().primitiveType();
                        builder.append(0 - primitiveValueNode.value().doubleValue(), CSSPrimitiveValue::unitTypeString(unitType));
                        continue;
                    }
                }
                
                builder.append(" + ");
                buildCSSTextRecursive(child, builder);
            }

            if (parens == GroupingParens::Include)
                builder.append(')');
            return;
        }
        
        if (operationNode.isCalcProductNode()) {
            // If root is a Product node, let s be a string initially containing "(".
            if (parens == GroupingParens::Include)
                builder.append('(');

            // Simplification already sorted children.
            auto& children = operationNode.children();
            ASSERT(children.size());
            // Serialize root’s first child, and append it to s.
            buildCSSTextRecursive(children.first(), builder);

            // For each child of root beyond the first:
            // If child is an Invert node, append " / " to s, then serialize the Invert’s child and append the result to s.
            // Otherwise, append " * " to s, then serialize child and append the result to s.
            for (unsigned i = 1; i < children.size(); ++i) {
                auto& child = children[i];
                if (is<CSSCalcInvertNode>(child)) {
                    builder.append(" / ");
                    buildCSSTextRecursive(downcast<CSSCalcInvertNode>(child.get()).child(), builder);
                    continue;
                }

                builder.append(" * ");
                buildCSSTextRecursive(child, builder);
            }

            if (parens == GroupingParens::Include)
                builder.append(')');
            return;
        }

        // If root is anything but a Sum, Negate, Product, or Invert node, serialize a math function for the
        // function corresponding to the node type, treating the node’s children as the function’s
        // comma-separated calculation arguments, and return the result.
        builder.append(functionPrefixForOperator(operationNode.calcOperator()));

        auto& children = operationNode.children();
        ASSERT(children.size());
        buildCSSTextRecursive(children.first(), builder, GroupingParens::Omit);

        for (unsigned i = 1; i < children.size(); ++i) {
            builder.append(", ");
            buildCSSTextRecursive(children[i], builder, GroupingParens::Omit);
        }
        
        builder.append(')');
        return;
    }
    
    if (is<CSSCalcNegateNode>(node)) {
        auto& negateNode = downcast<CSSCalcNegateNode>(node);
        // If root is a Negate node, let s be a string initially containing "(-1 * ".
        builder.append("-1 *");
        buildCSSTextRecursive(negateNode.child(), builder);
        return;
    }
    
    if (is<CSSCalcInvertNode>(node)) {
        auto& invertNode = downcast<CSSCalcInvertNode>(node);
        // If root is an Invert node, let s be a string initially containing "(1 / ".
        builder.append("1 / ");
        buildCSSTextRecursive(invertNode.child(), builder);
        return;
    }
}

void CSSCalcOperationNode::dump(TextStream& ts) const
{
    ts << "calc operation " << m_operator << " (category: " << category() << ", type " << primitiveType() << ")";

    TextStream::GroupScope scope(ts);
    ts << m_children.size() << " children";
    for (auto& child : m_children)
        ts.dumpProperty("node", child);
}

bool CSSCalcOperationNode::equals(const CSSCalcExpressionNode& exp) const
{
    if (type() != exp.type())
        return false;

    const CSSCalcOperationNode& other = static_cast<const CSSCalcOperationNode&>(exp);

    if (m_children.size() != other.m_children.size() || m_operator != other.m_operator)
        return false;

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (!compareCSSValue(m_children[i], other.m_children[i]))
            return false;
    }
    return true;
}

double CSSCalcOperationNode::evaluateOperator(CalcOperator op, const Vector<double>& children)
{
    switch (op) {
    case CalcOperator::Add: {
        double sum = 0;
        for (auto& child : children)
            sum += child;
        return sum;
    }
    case CalcOperator::Subtract:
        ASSERT(children.size() == 2);
        return children[0] - children[1];
    case CalcOperator::Multiply: {
        double product = 1;
        for (auto& child : children)
            product *= child;
        return product;
    }
    case CalcOperator::Divide:
        ASSERT(children.size() == 1 || children.size() == 2);
        if (children.size() == 1)
            return std::numeric_limits<double>::quiet_NaN();
        return children[0] / children[1];
    case CalcOperator::Min: {
        if (children.isEmpty())
            return std::numeric_limits<double>::quiet_NaN();
        double minimum = children[0];
        for (auto child : children)
            minimum = std::min(minimum, child);
        return minimum;
    }
    case CalcOperator::Max: {
        if (children.isEmpty())
            return std::numeric_limits<double>::quiet_NaN();
        double maximum = children[0];
        for (auto child : children)
            maximum = std::max(maximum, child);
        return maximum;
    }
    case CalcOperator::Clamp: {
        if (children.size() != 3)
            return std::numeric_limits<double>::quiet_NaN();
        double min = children[0];
        double value = children[1];
        double max = children[2];
        return std::max(min, std::min(value, max));
    }
    }
    ASSERT_NOT_REACHED();
    return 0;
}



}
