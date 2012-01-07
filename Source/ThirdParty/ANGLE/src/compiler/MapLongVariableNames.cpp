//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/MapLongVariableNames.h"

namespace {

TString mapLongName(int id, const TString& name, bool isVarying)
{
    ASSERT(name.size() > MAX_SHORTENED_IDENTIFIER_SIZE);
    TStringStream stream;
    stream << "webgl_";
    if (isVarying)
        stream << "v";
    stream << id << "_";
    stream << name.substr(0, MAX_SHORTENED_IDENTIFIER_SIZE - stream.str().size());
    return stream.str();
}

}  // anonymous namespace

MapLongVariableNames::MapLongVariableNames(
    std::map<std::string, std::string>& varyingLongNameMap)
    : mVaryingLongNameMap(varyingLongNameMap)
{
}

void MapLongVariableNames::visitSymbol(TIntermSymbol* symbol)
{
    ASSERT(symbol != NULL);
    if (symbol->getSymbol().size() > MAX_SHORTENED_IDENTIFIER_SIZE) {
        switch (symbol->getQualifier()) {
          case EvqVaryingIn:
          case EvqVaryingOut:
          case EvqInvariantVaryingIn:
          case EvqInvariantVaryingOut:
          case EvqUniform:
            symbol->setSymbol(
                mapVaryingLongName(symbol->getSymbol()));
            break;
          default:
            symbol->setSymbol(
                mapLongName(symbol->getId(), symbol->getSymbol(), false));
            break;
        };
    }
}

bool MapLongVariableNames::visitLoop(Visit, TIntermLoop* node)
{
    if (node->getInit())
        node->getInit()->traverse(this);
    return true;
}

TString MapLongVariableNames::mapVaryingLongName(const TString& name)
{
    std::map<std::string, std::string>::const_iterator it = mVaryingLongNameMap.find(name.c_str());
    if (it != mVaryingLongNameMap.end())
        return (*it).second.c_str();

    int id = mVaryingLongNameMap.size();
    TString mappedName = mapLongName(id, name, true);
    mVaryingLongNameMap.insert(
        std::map<std::string, std::string>::value_type(name.c_str(), mappedName.c_str()));
    return mappedName;
}
