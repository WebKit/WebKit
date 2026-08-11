/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief Memory object stress test
 *//*--------------------------------------------------------------------*/

#include "es3sMemoryTests.hpp"

#include "glsMemoryStressCase.hpp"
#include "deRandom.hpp"
#include "tcuTestLog.hpp"

#include "glw.h"

#include <vector>
#include <iostream>

using std::vector;
using tcu::TestLog;

using namespace deqp::gls;

namespace deqp
{
namespace gles3
{
namespace Stress
{

MemoryTests::MemoryTests(Context &testCtx) : TestCaseGroup(testCtx, "memory", "Memory stress tests")
{
}

MemoryTests::~MemoryTests(void)
{
}

void MemoryTests::init(void)
{
    const int MiB = 1024 * 1024;

    // Basic tests
    tcu::TestCaseGroup *basicGroup = new TestCaseGroup(m_context, "basic", "Basic allocation stress tests.");

    // Buffers
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, false,
        false, false, false, "buffer_1mb_no_write_no_use", "1MiB buffer allocations, no data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, true,
        false, false, false, "buffer_1mb_write_no_use", "1MiB buffer allocations, data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, false,
        true, false, false, "buffer_1mb_no_write_use", "1MiB buffer allocations, no data writes, data used"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, true,
        true, false, false, "buffer_1mb_write_use", "1MiB buffer allocations, data writes, data used"));

    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, false,
        false, false, false, "buffer_8mb_no_write_no_use", "8MiB buffer allocations, no data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, true,
        false, false, false, "buffer_8mb_write_no_use", "8MiB buffer allocations, data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, false,
        true, false, false, "buffer_8mb_no_write_use", "8MiB buffer allocations, no data writes, data used"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, true,
        true, false, false, "buffer_8mb_write_use", "8MiB buffer allocations, data writes, data used"));

    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, false,
        false, false, false, "buffer_32mb_no_write_no_use", "32MiB buffer allocations, no data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, true,
        false, false, false, "buffer_32mb_write_no_use", "32MiB buffer allocations, data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, false,
        true, false, false, "buffer_32mb_no_write_use", "32MiB buffer allocations, no data writes, data used"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, true,
        true, false, false, "buffer_32mb_write_use", "32MiB buffer allocations, data writes, data used"));

    // Textures
    basicGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                              MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, false, false, false, false,
                                              "texture_512x512_rgba_no_write_no_use",
                                              "512x512 RGBA texture allocations, no data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, true, false,
        false, false, "texture_512x512_rgba_write_no_use", "512x512 RGBA texture allocations, data writes, no use"));
    basicGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                              MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, false, true, false, false,
                                              "texture_512x512_rgba_no_write_use",
                                              "512x512 RGBA texture allocations, no data writes, data used"));
    basicGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, true, true,
        false, false, "texture_512x512_rgba_write_use", "512x512 RGBA texture allocations, data writes, data used"));

    // Random tests
    tcu::TestCaseGroup *randomGroup = new TestCaseGroup(m_context, "random", "Random allocation stress tests.");

    // Buffers
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, false, false,
        false, false, "buffer_small_no_write_no_use", "Random small buffer allocations, no data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, true, false,
        false, false, "buffer_small_write_no_use", "Random small allocations, data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, false, true,
        false, false, "buffer_small_no_write_use", "Random small allocations, no data writes, data used"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, true, true,
        false, false, "buffer_small_write_use", "Random small allocations, data writes, data used"));

    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, false, false, false,
                                               false, "buffer_large_no_write_no_use",
                                               "Random large buffer allocations, no data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, true,
        false, false, false, "buffer_large_write_no_use", "Random large buffer allocations, data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, false,
        true, false, false, "buffer_large_no_write_use", "Random large buffer allocations, no data writes, data used"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, true,
        true, false, false, "buffer_large_write_use", "Random large buffer allocations, data writes, data used"));

    // Textures
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, false, false,
        false, false, "texture_small_rgba_no_write_no_use", "Small RGBA texture allocations, no data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, true, false,
        false, false, "texture_small_rgba_write_no_use", "Small RGBA texture allocations, data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, false, true,
        false, false, "texture_small_rgba_no_write_use", "Small RGBA texture allocations, no data writes, data used"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, true, true,
        false, false, "texture_small_rgba_write_use", "Small RGBA texture allocations, data writes, data used"));

    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, false, false,
        false, false, "texture_large_rgba_no_write_no_use", "Large RGBA texture allocations, no data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, true, false,
        false, false, "texture_large_rgba_write_no_use", "Large RGBA texture allocations, data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, false, true,
        false, false, "texture_large_rgba_no_write_use", "Large RGBA texture allocations, no data writes, data used"));
    randomGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, true, true,
        false, false, "texture_large_rgba_write_use", "Large RGBA texture allocations, data writes, data used"));

    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB, false,
                                               false, false, false, "mixed_small_rgba_no_write_no_use",
                                               "Small RGBA mixed allocations, no data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB, true,
                                               false, false, false, "mixed_small_rgba_write_no_use",
                                               "Small RGBA mixed allocations, data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB, false,
                                               true, false, false, "mixed_small_rgba_no_write_use",
                                               "Small RGBA mixed allocations, no data writes, data used"));
    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB, true,
                                               true, false, false, "mixed_small_rgba_write_use",
                                               "Small RGBA mixed allocations, data writes, data used"));

    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512, 1024, 2 * MiB,
                                               32 * MiB, false, false, false, false, "mixed_large_rgba_no_write_no_use",
                                               "Large RGBA mixed allocations, no data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512, 1024, 2 * MiB,
                                               32 * MiB, true, false, false, false, "mixed_large_rgba_write_no_use",
                                               "Large RGBA mixed allocations, data writes, no use"));
    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512, 1024, 2 * MiB,
                                               32 * MiB, false, true, false, false, "mixed_large_rgba_no_write_use",
                                               "Large RGBA mixed allocations, no data writes, data used"));
    randomGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                               MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512, 1024, 2 * MiB,
                                               32 * MiB, true, true, false, false, "mixed_large_rgba_write_use",
                                               "Large RGBA mixed allocations, data writes, data used"));

    addChild(basicGroup);
    addChild(randomGroup);

    // Basic tests with clear
    tcu::TestCaseGroup *basicClearGroup =
        new TestCaseGroup(m_context, "basic_clear", "Basic allocation stress tests with glClear after OOM.");

    // Buffers
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, false,
        false, false, true, "buffer_1mb_no_write_no_use", "1MiB buffer allocations, no data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, true,
        false, false, true, "buffer_1mb_write_no_use", "1MiB buffer allocations, data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, false,
        true, false, true, "buffer_1mb_no_write_use", "1MiB buffer allocations, no data writes, data used"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 1 * MiB, 1 * MiB, true,
        true, false, true, "buffer_1mb_write_use", "1MiB buffer allocations, data writes, data used"));

    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, false,
        false, false, true, "buffer_8mb_no_write_no_use", "8MiB buffer allocations, no data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, true,
        false, false, true, "buffer_8mb_write_no_use", "8MiB buffer allocations, data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, false,
        true, false, true, "buffer_8mb_no_write_use", "8MiB buffer allocations, no data writes, data used"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 8 * MiB, 8 * MiB, true,
        true, false, true, "buffer_8mb_write_use", "8MiB buffer allocations, data writes, data used"));

    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, false,
        false, false, true, "buffer_32mb_no_write_no_use", "32MiB buffer allocations, no data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, true,
        false, false, true, "buffer_32mb_write_no_use", "32MiB buffer allocations, data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, false,
        true, false, true, "buffer_32mb_no_write_use", "32MiB buffer allocations, no data writes, data used"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 32 * MiB, 32 * MiB, true,
        true, false, true, "buffer_32mb_write_use", "32MiB buffer allocations, data writes, data used"));

    // Textures
    basicClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                   MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, false, false, false, true,
                                                   "texture_512x512_rgba_no_write_no_use",
                                                   "512x512 RGBA texture allocations, no data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, true, false,
        false, true, "texture_512x512_rgba_write_no_use", "512x512 RGBA texture allocations, data writes, no use"));
    basicClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                   MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, false, true, false, true,
                                                   "texture_512x512_rgba_no_write_use",
                                                   "512x512 RGBA texture allocations, no data writes, data used"));
    basicClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 512, 0, 0, true, true,
        false, true, "texture_512x512_rgba_write_use", "512x512 RGBA texture allocations, data writes, data used"));

    // Random tests
    tcu::TestCaseGroup *randomClearGroup =
        new TestCaseGroup(m_context, "random_clear", "Random allocation stress tests with glClear after OOM.");

    // Buffers
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, false, false,
        false, true, "buffer_small_no_write_no_use", "Random small buffer allocations, no data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, true, false,
        false, true, "buffer_small_write_no_use", "Random small allocations, data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, false, true,
        false, true, "buffer_small_no_write_use", "Random small allocations, no data writes, data used"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 64, 1 * MiB, true, true,
        false, true, "buffer_small_write_use", "Random small allocations, data writes, data used"));

    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, false,
        false, false, true, "buffer_large_no_write_no_use", "Random large buffer allocations, no data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, true,
        false, false, true, "buffer_large_write_no_use", "Random large buffer allocations, data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, false,
        true, false, true, "buffer_large_no_write_use", "Random large buffer allocations, no data writes, data used"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_BUFFER, 0, 0, 2 * MiB, 32 * MiB, true,
        true, false, true, "buffer_large_write_use", "Random large buffer allocations, data writes, data used"));

    // Textures
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, false, false,
        false, true, "texture_small_rgba_no_write_no_use", "Small RGBA texture allocations, no data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, true, false,
        false, true, "texture_small_rgba_write_no_use", "Small RGBA texture allocations, data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, false, true,
        false, true, "texture_small_rgba_no_write_use", "Small RGBA texture allocations, no data writes, data used"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 8, 256, 0, 0, true, true,
        false, true, "texture_small_rgba_write_use", "Small RGBA texture allocations, data writes, data used"));

    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, false, false,
        false, true, "texture_large_rgba_no_write_no_use", "Large RGBA texture allocations, no data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, true, false,
        false, true, "texture_large_rgba_write_no_use", "Large RGBA texture allocations, data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, false, true,
        false, true, "texture_large_rgba_no_write_use", "Large RGBA texture allocations, no data writes, data used"));
    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE, 512, 1024, 0, 0, true, true,
        false, true, "texture_large_rgba_write_use", "Large RGBA texture allocations, data writes, data used"));

    randomClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                    MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB,
                                                    false, false, false, true, "mixed_small_rgba_no_write_no_use",
                                                    "Small RGBA mixed allocations, no data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                    MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB,
                                                    true, false, false, true, "mixed_small_rgba_write_no_use",
                                                    "Small RGBA mixed allocations, data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                    MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB,
                                                    false, true, false, true, "mixed_small_rgba_no_write_use",
                                                    "Small RGBA mixed allocations, no data writes, data used"));
    randomClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                    MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 8, 256, 64, 1 * MiB,
                                                    true, true, false, true, "mixed_small_rgba_write_use",
                                                    "Small RGBA mixed allocations, data writes, data used"));

    randomClearGroup->addChild(new MemoryStressCase(
        m_context.getTestContext(), m_context.getRenderContext(), MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512,
        1024, 2 * MiB, 32 * MiB, false, false, false, true, "mixed_large_rgba_no_write_no_use",
        "Large RGBA mixed allocations, no data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                    MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512, 1024, 2 * MiB,
                                                    32 * MiB, true, false, false, true, "mixed_large_rgba_write_no_use",
                                                    "Large RGBA mixed allocations, data writes, no use"));
    randomClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                    MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512, 1024, 2 * MiB,
                                                    32 * MiB, false, true, false, true, "mixed_large_rgba_no_write_use",
                                                    "Large RGBA mixed allocations, no data writes, data used"));
    randomClearGroup->addChild(new MemoryStressCase(m_context.getTestContext(), m_context.getRenderContext(),
                                                    MEMOBJECTTYPE_TEXTURE | MEMOBJECTTYPE_BUFFER, 512, 1024, 2 * MiB,
                                                    32 * MiB, true, true, false, true, "mixed_large_rgba_write_use",
                                                    "Large RGBA mixed allocations, data writes, data used"));

    addChild(basicClearGroup);
    addChild(randomClearGroup);
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
