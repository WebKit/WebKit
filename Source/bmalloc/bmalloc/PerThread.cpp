/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PerThread.h"

#include "BExport.h"
#include "Cache.h"
#include "Heap.h"

namespace bmalloc {

#if !HAVE_PTHREAD_MACHDEP_H

template<> BEXPORT bool PerThreadStorage<PerHeapKind<Cache>>::s_didInitialize = false;
template<> BEXPORT pthread_key_t PerThreadStorage<PerHeapKind<Cache>>::s_key = 0;
template<> BEXPORT std::once_flag PerThreadStorage<PerHeapKind<Cache>>::s_onceFlag = { };

template<> BEXPORT bool PerThreadStorage<PerHeapKind<Heap>>::s_didInitialize = false;
template<> BEXPORT pthread_key_t PerThreadStorage<PerHeapKind<Heap>>::s_key = 0;
template<> BEXPORT std::once_flag PerThreadStorage<PerHeapKind<Heap>>::s_onceFlag = { };

#endif

} // namespace bmalloc
