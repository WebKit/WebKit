/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMtlTrampoline_DEFINED
#define GrMtlTrampoline_DEFINED

#include "include/gpu/GrTypes.h"

#include <memory>

class GrDirectContext;
class GrGpu;
struct GrContextOptions;
struct GrMtlBackendContext;

/*
 * This class is used to hold functions which trampoline from the Ganesh cpp code to the GrMtl
 * objective-c files.
 */
class GrMtlTrampoline {
public:
    static std::unique_ptr<GrGpu> MakeGpu(const GrMtlBackendContext&, const GrContextOptions&,
                                          GrDirectContext*);
};

#endif

