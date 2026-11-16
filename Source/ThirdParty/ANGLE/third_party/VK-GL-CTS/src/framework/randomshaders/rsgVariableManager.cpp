/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Variable manager.
 *//*--------------------------------------------------------------------*/

#include "rsgVariableManager.hpp"

#include <algorithm>
#include <map>
#include <set>

using std::map;
using std::set;
using std::vector;

namespace rsg
{

class SubValueRangeIterator
{
public:
    SubValueRangeIterator(const ConstValueRangeAccess &valueRange);
    ~SubValueRangeIterator(void)
    {
    }

    bool hasItem(void) const;
    ConstValueRangeAccess getItem(void) const;
    void next(void);

private:
    vector<ConstValueRangeAccess> m_stack;
};

SubValueRangeIterator::SubValueRangeIterator(const ConstValueRangeAccess &valueRange)
{
    m_stack.push_back(valueRange);
}

inline bool SubValueRangeIterator::hasItem(void) const
{
    return !m_stack.empty();
}

inline ConstValueRangeAccess SubValueRangeIterator::getItem(void) const
{
    return m_stack[m_stack.size() - 1];
}

void SubValueRangeIterator::next(void)
{
    ConstValueRangeAccess curItem = getItem();
    m_stack.pop_back(); // Remove current

    switch (curItem.getType().getBaseType())
    {
    case VariableType::TYPE_ARRAY:
    {
        int numElements = curItem.getType().getNumElements();
        for (int ndx = 0; ndx < numElements; ndx++)
            m_stack.push_back(curItem.member(ndx));
        break;
    }

    case VariableType::TYPE_STRUCT:
    {
        int numMembers = (int)curItem.getType().getMembers().size();
        for (int ndx = 0; ndx < numMembers; ndx++)
            m_stack.push_back(curItem.member(ndx));
        break;
    }

    default:
        break; // \todo [2011-02-03 pyry] Swizzle control?
    }
}

ValueEntry::ValueEntry(const Variable *variable) : m_variable(variable), m_valueRange(variable->getType())
{
}

VariableScope::VariableScope(void)
{
}

VariableScope::~VariableScope(void)
{
    for (vector<Variable *>::iterator i = m_declaredVariables.begin(); i != m_declaredVariables.end(); i++)
        delete *i;

    for (vector<Variable *>::iterator i = m_liveVariables.begin(); i != m_liveVariables.end(); i++)
        delete *i;
}

Variable *VariableScope::allocate(const VariableType &type, Variable::Storage storage, const char *name)
{
    Variable *variable = new Variable(type, storage, name);
    try
    {
        m_liveVariables.push_back(variable);
        return variable;
    }
    catch (const std::exception &)
    {
        delete variable;
        throw;
    }
}

void VariableScope::declare(Variable *variable)
{
    m_declaredVariables.push_back(variable);
    removeLive(variable);
}

void VariableScope::removeLive(const Variable *variable)
{
    vector<Variable *>::iterator pos = std::find(m_liveVariables.begin(), m_liveVariables.end(), variable);
    DE_ASSERT(pos != m_liveVariables.end());

    // \todo [pyry] Not so efficient
    m_liveVariables.erase(pos);
}

ValueScope::ValueScope(void)
{
}

ValueScope::~ValueScope(void)
{
    clear();
}

void ValueScope::clear(void)
{
    for (vector<ValueEntry *>::iterator i = m_entries.begin(); i != m_entries.end(); i++)
        delete *i;
    m_entries.clear();
}

ValueEntry *ValueScope::allocate(const Variable *variable)
{
    ValueEntry *entry = new ValueEntry(variable);
    try
    {
        m_entries.push_back(entry);
        return entry;
    }
    catch (const std::exception &)
    {
        delete entry;
        throw;
    }
}

class CompareEntryVariable
{
public:
    CompareEntryVariable(const Variable *variable) : m_variable(variable)
    {
    }

