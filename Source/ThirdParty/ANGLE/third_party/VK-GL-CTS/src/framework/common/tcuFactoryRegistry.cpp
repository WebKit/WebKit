/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Generic registry class for factories
 *//*--------------------------------------------------------------------*/

#include "tcuFactoryRegistry.hpp"

namespace tcu
{

// AbstractFactory

AbstractFactory::AbstractFactory(void)
{
}

AbstractFactory::~AbstractFactory(void)
{
}

// GenericFactoryRegistry

GenericFactoryRegistry::GenericFactoryRegistry(void)
{
}

GenericFactoryRegistry::~GenericFactoryRegistry(void)
{
    for (std::vector<AbstractFactory *>::const_iterator i = m_factories.begin(); i != m_factories.end(); ++i)
        delete *i;
}

AbstractFactory *GenericFactoryRegistry::getFactoryByIndex(size_t index)
{
    DE_ASSERT(index < m_factories.size());
    return m_factories[index];
}

const AbstractFactory *GenericFactoryRegistry::getFactoryByIndex(size_t index) const
{
    DE_ASSERT(index < m_factories.size());
    return m_factories[index];
}

AbstractFactory *GenericFactoryRegistry::getFactoryByName(const std::string &name)
{
    for (size_t index = 0; index < m_factories.size(); index++)
    {
        if (name == m_factories[index]->getName())
            return m_factories[index];
    }

    return nullptr;
}

const AbstractFactory *GenericFactoryRegistry::getFactoryByName(const std::string &name) const
{
    for (size_t index = 0; index < m_factories.size(); index++)
    {
        if (name == m_factories[index]->getName())
            return m_factories[index];
    }

    return nullptr;
}

void GenericFactoryRegistry::registerFactory(AbstractFactory *factory)
{
    DE_ASSERT(!getFactoryByName(factory->getName()));
    m_factories.push_back(factory);
}

// FactoryBase

FactoryBase::FactoryBase(const std::string &name, const std::string &description)
    : m_name(name)
    , m_description(description)
{
}

FactoryBase::~FactoryBase(void)
{
}

const char *FactoryBase::getName(void) const
{
    return m_name.c_str();
}

const char *FactoryBase::getDescription(void) const
{
    return m_description.c_str();
}

} // namespace tcu
