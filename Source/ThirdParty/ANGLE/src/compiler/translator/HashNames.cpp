//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/HashNames.h"

#include "compiler/translator/IntermNode.h"

namespace sh
{

namespace
{

// GLSL ES 3.00.6 section 3.9: the maximum length of an identifier is 1024 characters.
static const unsigned int kESSLMaxIdentifierLength = 1024u;

static const char *kHashedNamePrefix = "webgl_";

// Can't prefix with just _ because then we might introduce a double underscore, which is not safe
// in GLSL (ESSL 3.00.6 section 3.8: All identifiers containing a double underscore are reserved for
// use by the underlying implementation). u is short for user-defined.
static const char *kUnhashedNamePrefix              = "_u";
static const unsigned int kUnhashedNamePrefixLength = 2u;

TString HashName(const TString &name, ShHashFunction64 hashFunction)
{
    ASSERT(!name.empty());
    ASSERT(hashFunction);
    khronos_uint64_t number = (*hashFunction)(name.c_str(), name.length());
    TStringStream stream;
    stream << kHashedNamePrefix << std::hex << number;
    TString hashedName = stream.str();
    return hashedName;
}

}  // anonymous namespace

TString HashName(const TName &name, ShHashFunction64 hashFunction, NameMap *nameMap)
{
    if (name.getString().empty() || name.isInternal())
    {
        return name.getString();
    }
    if (hashFunction == nullptr)
    {
        if (name.getString().length() + kUnhashedNamePrefixLength > kESSLMaxIdentifierLength)
        {
            // If the identifier length is already close to the limit, we can't prefix it. This is
            // not a problem since there are no builtins or ANGLE's internal variables that would
            // have as long names and could conflict.
            return name.getString();
        }
        return kUnhashedNamePrefix + name.getString();
    }
    if (nameMap)
    {
        NameMap::const_iterator it = nameMap->find(name.getString().c_str());
        if (it != nameMap->end())
            return it->second.c_str();
    }
    TString hashedName = HashName(name.getString(), hashFunction);
    if (nameMap)
    {
        (*nameMap)[name.getString().c_str()] = hashedName.c_str();
    }
    return hashedName;
}

}  // namespace sh
