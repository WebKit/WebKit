//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef __INITIALIZEDLL_H
#define __INITIALIZEDLL_H


#include "compiler/osinclude.h"


bool InitProcess();
bool InitThread();
bool DetachThread();
bool DetachProcess();

#endif // __INITIALIZEDLL_H

