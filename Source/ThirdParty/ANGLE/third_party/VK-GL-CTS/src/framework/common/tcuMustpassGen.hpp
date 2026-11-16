#ifndef _TCUMUSTPASSGEN_HPP
#define _TCUMUSTPASSGEN_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2026 The Khronos Group Inc.
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
 * \brief Mustpass file generation runmode (--deqp-runmode=gen-mustpass).
 *
 * Reads a per-module mustpass spec file describing one or more configurations
 * (their include/exclude pattern files, output paths, and optional per-group
 * split rules), then walks the test tree once, evaluates every configuration's
 * filters per case in lockstep, and emits the kept case names to per-config
 * output files in alphabetical order.  Output is bit-identical to the
 * pre-existing scripts/mustpass.py-driven pipeline.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"

namespace tcu
{

class TestPackageRoot;
class TestContext;
class CommandLine;

void genMustpassFromSpec(TestPackageRoot &root, TestContext &testCtx, const CommandLine &cmdLine);

} // namespace tcu

#endif // _TCUMUSTPASSGEN_HPP
