//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <cctype>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "compiler/translator/TranslatorMetalDirect/RewriteKeywords.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

IdGen::IdGen() {}

template <typename String, typename StringToImmutable>
Name IdGen::createNewName(size_t count,
                          const String *baseNames,
                          const StringToImmutable &toImmutable)
{
    const unsigned id = mNext++;
    char idBuffer[std::numeric_limits<unsigned>::digits10 + 1];
    sprintf(idBuffer, "%u", id);

    mNewNameBuffer.clear();
    mNewNameBuffer += '_';
    mNewNameBuffer += idBuffer;

    // Note:
    // Double underscores are only allowed in C++ (and thus Metal) vendor identifiers, so here we
    // take care not to introduce any.

    for (size_t i = 0; i < count; ++i)
    {
        const ImmutableString baseName = toImmutable(baseNames[i]);
        if (!baseName.empty())
        {
            const char *base = baseName.data();
            if (baseName.beginsWith(kAngleInternalPrefix))
            {
                base += sizeof(kAngleInternalPrefix) - 1;
            }
            if (*base == '_')
            {
                ++base;
            }
            ASSERT(*base != '_');

            if (mNewNameBuffer.back() != '_')
            {
                mNewNameBuffer += '_';
            }

            mNewNameBuffer += base;
        }
    }

    return Name(ImmutableString(mNewNameBuffer), SymbolType::AngleInternal);
}

Name IdGen::createNewName(const ImmutableString &baseName)
{
    return createNewName({baseName});
}

Name IdGen::createNewName(const Name &baseName)
{
    return createNewName(baseName.rawName());
}

Name IdGen::createNewName(const char *baseName)
{
    return createNewName(ImmutableString(baseName));
}

Name IdGen::createNewName(std::initializer_list<ImmutableString> baseNames)
{
    return createNewName(baseNames.size(), baseNames.begin(),
                         [](const ImmutableString &s) { return s; });
}

Name IdGen::createNewName(std::initializer_list<Name> baseNames)
{
    return createNewName(baseNames.size(), baseNames.begin(),
                         [](const Name &s) { return s.rawName(); });
}

Name IdGen::createNewName(std::initializer_list<const char *> baseNames)
{
    return createNewName(baseNames.size(), baseNames.begin(),
                         [](const char *s) { return ImmutableString(s); });
}
