/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSValueList.h"

#include "CSSFunctionValue.h"
#include "CSSParserValues.h"
#include "CSSPrimitiveValue.h"
#include "CSSVariableDependentValue.h"
#include "CSSVariableValue.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSValueList::CSSValueList(ClassType classType, ValueListSeparator listSeparator)
    : CSSValue(classType)
{
    m_valueListSeparator = listSeparator;
}

CSSValueList::CSSValueList(ValueListSeparator listSeparator)
    : CSSValue(ValueListClass)
{
    m_valueListSeparator = listSeparator;
}

CSSValueList::CSSValueList(CSSParserValueList& parserValues)
    : CSSValue(ValueListClass)
{
    m_valueListSeparator = SpaceSeparator;
    m_values.reserveInitialCapacity(parserValues.size());
    for (unsigned i = 0, size = parserValues.size(); i < size; ++i) {
        RefPtr<CSSValue> value = parserValues.valueAt(i)->createCSSValue();
        ASSERT(value);
        m_values.uncheckedAppend(value.releaseNonNull());
    }
}

bool CSSValueList::removeAll(CSSValue* value)
{
    // FIXME: Why even take a pointer?
    if (!value)
        return false;

    return m_values.removeAllMatching([value](auto& current) {
        return current->equals(*value);
    }) > 0;
}

bool CSSValueList::hasValue(CSSValue* val) const
{
    // FIXME: Why even take a pointer?
    if (!val)
        return false;

    for (unsigned i = 0, size = m_values.size(); i < size; ++i) {
        if (m_values[i].get().equals(*val))
            return true;
    }
    return false;
}

