#ifndef _RSGVARIABLEMANAGER_HPP
#define _RSGVARIABLEMANAGER_HPP
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
 *
 * Memory management:
 *  Variable manager owns variable objects until they are either explictly
 *  removed or moved to currently active scope. After that the ownership
 *  is transferred to Scope or the object that called removeEntry().
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgVariable.hpp"
#include "rsgVariableValue.hpp"
#include "rsgNameAllocator.hpp"

#include <iterator>
#include <vector>
#include <set>

namespace rsg
{

class ValueEntry
{
public:
    ValueEntry(const Variable *variable);
    ~ValueEntry(void)
    {
    }

    const Variable *getVariable(void) const
    {
        return m_variable;
    }

    ConstValueRangeAccess getValueRange(void) const
    {
        return m_valueRange.asAccess();
    }
    ValueRangeAccess getValueRange(void)
    {
        return m_valueRange.asAccess();
    }

private:
    const Variable *m_variable;
    ValueRange m_valueRange;
};

// Variable scope manages variable allocation.
class VariableScope
{
public:
    VariableScope(void);
    ~VariableScope(void);

    Variable *allocate(const VariableType &type, Variable::Storage storage, const char *name);
    void declare(Variable *variable);          //!< Move from live set to declared set
    void removeLive(const Variable *variable); //!< Just remove from live set (when migrating to parent).

    const std::vector<Variable *> &getDeclaredVariables(void) const
    {
        return m_declaredVariables;
    }

    std::vector<Variable *> &getLiveVariables(void)
    {
        return m_liveVariables;
    }
    const std::vector<Variable *> &getLiveVariables(void) const
    {
        return m_liveVariables;
    }

private:
    VariableScope(const VariableScope &other);
    VariableScope &operator=(const VariableScope &other);

    std::vector<Variable *> m_declaredVariables; //!< Variables declared in this scope. Not available for expressions.
    std::vector<Variable *>
        m_liveVariables; //!< Live variables (available for expression) that can be declared in this scope.
};

class ValueScope
{
public:
    ValueScope(void);
    ~ValueScope(void);

    ValueEntry *allocate(const Variable *variable);
    ValueEntry *findEntry(const Variable *variable) const;
    void setValue(const Variable *variable, ConstValueRangeAccess value);
    void removeValue(const Variable *variable);

    std::vector<ValueEntry *> &getValues(void)
    {
        return m_entries;
    }
    const std::vector<ValueEntry *> &getValues(void) const
    {
        return m_entries;
    }

    void clear(void);

private:
    ValueScope(const ValueScope &other);
    ValueScope &operator=(const ValueScope &other);

    std::vector<ValueEntry *> m_entries;
};

class ReservedScalars
{
public:
    int numScalars;

    ReservedScalars(void) : numScalars(0)
    {
    }
};

// \todo [2011-05-26 pyry] Clean up this a bit, separate const variant.
template <typename Item, typename Iterator, class Filter>
class FilteredIterator
{
public:
    using iterator_category = std::input_iterator_tag;
    using value_type        = Item;
    using difference_type   = std::ptrdiff_t;
    using pointer           = Item *;
    using reference         = Item &;

    FilteredIterator(Iterator iter, Iterator end, Filter filter) : m_iter(iter), m_end(end), m_filter(filter)
    {
    }

    FilteredIterator operator+(ptrdiff_t offset) const
    {
        Iterator nextEntry = m_iter;
        while (offset--)
            nextEntry = findNext(m_filter, nextEntry, m_end);
        return FilteredIterator(nextEntry, m_end, m_filter);
    }

    FilteredIterator &operator++()
    {
        // Pre-increment
        m_iter = findNext(m_filter, m_iter, m_end);
        return *this;
    }

    FilteredIterator operator++(int)
    {
        // Post-increment
        FilteredIterator copy = *this;
        m_iter                = findNext(m_filter, m_iter, m_end);
        return copy;
    }

    bool operator==(const FilteredIterator &other) const
    {
        return m_iter == other.m_iter;
    }

    bool operator!=(const FilteredIterator &other) const
    {
        return m_iter != other.m_iter;
    }

    const Item &operator*(void)
    {
        DE_ASSERT(m_iter != m_end);
        DE_ASSERT(m_filter(*m_iter));
        return *m_iter;
    }

private:
    static Iterator findNext(Filter filter, Iterator iter, Iterator end)
    {
        do
            iter++;
        while (iter != end && !filter(*iter));
        return iter;
    }

    Iterator m_iter;
    Iterator m_end;
    Filter m_filter;
};

template <class Filter>
class ValueEntryIterator
    : public FilteredIterator<const ValueEntry *, std::vector<const ValueEntry *>::const_iterator, Filter>
{
public:
    ValueEntryIterator(std::vector<const ValueEntry *>::const_iterator begin,
                       std::vector<const ValueEntry *>::const_iterator end, Filter filter)
        : FilteredIterator<const ValueEntry *, std::vector<const ValueEntry *>::const_iterator, Filter>(begin, end,
                                                                                                        filter)
    {
    }
};

class VariableManager
{
public:
    VariableManager(NameAllocator &nameAllocator);
    ~VariableManager(void);

