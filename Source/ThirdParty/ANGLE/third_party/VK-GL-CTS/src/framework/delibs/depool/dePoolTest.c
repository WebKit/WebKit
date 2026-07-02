/*-------------------------------------------------------------------------
 * drawElements Memory Pool Library
 * --------------------------------
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
 * \brief Memory pool testing.
 *//*--------------------------------------------------------------------*/

#include "dePoolTest.h"
#include "dePoolArray.h"
#include "dePoolHeap.h"
#include "dePoolHash.h"
#include "dePoolSet.h"
#include "dePoolHashSet.h"
#include "dePoolHashArray.h"
#include "dePoolMultiSet.h"

void dePool_selfTest(void)
{
    dePoolArray_selfTest();
    dePoolHeap_selfTest();
    dePoolHash_selfTest();
    dePoolSet_selfTest();
    dePoolHashSet_selfTest();
    dePoolHashArray_selfTest();
    dePoolMultiSet_selfTest();
}
