//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MSLSymbolEnv_test.cpp:
//   Tests for the MSL translator's SymbolEnv.
//

#include "compiler/translator/Common.h"
#include "compiler/translator/PoolAlloc.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/msl/SymbolEnv.h"
#include "gtest/gtest.h"

namespace sh
{

// SymbolEnv's address space maps are keyed by VarField, which holds either a TVariable or a
// TField. Verify that a key compares equal only to a key wrapping the same symbol.
TEST(MSLSymbolEnv, VarFieldEquality)
{
    TScopedPoolAllocator scopedAllocator;
    TSymbolTable symbolTable;

    auto makeVariable = [&symbolTable]() -> const TVariable & {
        return *new TVariable(&symbolTable, ImmutableString("v"), new TType(EbtFloat, EbpHigh),
                              SymbolType::AngleInternal);
    };

    const TVariable &firstVariable  = makeVariable();
    const TVariable &secondVariable = makeVariable();
    ASSERT_NE(firstVariable.uniqueId().get(), secondVariable.uniqueId().get());

    const TField &field = *new TField(new TType(EbtFloat, EbpHigh), ImmutableString("f"),
                                      kNoSourceLoc, SymbolType::AngleInternal);

    EXPECT_TRUE(VarField(firstVariable) == VarField(firstVariable));
    EXPECT_TRUE(VarField(field) == VarField(field));
    EXPECT_FALSE(VarField(firstVariable) == VarField(secondVariable));
    EXPECT_FALSE(VarField(field) == VarField(firstVariable));
    EXPECT_FALSE(VarField(firstVariable) == VarField(field));
}

}  // namespace sh
