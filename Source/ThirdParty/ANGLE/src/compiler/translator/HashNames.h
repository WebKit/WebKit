//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_HASHNAMES_H_
#define COMPILER_TRANSLATOR_HASHNAMES_H_

#include <map>
#include <unordered_set>

#include "GLSLANG/ShaderLang.h"
#include "compiler/translator/Common.h"

namespace sh
{

class NameMap
{
  public:
    void insert(const TPersistString &name, const TPersistString &hashedName)
    {
        mNames[name] = hashedName;
        mHashedNames.insert(hashedName);
    }

    void clear()
    {
        mNames.clear();
        mHashedNames.clear();
    }

    bool containsHashedName(const TPersistString &hashedName) const
    {
        return mHashedNames.find(hashedName) != mHashedNames.end();
    }

    const std::map<TPersistString, TPersistString> &getInternalMap() const { return mNames; }

    using const_iterator = std::map<TPersistString, TPersistString>::const_iterator;

    const_iterator find(const TPersistString &name) const { return mNames.find(name); }
    const_iterator end() const { return mNames.end(); }
    const_iterator begin() const { return mNames.begin(); }

  private:
    std::map<TPersistString, TPersistString> mNames;
    std::unordered_set<TPersistString> mHashedNames;
};

class ImmutableString;
class TSymbol;

ImmutableString HashName(const ImmutableString &name,
                         char prefix,
                         ShHashFunction64 hashFunction,
                         NameMap *nameMap);

// Hash user-defined name for GLSL output, with special handling for internal names.
// The nameMap parameter is optional and is used to cache hashed names if set.
ImmutableString HashName(const TSymbol *symbol,
                         char prefix,
                         ShHashFunction64 hashFunction,
                         NameMap *nameMap);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_HASHNAMES_H_