Ref<CSSValueList> CSSValueList::copy()
{
    RefPtr<CSSValueList> newList;
    switch (m_valueListSeparator) {
    case SpaceSeparator:
        newList = createSpaceSeparated();
        break;
    case CommaSeparator:
        newList = createCommaSeparated();
        break;
    case SlashSeparator:
        newList = createSlashSeparated();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    for (auto& value : m_values)
        newList->append(value.get());
    return newList.releaseNonNull();
}

String CSSValueList::customCSSText() const
{
    StringBuilder result;
    String separator;
    switch (m_valueListSeparator) {
    case SpaceSeparator:
        separator = ASCIILiteral(" ");
        break;
    case CommaSeparator:
        separator = ASCIILiteral(", ");
        break;
    case SlashSeparator:
        separator = ASCIILiteral(" / ");
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    for (auto& value : m_values) {
        bool suppressSeparator = false;
        if (m_valueListSeparator == SpaceSeparator && value->isPrimitiveValue()) {
            auto* primitiveValue = &downcast<CSSPrimitiveValue>(*value.ptr());
            if (primitiveValue->parserOperator() == ',')
                suppressSeparator = true;
        }
        if (!suppressSeparator && !result.isEmpty())
            result.append(separator);
        result.append(value.get().cssText());
    }

    return result.toString();
}

bool CSSValueList::equals(const CSSValueList& other) const
{
    if (m_valueListSeparator != other.m_valueListSeparator)
        return false;

    if (m_values.size() != other.m_values.size())
        return false;

    for (unsigned i = 0, size = m_values.size(); i < size; ++i) {
        if (!m_values[i].get().equals(other.m_values[i]))
            return false;
    }
    return true;
}

bool CSSValueList::equals(const CSSValue& other) const
{
    if (m_values.size() != 1)
        return false;

    return m_values[0].get().equals(other);
}

void CSSValueList::addSubresourceStyleURLs(ListHashSet<URL>& urls, const StyleSheetContents* styleSheet) const
{
    for (unsigned i = 0, size = m_values.size(); i < size; ++i)
        m_values[i].get().addSubresourceStyleURLs(urls, styleSheet);
}

bool CSSValueList::traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const
{
    for (unsigned i = 0; i < m_values.size(); ++i) {
        if (m_values[i].get().traverseSubresources(handler))
            return true;
    }
    return false;
}

CSSValueList::CSSValueList(const CSSValueList& cloneFrom)
    : CSSValue(cloneFrom.classType(), /* isCSSOMSafe */ true)
{
    m_valueListSeparator = cloneFrom.m_valueListSeparator;
    m_values.reserveInitialCapacity(cloneFrom.m_values.size());
    for (unsigned i = 0, size = cloneFrom.m_values.size(); i < size; ++i)
        m_values.uncheckedAppend(*cloneFrom.m_values[i]->cloneForCSSOM());
}

Ref<CSSValueList> CSSValueList::cloneForCSSOM() const
{
    return adoptRef(*new CSSValueList(*this));
}


bool CSSValueList::containsVariables() const
{
    for (unsigned i = 0; i < m_values.size(); i++) {
        if (m_values[i]->isVariableValue())
            return true;
        if (m_values[i]->isFunctionValue()) {
            auto& functionValue = downcast<CSSFunctionValue>(*item(i));
            CSSValueList* args = functionValue.arguments();
            if (args && args->containsVariables())
                return true;
        } else if (m_values[i]->isValueList()) {
            auto& listValue = downcast<CSSValueList>(*item(i));
            if (listValue.containsVariables())
                return true;
        }
    }
    return false;
}

bool CSSValueList::checkVariablesForCycles(CustomPropertyValueMap& customProperties, HashSet<AtomicString>& seenProperties, HashSet<AtomicString>& invalidProperties) const
{
    for (unsigned i = 0; i < m_values.size(); i++) {
        auto* value = item(i);
        if (value->isVariableValue()) {
            auto& variableValue = downcast<CSSVariableValue>(*value);
            if (seenProperties.contains(variableValue.name()))
                return false;
            RefPtr<CSSValue> value = customProperties.get(variableValue.name());
            if (value && value->isVariableDependentValue() && !downcast<CSSVariableDependentValue>(*value).checkVariablesForCycles(variableValue.name(), customProperties, seenProperties, invalidProperties))
                return false;

            // Have to check the fallback values.
            auto* fallbackArgs = variableValue.fallbackArguments();
            if (!fallbackArgs || !fallbackArgs->length())
                continue;
            
            if (!fallbackArgs->checkVariablesForCycles(customProperties, seenProperties, invalidProperties))
                return false;
        } else if (value->isFunctionValue()) {
            auto& functionValue = downcast<CSSFunctionValue>(*value);
            auto* args = functionValue.arguments();
            if (args && !args->checkVariablesForCycles(customProperties, seenProperties, invalidProperties))
                return false;
        } else if (value->isValueList()) {
            auto& listValue = downcast<CSSValueList>(*value);
            if (!listValue.checkVariablesForCycles(customProperties, seenProperties, invalidProperties))
                return false;
        }
    }
    return true;
}

bool CSSValueList::buildParserValueSubstitutingVariables(CSSParserValue* result, const CustomPropertyValueMap& customProperties) const
{
    result->id = CSSValueInvalid;
    result->unit = CSSParserValue::ValueList;
    result->valueList = new CSSParserValueList();
    return buildParserValueListSubstitutingVariables(result->valueList, customProperties);
}

bool CSSValueList::buildParserValueListSubstitutingVariables(CSSParserValueList* parserList, const CustomPropertyValueMap& customProperties) const
{
    for (auto& value : m_values) {
        switch (value.get().classType()) {
        case FunctionClass: {
            CSSParserValue result;
            result.id = CSSValueInvalid;
            if (!downcast<CSSFunctionValue>(value.get()).buildParserValueSubstitutingVariables(&result, customProperties)) {
                WebCore::destroy(result);
                return false;
            }
            parserList->addValue(result);
            break;
        }
        case ValueListClass: {
            CSSParserValue result;
            result.id = CSSValueInvalid;
            if (!downcast<CSSValueList>(value.get()).buildParserValueSubstitutingVariables(&result, customProperties)) {
                WebCore::destroy(result);
                return false;
            }
            parserList->addValue(result);
            break;
        }
        case VariableClass: {
            if (!downcast<CSSVariableValue>(value.get()).buildParserValueListSubstitutingVariables(parserList, customProperties))
                return false;
            break;
        }
        case PrimitiveClass: {
            // FIXME: Will have to change this if we start preserving invalid tokens.
            CSSParserValue result;
            result.id = CSSValueInvalid;
            if (downcast<CSSPrimitiveValue>(value.get()).buildParserValue(&result))
                parserList->addValue(result);
            else
                WebCore::destroy(result);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return true;
}
    
} // namespace WebCore
