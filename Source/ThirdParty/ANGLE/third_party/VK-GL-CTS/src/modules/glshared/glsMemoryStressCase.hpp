#ifndef _GLSMEMORYSTRESSCASE_HPP
#define _GLSMEMORYSTRESSCASE_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Module
 * ---------------------------------------------
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

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"
#include "gluRenderContext.hpp"

#include <vector>

namespace deqp
{
namespace gls
{

enum MemObjectType
{
    MEMOBJECTTYPE_TEXTURE = (1 << 0),
    MEMOBJECTTYPE_BUFFER  = (1 << 1),

    MEMOBJECTTYPE_LAST
};

struct MemObjectConfig
{
    int minBufferSize;
    int maxBufferSize;

    int minTextureSize;
    int maxTextureSize;

    bool useUnusedData;
    bool write;
    bool use;
};

class MemoryStressCase : public tcu::TestCase
{
public:
    MemoryStressCase(tcu::TestContext &testCtx, glu::RenderContext &renderContext, uint32_t objectTypes,
                     int minTextureSize, int maxTextureSize, int minBufferSize, int maxBufferSize, bool write, bool use,
                     bool useUnusedData, bool clearAfterOOM, const char *name, const char *desc);
    ~MemoryStressCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    int m_iteration;
    int m_iterationCount;
    std::vector<int> m_allocated;
    MemObjectType m_objectTypes;
    MemObjectConfig m_config;
    bool m_zeroAlloc;
    bool m_clearAfterOOM;
    glu::RenderContext &m_renderCtx;

    MemoryStressCase(const MemoryStressCase &);
    MemoryStressCase &operator=(const MemoryStressCase &);
};

} // namespace gls
} // namespace deqp

#endif // _GLSMEMORYSTRESSCASE_HPP
