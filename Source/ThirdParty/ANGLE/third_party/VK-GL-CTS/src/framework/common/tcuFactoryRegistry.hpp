#ifndef _TCUFACTORYREGISTRY_HPP
#define _TCUFACTORYREGISTRY_HPP
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

#include "tcuDefs.hpp"

#include <string>
#include <vector>

namespace tcu
{

class AbstractFactory
{
public:
    AbstractFactory(void);
    virtual ~AbstractFactory(void);

    virtual const char *getName(void) const = 0;
};

class GenericFactoryRegistry
{
public:
    GenericFactoryRegistry(void);
    ~GenericFactoryRegistry(void);

    size_t size(void) const
    {
        return m_factories.size();
    }
    bool empty(void) const
    {
        return m_factories.empty();
    }

    void registerFactory(AbstractFactory *factory);

    AbstractFactory *getFactoryByName(const std::string &name);
    const AbstractFactory *getFactoryByName(const std::string &name) const;

    AbstractFactory *getFactoryByIndex(size_t index);
    const AbstractFactory *getFactoryByIndex(size_t index) const;

private:
    GenericFactoryRegistry(const GenericFactoryRegistry &);
    GenericFactoryRegistry &operator=(const GenericFactoryRegistry &);

    std::vector<AbstractFactory *> m_factories;
};

class FactoryBase : public AbstractFactory
{
public:
    FactoryBase(const std::string &name, const std::string &description);
    ~FactoryBase(void);

    const char *getName(void) const;
    const char *getDescription(void) const;

private:
    const std::string m_name;
    const std::string m_description;
};

template <class Factory>
class FactoryRegistry
{
public:
    FactoryRegistry(void)
    {
    }
    ~FactoryRegistry(void)
    {
    }

    bool empty(void) const
    {
        return m_registry.empty();
    }
    size_t size(void) const
    {
        return m_registry.size();
    }
    size_t getFactoryCount(void) const
    {
        return m_registry.size();
    }

    void registerFactory(Factory *factory)
    {
        m_registry.registerFactory(factory);
    }

    Factory *getFactoryByName(const std::string &name);
    const Factory *getFactoryByName(const std::string &name) const;

    Factory *getFactoryByIndex(size_t index);
    const Factory *getFactoryByIndex(size_t index) const;

    Factory *getDefaultFactory(void)
    {
        return getFactoryByIndex(0);
    }
    const Factory *getDefaultFactory(void) const
    {
        return getFactoryByIndex(0);
    }

private:
    GenericFactoryRegistry m_registry;
};

template <class Factory>
inline Factory *FactoryRegistry<Factory>::getFactoryByName(const std::string &name)
{
    return static_cast<Factory *>(m_registry.getFactoryByName(name));
}

template <class Factory>
inline const Factory *FactoryRegistry<Factory>::getFactoryByName(const std::string &name) const
{
    return static_cast<const Factory *>(m_registry.getFactoryByName(name));
}

template <class Factory>
inline Factory *FactoryRegistry<Factory>::getFactoryByIndex(size_t index)
{
    return static_cast<Factory *>(m_registry.getFactoryByIndex(index));
}

template <class Factory>
inline const Factory *FactoryRegistry<Factory>::getFactoryByIndex(size_t index) const
{
    return static_cast<const Factory *>(m_registry.getFactoryByIndex(index));
}

} // namespace tcu

#endif // _TCUFACTORYREGISTRY_HPP
