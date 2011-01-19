//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// This file contains the nspr specific functions
//
#include "compiler/osinclude.h"

//
// Thread Local Storage Operations
//
OS_TLSIndex OS_AllocTLSIndex()
{
    PRUintn index;
    PRStatus status = PR_NewThreadPrivateIndex(&index, NULL);

    if (status) {
        assert(0 && "OS_AllocTLSIndex(): Unable to allocate Thread Local Storage");
        return OS_INVALID_TLS_INDEX;
    }

    return index;
}

bool OS_SetTLSValue(OS_TLSIndex nIndex, void *lpvValue)
{
    if (nIndex == OS_INVALID_TLS_INDEX) {
        assert(0 && "OS_SetTLSValue(): Invalid TLS Index");
        return false;
    }

    return PR_SetThreadPrivate(nIndex, lpvValue) == 0;
}

bool OS_FreeTLSIndex(OS_TLSIndex nIndex)
{
    // Can't delete TLS keys with nspr
    return true;
}