    bool operator==(const ValueEntry *entry) const
    {
        return entry->getVariable() == m_variable;
    }

private:
    const Variable *m_variable;
};

bool operator==(const ValueEntry *entry, const CompareEntryVariable &cmp)
{
    return cmp == entry;
}

ValueEntry *ValueScope::findEntry(const Variable *variable) const
{
    vector<ValueEntry *>::const_iterator pos =
        std::find(m_entries.begin(), m_entries.end(), CompareEntryVariable(variable));
    return pos != m_entries.end() ? *pos : nullptr;
}

void ValueScope::setValue(const Variable *variable, ConstValueRangeAccess value)
{
    ValueEntry *entry = findEntry(variable);
    DE_ASSERT(entry);

    ValueRangeAccess dst = entry->getValueRange();
    dst.getMin()         = value.getMin().value();
    dst.getMax()         = value.getMax().value();
}

void ValueScope::removeValue(const Variable *variable)
{
    vector<ValueEntry *>::iterator pos = std::find(m_entries.begin(), m_entries.end(), CompareEntryVariable(variable));
    if (pos != m_entries.end())
    {
        ValueEntry *entry = *pos;
        m_entries.erase(pos);
        delete entry;
    }
}

VariableManager::VariableManager(NameAllocator &nameAllocator)
    : m_numAllocatedScalars(0)
    , m_numAllocatedShaderInScalars(0)
    , m_numAllocatedShaderInVariables(0)
    , m_numAllocatedUniformScalars(0)
    , m_nameAllocator(nameAllocator)
{
}

VariableManager::~VariableManager(void)
{
}

Variable *VariableManager::allocate(const VariableType &type)
{
    return allocate(type, Variable::STORAGE_LOCAL, m_nameAllocator.allocate().c_str());
}

Variable *VariableManager::allocate(const VariableType &type, Variable::Storage storage, const char *name)
{
    VariableScope &varScope = getCurVariableScope();
    ValueScope &valueScope  = getCurValueScope();
    int numScalars          = type.getScalarSize();

    // Allocate in current scope
    Variable *variable = varScope.allocate(type, Variable::STORAGE_LOCAL, name);

    // Allocate value entry
    ValueEntry *valueEntry = valueScope.allocate(variable);

    // Add to cache
    m_entryCache.push_back(valueEntry);

    m_numAllocatedScalars += numScalars;

    // Set actual storage - affects uniform/shader in allocations.
    setStorage(variable, storage);

    return variable;
}

void VariableManager::setStorage(Variable *variable, Variable::Storage storage)
{
    int numScalars = variable->getType().getScalarSize();

    // Decrement old.
    if (variable->getStorage() == Variable::STORAGE_SHADER_IN)
    {
        m_numAllocatedShaderInScalars -= numScalars;
        m_numAllocatedShaderInVariables -= 1;
    }
    else if (variable->getStorage() == Variable::STORAGE_UNIFORM)
        m_numAllocatedUniformScalars -= numScalars;

    // Add new.
    if (storage == Variable::STORAGE_SHADER_IN)
    {
        m_numAllocatedShaderInScalars += numScalars;
        m_numAllocatedShaderInVariables += 1;
    }
    else if (storage == Variable::STORAGE_UNIFORM)
        m_numAllocatedUniformScalars += numScalars;

    variable->setStorage(storage);
}

bool VariableManager::canDeclareInCurrentScope(const Variable *variable) const
{
    const vector<Variable *> &curLiveVars = getCurVariableScope().getLiveVariables();
    return std::find(curLiveVars.begin(), curLiveVars.end(), variable) != curLiveVars.end();
}

const vector<Variable *> &VariableManager::getLiveVariables(void) const
{
    return getCurVariableScope().getLiveVariables();
}

void VariableManager::declareVariable(Variable *variable)
{
    // Remove from cache if exists in there.
    std::vector<const ValueEntry *>::iterator pos =
        std::find(m_entryCache.begin(), m_entryCache.end(), CompareEntryVariable(variable));
    if (pos != m_entryCache.end())
        m_entryCache.erase(pos);

    DE_ASSERT(std::find(m_entryCache.begin(), m_entryCache.end(), CompareEntryVariable(variable)) ==
              m_entryCache.end());

    // Remove from scope stack.
    for (vector<ValueScope *>::const_iterator stackIter = m_valueScopeStack.begin();
         stackIter != m_valueScopeStack.end(); stackIter++)
    {
        ValueScope *scope = *stackIter;
        scope->removeValue(variable);
    }

    // Declare in current scope.
    getCurVariableScope().declare(variable);
}

const ValueEntry *VariableManager::getValue(const Variable *variable) const
{
    vector<const ValueEntry *>::const_iterator pos =
        std::find(m_entryCache.begin(), m_entryCache.end(), CompareEntryVariable(variable));
    return pos != m_entryCache.end() ? *pos : nullptr;
}

void VariableManager::removeValueFromCurrentScope(const Variable *variable)
{
    // Remove from cache
    std::vector<const ValueEntry *>::iterator pos =
        std::find(m_entryCache.begin(), m_entryCache.end(), CompareEntryVariable(variable));
    DE_ASSERT(pos != m_entryCache.end());
    m_entryCache.erase(pos);

    // Remove from current scope \note May not exist in there.
    getCurValueScope().removeValue(variable);
}

const ValueEntry *VariableManager::getParentValue(const Variable *variable) const
{
    if (m_valueScopeStack.size() < 2)
        return nullptr; // Only single value scope

    for (vector<ValueScope *>::const_reverse_iterator i = m_valueScopeStack.rbegin() + 1; i != m_valueScopeStack.rend();
         i++)
    {
        const ValueScope *scope = *i;
        ValueEntry *entry       = scope->findEntry(variable);

        if (entry)
            return entry;
    }

    return nullptr; // Not found in stack
}

void VariableManager::setValue(const Variable *variable, ConstValueRangeAccess value)
{
    ValueScope &curScope = getCurValueScope();

    if (!curScope.findEntry(variable))
    {
        // New value, allocate and update cache.
        ValueEntry *newEntry = curScope.allocate(variable);
        std::vector<const ValueEntry *>::iterator cachePos =
            std::find(m_entryCache.begin(), m_entryCache.end(), CompareEntryVariable(variable));

        if (cachePos != m_entryCache.end())
            *cachePos = newEntry;
        else
            m_entryCache.push_back(newEntry);
    }

    curScope.setValue(variable, value);
}

void VariableManager::reserve(ReservedScalars &store, int numScalars)
{
    DE_ASSERT(store.numScalars == 0);
    store.numScalars = numScalars;
    m_numAllocatedScalars += numScalars;
}

void VariableManager::release(ReservedScalars &store)
{
    m_numAllocatedScalars -= store.numScalars;
    store.numScalars = 0;
}

void VariableManager::pushVariableScope(VariableScope &scope)
{
    // Expects emtpy scope
    DE_ASSERT(scope.getDeclaredVariables().size() == 0);
    DE_ASSERT(scope.getLiveVariables().size() == 0);

    m_variableScopeStack.push_back(&scope);
}

void VariableManager::popVariableScope(void)
{
    VariableScope &curScope = getCurVariableScope();

    // Migrate live variables to parent scope.
    // Variables allocated in child scopes can be declared in any parent scope but not the other way around.
    if (m_variableScopeStack.size() > 1)
    {
        VariableScope &parentScope        = *m_variableScopeStack[m_variableScopeStack.size() - 2];
        vector<Variable *> &curLiveVars   = curScope.getLiveVariables();
        vector<Variable *> &parenLiveVars = parentScope.getLiveVariables();

        while (!curLiveVars.empty())
        {
            Variable *liveVar = curLiveVars.back();
            parenLiveVars.push_back(liveVar);
            curLiveVars.pop_back();
        }
    }

    // All variables should be either migrated to parent or declared (in case of root scope).
    DE_ASSERT(curScope.getLiveVariables().size() == 0);

    m_variableScopeStack.pop_back();
}

void VariableManager::pushValueScope(ValueScope &scope)
{
    // Value scope should be empty
    DE_ASSERT(scope.getValues().size() == 0);

    m_valueScopeStack.push_back(&scope);
}

void VariableManager::popValueScope(void)
{
    ValueScope &oldScope = getCurValueScope();

    // Pop scope and clear cache.
    m_valueScopeStack.pop_back();
    m_entryCache.clear();

    // Re-build entry cache.
    if (!m_valueScopeStack.empty())
    {
        ValueScope &newTopScope = getCurValueScope();

        // Speed up computing intersections.
        map<const Variable *, const ValueEntry *> oldValues;
        const vector<ValueEntry *> &oldEntries = oldScope.getValues();

        for (vector<ValueEntry *>::const_iterator valueIter = oldEntries.begin(); valueIter != oldEntries.end();
             valueIter++)
            oldValues[(*valueIter)->getVariable()] = *valueIter;

        set<const Variable *> addedVars;

        // Re-build based on current stack.
        for (vector<ValueScope *>::reverse_iterator scopeIter = m_valueScopeStack.rbegin();
             scopeIter != m_valueScopeStack.rend(); scopeIter++)
        {
            const ValueScope *scope                  = *scopeIter;
            const vector<ValueEntry *> &valueEntries = scope->getValues();

            for (vector<ValueEntry *>::const_iterator valueIter = valueEntries.begin(); valueIter != valueEntries.end();
                 valueIter++)
            {
                const ValueEntry *entry = *valueIter;
                const Variable *var     = entry->getVariable();

                if (addedVars.find(var) != addedVars.end())
                    continue; // Already in cache, set deeper in scope stack.

                DE_ASSERT(std::find(m_entryCache.begin(), m_entryCache.end(), CompareEntryVariable(var)) ==
                          m_entryCache.end());

                if (oldValues.find(var) != oldValues.end())
                {
                    const ValueEntry *oldEntry = oldValues[var];

                    // Build new intersected value and store into current scope.
                    ValueRange intersectedValue(var->getType());
                    DE_ASSERT(oldEntry->getValueRange().intersects(entry->getValueRange())); // Must intersect
                    ValueRange::computeIntersection(intersectedValue, entry->getValueRange(),
                                                    oldEntry->getValueRange());

                    if (!newTopScope.findEntry(var))
                        newTopScope.allocate(var);

                    newTopScope.setValue(var, intersectedValue.asAccess());

                    // Add entry from top scope to cache.
                    m_entryCache.push_back(newTopScope.findEntry(var));
                }
                else
                    m_entryCache.push_back(entry); // Just add to cache.

                addedVars.insert(var); // Record as cached variable.
            }
        }

        // Copy entries from popped scope that don't yet exist in the stack.
        for (vector<ValueEntry *>::const_iterator valueIter = oldEntries.begin(); valueIter != oldEntries.end();
             valueIter++)
        {
            const ValueEntry *oldEntry = *valueIter;
            const Variable *var        = oldEntry->getVariable();

            if (addedVars.find(var) == addedVars.end())
                setValue(var, oldEntry->getValueRange());
        }
    }
}

} // namespace rsg
