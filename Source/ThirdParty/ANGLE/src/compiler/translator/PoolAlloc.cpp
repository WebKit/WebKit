//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/PoolAlloc.h"

#include "common/debug.h"
#include "common/tls.h"

TLSIndex PoolIndex = TLS_INVALID_INDEX;

bool InitializePoolIndex()
{
    ASSERT(PoolIndex == TLS_INVALID_INDEX);

    PoolIndex = CreateTLSIndex(nullptr);
    return PoolIndex != TLS_INVALID_INDEX;
}

void FreePoolIndex()
{
    ASSERT(PoolIndex != TLS_INVALID_INDEX);

    DestroyTLSIndex(PoolIndex);
    PoolIndex = TLS_INVALID_INDEX;
}

angle::PoolAllocator *GetGlobalPoolAllocator()
{
    ASSERT(PoolIndex != TLS_INVALID_INDEX);
    return static_cast<angle::PoolAllocator *>(GetTLSValue(PoolIndex));
}

void SetGlobalPoolAllocator(angle::PoolAllocator *poolAllocator)
{
    ASSERT(PoolIndex != TLS_INVALID_INDEX);
    SetTLSValue(PoolIndex, poolAllocator);
}
