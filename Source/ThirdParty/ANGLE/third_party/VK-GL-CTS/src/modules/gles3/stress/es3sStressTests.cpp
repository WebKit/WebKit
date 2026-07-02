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
 * \brief Stress tests.
 *//*--------------------------------------------------------------------*/

#include "es3sStressTests.hpp"

#include "es3sMemoryTests.hpp"
#include "es3sOcclusionQueryTests.hpp"
#include "es3sSyncTests.hpp"
#include "es3sLongRunningTests.hpp"
#include "es3sSpecialFloatTests.hpp"
#include "es3sDrawTests.hpp"
#include "es3sVertexArrayTests.hpp"
#include "es3sLongShaderTests.hpp"
#include "es3sLongRunningShaderTests.hpp"

namespace deqp
{
namespace gles3
{
namespace Stress
{

StressTests::StressTests(Context &context) : TestCaseGroup(context, "stress", "Stress tests")
{
}

StressTests::~StressTests(void)
{
}

void StressTests::init(void)
{
    addChild(new MemoryTests(m_context));
    addChild(new OcclusionQueryTests(m_context));
    addChild(new SyncTests(m_context));
    addChild(new LongRunningTests(m_context));
    addChild(new SpecialFloatTests(m_context));
    addChild(new DrawTests(m_context));
    addChild(new VertexArrayTests(m_context));
    addChild(new LongShaderTests(m_context));
    addChild(new LongRunningShaderTests(m_context));
}

} // namespace Stress
} // namespace gles3
} // namespace deqp
