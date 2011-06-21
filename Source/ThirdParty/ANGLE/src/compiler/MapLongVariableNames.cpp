//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/MapLongVariableNames.h"

namespace {

TString mapLongName(int id, const TString& name, bool isVarying)
{
    ASSERT(name.size() > MAX_IDENTIFIER_NAME_SIZE);
    TStringStream stream;
    stream << "webgl_";
    if (isVarying)
        stream << "v";
    stream << id << "_";
    stream << name.substr(0, MAX_IDENTIFIER_NAME_SIZE - stream.str().size());
    return stream.str();
}

}  // anonymous namespace

MapLongVariableNames::MapLongVariableNames(
    TMap<TString, TString>& varyingLongNameMap)
    : mVaryingLongNameMap(varyingLongNameMap)
{
}

void MapLongVariableNames::visitSymbol(TIntermSymbol* symbol)
{
    ASSERT(symbol != NULL);
    if (symbol->getSymbol().size() > MAX_IDENTIFIER_NAME_SIZE) {
        switch (symbol->getQualifier()) {
          case EvqVaryingIn:
          case EvqVaryingOut:
          case EvqInvariantVaryingIn:
          case EvqInvariantVaryingOut:
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

void MapLongVariableNames::visitConstantUnion(TIntermConstantUnion*)
{
}

bool MapLongVariableNames::visitBinary(Visit, TIntermBinary*)
{
    return true;
}

bool MapLongVariableNames::visitUnary(Visit, TIntermUnary*)
{
    return true;
}

bool MapLongVariableNames::visitSelection(Visit, TIntermSelection*)
{
    return true;
}

bool MapLongVariableNames::visitAggregate(Visit, TIntermAggregate*)
{
    return true;
}

bool MapLongVariableNames::visitLoop(Visit, TIntermLoop* node)
{
    if (node->getInit())
        node->getInit()->traverse(this);
    return true;
}

bool MapLongVariableNames::visitBranch(Visit, TIntermBranch*)
{
    return true;
}

TString MapLongVariableNames::mapVaryingLongName(const TString& name)
{
    TMap<TString, TString>::const_iterator it = mVaryingLongNameMap.find(name);
    if (it != mVaryingLongNameMap.end())
        return (*it).second;

    int id = mVaryingLongNameMap.size();
    TString mappedName = mapLongName(id, name, true);
    mVaryingLongNameMap.insert(
        TMap<TString, TString>::value_type(name, mappedName));
    return mappedName;
}