    int getNumAllocatedScalars(void) const
    {
        return m_numAllocatedScalars;
    }
    int getNumAllocatedShaderInScalars(void) const
    {
        return m_numAllocatedShaderInScalars;
    }
    int getNumAllocatedShaderInVariables(void) const
    {
        return m_numAllocatedShaderInVariables;
    }
    int getNumAllocatedUniformScalars(void) const
    {
        return m_numAllocatedUniformScalars;
    }

    void reserve(ReservedScalars &store, int numScalars);
    void release(ReservedScalars &store);

    Variable *allocate(const VariableType &type);
    Variable *allocate(const VariableType &type, Variable::Storage storage, const char *name);

    void setStorage(Variable *variable, Variable::Storage storage);

    void setValue(const Variable *variable, ConstValueRangeAccess value);
    const ValueEntry *getValue(const Variable *variable) const;
    const ValueEntry *getParentValue(const Variable *variable) const;

    void removeValueFromCurrentScope(const Variable *variable);

    void declareVariable(Variable *variable);
    bool canDeclareInCurrentScope(const Variable *variable) const;
    const std::vector<Variable *> &getLiveVariables(void) const;

    void pushVariableScope(VariableScope &scope);
    void popVariableScope(void);

    void pushValueScope(ValueScope &scope);
    void popValueScope(void);

    template <class Filter>
    ValueEntryIterator<Filter> getBegin(Filter filter = Filter()) const;

    template <class Filter>
    ValueEntryIterator<Filter> getEnd(Filter filter = Filter()) const;

    template <class Filter>
    bool hasEntry(Filter filter = Filter()) const;

private:
    VariableManager(const VariableManager &other);
    VariableManager &operator=(const VariableManager &other);

    VariableScope &getCurVariableScope(void)
    {
        return *m_variableScopeStack.back();
    }
    const VariableScope &getCurVariableScope(void) const
    {
        return *m_variableScopeStack.back();
    }

    ValueScope &getCurValueScope(void)
    {
        return *m_valueScopeStack.back();
    }
    const ValueScope &getCurValueScope(void) const
    {
        return *m_valueScopeStack.back();
    }

    std::vector<VariableScope *> m_variableScopeStack;
    std::vector<ValueScope *> m_valueScopeStack;

    std::vector<const ValueEntry *> m_entryCache; //!< For faster value entry access.

    int m_numAllocatedScalars;
    int m_numAllocatedShaderInScalars;
    int m_numAllocatedShaderInVariables;
    int m_numAllocatedUniformScalars;
    NameAllocator &m_nameAllocator;
};

template <class Filter>
ValueEntryIterator<Filter> VariableManager::getBegin(Filter filter) const
{
    std::vector<const ValueEntry *>::const_iterator first = m_entryCache.begin();
    while (first != m_entryCache.end() && !filter(*first))
        first++;
    return ValueEntryIterator<Filter>(first, m_entryCache.end(), filter);
}

template <class Filter>
ValueEntryIterator<Filter> VariableManager::getEnd(Filter filter) const
{
    return ValueEntryIterator<Filter>(m_entryCache.end(), m_entryCache.end(), filter);
}

template <class Filter>
bool VariableManager::hasEntry(Filter filter) const
{
    for (std::vector<const ValueEntry *>::const_iterator i = m_entryCache.begin(); i != m_entryCache.end(); i++)
    {
        if (filter(*i))
            return true;
    }
    return false;
}

// Common filters

class AnyEntry
{
public:
    typedef ValueEntryIterator<AnyEntry> Iterator;

    bool operator()(const ValueEntry *entry) const
    {
        DE_UNREF(entry);
        return true;
    }
};

class IsWritableEntry
{
public:
    bool operator()(const ValueEntry *entry) const
    {
        switch (entry->getVariable()->getStorage())
        {
        case Variable::STORAGE_LOCAL:
        case Variable::STORAGE_SHADER_OUT:
        case Variable::STORAGE_PARAMETER_IN:
        case Variable::STORAGE_PARAMETER_OUT:
        case Variable::STORAGE_PARAMETER_INOUT:
            return true;

        default:
            return false;
        }
    }
};

template <Variable::Storage Storage>
class EntryStorageFilter
{
public:
    typedef ValueEntryIterator<EntryStorageFilter<Storage>> Iterator;

    bool operator()(const ValueEntry *entry) const
    {
        return entry->getVariable()->getStorage() == Storage;
    }
};

typedef EntryStorageFilter<Variable::STORAGE_LOCAL> LocalEntryFilter;
typedef EntryStorageFilter<Variable::STORAGE_SHADER_IN> ShaderInEntryFilter;
typedef EntryStorageFilter<Variable::STORAGE_SHADER_OUT> ShaderOutEntryFilter;

} // namespace rsg

#endif // _RSGVARIABLEMANAGER_HPP
